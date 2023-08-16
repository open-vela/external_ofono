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

#include <string.h>
#include <glib.h>
#include <gdbus.h>

#include "ofono.h"
#include "storage.h"

#define SETTINGS_KEY "ofono"
#define SETTINGS_STORE "ofonosetting"
#define SETTINGS_GROUP "Settings"

#define DEFAULT_SLOT_NOT_SET -1

struct ofono_manager {
	GKeyFile *settings;
	int data_slot;
	int voicecall_slot;
};

static void append_modem(struct ofono_modem *modem, void *userdata)
{
	DBusMessageIter *array = userdata;
	const char *path = ofono_modem_get_path(modem);
	DBusMessageIter entry, dict;

	if (ofono_modem_is_registered(modem) == FALSE)
		return;

	dbus_message_iter_open_container(array, DBUS_TYPE_STRUCT,
						NULL, &entry);
	dbus_message_iter_append_basic(&entry, DBUS_TYPE_OBJECT_PATH,
					&path);
	dbus_message_iter_open_container(&entry, DBUS_TYPE_ARRAY,
				OFONO_PROPERTIES_ARRAY_SIGNATURE,
				&dict);

	__ofono_modem_append_properties(modem, &dict);
	dbus_message_iter_close_container(&entry, &dict);
	dbus_message_iter_close_container(array, &entry);
}

static DBusMessage *manager_get_modems(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter array;

	reply = dbus_message_new_method_return(msg);
	if (reply == NULL)
		return NULL;

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					DBUS_STRUCT_BEGIN_CHAR_AS_STRING
					DBUS_TYPE_OBJECT_PATH_AS_STRING
					DBUS_TYPE_ARRAY_AS_STRING
					DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
					DBUS_TYPE_STRING_AS_STRING
					DBUS_TYPE_VARIANT_AS_STRING
					DBUS_DICT_ENTRY_END_CHAR_AS_STRING
					DBUS_STRUCT_END_CHAR_AS_STRING,
					&array);
	__ofono_modem_foreach(append_modem, &array);
	dbus_message_iter_close_container(&iter, &array);

	return reply;
}

static void append_properties(struct ofono_manager *mgr, DBusMessageIter *dict)
{
	ofono_dbus_dict_append(dict, "DataSlot", DBUS_TYPE_INT32, &mgr->data_slot);
	ofono_dbus_dict_append(dict, "VoiceCallSlot", DBUS_TYPE_INT32, &mgr->voicecall_slot);
}

static DBusMessage *manager_set_property(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct ofono_manager *manager = data;
	DBusMessageIter iter, var;
	const char *name;
	int new_dds, new_dcs;

	if (dbus_message_iter_init(msg, &iter) == FALSE)
		return __ofono_error_invalid_args(msg);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
		return __ofono_error_invalid_args(msg);

	dbus_message_iter_get_basic(&iter, &name);
	dbus_message_iter_next(&iter);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT)
		return __ofono_error_invalid_args(msg);

	dbus_message_iter_recurse(&iter, &var);

	if (g_str_equal(name, "DataSlot") == TRUE) {
		if (dbus_message_iter_get_arg_type(&var) != DBUS_TYPE_INT32)
			return __ofono_error_invalid_args(msg);

		dbus_message_iter_get_basic(&var, &new_dds);
		if (new_dds == manager->data_slot) {
			return NULL;
		}

		manager->data_slot = new_dds;
		g_key_file_set_integer(manager->settings, SETTINGS_GROUP, "DataSlot", new_dds);

		g_dbus_send_reply(conn, msg, DBUS_TYPE_INVALID);

		// Notify all watches of data slot changed.
		ofono_dbus_signal_property_changed(conn, OFONO_MANAGER_PATH,
			OFONO_MANAGER_INTERFACE, "DataSlot", DBUS_TYPE_INT32, &new_dds);
		return NULL;
	} else if (g_str_equal(name, "VoiceCallSlot")) {
		if (dbus_message_iter_get_arg_type(&var) != DBUS_TYPE_INT32)
			return __ofono_error_invalid_args(msg);

		dbus_message_iter_get_basic(&var, &new_dcs);

		if (new_dcs == manager->voicecall_slot) {
			return NULL;
		}

		manager->voicecall_slot = new_dcs;
		g_key_file_set_integer(manager->settings, SETTINGS_GROUP, "VoiceCallSlot", new_dcs);
		storage_sync(SETTINGS_KEY, SETTINGS_STORE, manager->settings);

		g_dbus_send_reply(conn, msg, DBUS_TYPE_INVALID);

		// Notify all watches of voicecall slot changed.
		ofono_dbus_signal_property_changed(conn, OFONO_MANAGER_PATH,
			OFONO_MANAGER_INTERFACE, "VoiceCallSlot", DBUS_TYPE_INT32, &new_dcs);
		return NULL;
	}

	return __ofono_error_invalid_args(msg);
}

