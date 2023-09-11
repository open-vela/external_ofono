/*
 *
 *  oFono - Open Source Telephony
 *
 *  Copyright (C) 2017  Intel Corporation. All rights reserved.
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

#include "common.h"
#include "storage.h"

#define SETTINGS_KEY "ims"
#define SETTINGS_STORE "imssetting"
#define SETTINGS_GROUP "Settings"
#define VOICE_CAPABLE_FLAG 0x1
#define SMS_CAPABLE_FLAG 0x4

struct ofono_ims {
	int reg_info;
	int ext_info;
	struct ofono_watchlist *status_watches;
	const struct ofono_ims_driver *driver;
	void *driver_data;
	struct ofono_atom *atom;
	DBusMessage *pending;
	unsigned int radio_online_watch;
	ofono_bool_t user_setting;
	GKeyFile *settings;
	GKeyFile *imsi_settings;
	enum ofono_sim_state sim_state;
	struct ofono_sim *sim;
	char *imsi;
	char *ph_number_from_setting;
	char ph_number[OFONO_MAX_PHONE_NUMBER_LENGTH + 1];
};

static GSList *g_drivers = NULL;

static void extract_number_from_uris(const char *uri, char *ph_number)
{
	const char *ssp_start = NULL;
	const char *ssp_end = NULL;
	int ssp_length;

	if (uri == NULL) {
		ofono_error("uri is null, return! \n");
		return;
	}

	if (strstr(uri, "tel") == NULL && strstr(uri, "sip") == NULL) {
		ofono_error("invaild uri, return! \n");
		return;
	}

	/* ssp: SchemeSpecificPart */
	ssp_start = strchr(uri, '+');
	if (ssp_start == NULL) {
		ofono_error("uri does not contain a phone number! \n");
		return;
	}

	ssp_end = strchr(ssp_start, '@');
	if (ssp_end == NULL) {
		ssp_end = strchr(ssp_start, '\0');
	}

	ssp_length = ssp_end - ssp_start;
	if (ssp_length > 0 && ssp_length <= OFONO_MAX_PHONE_NUMBER_LENGTH) {
		strncpy(ph_number, ssp_start, ssp_length);
		ph_number[ssp_length] = '\0';
	} else {
		ofono_error("extract phone number from uri failed !");
	}

	return;
}

static void ims_load_settings(struct ofono_ims *ims)
{
	GError *error;

	ims->settings = storage_open(SETTINGS_KEY, SETTINGS_STORE);
	if (ims->settings == NULL) {
		ofono_warn("ims setting storage open failed");
		ims->user_setting = TRUE;
		return;
	}

	error = NULL;
	ims->user_setting = g_key_file_get_boolean(ims->settings, SETTINGS_GROUP,
					"ImsOn", &error);
	if (error) {
		ofono_error("ims switcher storage read failed");

		g_error_free(error);
		ims->user_setting = TRUE;
		g_key_file_set_boolean(ims->settings, SETTINGS_GROUP,
						"ImsOn", ims->user_setting);
	}
}

static void ims_load_settings_from_imsi(struct ofono_ims *ims)
{
	struct ofono_modem *modem = __ofono_atom_get_modem(ims->atom);
	struct ofono_sim *sim = __ofono_atom_find(OFONO_ATOM_TYPE_SIM, modem);
	GError *error;
	const char *imsi;

	ims->sim = sim;

	imsi = ofono_sim_get_imsi(ims->sim);
	if (imsi == NULL)
		return;

	ims->imsi_settings = storage_open(imsi, SETTINGS_STORE);
	if (ims->imsi_settings == NULL) {
		ofono_error("ims imsi setting storage open failed");
		return;
	}

	ims->imsi = g_strdup(imsi);

	error = NULL;
	ims->ph_number_from_setting = g_key_file_get_string(ims->imsi_settings, SETTINGS_GROUP,
					"ImsNumber", &error);

	if (error) {
		ofono_error("ims number storage read failed");

		g_error_free(error);
		ims->ph_number_from_setting = ims->ph_number;
		g_key_file_set_string(ims->imsi_settings, SETTINGS_GROUP,
						"ImsNumber", ims->ph_number_from_setting);
	}
}

static void ims_close_settings(struct ofono_ims *ims)
{
	if (ims->settings) {
		storage_close(SETTINGS_KEY, SETTINGS_STORE, ims->settings, TRUE);
		ims->settings = NULL;
	}
}

