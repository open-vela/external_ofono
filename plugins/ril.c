/*
 *
 *  oFono - Open Source Telephony - RIL-based devices
 *
 *  Copyright (C) 2008-2011  Intel Corporation. All rights reserved.
 *  Copyright (C) 2012-2014 Canonical Ltd.
 *  Copyright (C) 2013 Jolla Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <glib.h>

#define OFONO_API_SUBJECT_TO_CHANGE
#include <ofono/plugin.h>
#include <ofono/log.h>
#include <ofono/modem.h>
#include <ofono/devinfo.h>
#include <ofono/phonebook.h>
#include <ofono/netreg.h>
#include <ofono/voicecall.h>
#include <ofono/sms.h>
#include <ofono/cbs.h>
#include <ofono/sim.h>
#include <ofono/ussd.h>
#include <ofono/ims.h>
#include <ofono/call-forwarding.h>
#include <ofono/call-settings.h>
#include <ofono/call-barring.h>
#include <ofono/call-meter.h>
#include <ofono/call-volume.h>
#include <ofono/radio-settings.h>
#include <ofono/gprs.h>
#include <ofono/gprs-context.h>
#include <ofono/audio-settings.h>
#include <ofono/types.h>
#include <ofono/abnormal-event.h>

#include <gril/gril.h>

#include "ofono.h"

#include "ril.h"
#include "drivers/rilmodem/rilmodem.h"
#include "drivers/rilmodem/vendor.h"
#include "src/common.h"

#define	RADIO_GID 1001
#define	RADIO_UID 1001

#define MAX_SIM_STATUS_RETRIES 15

/* this gives 30s retry for rild to initialize */
#define RILD_MAX_CONNECT_RETRIES 30
#define RILD_CONNECT_RETRY_TIME_S 1

char *RILD_CMD_SOCKET[] = {"/dev/socket/rild", "/dev/socket/rild1"};
char *GRIL_HEX_PREFIX[] = {"Device 0: ", "Device 1: "};

struct ril_data {
	GRil *ril;
	enum ofono_ril_vendor vendor;
	int sim_status_retries;
	ofono_bool_t connected;
	ofono_bool_t ofono_online;
	int radio_state;
	struct ofono_sim *sim;
	struct ofono_radio_settings *radio_settings;
	int rild_connect_retries;
	unsigned int sim_watch_for_phonebook;
};

static void ril_debug(const char *str, void *user_data)
{
	const char *prefix = user_data;

	ofono_info("%s%s", prefix, str);
}

static void ril_radio_state_changed(struct ril_msg *message, gpointer user_data)
{
	struct ofono_modem *modem = user_data;
	struct ril_data *rd = ofono_modem_get_data(modem);
	struct parcel rilp;
	int radio_state;

	g_ril_init_parcel(message, &rilp);

	radio_state = parcel_r_int32(&rilp);

	if (rilp.malformed) {
		ofono_error("%s: malformed parcel received", __func__);
		ofono_modem_set_powered(modem, FALSE);
		return;
	}

	g_ril_append_print_buf(rd->ril, "(state: %s)",
				ril_radio_state_to_string(radio_state));
	g_ril_print_unsol(rd->ril, message);

	if (radio_state != rd->radio_state) {
		ofono_info("%s: state: %s rd->ofono_online: %d",
				__func__,
				ril_radio_state_to_string(radio_state),
				rd->ofono_online);

		rd->radio_state = radio_state;

		switch (radio_state) {
		case RADIO_STATE_ON:
			break;

		case RADIO_STATE_UNAVAILABLE:
		case RADIO_STATE_OFF:
			/*
			 * Unexpected radio state change, as we are supposed to
			 * be online. UNAVAILABLE has been seen occassionally
			 * when powering off the phone. We wait 5 secs to avoid
			 * too fast re-spawns, then exit with error to make
			 * upstart re-start ofono.
			 */
			if (rd->ofono_online)
				ofono_error("%s: radio self-powered off!",
						__func__);

			break;
		}

		ofono_modem_process_radio_state(modem, radio_state);
	}
}

