/*
 *
 *  oFono - Open Source Telephony
 *
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

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>

#include <glib.h>
#include <gdbus.h>

#include "ofono.h"
#include "netmonagent.h"

#define CELL_INFO_DICT_APPEND(p_dict, key, info, type, dbus_type)	do { \
	type value; \
	if (info < 0) \
		break; \
	value = (type) info; \
	ofono_dbus_dict_append(p_dict, key, dbus_type, &value); \
} while (0)

static GSList *g_drivers = NULL;

struct ofono_netmon {
	const struct ofono_netmon_driver *driver;
	DBusMessage *pending;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter arr;
	void *driver_data;
	struct ofono_atom *atom;
	struct netmon_agent *agent;
};

static const char *cell_type_to_tech_name(enum ofono_netmon_cell_type type)
{
	switch (type) {
	case OFONO_NETMON_CELL_TYPE_GSM:
		return "gsm";
	case OFONO_NETMON_CELL_TYPE_CDMA:
		return "cdma";
	case OFONO_NETMON_CELL_TYPE_LTE:
		return "lte";
	case OFONO_NETMON_CELL_TYPE_UMTS:
		return "umts";
	case OFONO_NETMON_CELL_TYPE_TDSCDMA:
		return "tdscdma";
	}

	return NULL;
}

static void netmon_cell_info_dict_append(DBusMessageIter *dict,
					const struct ofono_cell_info* cell)
{
	char *mcc, *mnc;
	const char *technology;

	if (cell == NULL)
		return;

	mcc = g_strdup(cell->mcc);
	mnc = g_strdup(cell->mnc);

	if (mcc && strlen(mcc))
		ofono_dbus_dict_append(dict, "MobileCountryCode", DBUS_TYPE_STRING, &mcc);

	if (mnc && strlen(mnc))
		ofono_dbus_dict_append(dict, "MobileNetworkCode", DBUS_TYPE_STRING, &mnc);

	technology = cell_type_to_tech_name(cell->type);
	if (technology != NULL)
		ofono_dbus_dict_append(dict, "Technology", DBUS_TYPE_STRING, &technology);

	ofono_dbus_dict_append(dict, "Registered", DBUS_TYPE_INT32, &cell->registered);

	if (cell->lac >=0)
		ofono_dbus_dict_append(dict, "LocationAreaCode", DBUS_TYPE_UINT16, &cell->lac);

	if (cell->ci >= 0)
		ofono_dbus_dict_append(dict, "CellId", DBUS_TYPE_UINT32, &cell->ci);

	if (cell->arfcn >= 0)
		ofono_dbus_dict_append(dict, "ARFCN", DBUS_TYPE_UINT16, &cell->arfcn);

	if (cell->bsic >= 0)
		ofono_dbus_dict_append(dict, "BSIC", DBUS_TYPE_BYTE, &cell->bsic);

	if (cell->rxlev >= 0)
		ofono_dbus_dict_append(dict, "ReceivedSignalStrength", DBUS_TYPE_BYTE, &cell->rxlev);

	if (cell->tadv >= 0)
		ofono_dbus_dict_append(dict, "TimingAdvance", DBUS_TYPE_BYTE, &cell->tadv);

	if (cell->psc >= 0)
		ofono_dbus_dict_append(dict, "PrimaryScramblingCode", DBUS_TYPE_UINT16, &cell->psc);

	if (cell->ber >= 0)
		ofono_dbus_dict_append(dict, "BitErrorRate", DBUS_TYPE_BYTE, &cell->ber);

	if (cell->rssi >= 0)
		ofono_dbus_dict_append(dict, "Strength", DBUS_TYPE_BYTE, &cell->rssi);

	if (cell->rscp >= 0)
		ofono_dbus_dict_append(dict, "ReceivedSignalCodePower", DBUS_TYPE_BYTE, &cell->rscp);

	if (cell->ecno >= 0)
		ofono_dbus_dict_append(dict, "ReceivedEnergyRatio", DBUS_TYPE_BYTE, &cell->ecno);

	if (cell->rsrq >= 0)
		ofono_dbus_dict_append(dict, "ReferenceSignalReceivedQuality", DBUS_TYPE_BYTE, &cell->rsrq);

	if (cell->rsrp >= 0)
		ofono_dbus_dict_append(dict, "ReferenceSignalReceivedPower", DBUS_TYPE_BYTE, &cell->rsrp);

	if (cell->earfcn >= 0)
		ofono_dbus_dict_append(dict, "EARFCN", DBUS_TYPE_UINT16, &cell->earfcn);

	if (cell->eband >= 0)
		ofono_dbus_dict_append(dict, "EBand", DBUS_TYPE_BYTE, &cell->eband);

	if (cell->cqi >= 0)
		ofono_dbus_dict_append(dict, "ChannelQualityIndicator", DBUS_TYPE_BYTE, &cell->cqi);

	if (cell->pci >= 0)
		ofono_dbus_dict_append(dict, "PhysicalCellId", DBUS_TYPE_UINT16, &cell->pci);

	if (cell->tac >= 0)
		ofono_dbus_dict_append(dict, "TrackingAreaCode", DBUS_TYPE_UINT16, &cell->tac);

	ofono_dbus_dict_append(dict, "SingalToNoiseRatio", DBUS_TYPE_INT32, &cell->snr);

	g_free(mcc);
	g_free(mnc);
}

static void append_cell_struct(const struct ofono_cell_info *cell,
					DBusMessageIter *iter)
{
	DBusMessageIter entry, dict;

	dbus_message_iter_open_container(iter, DBUS_TYPE_STRUCT, NULL, &entry);
	dbus_message_iter_open_container(&entry, DBUS_TYPE_ARRAY,
					OFONO_PROPERTIES_ARRAY_SIGNATURE,
					&dict);
	netmon_cell_info_dict_append(&dict, cell);
	dbus_message_iter_close_container(&entry, &dict);
	dbus_message_iter_close_container(iter, &entry);
}

void ofono_netmon_serving_cell_notify(struct ofono_netmon *netmon,
					int total,
					const struct ofono_cell_info* list)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(netmon->atom);
	DBusMessage *signal;
	DBusMessageIter iter;
	DBusMessageIter array;
	const char *key = "CellList";
	int i;

	if (total == 0 || list == NULL)
		return;

	signal = dbus_message_new_signal(path, OFONO_NETMON_INTERFACE,
						"PropertyChanged");

	if (signal == NULL)
		return;

	dbus_message_iter_init_append(signal, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &key);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					DBUS_STRUCT_BEGIN_CHAR_AS_STRING
					DBUS_TYPE_ARRAY_AS_STRING
					DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
					DBUS_TYPE_STRING_AS_STRING
					DBUS_TYPE_VARIANT_AS_STRING
					DBUS_DICT_ENTRY_END_CHAR_AS_STRING
					DBUS_STRUCT_END_CHAR_AS_STRING,
					&array);

	for (i = 0; i < total; i++) {
		if (list[i].registered)
			append_cell_struct(&list[i], &array);
	}

	dbus_message_iter_close_container(&iter, &array);
	g_dbus_send_message(conn, signal);
}

static void serving_cell_info_callback(const struct ofono_error *error,
					int total,
					const struct ofono_cell_info* list,
					void *data)
{
	struct ofono_netmon *netmon = data;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter array;
	int i;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error occurred during cell list");
		__ofono_dbus_pending_reply(&netmon->pending,
					__ofono_error_failed(netmon->pending));
		return;
	}

	reply = dbus_message_new_method_return(netmon->pending);

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					DBUS_STRUCT_BEGIN_CHAR_AS_STRING
					DBUS_TYPE_ARRAY_AS_STRING
					DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
					DBUS_TYPE_STRING_AS_STRING
					DBUS_TYPE_VARIANT_AS_STRING
					DBUS_DICT_ENTRY_END_CHAR_AS_STRING
					DBUS_STRUCT_END_CHAR_AS_STRING,
					&array);

	for (i = 0; i < total; i++) {
		if (list[i].registered
			&& list[i].type <= OFONO_NETMON_CELL_TYPE_TDSCDMA)
			append_cell_struct(&list[i], &array);
	}

	dbus_message_iter_close_container(&iter, &array);

	__ofono_dbus_pending_reply(&netmon->pending, reply);
}

static DBusMessage *netmon_get_serving_cell_info(DBusConnection *conn,
			DBusMessage *msg, void *data)
{
	struct ofono_netmon *netmon = data;

	if (!netmon->driver->request_update)
		return __ofono_error_not_implemented(msg);

	if (netmon->pending)
		return __ofono_error_busy(msg);

	netmon->pending = dbus_message_ref(msg);

	netmon->driver->request_update(netmon,
					serving_cell_info_callback, netmon);

	return NULL;
}

static void periodic_updates_enabled_cb(const struct ofono_error *error,
					void *data)
{
	struct ofono_netmon *netmon = data;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		ofono_error("Error enabling periodic updates");

		netmon_agent_free(netmon->agent);
		return;
	}
}

static void periodic_updates_disabled_cb(const struct ofono_error *error,
					void *data)
{
	if (error->type != OFONO_ERROR_TYPE_NO_ERROR)
		ofono_error("Error disabling periodic updates");
}

static void agent_removed_cb(gpointer user_data)
{
	struct ofono_netmon *netmon = user_data;

	netmon->agent = NULL;

	netmon->driver->enable_periodic_update(netmon, 0, 0,
						periodic_updates_disabled_cb,
						NULL);
}

static DBusMessage *netmon_register_agent(DBusConnection *conn,
				DBusMessage *msg, void *data)
{
	struct ofono_netmon *netmon = data;
	const char *agent_path;
	const unsigned int enable = 1;
	unsigned int period;

	if (netmon->agent)
		return __ofono_error_busy(msg);

	if (!netmon->driver->enable_periodic_update)
		return __ofono_error_not_implemented(msg);

	if (dbus_message_get_args(msg, NULL,
				DBUS_TYPE_OBJECT_PATH, &agent_path,
				DBUS_TYPE_UINT32, &period,
				DBUS_TYPE_INVALID) == FALSE)
		return __ofono_error_invalid_args(msg);

	if (!dbus_validate_path(agent_path, NULL))
		return __ofono_error_invalid_format(msg);

	if (!period)
		return __ofono_error_invalid_args(msg);

	/* minimum period is 5 seconds, to avoid frequent updates*/
	if (period < 5)
		period = 5;

	netmon->agent = netmon_agent_new(agent_path,
					dbus_message_get_sender(msg));

	if (netmon->agent == NULL)
		return __ofono_error_failed(msg);

	netmon_agent_set_removed_notify(netmon->agent, agent_removed_cb, netmon);

	netmon->driver->enable_periodic_update(netmon, enable, period,
					periodic_updates_enabled_cb, netmon);

	return dbus_message_new_method_return(msg);
}