static void ims_close_settings_from_imsi(struct ofono_ims *ims)
{
	if (ims->imsi_settings) {
		storage_close(ims->imsi, SETTINGS_STORE, ims->imsi_settings, TRUE);

		g_free(ims->imsi);
		ims->imsi = NULL;
		ims->imsi_settings = NULL;
	}
}

static DBusMessage *ims_get_properties(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct ofono_ims *ims = data;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter dict;
	dbus_bool_t value;
	const char *ph_number;

	reply = dbus_message_new_method_return(msg);
	if (reply == NULL)
		return NULL;

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					OFONO_PROPERTIES_ARRAY_SIGNATURE,
					&dict);

	value = ims->reg_info ? TRUE : FALSE;
	ofono_dbus_dict_append(&dict, "Registered", DBUS_TYPE_BOOLEAN, &value);

	if (ims->ext_info != -1) {
		value = ims->ext_info & VOICE_CAPABLE_FLAG ? TRUE : FALSE;
		ofono_dbus_dict_append(&dict, "VoiceCapable",
					DBUS_TYPE_BOOLEAN, &value);

		value = ims->ext_info & SMS_CAPABLE_FLAG ? TRUE : FALSE;
		ofono_dbus_dict_append(&dict, "SmsCapable",
					DBUS_TYPE_BOOLEAN, &value);
	}

	ph_number = ims->ph_number_from_setting;
	ofono_dbus_dict_append(&dict, "SubscriberUriNumber", DBUS_TYPE_STRING, &ph_number);

	value = ims->user_setting;
	ofono_dbus_dict_append(&dict, "ImsSwitchStatus", DBUS_TYPE_BOOLEAN, &value);

	dbus_message_iter_close_container(&iter, &dict);

	return reply;
}

static void ims_set_sms_capable(struct ofono_ims *ims, ofono_bool_t status)
{
	const char *path = __ofono_atom_get_path(ims->atom);
	DBusConnection *conn = ofono_dbus_get_connection();
	dbus_bool_t new_value = status;
	dbus_bool_t old_value = ims->ext_info & SMS_CAPABLE_FLAG ? TRUE :
								FALSE;

	if (ims->ext_info >= 0 && old_value == new_value)
		return;

	ofono_dbus_signal_property_changed(conn, path,
						OFONO_IMS_INTERFACE,
						"SmsCapable",
						DBUS_TYPE_BOOLEAN,
						&new_value);
}

static void ims_set_voice_capable(struct ofono_ims *ims, ofono_bool_t status)
{
	const char *path = __ofono_atom_get_path(ims->atom);
	DBusConnection *conn = ofono_dbus_get_connection();
	dbus_bool_t new_value = status;
	dbus_bool_t old_value = ims->ext_info & VOICE_CAPABLE_FLAG ? TRUE :
								FALSE;

	if (ims->ext_info >= 0 && old_value == new_value)
		return;

	ofono_dbus_signal_property_changed(conn, path,
						OFONO_IMS_INTERFACE,
						"VoiceCapable",
						DBUS_TYPE_BOOLEAN,
						&new_value);
}

static void ims_set_registered(struct ofono_ims *ims, ofono_bool_t status)
{
	const char *path = __ofono_atom_get_path(ims->atom);
	DBusConnection *conn = ofono_dbus_get_connection();
	dbus_bool_t new_value = status;
	dbus_bool_t old_value = ims->reg_info ? TRUE : FALSE;

	if (old_value == new_value)
		return;

	ofono_dbus_signal_property_changed(conn, path,
						OFONO_IMS_INTERFACE,
						"Registered",
						DBUS_TYPE_BOOLEAN,
						&new_value);
}

int ofono_ims_get_reg_info(struct ofono_ims *ims)
{
	if (ims == NULL)
		return -1;

	return ims->reg_info;
}

int ofono_ims_get_ext_info(struct ofono_ims *ims)
{
	if (ims == NULL)
		return -1;

	return ims->ext_info;
}

ofono_bool_t ofono_ims_has_sms_capable(int reg_info, int ext_info)
{
	return reg_info && (ext_info & SMS_CAPABLE_FLAG);
}

static void notify_status_watches(struct ofono_ims *ims)
{
	struct ofono_watchlist_item *item;
	GSList *l;
	ofono_ims_status_notify_cb_t notify;

	if (ims->status_watches == NULL)
		return;

	for (l = ims->status_watches->items; l; l = l->next) {
		item = l->data;
		notify = item->notify;

		notify(ims->reg_info, ims->ext_info, item->notify_data);
	}
}