int ril_create(struct ofono_modem *modem, enum ofono_ril_vendor vendor)
{
	ofono_bool_t lte_cap;
	struct ril_data *rd = g_try_new0(struct ril_data, 1);
	if (rd == NULL) {
		errno = ENOMEM;
		goto error;
	}

	DBG("");

	rd->vendor = vendor;
	rd->ofono_online = FALSE;
	rd->radio_state = RADIO_STATE_OFF;
	rd->sim_watch_for_phonebook = 0;

	lte_cap = getenv("OFONO_RIL_RAT_LTE") ? TRUE : FALSE;
	ofono_modem_set_boolean(modem, MODEM_PROP_LTE_CAPABLE, lte_cap);

	ofono_modem_set_data(modem, rd);

	return 0;

error:
	g_free(rd);

	return -errno;
}

static int ril_probe(struct ofono_modem *modem)
{
	return ril_create(modem, OFONO_RIL_VENDOR_AOSP);
}

void ril_remove(struct ofono_modem *modem)
{
	struct ril_data *rd = ofono_modem_get_data(modem);

	ofono_modem_set_data(modem, NULL);

	if (!rd)
		return;

	if (rd->sim_watch_for_phonebook) {
		__ofono_modem_remove_atom_watch(modem, rd->sim_watch_for_phonebook);
		rd->sim_watch_for_phonebook = 0;
	}

	g_ril_unref(rd->ril);

	g_free(rd);
}

void phonebook_create(struct ofono_atom *atom,
			enum ofono_atom_watch_condition cond, void *data)
{
	struct ofono_modem *modem = data;

	ofono_debug("phonebook create");

	struct ril_data *rd = ofono_modem_get_data(modem);

	if (cond == OFONO_ATOM_WATCH_CONDITION_REGISTERED) {
		ofono_phonebook_create(modem, rd->vendor, RILMODEM, modem);
	}
}

void ofono_phonebook_pre_create(struct ofono_modem *modem)
{
	ofono_debug("phonebook pre create");

	struct ril_data *rd = ofono_modem_get_data(modem);

	rd->sim_watch_for_phonebook = __ofono_modem_add_atom_watch(modem, OFONO_ATOM_TYPE_SIM,
							phonebook_create, modem, NULL);
}

void ril_pre_sim(struct ofono_modem *modem)
{
	struct ril_data *rd = ofono_modem_get_data(modem);

	DBG("");

	ofono_radio_settings_create(modem, rd->vendor, RILMODEM, rd->ril);
	ofono_devinfo_create(modem, rd->vendor, RILMODEM, rd->ril);
	ofono_voicecall_create(modem, rd->vendor, RILMODEM, rd->ril);
	ofono_call_volume_create(modem, rd->vendor, RILMODEM, rd->ril);

	if(rd->sim_watch_for_phonebook == 0)
		ofono_phonebook_pre_create(modem);

	rd->sim = ofono_sim_create(modem, rd->vendor, RILMODEM, rd->ril);
}

void ril_post_sim(struct ofono_modem *modem)
{
	struct ril_data *rd = ofono_modem_get_data(modem);
	struct ofono_message_waiting *mw;

	/* TODO: this function should setup:
	 *  - phonebook
	 *  - stk ( SIM toolkit )
	 *  - radio_settings
	 */
	ofono_sms_create(modem, rd->vendor, RILMODEM, rd->ril);

	mw = ofono_message_waiting_create(modem);
	if (mw)
		ofono_message_waiting_register(mw);

	ofono_call_forwarding_create(modem, rd->vendor, RILMODEM, rd->ril);
	ofono_stk_create(modem, rd->vendor, RILMODEM, rd->ril);
}

