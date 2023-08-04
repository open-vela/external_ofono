/*
 *
 *  oFono - Open Source Telephony
 *
 *  Copyright (C) 2008-2016  Intel Corporation. All rights reserved.
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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <glib.h>

#include <ofono/log.h>
#include <ofono/modem.h>
#include <ofono/netmon.h>
#include <ofono/netreg.h>

#include "gril.h"

#include "rilmodem.h"
#include "common.h"

/*
 * Defined below are copy of
 * RIL_CellInfoType defined in Ril.h
 */
#define NETMON_RIL_CELLINFO_TYPE_GSM		1
#define NETMON_RIL_CELLINFO_TYPE_CDMA		2
#define NETMON_RIL_CELLINFO_TYPE_LTE		3
#define NETMON_RIL_CELLINFO_TYPE_UMTS		4
#define NETMON_RIL_CELLINFO_TYPE_TDSCDMA	5

/* size of RIL_CellInfoGsm */
#define NETMON_RIL_CELLINFO_SIZE_GSM		24
/* size of RIL_CellInfoCDMA */
#define NETMON_RIL_CELLINFO_SIZE_CDMA		40
/* size of RIL_CellInfoLte */
#define NETMON_RIL_CELLINFO_SIZE_LTE		44
/* size of RIL_CellInfoWcdma */
#define NETMON_RIL_CELLINFO_SIZE_UMTS		28
/* size of RIL_CellInfoTdscdma */
#define NETMON_RIL_CELLINFO_SIZE_TDSCDMA	24

#define MSECS_RATE_INVALID	(0x7fffffff)
#define SECS_TO_MSECS(x)	((x) * 1000)

struct netmon_data {
	GRil *ril;
};

static gboolean ril_delayed_register(gpointer user_data)
{
	struct ofono_netmon *netmon = user_data;

	ofono_netmon_register(netmon);

	return FALSE;
}

static int process_cellinfo_list(struct ril_msg *message,
					struct ofono_netmon *netmon, gpointer user_data)
{
	struct netmon_data *nmd = ofono_netmon_get_data(netmon);
	struct cb_data *cbd = user_data;
	ofono_netmon_cell_list_cb_t cb;
	struct parcel rilp;
	struct ofono_cell_info* list;
	int cell_info_cnt;
	int cell_type;
	int registered;
	int mcc, mnc;
	int i;

	if (message->error != RIL_E_SUCCESS) {
		if (cbd) {
			cb = cbd->cb;
			CALLBACK_WITH_FAILURE(cb, 0, NULL, cbd->data);
		}

		return OFONO_ERROR_TYPE_FAILURE;
	}

	g_ril_print_unsol_no_args(nmd->ril, message);
	g_ril_init_parcel(message, &rilp);

	cell_info_cnt = parcel_r_int32(&rilp);

	list = g_new0(struct ofono_cell_info, cell_info_cnt);
	if (list == NULL) {
		if (cbd) {
			cb = cbd->cb;
			CALLBACK_WITH_FAILURE(cb, 0, NULL, cbd->data);
		}

		return OFONO_ERROR_TYPE_FAILURE;
	}

