/*
 *
 *  oFono - Open Source Telephony
 *
 *  Copyright (C) 2008-2011  Intel Corporation. All rights reserved.
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
#include <string.h>

#include <glib.h>

#define OFONO_API_SUBJECT_TO_CHANGE
#include <ofono/types.h>
#include <ofono/log.h>
#include <ofono/plugin.h>
#include <ofono/modem.h>
#include <ofono/gprs-provision.h>
#include <ofono/carrier-config.h>

#include "mbpi.h"

static int provision_get_settings(const char *mcc, const char *mnc,
				const char *spn,
				struct ofono_gprs_provision_data **settings,
				int *count)
{
	GSList *l;
	GSList *apns;
	GError *error = NULL;
	int ap_count;
	int i;

	DBG("Provisioning for MCC %s, MNC %s, SPN '%s'", mcc, mnc, spn);

	apns = mbpi_lookup_apn(mcc, mnc, FALSE, &error);
	if (apns == NULL) {
		if (error != NULL) {
			ofono_error("%s", error->message);
			g_error_free(error);
		}

		return -ENOENT;
	}

	ap_count = g_slist_length(apns);

	DBG("Found %d APs", ap_count);

	*settings = g_try_new0(struct ofono_gprs_provision_data, ap_count);
	if (*settings == NULL) {
		ofono_error("Provisioning failed: %s", g_strerror(errno));

		for (l = apns; l; l = l->next)
			mbpi_ap_free(l->data);

		g_slist_free(apns);

		return -ENOMEM;
	}

	*count = ap_count;

	for (l = apns, i = 0; l; l = l->next, i++) {
		struct ofono_gprs_provision_data *ap = l->data;

		DBG("Name: '%s'", ap->name);
		DBG("APN: '%s'", ap->apn);
		DBG("Type: %s", mbpi_ap_type(ap->type));
		DBG("Protocal: %d", ap->proto);
		DBG("Username: '%s'", ap->username);
		DBG("Password: '%s'", ap->password);

		memcpy(*settings + i, ap,
			sizeof(struct ofono_gprs_provision_data));

		g_free(ap);
	}

	g_slist_free(apns);

	return 0;
}

static int provision_get_carrier_configs(const char *mcc, const char *mnc,
				int mvno_type, const char* mvno_value,
				struct ofono_carrier_config_data **configs)
{
	struct ofono_carrier_config_data *lookup_result;
	GError *error = NULL;

	ofono_debug("Provisioning carrier config for MCC %s, MNC %s", mcc, mnc);

	lookup_result = mbpi_lookup_carrier_config(mcc, mnc, &error);
	if (lookup_result == NULL) {
		if (error != NULL) {
			ofono_error("%s", error->message);
			g_error_free(error);
		}

		return -ENOENT;
	}

	*configs = g_try_new0(struct ofono_carrier_config_data, 1);
	if (*configs == NULL) {
		ofono_error("Provisioning carrier config failed: %s", g_strerror(errno));
		mbpi_carrier_config_free(lookup_result);

		return -ENOMEM;
	}

	memcpy(*configs, lookup_result, sizeof(struct ofono_carrier_config_data));
	mbpi_carrier_config_free(lookup_result);

	return 0;
}

static struct ofono_gprs_provision_driver provision_driver = {
	.name		= "Provisioning",
	.get_settings	= provision_get_settings
};

static struct ofono_carrier_config_driver carrier_config_driver = {
	.name		= "CarrierConfig",
	.get_carrier_configs	= provision_get_carrier_configs
};

static int provision_init(void)
{
	ofono_gprs_provision_driver_register(&provision_driver);
	ofono_carrier_config_driver_register(&carrier_config_driver);

	return 0;
}

static void provision_exit(void)
{
	ofono_gprs_provision_driver_unregister(&provision_driver);
	ofono_carrier_config_driver_unregister(&carrier_config_driver);
}

OFONO_PLUGIN_DEFINE(provision, "Provisioning Plugin", VERSION,
			OFONO_PLUGIN_PRIORITY_DEFAULT,
			provision_init, provision_exit)