void ril_post_online(struct ofono_modem *modem)
{
	struct ril_data *rd = ofono_modem_get_data(modem);
	struct ofono_gprs *gprs;
	struct ofono_gprs_context *gc;

	ofono_cbs_create(modem, rd->vendor, RILMODEM, rd->ril);
	ofono_netreg_create(modem, rd->vendor, RILMODEM, rd->ril);
	ofono_netmon_create(modem, rd->vendor, RILMODEM, rd->ril);
	ofono_ussd_create(modem, rd->vendor, RILMODEM, rd->ril);
	ofono_call_settings_create(modem, rd->vendor, RILMODEM, rd->ril);
	ofono_call_barring_create(modem, rd->vendor, RILMODEM, rd->ril);
	ofono_ims_create(modem, RILMODEM, rd->ril);
	ofono_lte_create(modem, rd->vendor, RILMODEM, rd->ril);

	gprs = ofono_gprs_create(modem, rd->vendor, RILMODEM, rd->ril);
	if (is_gprs_context_type_support("internet")) {
		gc = ofono_gprs_context_create(modem, rd->vendor, RILMODEM, rd->ril);

		if (gc) {
			ofono_gprs_context_set_type(gc,
						OFONO_GPRS_CONTEXT_TYPE_INTERNET);
			ofono_gprs_add_context(gprs, gc);
		}
	}

	if (is_gprs_context_type_support("mms")) {
		gc = ofono_gprs_context_create(modem, rd->vendor, RILMODEM, rd->ril);

		if (gc) {
			ofono_gprs_context_set_type(gc, OFONO_GPRS_CONTEXT_TYPE_MMS);
			ofono_gprs_add_context(gprs, gc);
		}
	}

	if (is_gprs_context_type_support("ims")) {
		gc = ofono_gprs_context_create(modem, rd->vendor, RILMODEM, rd->ril);

		if (gc) {
			ofono_gprs_context_set_type(gc, OFONO_GPRS_CONTEXT_TYPE_IMS);
			ofono_gprs_add_context(gprs, gc);
		}
	}

	if (is_gprs_context_type_support("hipri")) {
		gc = ofono_gprs_context_create(modem, rd->vendor, RILMODEM, rd->ril);

		if (gc) {
			ofono_gprs_context_set_type(gc, OFONO_GPRS_CONTEXT_TYPE_HIPRI);
			ofono_gprs_add_context(gprs, gc);
		}
	}

	if (is_gprs_context_type_support("supl")) {
		gc = ofono_gprs_context_create(modem, rd->vendor, RILMODEM, rd->ril);

		if (gc) {
			ofono_gprs_context_set_type(gc, OFONO_GPRS_CONTEXT_TYPE_SUPL);
			ofono_gprs_add_context(gprs, gc);
		}
	}

	if (is_gprs_context_type_support("emergency")) {
		gc = ofono_gprs_context_create(modem, rd->vendor, RILMODEM, rd->ril);

		if (gc) {
			ofono_gprs_context_set_type(gc, OFONO_GPRS_CONTEXT_TYPE_EMERGENCY);
			ofono_gprs_add_context(gprs, gc);
		}
	}
}

static void ril_set_online_cb(struct ril_msg *message, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	struct ril_data *rd = cbd->user;
	ofono_modem_online_cb_t cb = cbd->cb;

	g_ril_print_response_no_args(rd->ril, message);

	if (message->error == RIL_E_SUCCESS) {
		ofono_debug("%s: set_online OK: rd->ofono_online: %d", __func__,
			rd->ofono_online);
		CALLBACK_WITH_SUCCESS(cb, cbd->data);
	} else {
		ofono_error("%s: set_online: %d failed", __func__,
				rd->ofono_online);
		CALLBACK_WITH_FAILURE(cb, cbd->data);
	}
}

static void ril_send_power(struct ril_data *rd, ofono_bool_t online,
				GRilResponseFunc func,
				gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_modem_online_cb_t cb;
	GDestroyNotify notify = NULL;
	struct parcel rilp;

	if (cbd != NULL) {
		notify = g_free;
		cb = cbd->cb;
	}

	DBG("(online = 1, offline = 0)): %i", online);

	parcel_init(&rilp);
	parcel_w_int32(&rilp, 1);
	parcel_w_int32(&rilp, online);

	g_ril_append_print_buf(rd->ril, "(%d)", online);

	if (g_ril_send(rd->ril, RIL_REQUEST_RADIO_POWER, &rilp,
			func, cbd, notify) == 0 && cbd != NULL) {
		CALLBACK_WITH_FAILURE(cb, cbd->data);
		g_free(cbd);
	}
}

void ril_set_online(struct ofono_modem *modem, ofono_bool_t online,
			ofono_modem_online_cb_t callback, void *data)
{
	struct ril_data *rd = ofono_modem_get_data(modem);
	struct cb_data *cbd = cb_data_new(callback, data, rd);

	rd->ofono_online = online;

	DBG("setting rd->ofono_online to: %d", online);

	ril_send_power(rd, online, ril_set_online_cb, cbd);
}