	for (i = 0; i < cell_info_cnt; i++) {
		cell_type = parcel_r_int32(&rilp);

		registered = parcel_r_int32(&rilp);

		/* skipping unneeded timeStampType in Ril cell info */
		(void)parcel_r_int32(&rilp);

		/*skipping timeStamp which is a uint64_t type */
		(void)parcel_r_int32(&rilp);
		(void)parcel_r_int32(&rilp);

		list[i].type = cell_type;
		list[i].registered = registered;

		mcc = parcel_r_int32(&rilp);
		mnc = parcel_r_int32(&rilp);

		if (mcc >= 0 && mcc <= 999)
			snprintf(list[i].mcc, sizeof(list[i].mcc), "%03d", mcc);
		else
			strcpy(list[i].mcc, "");

		if (mnc >= 0 && mnc <= 99)
			snprintf(list[i].mnc, sizeof(list[i].mnc), "%02d", mnc);
		else if (mnc > 99 && mnc <= 999)
			snprintf(list[i].mnc, sizeof(list[i].mnc), "%03d", mnc);
		else
			strcpy(list[i].mnc, "");

		if (cell_type == NETMON_RIL_CELLINFO_TYPE_GSM) {
			list[i].lac = parcel_r_int32(&rilp);
			list[i].ci= parcel_r_int32(&rilp);
			list[i].rssi = parcel_r_int32(&rilp);
			list[i].ber = parcel_r_int32(&rilp);

			list[i].lac = (list[i].lac >= 0 && list[i].lac <= 65535) ? list[i].lac : -1;
			list[i].ci= (list[i].ci>= 0 && list[i].ci<= 65535) ? list[i].ci: -1;
			list[i].rssi = (list[i].rssi >= 0 && list[i].rssi <= 31) ? list[i].rssi : -1;
			list[i].ber = (list[i].ber >= 0 && list[i].ber <= 7) ? list[i].ber : -1;
		} else if (cell_type == NETMON_RIL_CELLINFO_TYPE_UMTS) {
			list[i].lac = parcel_r_int32(&rilp);
			list[i].ci= parcel_r_int32(&rilp);
			list[i].psc = parcel_r_int32(&rilp);
			list[i].rssi = parcel_r_int32(&rilp);
			list[i].ber = parcel_r_int32(&rilp);

			list[i].lac = (list[i].lac >= 0 && list[i].lac <= 65535) ? list[i].lac : -1;
			list[i].ci= (list[i].ci>= 0 && list[i].ci<= 268435455) ? list[i].ci: -1;
			list[i].psc = (list[i].psc >= 0 && list[i].psc <= 511) ? list[i].psc : -1;
			list[i].rssi = (list[i].rssi >= 0 && list[i].rssi <= 31) ? list[i].rssi : -1;
			list[i].ber = (list[i].ber >= 0 && list[i].ber <= 7) ? list[i].ber : -1;
		} else if (cell_type == NETMON_RIL_CELLINFO_TYPE_LTE) {
			list[i].ci =  parcel_r_int32(&rilp);
			list[i].pci = parcel_r_int32(&rilp);
			list[i].tac = parcel_r_int32(&rilp);
			list[i].rssi = parcel_r_int32(&rilp);
			list[i].rsrp = parcel_r_int32(&rilp);
			list[i].rsrq = parcel_r_int32(&rilp);
			list[i].snr = parcel_r_int32(&rilp);
			list[i].cqi = parcel_r_int32(&rilp);
			list[i].tadv = parcel_r_int32(&rilp);

			list[i].ci = (list[i].ci >= 0 && list[i].ci <= 268435455) ? list[i].ci : -1;
			list[i].pci = (list[i].pci >= 0 && list[i].pci <= 503) ? list[i].pci : -1;
			list[i].tac = (list[i].tac >= 0 && list[i].tac <= 65535) ? list[i].tac : -1;
			list[i].rssi = (list[i].rssi >= 0 && list[i].rssi <= 31) ? list[i].rssi : -1;
			list[i].rsrp = (list[i].rsrp >= 44 && list[i].rsrp <= 140) ? -list[i].rsrp : -1;
			list[i].rsrq = (list[i].rsrq >= 3 && list[i].rsrq <= 20) ? -list[i].rsrq : -1;
			list[i].snr = (list[i].snr >= -200 && list[i].snr <= 300) ? list[i].snr : -1;
			list[i].cqi = (list[i].cqi >= 0 && list[i].cqi <= 15) ? list[i].cqi : -1;
			list[i].tadv = (list[i].tadv >=0 && list[i].tadv <= 63) ? list[i].tadv : -1;

			list[i].level = get_signal_level_from_rsrp(list[i].rsrp);
		}
	}

	if (cbd) {
		cb = cbd->cb;
		CALLBACK_WITH_SUCCESS(cb, i, list, cbd->data);
	} else {
		ofono_netmon_serving_cell_notify(netmon, i, list);
	}

	g_free(list);
	return OFONO_ERROR_TYPE_NO_ERROR;
}

static void ril_netmon_update_cb(struct ril_msg *message, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	struct ofono_netmon *netmon = cbd->data;

	process_cellinfo_list(message, netmon, cbd);
}

static void ril_cellinfo_notify(struct ril_msg *message, gpointer user_data)
{
	struct ofono_netmon *netmon = user_data;

	process_cellinfo_list(message, netmon, NULL);
}

