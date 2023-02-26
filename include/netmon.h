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

#ifndef __OFONO_NETMON_H
#define __OFONO_NETMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ofono/types.h>

struct ofono_netmon;

struct ofono_cell_info {
	char mcc[OFONO_MAX_MCC_LENGTH + 1];
	char mnc[OFONO_MAX_MNC_LENGTH + 1];
	unsigned int ci;
	unsigned int pci;
	unsigned int lac;
	unsigned int arfcn;
	unsigned int bsic;
	unsigned int rxlev;
	unsigned int tadv;
	unsigned int psc;
	unsigned int ber;
	unsigned int rssi;
	unsigned int rscp;
	unsigned int ecno;
	unsigned int rsrq;
	unsigned int rsrp;
	unsigned int earfcn;
	unsigned int eband;
	unsigned int cqi;
	unsigned int tac;
	int snr;
	int type;
	ofono_bool_t registered;
};

typedef void (*ofono_netmon_cb_t)(const struct ofono_error *error,
					void *data);

typedef void (*ofono_netmon_cell_list_cb_t)(const struct ofono_error *error,
					int total,
					const struct ofono_cell_info* list,
					void *data);

struct ofono_netmon_driver {
	const char *name;
	int (*probe)(struct ofono_netmon *netmon, unsigned int vendor,
					void *data);
	void (*remove)(struct ofono_netmon *netmon);
	void (*request_update)(struct ofono_netmon *netmon,
					ofono_netmon_cell_list_cb_t cb, void *data);
	void (*enable_periodic_update)(struct ofono_netmon *netmon,
					unsigned int enable,
					unsigned int period,
					ofono_netmon_cb_t cb, void *data);
	void (*neighbouring_cell_update)(struct ofono_netmon *netmon,
					ofono_netmon_cell_list_cb_t cb, void *data);
};

enum ofono_netmon_cell_type {
	OFONO_NETMON_CELL_TYPE_GSM,
	OFONO_NETMON_CELL_TYPE_UMTS,
	OFONO_NETMON_CELL_TYPE_LTE,
};

enum ofono_netmon_info {
	OFONO_NETMON_INFO_MCC, /* char *, up to 3 digits + null */
	OFONO_NETMON_INFO_MNC, /* char *, up to 3 digits + null */
	OFONO_NETMON_INFO_LAC, /* int */
	OFONO_NETMON_INFO_CI, /* int */
	OFONO_NETMON_INFO_ARFCN, /* int */
	OFONO_NETMON_INFO_BSIC, /* int */
	OFONO_NETMON_INFO_RXLEV, /* int */
	OFONO_NETMON_INFO_BER, /* int */
	OFONO_NETMON_INFO_RSSI, /* int */
	OFONO_NETMON_INFO_TIMING_ADVANCE, /* int */
	OFONO_NETMON_INFO_PSC, /* int */
	OFONO_NETMON_INFO_RSCP, /* int */
	OFONO_NETMON_INFO_ECN0, /* int */
	OFONO_NETMON_INFO_RSRQ, /* int */
	OFONO_NETMON_INFO_RSRP, /* int */
	OFONO_NETMON_INFO_EARFCN, /* int */
	OFONO_NETMON_INFO_EBAND, /* int */
	OFONO_NETMON_INFO_CQI, /* int */
	OFONO_NETMON_INFO_PCI, /* int */
	OFONO_NETMON_INFO_TAC, /* int */
	OFONO_NETMON_INFO_SNR, /* int */
	OFONO_NETMON_INFO_INVALID,
};

void ofono_netmon_serving_cell_notify(struct ofono_netmon *netmon,
					int total,
					const struct ofono_cell_info* list);

int ofono_netmon_driver_register(const struct ofono_netmon_driver *d);

void ofono_netmon_driver_unregister(const struct ofono_netmon_driver *d);

struct ofono_netmon *ofono_netmon_create(struct ofono_modem *modem,
						unsigned int vendor,
						const char *driver, void *data);

void ofono_netmon_register(struct ofono_netmon *netmon);

void ofono_netmon_remove(struct ofono_netmon *netmon);

void ofono_netmon_set_data(struct ofono_netmon *netmon, void *data);

void *ofono_netmon_get_data(struct ofono_netmon *netmon);

void ofono_netmon_neighbouring_cell_notify(struct ofono_netmon *netmon,
					int total,
					const struct ofono_cell_info* cell,
					void *data);

#ifdef __cplusplus
}
#endif

#endif /* __OFONO_NETMON_H */