static void ril_connected(struct ril_msg *message, gpointer user_data)
{
	struct ofono_modem *modem = (struct ofono_modem *) user_data;
	struct ril_data *rd = ofono_modem_get_data(modem);

	ofono_info("[%d,UNSOL]< %s", g_ril_get_slot(rd->ril),
		g_ril_unsol_request_to_string(rd->ril, message->req));

	/* TODO: need a disconnect function to restart things! */
	rd->connected = TRUE;

	DBG("calling set_powered(TRUE)");

	ofono_modem_set_powered(modem, TRUE);
}

static void ril_modem_restart(struct ril_msg *message, gpointer user_data)
{
	struct ofono_modem *modem = (struct ofono_modem *) user_data;
	struct ril_data *rd = ofono_modem_get_data(modem);

	ofono_info("[%d,UNSOL]< %s", g_ril_get_slot(rd->ril),
		g_ril_unsol_request_to_string(rd->ril, message->req));

	ofono_modem_restart(modem);
}

static void ril_oem_hook_raw(struct ril_msg *message, gpointer user_data)
{
	struct ofono_modem *modem = (struct ofono_modem *) user_data;
	struct ril_data *rd = ofono_modem_get_data(modem);
	struct parcel rilp;
	unsigned char *response;
	int len;

	g_ril_init_parcel(message, &rilp);
	if (rilp.malformed) {
		ofono_error("%s: malformed parcel received", __func__);
		return;
	}

	response = parcel_r_raw(&rilp, &len);
	if (response == NULL) {
		ofono_error("%s: no strings", __func__);
		return;
	}

	g_ril_print_unsol(rd->ril, message);
	ofono_oem_hook_raw(modem, response, len);
	g_free(response);
}

static void ril_abnormal_event(struct ril_msg *message, gpointer user_data)
{
	struct parcel rilp;
	int type_id;
	char *data;
	int data_len;
	struct ofono_modem *modem = (struct ofono_modem *) user_data;

	g_ril_init_parcel(message, &rilp);
	type_id = parcel_r_int32(&rilp);
	data_len = parcel_r_int32(&rilp);
	data = parcel_r_string(&rilp);
	ofono_handle_abnormal_event(modem, type_id, data, data_len);

	g_free(data);
}

static int create_gril(struct ofono_modem *modem)
{
	struct ril_data *rd = ofono_modem_get_data(modem);
	int slot_id = ofono_modem_get_integer(modem, "Slot");
#ifndef CONFIG_ARCH_SIM
	ofono_info("Using %s as socket for slot %d.",
					RILD_CMD_SOCKET[slot_id], slot_id);
#endif
	/* RIL expects user radio to connect to the socket */
	rd->ril = g_ril_new_with_ucred(RILD_CMD_SOCKET[slot_id],
						OFONO_RIL_VENDOR_AOSP,
						RADIO_UID, RADIO_GID);

	/* NOTE: Since AT modems open a tty, and then call
	 * g_at_chat_new(), they're able to return -EIO if
	 * the first fails, and -ENOMEM if the second fails.
	 * in our case, we already return -EIO if the ril_new
	 * fails.  If this is important, we can create a ril_socket
	 * abstraction... ( probaby not a bad idea ).
	 */

	if (rd->ril == NULL) {
#ifndef CONFIG_ARCH_SIM
		ofono_error("g_ril_new() failed to create modem!");
#endif
		return -EIO;
	}
	g_ril_set_slot(rd->ril, slot_id);

	if (getenv("OFONO_RIL_TRACE"))
		g_ril_set_trace(rd->ril, TRUE);

	if (getenv("OFONO_RIL_HEX_TRACE"))
		g_ril_set_debugf(rd->ril, ril_debug, GRIL_HEX_PREFIX[slot_id]);

	g_ril_register(rd->ril, RIL_UNSOL_RIL_CONNECTED,
			ril_connected, modem);

	g_ril_register(rd->ril, RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED,
			ril_radio_state_changed, modem);

	g_ril_register(rd->ril, RIL_UNSOL_MODEM_RESTART,
			ril_modem_restart, modem);

	g_ril_register(rd->ril, RIL_UNSOL_OEM_HOOK_RAW,
			ril_oem_hook_raw, modem);

	g_ril_register(rd->ril, RIL_UNSOL_ABNORMAL_EVENT,
			ril_abnormal_event, modem);

	return 0;
}