void ofono_ims_status_notify(struct ofono_ims *ims, int reg_info,
				int ext_info, char *subscriber_uri)
{
	dbus_bool_t new_reg_info;
	dbus_bool_t new_voice_capable, new_sms_capable;

	if (ims == NULL)
		return;

	ofono_debug("%s reg_info:%d ext_info:%d", __ofono_atom_get_path(ims->atom),
						reg_info, ext_info);

	if (ims->ext_info == ext_info && ims->reg_info == reg_info)
		return;

	new_reg_info = reg_info ? TRUE : FALSE;
	ims_set_registered(ims, new_reg_info);

	extract_number_from_uris(subscriber_uri, ims->ph_number);

	if (ext_info < 0)
		goto skip;

	new_voice_capable = ext_info & VOICE_CAPABLE_FLAG ? TRUE : FALSE;
	ims_set_voice_capable(ims, new_voice_capable);

	new_sms_capable = ext_info & SMS_CAPABLE_FLAG ? TRUE: FALSE;
	ims_set_sms_capable(ims, new_sms_capable);

skip:
	ims->reg_info = reg_info;
	ims->ext_info = ext_info;
	ims->ph_number_from_setting = NULL;

	notify_status_watches(ims);
}

static void registration_status_cb(const struct ofono_error *error,
						int reg_info, int ext_info,
						char *subscriber_uri, void *data)
{
	struct ofono_ims *ims = data;

	ofono_ims_status_notify(ims, reg_info, ext_info, subscriber_uri);
}

static void register_cb(const struct ofono_error *error, void *data)
{
	struct ofono_ims *ims = data;
	DBusMessage *reply;

	if (error->type == OFONO_ERROR_TYPE_NO_ERROR)
		reply = dbus_message_new_method_return(ims->pending);
	else
		reply = __ofono_error_failed(ims->pending);

	__ofono_dbus_pending_reply(&ims->pending, reply);

	if (ims->driver->registration_status == NULL)
		return;

	ims->driver->registration_status(ims, registration_status_cb, ims);
}

static DBusMessage *ofono_ims_send_register(DBusConnection *conn,
						DBusMessage *msg, void *data)
{
	struct ofono_ims *ims = data;
	const char *path = __ofono_atom_get_path(ims->atom);

	if (ims->pending)
		return __ofono_error_busy(msg);

	if (ims->driver->ims_register == NULL)
		return __ofono_error_not_implemented(msg);

	ims->pending = dbus_message_ref(msg);

	ims->driver->ims_register(ims, register_cb, ims);

	ims->user_setting = TRUE;
	g_key_file_set_boolean(ims->settings, SETTINGS_GROUP,
					"ImsOn", ims->user_setting);

	storage_sync(SETTINGS_KEY, SETTINGS_STORE, ims->settings);

	ofono_dbus_signal_property_changed(conn, path,
					OFONO_IMS_INTERFACE,
					"ImsSwitchStatus", DBUS_TYPE_BOOLEAN, &ims->user_setting);

	return NULL;
}

static DBusMessage *ofono_ims_unregister(DBusConnection *conn,
						DBusMessage *msg, void *data)
{
	struct ofono_ims *ims = data;
	const char *path = __ofono_atom_get_path(ims->atom);

	if (ims->pending)
		return __ofono_error_busy(msg);

	if (ims->driver->ims_unregister == NULL)
		return __ofono_error_not_implemented(msg);

	ims->pending = dbus_message_ref(msg);

	ims->driver->ims_unregister(ims, register_cb, ims);

	ims->user_setting = FALSE;
	g_key_file_set_boolean(ims->settings, SETTINGS_GROUP,
					"ImsOn", ims->user_setting);

	storage_sync(SETTINGS_KEY, SETTINGS_STORE, ims->settings);

	ofono_dbus_signal_property_changed(conn, path,
					OFONO_IMS_INTERFACE,
					"ImsSwitchStatus", DBUS_TYPE_BOOLEAN, &ims->user_setting);

	return NULL;
}

static void ims_config_cb(const struct ofono_error *error, void *data)
{
	ofono_debug("%s, error type = %d", __func__, error->type);
}

static void send_ims_config(struct ofono_ims *ims)
{
	const struct ofono_ims_driver *driver = ims->driver;
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(ims->atom);

	if (driver == NULL)
		return;

	if (driver->ims_register == NULL)
		return;

	ofono_dbus_signal_property_changed(conn, path,
					OFONO_IMS_INTERFACE,
					"ImsSwitchStatus", DBUS_TYPE_BOOLEAN, &ims->user_setting);

	if (ims->user_setting)
		driver->ims_register(ims, ims_config_cb, ims);
	else
		driver->ims_unregister(ims, ims_config_cb, ims);
}