static DBusMessage *netmon_unregister_agent(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct ofono_netmon *netmon = data;
	const char *agent_path;
	const char *agent_bus = dbus_message_get_sender(msg);

	if (!netmon->driver->enable_periodic_update)
		return __ofono_error_not_implemented(msg);

	if (dbus_message_get_args(msg, NULL,
					DBUS_TYPE_OBJECT_PATH, &agent_path,
					DBUS_TYPE_INVALID) == FALSE)
		return __ofono_error_invalid_args(msg);

	if (netmon->agent == NULL)
		return __ofono_error_failed(msg);

	if (!netmon_agent_matches(netmon->agent, agent_path, agent_bus))
		return __ofono_error_access_denied(msg);

	netmon_agent_free(netmon->agent);

	return dbus_message_new_method_return(msg);
}

static DBusMessage *netmon_periodic_update(DBusConnection *conn,
				DBusMessage *msg, void *data)
{
	struct ofono_netmon *netmon = data;
	const unsigned int enable = 1;
	unsigned int period;

	if (!netmon->driver->enable_periodic_update)
		return __ofono_error_not_implemented(msg);

	if (netmon->pending)
		return __ofono_error_busy(msg);

	if (dbus_message_get_args(msg, NULL,
				DBUS_TYPE_UINT32, &period,
				DBUS_TYPE_INVALID) == FALSE)
		return __ofono_error_invalid_args(msg);

	if (!period)
		return __ofono_error_invalid_args(msg);

	/* minimum period is 5 seconds, to avoid frequent updates*/
	if (period < 5)
		period = 5;

	netmon->pending = dbus_message_ref(msg);

	netmon->driver->enable_periodic_update(netmon, enable, period,
					periodic_updates_enabled_cb, netmon);

	return NULL;
}