static gboolean connect_rild(gpointer user_data)
{
	struct ofono_modem *modem = (struct ofono_modem *) user_data;
	struct ril_data *rd = ofono_modem_get_data(modem);
#ifndef CONFIG_ARCH_SIM
	ofono_info("Trying to reconnect to rild...");
#endif
	if (rd->rild_connect_retries++ < RILD_MAX_CONNECT_RETRIES) {
		if (create_gril(modem) < 0)
			return TRUE;
	} else {
#ifndef CONFIG_ARCH_SIM
		ofono_error("Failed to connect to rild.");
#endif
		return FALSE;
	}

	return FALSE;
}

int ril_enable(struct ofono_modem *modem)
{
	int ret;

	DBG("");

	ret = create_gril(modem);
	if (ret < 0)
		g_timeout_add_seconds(RILD_CONNECT_RETRY_TIME_S,
					connect_rild, modem);

	return -EINPROGRESS;
}

static void power_off_cb(struct ril_msg *message, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	struct ril_data *rd = cbd->user;
	struct ofono_modem *modem = cbd->data;

	if (rd) {
		g_ril_unref(rd->ril);
		rd->ril = NULL;
	}

	ofono_modem_set_powered(modem, FALSE);
}

int ril_disable(struct ofono_modem *modem)
{
	struct ril_data *rd = ofono_modem_get_data(modem);
	struct cb_data *cbd = cb_data_new(NULL, modem, rd);

	DBG("%p", modem);

	ril_send_power(rd, FALSE, power_off_cb, cbd);

	return -EINPROGRESS;
}

static void ril_query_modem_activity_info_cb(struct ril_msg *message, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_modem_activity_info_query_cb_t cb = cbd->cb;
	struct parcel rilp;
	int activity_info[OFONO_MODEM_ACTIVITY_INFO_ARRAY_LENGTH];

	if (message->error != RIL_E_SUCCESS) {
		ofono_error("%s: RIL_REQUEST_GET_ACTIVITY_INFO reply failure: %s",
				__func__,
				ril_error_to_string(message->error));

		CALLBACK_WITH_FAILURE(cb, NULL, 0, cbd->data);
		return;
	}

	g_ril_init_parcel(message, &rilp);

	for (int i = 0; i < OFONO_MODEM_ACTIVITY_INFO_ARRAY_LENGTH; i++) {
		activity_info[i] = parcel_r_int32(&rilp);
	}

	CALLBACK_WITH_SUCCESS(cb, activity_info,
			OFONO_MODEM_ACTIVITY_INFO_ARRAY_LENGTH,
			cbd->data);
}

static void ril_query_modem_activity_info(struct ofono_modem *modem,
		ofono_modem_activity_info_query_cb_t cb,
		void *data)
{
	struct ril_data *rd = ofono_modem_get_data(modem);
	struct cb_data *cbd = cb_data_new(cb, data, modem);

	if (g_ril_send(rd->ril, RIL_REQUEST_GET_ACTIVITY_INFO, NULL,
			ril_query_modem_activity_info_cb, cbd, g_free) > 0)
		return;

	g_free(cbd);
	CALLBACK_WITH_FAILURE(cb, NULL, 0, data);
}

static void ril_enable_modem_cb(struct ril_msg *message, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	struct ofono_modem *modem = cbd->user;
	struct ril_data *rd = ofono_modem_get_data(modem);
	ofono_modem_enable_cb_t cb = cbd->cb;

	if (message->error != RIL_E_SUCCESS) {
		ofono_error("%s: RIL_REQUEST_ENABLE_MODEM reply failure: %s",
				__func__,
				ril_error_to_string(message->error));
		CALLBACK_WITH_FAILURE(cb, cbd->data);
		return;
	}

	g_ril_print_response_no_args(rd->ril, message);

	CALLBACK_WITH_SUCCESS(cb, cbd->data);
}

