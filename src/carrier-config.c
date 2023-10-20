/*
 *
 *  oFono - Open Source Telephony
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include "ofono.h"

static GSList *g_drivers = NULL;

void __ofono_carrier_config_free_configs(
				struct ofono_carrier_config_data *configs)
{
	g_free(configs->spn_name);
	g_free(configs);
}

ofono_bool_t __ofono_carrier_config_get_configs(const char *mcc,
				const char *mnc, int mvno_type, const char* mvno_value,
				struct ofono_carrier_config_data *configs)
{
	GSList *d;

	if (mcc == NULL || strlen(mcc) == 0 || mnc == NULL || strlen(mnc) == 0)
		return FALSE;

	for (d = g_drivers; d != NULL; d = d->next) {
		const struct ofono_carrier_config_driver *driver = d->data;

		if (driver->get_carrier_configs == NULL)
			continue;

		ofono_debug("Calling carrierconfig plugin '%s'", driver->name);

		if (driver->get_carrier_configs(mcc, mnc, 0, "", configs) < 0)
			continue;

		return TRUE;
	}

	return FALSE;
}

static gint compare_priority(gconstpointer a, gconstpointer b)
{
	const struct ofono_carrier_config_driver *plugin1 = a;
	const struct ofono_carrier_config_driver *plugin2 = b;

	return plugin2->priority - plugin1->priority;
}

int ofono_carrier_config_driver_register(
			const struct ofono_carrier_config_driver *driver)
{
	DBG("driver: %p name: %s", driver, driver->name);

	g_drivers = g_slist_insert_sorted(g_drivers, (void *) driver,
						compare_priority);
	return 0;
}

void ofono_carrier_config_driver_unregister(
			const struct ofono_carrier_config_driver *driver)
{
	DBG("driver: %p name: %s", driver, driver->name);

	g_drivers = g_slist_remove(g_drivers, driver);
}