void ofono_netmon_neighbouring_cell_notify(struct ofono_netmon *netmon,
					int total,
					const struct ofono_cell_info* list,
					void *data)
{
	// like AOSP, no indication message for neighbouring cell list.
}

static void neighbouring_cell_info_callback(const struct ofono_error *error,
						int total,
						const struct ofono_cell_info* list,
						void *data)
{
	struct ofono_netmon *netmon = data;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter array;
	int i;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error occurred during neighbouring cell list");
		__ofono_dbus_pending_reply(&netmon->pending,
					__ofono_error_failed(netmon->pending));
		return;
	}

	reply = dbus_message_new_method_return(netmon->pending);

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					DBUS_STRUCT_BEGIN_CHAR_AS_STRING
					DBUS_TYPE_ARRAY_AS_STRING
					DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
					DBUS_TYPE_STRING_AS_STRING
					DBUS_TYPE_VARIANT_AS_STRING
					DBUS_DICT_ENTRY_END_CHAR_AS_STRING
					DBUS_STRUCT_END_CHAR_AS_STRING,
					&array);

	for (i = 0; i < total; i++) {
		append_cell_struct(&list[i], &array);
	}

	dbus_message_iter_close_container(&iter, &array);

	__ofono_dbus_pending_reply(&netmon->pending, reply);
}