static void ril_enable_abnormal_event_cb(struct ril_msg *message, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_modem_enable_abnormal_event_cb_t cb = cbd->cb;

	if (message->error != RIL_E_SUCCESS) {
		ofono_error("enable/disable abnormal event fail");
		CALLBACK_WITH_FAILURE(cb, -1, cbd->data);
	} else {
		CALLBACK_WITH_SUCCESS(cb, 0, cbd->data);
	}
}

static void ril_enable_modem(struct ofono_modem *modem, ofono_bool_t enable,
		ofono_modem_enable_cb_t cb, void *data)
{
	struct ril_data *rd = ofono_modem_get_data(modem);
	struct cb_data *cbd = cb_data_new(cb, data, modem);
	struct parcel rilp;

	parcel_init(&rilp);

	parcel_w_int32(&rilp, 1);	/* Number of params */
	parcel_w_int32(&rilp, enable);

	g_ril_append_print_buf(rd->ril, "(%d)", enable);

	if (g_ril_send(rd->ril, RIL_REQUEST_ENABLE_MODEM, &rilp,
			ril_enable_modem_cb, cbd, g_free) > 0)
		return;

	g_free(cbd);
	CALLBACK_WITH_FAILURE(cb, data);
}

static void ril_enable_modem_abnormal_event(struct ofono_modem *modem, ofono_bool_t enable,
		int module_mask, int from_event_id, int to_event_id, ofono_modem_enable_abnormal_event_cb_t cb, void *data)
{
	struct parcel rilp;
	struct ril_data *rd = ofono_modem_get_data(modem);
	struct cb_data *cbd = cb_data_new(cb, data, modem);

	ofono_debug("enable abnormal event switch,%d,%d,%d,%d", enable, module_mask, from_event_id, to_event_id);
	parcel_init(&rilp);
	parcel_w_int32(&rilp, 4);
	parcel_w_int32(&rilp, enable);
	parcel_w_int32(&rilp, module_mask);
	parcel_w_int32(&rilp, from_event_id);
	parcel_w_int32(&rilp, to_event_id);
	if (g_ril_send(rd->ril, RIL_REQUEST_ENABLE_ABNORMAL_EVENT, &rilp,
			ril_enable_abnormal_event_cb, cbd, g_free) > 0)
		return;

	g_free(cbd);
	CALLBACK_WITH_FAILURE(cb, -1, data);
}

static void ril_query_modem_status_cb(struct ril_msg *message, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	struct ofono_modem *modem = cbd->user;
	struct ril_data *rd = ofono_modem_get_data(modem);
	ofono_modem_status_query_cb_t cb = cbd->cb;
	struct parcel rilp;
	int numparams;
	int status;

	if (message->error != RIL_E_SUCCESS) {
		ofono_error("%s: RIL_REQUEST_GET_MODEM_STATUS reply failure: %s",
				__func__,
				ril_error_to_string(message->error));

		CALLBACK_WITH_FAILURE(cb, -1, cbd->data);
		return;
	}

	g_ril_print_response_no_args(rd->ril, message);

	g_ril_init_parcel(message, &rilp);

	numparams = parcel_r_int32(&rilp);
	if (numparams != 1)
		goto error;

	status = parcel_r_int32(&rilp);

	CALLBACK_WITH_SUCCESS(cb, status, cbd->data);
	return;

error:
	CALLBACK_WITH_FAILURE(cb, -1, cbd->data);
}

static void ril_query_modem_status(struct ofono_modem *modem,
		ofono_modem_status_query_cb_t cb,
		void *data)
{
	struct ril_data *rd = ofono_modem_get_data(modem);
	struct cb_data *cbd = cb_data_new(cb, data, modem);

	if (g_ril_send(rd->ril, RIL_REQUEST_GET_MODEM_STATUS, NULL,
			ril_query_modem_status_cb, cbd, g_free) > 0)
		return;

	g_free(cbd);
	CALLBACK_WITH_FAILURE(cb, -1, data);
}