static DBusMessage *manager_get_properties(DBusConnection *conn,
						DBusMessage *msg, void *data)
{
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter dict;
	struct ofono_manager *manager = data;

	reply = dbus_message_new_method_return(msg);
	if (reply == NULL)
		return NULL;

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					OFONO_PROPERTIES_ARRAY_SIGNATURE,
					&dict);
	append_properties(manager, &dict);
	dbus_message_iter_close_container(&iter, &dict);

	return reply;
}

static const GDBusMethodTable manager_methods[] = {
	{ GDBUS_METHOD("GetModems",
				NULL, GDBUS_ARGS({ "modems", "a(oa{sv})" }),
				manager_get_modems) },
	{ GDBUS_METHOD("GetProperties",
			NULL, GDBUS_ARGS({ "properties", "a{sv}" }),
			manager_get_properties) },
	{ GDBUS_ASYNC_METHOD("SetProperty",
			GDBUS_ARGS({ "property", "s" }, { "value", "v" }),
			NULL, manager_set_property) },
	{ }
};

static const GDBusSignalTable manager_signals[] = {
	{ GDBUS_SIGNAL("ModemAdded",
		GDBUS_ARGS({ "path", "o" }, { "properties", "a{sv}" })) },
	{ GDBUS_SIGNAL("ModemRemoved",
		GDBUS_ARGS({ "path", "o" })) },
	{ GDBUS_SIGNAL("PropertyChanged",
			GDBUS_ARGS({ "name", "s" }, { "value", "v" })) },
	{ }
};

static void manager_data_free(void *user_data)
{
	struct ofono_manager *manager = user_data;

	if (manager->settings) {
		storage_close(SETTINGS_KEY, SETTINGS_STORE, manager->settings, TRUE);
	}

	g_free(manager);
}

int __ofono_manager_init(void)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_manager *manager;
	gboolean ret;
	GError *error;

	manager = g_try_malloc0(sizeof(struct ofono_manager));
	if (manager == NULL)
		return -ENOMEM;

	manager->settings = storage_open(SETTINGS_KEY, SETTINGS_STORE);
	manager->data_slot = g_key_file_get_integer(
		manager->settings, SETTINGS_GROUP, "DataSlot", NULL);

	error = NULL;
	manager->voicecall_slot = g_key_file_get_integer(
		manager->settings, SETTINGS_GROUP, "VoiceCallSlot", &error);

	if (error) {
		g_error_free(error);
		manager->voicecall_slot = DEFAULT_SLOT_NOT_SET;
	}

	ret = g_dbus_register_interface(conn, OFONO_MANAGER_PATH,
					OFONO_MANAGER_INTERFACE,
					manager_methods, manager_signals,
					NULL, manager, manager_data_free);

	if (ret == FALSE) {
		manager_data_free(manager);
		return -1;
	}

	return 0;
}

void __ofono_manager_cleanup(void)
{
	DBusConnection *conn = ofono_dbus_get_connection();

	g_dbus_unregister_interface(conn, OFONO_MANAGER_PATH,
					OFONO_MANAGER_INTERFACE);
}