static DBusMessage *netmon_get_neighbouring_cell_info(DBusConnection *conn,
			DBusMessage *msg, void *data)
{
	struct ofono_netmon *netmon = data;

	if (!netmon->driver->neighbouring_cell_update)
		return __ofono_error_not_implemented(msg);

	if (netmon->pending)
		return __ofono_error_busy(msg);

	netmon->pending = dbus_message_ref(msg);

	netmon->driver->neighbouring_cell_update(netmon,
				neighbouring_cell_info_callback, netmon);

	return NULL;
}

static const GDBusMethodTable netmon_methods[] = {
	{ GDBUS_ASYNC_METHOD("GetServingCellInformation",
			NULL, GDBUS_ARGS({ "cellinfo", "a(a{sv})" }),
			netmon_get_serving_cell_info) },
	{ GDBUS_METHOD("RegisterAgent",
			GDBUS_ARGS({ "path", "o"}, { "period", "u"}), NULL,
			netmon_register_agent) },
	{ GDBUS_METHOD("UnregisterAgent",
			GDBUS_ARGS({ "agent", "o" }), NULL,
			netmon_unregister_agent) },
	{ GDBUS_ASYNC_METHOD("GetNeighbouringCellInformation",
			NULL, GDBUS_ARGS({ "cellinfo", "a(a{sv})" }),
			netmon_get_neighbouring_cell_info) },
	{ GDBUS_ASYNC_METHOD("CellInfoUpdateRate",
			GDBUS_ARGS({ "period", "u"}), NULL,
			netmon_periodic_update) },
	{ }
};

static const GDBusSignalTable netmon_signals[] = {
	{ GDBUS_SIGNAL("PropertyChanged",
			GDBUS_ARGS({ "name", "s" }, { "value", "v" })) },
	{ }
};

int ofono_netmon_driver_register(const struct ofono_netmon_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	if (d->probe == NULL)
		return -EINVAL;

	g_drivers = g_slist_prepend(g_drivers, (void *) d);

	return 0;
}

void ofono_netmon_driver_unregister(const struct ofono_netmon_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	g_drivers = g_slist_remove(g_drivers, (void *) d);
}

static void netmon_unregister(struct ofono_atom *atom)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem *modem = __ofono_atom_get_modem(atom);
	const char *path = __ofono_atom_get_path(atom);

	ofono_modem_remove_interface(modem, OFONO_NETMON_INTERFACE);
	g_dbus_unregister_interface(conn, path, OFONO_NETMON_INTERFACE);
}

static void netmon_remove(struct ofono_atom *atom)
{
	struct ofono_netmon *netmon = __ofono_atom_get_data(atom);

	if (netmon == NULL)
		return;

	if (netmon->driver && netmon->driver->remove)
		netmon->driver->remove(netmon);

	g_free(netmon);
}

struct ofono_netmon *ofono_netmon_create(struct ofono_modem *modem,
			unsigned int vendor, const char *driver, void *data)
{
	struct ofono_netmon *netmon;
	GSList *l;

	if (driver == NULL)
		return NULL;

	netmon = g_try_new0(struct ofono_netmon, 1);

	if (netmon == NULL)
		return NULL;

	netmon->atom = __ofono_modem_add_atom(modem, OFONO_ATOM_TYPE_NETMON,
						netmon_remove, netmon);

	for (l = g_drivers; l; l = l->next) {
		const struct ofono_netmon_driver *drv = l->data;

		if (g_strcmp0(drv->name, driver))
			continue;

		if (drv->probe(netmon, vendor, data) < 0)
			continue;

		netmon->driver = drv;
		break;
	}

	return netmon;
}

void ofono_netmon_register(struct ofono_netmon *netmon)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem *modem = __ofono_atom_get_modem(netmon->atom);
	const char *path = __ofono_atom_get_path(netmon->atom);

	if (!g_dbus_register_interface(conn, path,
				OFONO_NETMON_INTERFACE,
				netmon_methods,
				netmon_signals,
				NULL, netmon, NULL)) {
		ofono_error("Could not create %s interface",
				OFONO_NETMON_INTERFACE);
		return;
	}

	ofono_modem_add_interface(modem, OFONO_NETMON_INTERFACE);

	__ofono_atom_register(netmon->atom, netmon_unregister);
}

void ofono_netmon_remove(struct ofono_netmon *netmon)
{
	__ofono_atom_free(netmon->atom);
}

void ofono_netmon_set_data(struct ofono_netmon *netmon, void *data)
{
	netmon->driver_data = data;
}

void *ofono_netmon_get_data(struct ofono_netmon *netmon)
{
	return netmon->driver_data;
}