static void ril_oem_request_raw_cb(struct ril_msg *message,
				gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_modem_oem_req_raw_cb_t cb = cbd->cb;
	struct ril_data *rd = cbd->user;
	unsigned char *response;
	struct parcel rilp;
	int resp_len;

	if (message->error != RIL_E_SUCCESS)
		goto error;

	g_ril_init_parcel(message, &rilp);

	response = parcel_r_raw(&rilp, &resp_len);
	if (response == NULL) {
		ofono_error("%s: malformed parcel", __func__);
		goto error;
	}

	g_ril_append_print_buf(rd->ril, "%d", resp_len);
	g_ril_print_response(rd->ril, message);

	CALLBACK_WITH_SUCCESS(cb, response, resp_len, cbd->data);
	g_free(response);
	return;

error:
	CALLBACK_WITH_FAILURE(cb, NULL, 0, cbd->data);
}

static void ril_request_oem_hook_raw(struct ofono_modem *modem, unsigned char oem_req[],
				int req_len, ofono_modem_oem_req_raw_cb_t cb, void *data)
{
	struct ril_data *rd = ofono_modem_get_data(modem);
	struct cb_data *cbd = cb_data_new(cb, data, rd);
	struct parcel rilp;

	parcel_init(&rilp);
	parcel_w_raw(&rilp, oem_req, req_len);

	if (g_ril_send(rd->ril, RIL_REQUEST_OEM_HOOK_RAW, &rilp,
			ril_oem_request_raw_cb, cbd, g_free) > 0)
		return;

	g_free(cbd);
	CALLBACK_WITH_FAILURE(cb, NULL, 0, data);
}

static void ril_oem_request_strings_cb(struct ril_msg *message,
					gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_modem_oem_req_str_cb_t cb = cbd->cb;
	struct ril_data *rd = cbd->user;
	struct parcel rilp;
	char **response;

	if (message->error != RIL_E_SUCCESS)
		goto error;

	g_ril_init_parcel(message, &rilp);

	response = parcel_r_strv(&rilp);
	if (response == NULL) {
		ofono_error("%s: parse error", __func__);
		goto error;
	}

	g_ril_print_response_no_args(rd->ril, message);

	CALLBACK_WITH_SUCCESS(cb, response, g_strv_length(response), cbd->data);
	g_strfreev(response);
	return;

error:
	CALLBACK_WITH_FAILURE(cb, NULL, 0, cbd->data);
}

static void ril_request_oem_hook_strings(struct ofono_modem *modem, char *oem_req[],
				int req_len, ofono_modem_oem_req_str_cb_t cb, void *data)
{
	struct ril_data *rd = ofono_modem_get_data(modem);
	struct cb_data *cbd = cb_data_new(cb, data, rd);
	struct parcel rilp;
	int i;

	parcel_init(&rilp);
	parcel_w_int32(&rilp, req_len);

	for (i = 0; i < req_len; i++)
		parcel_w_string(&rilp, oem_req[i]);

	if (g_ril_send(rd->ril, RIL_REQUEST_OEM_HOOK_STRINGS, &rilp,
			ril_oem_request_strings_cb, cbd, g_free) > 0)
		return;

	g_free(cbd);
	CALLBACK_WITH_FAILURE(cb, NULL, 0, data);
}

static struct ofono_modem_driver ril_driver = {
	.name = "ril",
	.probe = ril_probe,
	.remove = ril_remove,
	.enable = ril_enable,
	.disable = ril_disable,
	.pre_sim = ril_pre_sim,
	.post_sim = ril_post_sim,
	.post_online = ril_post_online,
	.set_online = ril_set_online,
	.query_activity_info = ril_query_modem_activity_info,
	.enable_modem = ril_enable_modem,
	.query_modem_status = ril_query_modem_status,
	.request_oem_raw = ril_request_oem_hook_raw,
	.request_oem_strings = ril_request_oem_hook_strings,
	.enable_modem_abnormal_event = ril_enable_modem_abnormal_event,
};

/*
 * This plugin is a generic ( aka default ) device plugin for RIL-based devices.
 * The plugin 'rildev' is used to determine which RIL plugin should be loaded
 * based upon an environment variable.
 */
static int ril_init(void)
{
	int retval = ofono_modem_driver_register(&ril_driver);

	if (retval != 0)
		DBG("ofono_modem_driver_register returned: %d", retval);

	return retval;
}

static void ril_exit(void)
{
	DBG("");
	ofono_modem_driver_unregister(&ril_driver);
}

OFONO_PLUGIN_DEFINE(ril, "RIL modem driver", VERSION,
			OFONO_PLUGIN_PRIORITY_DEFAULT, ril_init, ril_exit)