static void set_capability_cb(const struct ofono_error *error, void *data)
{
	struct ofono_ims *ims = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		ofono_error("Error occurred during set capability");
		reply = __ofono_error_failed(ims->pending);
		__ofono_dbus_pending_reply(&ims->pending, reply);
		return;
	}

	reply = dbus_message_new_method_return(ims->pending);
	__ofono_dbus_pending_reply(&ims->pending, reply);

	if (ims->driver->registration_status == NULL)
		return;

	ims->driver->registration_status(ims, registration_status_cb, ims);
}

static DBusMessage *ofono_ims_set_capability(DBusConnection *conn,
						DBusMessage *msg, void *data)
{
	struct ofono_ims *ims = data;
	int cap;

	if (ims->pending)
		return __ofono_error_busy(msg);

	if (ims->driver->set_capable == NULL)
		return __ofono_error_not_implemented(msg);

	if (dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &cap,
				DBUS_TYPE_INVALID) == FALSE)
		return __ofono_error_invalid_args(msg);

	ims->pending = dbus_message_ref(msg);

	ims->driver->set_capable(ims, cap, set_capability_cb, ims);

	return NULL;
}

unsigned int __ofono_ims_add_status_watch(struct ofono_ims *ims,
				ofono_ims_status_notify_cb_t notify,
				void *data, ofono_destroy_func destroy)
{
	struct ofono_watchlist_item *item;

	DBG("%p", ims);

	if (ims == NULL)
		return 0;

	if (notify == NULL)
		return 0;

	item = g_new0(struct ofono_watchlist_item, 1);

	item->notify = notify;
	item->destroy = destroy;
	item->notify_data = data;

	return __ofono_watchlist_add_item(ims->status_watches, item);
}

gboolean __ofono_ims_remove_status_watch(struct ofono_ims *ims,
						unsigned int id)
{
	DBG("%p", ims);

	if (ims == NULL)
		return FALSE;

	return __ofono_watchlist_remove_item(ims->status_watches, id);
}

static const GDBusMethodTable ims_methods[] = {
	{ GDBUS_METHOD("GetProperties",
			NULL, GDBUS_ARGS({ "properties", "a{sv}" }),
			ims_get_properties) },
	{ GDBUS_ASYNC_METHOD("Register", NULL, NULL,
			ofono_ims_send_register) },
	{ GDBUS_ASYNC_METHOD("Unregister", NULL, NULL,
			ofono_ims_unregister) },
	{ GDBUS_ASYNC_METHOD("SetCapability",
			GDBUS_ARGS({ "capable", "i" }), NULL,
			ofono_ims_set_capability) },
	{ }
};

static const GDBusSignalTable ims_signals[] = {
	{ GDBUS_SIGNAL("PropertyChanged",
			GDBUS_ARGS({ "name", "s" }, { "value", "v" })) },
	{ }
};

static void ims_radio_state_change(int state, void *data)
{
}

static void ims_sim_state_change(int state, void *data)
{
	struct ofono_atom *atom = data;
	struct ofono_ims *ims = __ofono_atom_get_data(atom);

	ofono_debug("%s , old state = %d, new state = %d", __func__, ims->sim_state, state);

	if (ims->sim_state == state)
		return;
	ims->sim_state = state;

	switch (state) {
	case OFONO_SIM_STATE_NOT_PRESENT:
	case OFONO_SIM_STATE_RESETTING:
	case OFONO_SIM_STATE_ERROR:
		ims_close_settings(ims);
		ims_close_settings_from_imsi(ims);
		break;
	case OFONO_SIM_STATE_READY:
		ims_load_settings(ims);
		send_ims_config(ims);
		ims_load_settings_from_imsi(ims);
		break;
	}
}

static void ims_atom_remove(struct ofono_atom *atom)
{
	struct ofono_ims *ims = __ofono_atom_get_data(atom);

	DBG("atom: %p", atom);

	if (ims == NULL)
		return;

	if (ims->driver && ims->driver->remove)
		ims->driver->remove(ims);

	g_free(ims);
}