static void setup_cell_info_notify(struct ofono_netmon *netmon)
{
	struct netmon_data *nmd = ofono_netmon_get_data(netmon);
	struct parcel rilp;

	parcel_init(&rilp);

	parcel_w_int32(&rilp, 1);	/* Number of elements */

	parcel_w_int32(&rilp, MSECS_RATE_INVALID);

	if (g_ril_send(nmd->ril, RIL_REQUEST_SET_UNSOL_CELL_INFO_LIST_RATE,
			&rilp, NULL, NULL, NULL) == 0)
		ofono_error("%s: setup failed\n", __func__);

	if (g_ril_register(nmd->ril, RIL_UNSOL_CELL_INFO_LIST,
				ril_cellinfo_notify, netmon) == 0)
		ofono_error("%s: setup failed\n", __func__);
}

static int ril_netmon_probe(struct ofono_netmon *netmon,
		unsigned int vendor, void *user)
{
	GRil *ril = user;
	struct netmon_data *ud = g_new0(struct netmon_data, 1);

	ud->ril = g_ril_clone(ril);

	ofono_netmon_set_data(netmon, ud);

	setup_cell_info_notify(netmon);

	g_idle_add(ril_delayed_register, netmon);

	return 0;
}

static void ril_netmon_remove(struct ofono_netmon *netmon)
{
	struct netmon_data *nmd = ofono_netmon_get_data(netmon);

	ofono_netmon_set_data(netmon, NULL);
	g_ril_unref(nmd->ril);
}

static void ril_netmon_request_update(struct ofono_netmon *netmon,
		ofono_netmon_cell_list_cb_t cb, void *data)
{
	struct netmon_data *nmd = ofono_netmon_get_data(netmon);
	struct cb_data *cbd = cb_data_new(cb, data, nmd);

	if (g_ril_send(nmd->ril, RIL_REQUEST_GET_CELL_INFO_LIST, NULL,
			ril_netmon_update_cb, cbd, g_free) > 0)
		return;

	g_free(cbd);
	CALLBACK_WITH_FAILURE(cb, 0, NULL, data);
}

static void periodic_update_cb(struct ril_msg *message, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	ofono_netmon_cb_t cb = cbd->cb;

	if (message->error != RIL_E_SUCCESS) {
		CALLBACK_WITH_FAILURE(cb, cbd->data);
		return;
	}

	CALLBACK_WITH_SUCCESS(cb, cbd->data);
}

static void ril_netmon_periodic_update(struct ofono_netmon *netmon,
			unsigned int enable, unsigned int period,
			ofono_netmon_cb_t cb, void *data)
{
	struct netmon_data *nmd = ofono_netmon_get_data(netmon);
	struct cb_data *cbd = cb_data_new(cb, data, nmd);
	struct parcel rilp;

	parcel_init(&rilp);

	parcel_w_int32(&rilp, 1);	/* Number of elements */

	if (enable)
		parcel_w_int32(&rilp, SECS_TO_MSECS(period));
	else
		parcel_w_int32(&rilp, MSECS_RATE_INVALID);

	if (g_ril_send(nmd->ril, RIL_REQUEST_SET_UNSOL_CELL_INFO_LIST_RATE,
			&rilp, periodic_update_cb, cbd, g_free) > 0)
		return;

	g_free(cbd);
	CALLBACK_WITH_FAILURE(cb, data);
}

static void ril_netmon_neighbouring_cell_update(struct ofono_netmon *netmon,
					ofono_netmon_cell_list_cb_t cb, void *data)
{
	struct netmon_data *nmd = ofono_netmon_get_data(netmon);
	struct cb_data *cbd = cb_data_new(cb, data, nmd);

	if (g_ril_send(nmd->ril, RIL_REQUEST_GET_NEIGHBORING_CELL_IDS, NULL,
			ril_netmon_update_cb, cbd, g_free) > 0)
		return;

	g_free(cbd);
	CALLBACK_WITH_FAILURE(cb, 0, NULL, data);
}

static const struct ofono_netmon_driver driver = {
	.name				= RILMODEM,
	.probe				= ril_netmon_probe,
	.remove				= ril_netmon_remove,
	.request_update			= ril_netmon_request_update,
	.enable_periodic_update		= ril_netmon_periodic_update,
	.neighbouring_cell_update	= ril_netmon_neighbouring_cell_update,
};

void ril_netmon_init(void)
{
	ofono_netmon_driver_register(&driver);
}

void ril_netmon_exit(void)
{
	ofono_netmon_driver_unregister(&driver);
}
