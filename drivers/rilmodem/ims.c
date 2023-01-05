/*
 *
 *  oFono - Open Source Telephony
 *
 *  Copyright (C) 2023 Xiaomi Corporation.
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <ofono.h>

#include <ofono/log.h>
#include <ofono/modem.h>
#include <ofono/ims.h>

#include <gril/gril.h>

#include "common.h"
#include "rilmodem.h"

struct ril_ims_data
{
	GRil *ril;
};

static void ril_registration_status_cb(struct ril_msg *message, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_ims_status_cb_t cb = cbd->cb;
	struct ofono_error error;
	int reg_info;
	int ext_info;
	struct parcel rilp;

	if(cb == NULL)
		return;

	if (message->error != RIL_E_SUCCESS)
		goto error;

	g_ril_init_parcel(message, &rilp);

	if (rilp.size < sizeof(int32_t))
		goto error;

	reg_info = parcel_r_int32(&rilp);
	ext_info = parcel_r_int32(&rilp);

	DBG("ril_registration_status_cb reg_info:%d, ext_info:%d", reg_info, ext_info);
	cb(&error, reg_info, ext_info, cbd->data);
	return;

error:
	CALLBACK_WITH_FAILURE(cb, -1, -1, cbd->data);
}

static void ril_ims_registration_status(struct ofono_ims *ims,
					ofono_ims_status_cb_t cb, void *data)
{
	struct ril_ims_data *rid = ofono_ims_get_data(ims);
	struct cb_data *cbd = cb_data_new(cb, data, rid);

	DBG("ril_ims_registration_status");

	if (g_ril_send(rid->ril, RIL_REQUEST_IMS_REGISTRATION_STATE, NULL,
			ril_registration_status_cb, cbd, g_free) == 0) {
		g_free(cbd);

		if (cb != NULL)
			CALLBACK_WITH_FAILURE(cb, -1, -1, data);
	}
}

static void ril_ims_register_cb(struct ril_msg *message, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_ims_register_cb_t cb = cbd->cb;
	struct ril_ims_data *rid = cbd->user;
	struct ofono_error error;

	if (message->error == RIL_E_SUCCESS) {
		decode_ril_error(&error, "OK");

		g_ril_print_response_no_args(rid->ril, message);
	} else {
		decode_ril_error(&error, "FAIL");
	}

	if (cb != NULL)
		cb(&error, cbd->data);
}

static void ims_registration_notify(struct ril_msg *message, gpointer user_data)
{
	struct ofono_ims *ims = user_data;
	struct ril_ims_data *rid = ofono_ims_get_data(ims);
	int reg_info;
	int ext_info;
	struct parcel rilp;

	g_ril_print_unsol_no_args(rid->ril, message);

	g_ril_init_parcel(message, &rilp);

	if (message->error != RIL_E_SUCCESS)
		return;

	if (rilp.size < sizeof(int32_t))
		return;

	reg_info = parcel_r_int32(&rilp);
	ext_info = parcel_r_int32(&rilp);

	DBG("reg_info:%d, ext_info:%d", reg_info, ext_info);
	ofono_ims_status_notify(ims, reg_info, ext_info);
}

static void send_ims_register_status(struct ofono_ims *ims,
					ofono_ims_register_cb_t cb, void *data, int state)
{
	struct ril_ims_data *rid = ofono_ims_get_data(ims);
	struct cb_data *cbd = cb_data_new(cb, data, rid);
	struct parcel rilp;

	parcel_init(&rilp);
	parcel_w_int32(&rilp, state);

	if (g_ril_send(rid->ril, RIL_REQUEST_IMS_REG_STATE_CHANGE, &rilp,
			ril_ims_register_cb, cbd, g_free) == 0) {
		g_free(cbd);

		if (cb != NULL)
			CALLBACK_WITH_FAILURE(cb, data);
	}
}

static void ril_ims_register(struct ofono_ims *ims,
					ofono_ims_register_cb_t cb, void *data)
{
	DBG("turn on ims");
	send_ims_register_status(ims, cb, data, 1);
}

static void ril_ims_unregister(struct ofono_ims *ims,
					ofono_ims_register_cb_t cb, void *data)
{
	DBG("turn off ims");
	send_ims_register_status(ims, cb, data, 0);
}

static void ril_ims_set_capable(struct ofono_ims *ims, int cap,
					ofono_ims_register_cb_t cb, void *data)
{
	DBG("set capable");
	struct ril_ims_data *rid = ofono_ims_get_data(ims);
	struct cb_data *cbd = cb_data_new(cb, data, rid);
	struct parcel rilp;

	parcel_init(&rilp);
	parcel_w_int32(&rilp, cap);

	if (g_ril_send(rid->ril, RIL_REQUEST_IMS_SET_SERVICE_STATUS, &rilp,
			ril_ims_register_cb, cbd, g_free) == 0) {
		g_free(cbd);
		if(cb != NULL)
			CALLBACK_WITH_FAILURE(cb, data);
	}
}

static void ril_ims_support_cb(struct ril_msg *message, gpointer user_data)
{
	struct ofono_ims *ims = user_data;
	struct ril_ims_data *rid = ofono_ims_get_data(ims);

	if (message->error != RIL_E_SUCCESS){
		ofono_ims_remove(ims);
		return;
	}

	DBG("ril support ims");

	ofono_ims_register(ims);

	g_ril_register(rid->ril, RIL_UNSOL_RESPONSE_IMS_NETWORK_STATE_CHANGED,
			ims_registration_notify, ims);
}

static gboolean ril_delayed_register(gpointer user_data)
{
	struct ofono_ims *ims = user_data;
	struct ril_ims_data *rid = ofono_ims_get_data(ims);

	g_ril_send(rid->ril, RIL_REQUEST_IMS_REGISTRATION_STATE, NULL,
			ril_ims_support_cb, ims, NULL);

	return FALSE;
}

static int ril_ims_probe(struct ofono_ims *ims, void *data)
{
	struct ril_ims_data *rid = g_new0(struct ril_ims_data, 1);
	if (rid == NULL)
		return 0;

	rid->ril = g_ril_clone(data);

	ofono_ims_set_data(ims, rid);

	g_idle_add(ril_delayed_register, ims);

	return 0;
}

static void ril_ims_remove(struct ofono_ims *ims)
{
	struct ril_ims_data *rid = ofono_ims_get_data(ims);

	DBG("at ims remove");

	ofono_ims_set_data(ims, NULL);

	g_ril_unref(rid->ril);
	g_free(rid);
}

static const struct ofono_ims_driver driver = {
	.name			= "RILMODEM",
	.probe			= ril_ims_probe,
	.remove			= ril_ims_remove,
	.ims_register		= ril_ims_register,
	.ims_unregister		= ril_ims_unregister,
	.set_capable		= ril_ims_set_capable,
	.registration_status	= ril_ims_registration_status,
};

void ril_ims_init(void)
{
	ofono_ims_driver_register(&driver);
}

void ril_ims_exit(void)
{
	ofono_ims_driver_unregister(&driver);
}
