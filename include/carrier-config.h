/*
 *
 *  oFono - Open Telephony stack for Linux
 *
 *  Copyright (C) 2011  Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef __OFONO_CARRIER_CONFIG_H
#define __OFONO_CARRIER_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

struct ofono_carrier_config_data {
	char *spn_name;
};

struct ofono_carrier_config_driver {
	const char *name;
	int priority;
	int (*get_carrier_configs)(const char *mcc, const char *mnc,
			int mvno_type, const char* mvno_value,
				struct ofono_carrier_config_data *configs);
};

int ofono_carrier_config_driver_register(
			const struct ofono_carrier_config_driver *driver);
void ofono_carrier_config_driver_unregister(
			const struct ofono_carrier_config_driver *driver);

#ifdef __cplusplus
}
#endif

#endif /* __OFONO_CARRIER_CONFIG_H */