struct ofono_ims *ofono_ims_create(struct ofono_modem *modem,
					const char *driver, void *data)
{
	struct ofono_ims *ims;
	GSList *l;

	if (driver == NULL)
		return NULL;

	ims = g_try_new0(struct ofono_ims, 1);

	if (ims == NULL)
		return NULL;

	ims->atom = __ofono_modem_add_atom(modem, OFONO_ATOM_TYPE_IMS,
						ims_atom_remove, ims);

	__ofono_atom_add_radio_state_watch(ims->atom, ims_radio_state_change);
	__ofono_atom_add_sim_state_watch(ims->atom, ims_sim_state_change);

	ims->reg_info = 0;
	ims->ext_info = 0;
	ims->ph_number[0] = '\0';
	ims->ph_number_from_setting = NULL;
	ims->settings = NULL;
	ims->imsi_settings = NULL;
	ims->sim_state = OFONO_SIM_STATE_NOT_PRESENT;

	for (l = g_drivers; l; l = l->next) {
		const struct ofono_ims_driver *drv = l->data;

		if (g_strcmp0(drv->name, driver))
			continue;

		if (drv->probe(ims, data) < 0)
			continue;

		ims->driver = drv;
		break;
	}

	ofono_debug("IMS atom created");

	return ims;
}

int ofono_ims_driver_register(const struct ofono_ims_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	if (d->probe == NULL)
		return -EINVAL;

	g_drivers = g_slist_prepend(g_drivers, (void *) d);

	return 0;
}

void ofono_ims_driver_unregister(const struct ofono_ims_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	g_drivers = g_slist_remove(g_drivers, (void *) d);
}

static void ims_atom_unregister(struct ofono_atom *atom)
{
	struct ofono_ims *ims = __ofono_atom_get_data(atom);
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem *modem = __ofono_atom_get_modem(atom);
	const char *path = __ofono_atom_get_path(atom);

	ims_close_settings(ims);
	ims_close_settings_from_imsi(ims);

	__ofono_watchlist_free(ims->status_watches);
	ims->status_watches = NULL;

	if (ims->radio_online_watch) {
		__ofono_modem_remove_online_watch(modem, ims->radio_online_watch);
		ims->radio_online_watch = 0;
	}

	ofono_modem_remove_interface(modem, OFONO_IMS_INTERFACE);
	g_dbus_unregister_interface(conn, path, OFONO_IMS_INTERFACE);
}

static void radio_online_watch_cb(struct ofono_modem *modem,
						ofono_bool_t online,
						void *data)
{
	struct ofono_ims *ims = data;

	if (!online) {
		// TODO : need to remove when vowifi is supported
		ims_set_registered(ims, FALSE);
		ims_set_voice_capable(ims, FALSE);
		ims_set_sms_capable(ims, FALSE);
	}
}

static void ofono_ims_finish_register(struct ofono_ims *ims)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem *modem = __ofono_atom_get_modem(ims->atom);
	const char *path = __ofono_atom_get_path(ims->atom);

	if (!g_dbus_register_interface(conn, path,
				OFONO_IMS_INTERFACE,
				ims_methods, ims_signals, NULL,
				ims, NULL)) {
		ofono_error("could not create %s interface",
				OFONO_IMS_INTERFACE);
		return;
	}

	ims->status_watches = __ofono_watchlist_new(g_free);

	ims->radio_online_watch = __ofono_modem_add_online_watch(modem,
				radio_online_watch_cb,
				ims, NULL);

	ofono_modem_add_interface(modem, OFONO_IMS_INTERFACE);
	__ofono_atom_register(ims->atom, ims_atom_unregister);
}

static void registration_init_cb(const struct ofono_error *error,
						int reg_info, int ext_info,
						char *subscriber_uri, void *data)
{
	struct ofono_ims *ims = data;

	if (error->type == OFONO_ERROR_TYPE_NO_ERROR) {
		ims->reg_info = reg_info;
		ims->ext_info = ext_info;

		extract_number_from_uris(subscriber_uri, ims->ph_number);
	}

	ofono_ims_finish_register(ims);
}

void ofono_ims_register(struct ofono_ims *ims)
{
	if (!ims->driver->registration_status) {
		ofono_ims_finish_register(ims);
		return;
	}

	ims->driver->registration_status(ims, registration_init_cb, ims);
}

void ofono_ims_remove(struct ofono_ims *ims)
{
	__ofono_atom_free(ims->atom);
}

void ofono_ims_set_data(struct ofono_ims *ims, void *data)
{
	ims->driver_data = data;
}

void *ofono_ims_get_data(const struct ofono_ims *ims)
{
	return ims->driver_data;
}
