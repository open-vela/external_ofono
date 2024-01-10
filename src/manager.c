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

#define SLOT_NOT_SET "SLOT_NOT_SET"

struct ofono_manager {
	GKeyFile *settings;
	char data_slot[OFONO_MODEM_PATH_LENGTH];
	char voicecall_slot[OFONO_MODEM_PATH_LENGTH];
	char sms_slot[OFONO_MODEM_PATH_LENGTH];
};

static ofono_bool_t modem_path_compare(struct ofono_modem *modem,
					void *userdata)
{
	const char *path = userdata;
	const char *value = ofono_modem_get_path(modem);

	return g_str_equal(path, value);
}

static ofono_bool_t default_modem_can_set(const char *path)
{
	struct ofono_modem *modem;
	struct ofono_sim *sim;

	modem = ofono_modem_find(modem_path_compare, (void *)path);
	if (modem == NULL || !ofono_modem_get_online(modem))
		return FALSE;

	sim = ofono_modem_get_sim(modem);
	if (ofono_sim_get_state(sim) != OFONO_SIM_STATE_READY)
		return FALSE;

	return TRUE;
}

static ofono_bool_t modem_active_compare(struct ofono_modem *modem,
					void *userdata)
{
	struct ofono_sim *sim;
	if (!ofono_modem_get_online(modem))
		return FALSE;

	sim = ofono_modem_get_sim(modem);
	if (ofono_sim_get_state(sim) != OFONO_SIM_STATE_READY)
		return FALSE;

	return TRUE;
}

static struct ofono_modem *get_active_modem(void)
{
	return ofono_modem_find(modem_active_compare, NULL);
}

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

static void append_properties(char *dds, char *dcs, char *dss, DBusMessageIter *dict)
{
	ofono_dbus_dict_append(dict, "DataSlot", DBUS_TYPE_STRING, &dds);
	ofono_dbus_dict_append(dict, "VoiceCallSlot", DBUS_TYPE_STRING, &dcs);
	ofono_dbus_dict_append(dict, "SmsSlot", DBUS_TYPE_STRING, &dss);
}

static DBusMessage *manager_set_property(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct ofono_manager *manager = data;
	DBusMessageIter iter, var;
	const char *name, *new_dds, *new_dcs, *new_dss;

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
		if (dbus_message_iter_get_arg_type(&var) != DBUS_TYPE_STRING)
			return __ofono_error_invalid_args(msg);

		dbus_message_iter_get_basic(&var, &new_dds);
		if (!default_modem_can_set(new_dds))
			return __ofono_error_invalid_args(msg);

		if (g_str_equal(new_dds, manager->data_slot) == TRUE) {
			return NULL;
		}

		strlcpy(manager->data_slot, new_dds, sizeof(manager->data_slot));
		g_key_file_set_string(manager->settings, SETTINGS_GROUP, "DataSlot", new_dds);

		g_dbus_send_reply(conn, msg, DBUS_TYPE_INVALID);

		// Notify all watches of data slot changed.
		ofono_dbus_signal_property_changed(conn, OFONO_MANAGER_PATH,
			OFONO_MANAGER_INTERFACE, "DataSlot", DBUS_TYPE_STRING, &new_dds);
		return NULL;
	} else if (g_str_equal(name, "VoiceCallSlot")) {
		if (dbus_message_iter_get_arg_type(&var) != DBUS_TYPE_STRING)
			return __ofono_error_invalid_args(msg);

		dbus_message_iter_get_basic(&var, &new_dcs);
		if (!default_modem_can_set(new_dcs) && !g_str_equal(new_dcs, SLOT_NOT_SET))
			return __ofono_error_invalid_args(msg);

		if (g_str_equal(new_dcs, manager->voicecall_slot) == TRUE) {
			return NULL;
		}

		strlcpy(manager->voicecall_slot, new_dcs, sizeof(manager->voicecall_slot));
		g_key_file_set_string(manager->settings, SETTINGS_GROUP, "VoiceCallSlot", new_dcs);
		storage_sync(SETTINGS_KEY, SETTINGS_STORE, manager->settings);

		g_dbus_send_reply(conn, msg, DBUS_TYPE_INVALID);

		// Notify all watches of voicecall slot changed.
		ofono_dbus_signal_property_changed(conn, OFONO_MANAGER_PATH,
			OFONO_MANAGER_INTERFACE, "VoiceCallSlot", DBUS_TYPE_STRING, &new_dcs);
		return NULL;
	} else if (g_str_equal(name, "SmsSlot")) {
		if (dbus_message_iter_get_arg_type(&var) != DBUS_TYPE_STRING)
			return __ofono_error_invalid_args(msg);

		dbus_message_iter_get_basic(&var, &new_dss);
		if (!default_modem_can_set(new_dss) && !g_str_equal(new_dss, SLOT_NOT_SET))
			return __ofono_error_invalid_args(msg);

		if (g_str_equal(new_dss, manager->sms_slot) == TRUE) {
			return NULL;
		}

		strlcpy(manager->sms_slot, new_dss, sizeof(manager->sms_slot));
		g_key_file_set_string(manager->settings, SETTINGS_GROUP, "SmsSlot", new_dss);
		storage_sync(SETTINGS_KEY, SETTINGS_STORE, manager->settings);

		g_dbus_send_reply(conn, msg, DBUS_TYPE_INVALID);

		// Notify all watches of sms slot changed.
		ofono_dbus_signal_property_changed(conn, OFONO_MANAGER_PATH,
			OFONO_MANAGER_INTERFACE, "SmsSlot", DBUS_TYPE_STRING, &new_dss);

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
	append_properties(manager->data_slot, manager->voicecall_slot, manager->sms_slot, &dict);
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
	char *data_slot = NULL; 
	char *voicecall_slot = NULL;
	char *sms_slot = NULL;
	struct ofono_modem *modem;

	manager = g_try_malloc0(sizeof(struct ofono_manager));
	if (manager == NULL)
		return -ENOMEM;

	manager->settings = storage_open(SETTINGS_KEY, SETTINGS_STORE);
	data_slot = g_key_file_get_string(
		manager->settings, SETTINGS_GROUP, "DataSlot", NULL);
	if (data_slot == NULL) {
		modem = get_active_modem();
		if (modem)
			strlcpy(manager->data_slot, ofono_modem_get_path(modem), sizeof(manager->data_slot));
		else
			strlcpy(manager->data_slot, SLOT_NOT_SET, sizeof(manager->data_slot));
	} else
		strlcpy(manager->data_slot, data_slot, sizeof(manager->data_slot));

	voicecall_slot = g_key_file_get_string(
		manager->settings, SETTINGS_GROUP, "VoiceCallSlot", NULL);

	if (voicecall_slot == NULL)
		strlcpy(manager->voicecall_slot, SLOT_NOT_SET, sizeof(manager->voicecall_slot));
	else
		strlcpy(manager->voicecall_slot, voicecall_slot, sizeof(manager->voicecall_slot));

	sms_slot = g_key_file_get_string(
		manager->settings, SETTINGS_GROUP, "SmsSlot", NULL);

	if (sms_slot == NULL)
		strlcpy(manager->sms_slot, SLOT_NOT_SET, sizeof(manager->sms_slot));
	else
		strlcpy(manager->sms_slot, sms_slot, sizeof(manager->sms_slot));

	g_free(data_slot);
	g_free(voicecall_slot);
	g_free(sms_slot);
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
