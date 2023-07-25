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
#include <stdio.h>
#include <errno.h>

#include <glib.h>
#include <gdbus.h>

#include "ofono.h"
#include "common.h"
#include "storage.h"
#include "missing.h"

#define DEFAULT_POWERED_TIMEOUT (20)
#define DEFAULT_OEM_REQ_STRING_MAX_LEN 10
#define SETTINGS_KEY "modem"
#define SETTINGS_STORE "store"
#define SETTINGS_GROUP "Settings"

static GSList *g_devinfo_drivers;
static GSList *g_driver_list;
static GSList *g_modem_list;

static int next_modem_id;
static gboolean powering_down;
static int modems_remaining;

static struct ofono_watchlist *g_modemwatches;

static gboolean query_manufacturer(gpointer user);

enum property_type {
	PROPERTY_TYPE_INVALID = 0,
	PROPERTY_TYPE_STRING,
	PROPERTY_TYPE_INTEGER,
	PROPERTY_TYPE_BOOLEAN,
};

enum modem_state {
	MODEM_STATE_POWER_OFF,  /* ril not connected */
	MODEM_STATE_AWARE,      /* ril connected but disabled */
	MODEM_STATE_ALIVE,      /* ril alive */
};

struct ofono_modem {
	char			*path;
	enum modem_state	modem_state;
	GSList			*atoms;
	struct ofono_watchlist	*atom_watches;
	GSList			*interface_list;
	GSList			*feature_list;
	unsigned int		call_ids;
	DBusMessage		*pending;
	guint			interface_update;
	ofono_bool_t		powered;
	ofono_bool_t		powered_pending;
	ofono_bool_t		lockdown;
	char			*lock_owner;
	guint			lock_watch;
	guint			timeout;
	guint			timeout_hint;
	ofono_bool_t		online;
	enum radio_status	radio_status;
	struct ofono_watchlist	*online_watches;
	struct ofono_watchlist	*powered_watches;
	guint			emergency;
	GHashTable		*properties;
	struct ofono_sim	*sim;
	unsigned int		sim_watch;
	unsigned int		sim_ready_watch;
	const struct ofono_modem_driver *driver;
	void			*driver_data;
	char			*driver_type;
	char			*name;
	GKeyFile		*settings;
};

struct ofono_devinfo {
	char *manufacturer;
	char *model;
	char *revision;
	char *serial;
	char *svn;
	unsigned int dun_watch;
	const struct ofono_devinfo_driver *driver;
	void *driver_data;
	struct ofono_atom *atom;
};

struct ofono_atom {
	enum ofono_atom_type type;
	enum modem_state modem_state;
	void (*destruct)(struct ofono_atom *atom);
	void (*unregister)(struct ofono_atom *atom);
	void (*dispatch)(int command_id, void *data);
	void (*sim_state_change)(int sim_state, void *data);
	void (*radio_state_change)(int radio_state, void *data);
	void *data;
	struct ofono_modem *modem;
};

struct atom_watch {
	struct ofono_watchlist_item item;
	enum ofono_atom_type type;
};

struct modem_property {
	enum property_type type;
	void *value;
};

static const char *modem_type_to_string(enum ofono_modem_type type)
{
	switch (type) {
	case OFONO_MODEM_TYPE_HARDWARE:
		return "hardware";
	case OFONO_MODEM_TYPE_HFP:
		return "hfp";
	case OFONO_MODEM_TYPE_SAP:
		return "sap";
	case OFONO_MODEM_TYPE_TEST:
		return "test";
	}

	return "unknown";
}

static void modem_load_settings(struct ofono_modem *modem)
{
	modem->settings = storage_open(SETTINGS_KEY, SETTINGS_STORE);
	if (modem->settings == NULL) {
		return;
	}

	if (g_key_file_has_group(modem->settings, SETTINGS_GROUP)
		&& g_key_file_has_key(modem->settings, SETTINGS_GROUP,
				"Online", NULL)) {
		modem->online = g_key_file_get_boolean(modem->settings, SETTINGS_GROUP,
							"Online", NULL);
	} else {
		modem->online = TRUE;
	}
}

static void modem_close_settings(struct ofono_modem *modem)
{
	if (modem->settings) {
		storage_close(SETTINGS_KEY, SETTINGS_STORE, modem->settings, TRUE);

		modem->settings = NULL;
	}
}

unsigned int __ofono_modem_callid_next(struct ofono_modem *modem)
{
	unsigned int i;

	for (i = 1; i < sizeof(modem->call_ids) * 8; i++) {
		if (modem->call_ids & (1 << i))
			continue;

		return i;
	}

	return 0;
}

void __ofono_modem_callid_hold(struct ofono_modem *modem, int id)
{
	modem->call_ids |= (1 << id);
}

void __ofono_modem_callid_release(struct ofono_modem *modem, int id)
{
	modem->call_ids &= ~(1 << id);
}

void ofono_modem_set_data(struct ofono_modem *modem, void *data)
{
	if (modem == NULL)
		return;

	modem->driver_data = data;
}

void *ofono_modem_get_data(struct ofono_modem *modem)
{
	if (modem == NULL)
		return NULL;

	return modem->driver_data;
}

const char *ofono_modem_get_path(struct ofono_modem *modem)
{
	if (modem)
		return modem->path;

	return NULL;
}

struct ofono_sim *ofono_modem_get_sim(struct ofono_modem *modem)
{
	return __ofono_atom_find(OFONO_ATOM_TYPE_SIM, modem);
}

struct ofono_gprs *ofono_modem_get_gprs(struct ofono_modem *modem)
{
	return __ofono_atom_find(OFONO_ATOM_TYPE_GPRS, modem);
}

struct ofono_voicecall *ofono_modem_get_voicecall(struct ofono_modem *modem)
{
	return __ofono_atom_find(OFONO_ATOM_TYPE_VOICECALL, modem);
}

struct ofono_atom *__ofono_modem_add_atom(struct ofono_modem *modem,
					enum ofono_atom_type type,
					void (*destruct)(struct ofono_atom *),
					void *data)
{
	struct ofono_atom *atom;

	if (modem == NULL)
		return NULL;

	atom = g_new0(struct ofono_atom, 1);

	atom->type = type;
	atom->modem_state = modem->modem_state;
	atom->destruct = destruct;
	atom->data = data;
	atom->modem = modem;
	atom->dispatch = NULL;
	atom->radio_state_change = NULL;
	atom->sim_state_change = NULL;

	modem->atoms = g_slist_prepend(modem->atoms, atom);

	return atom;
}

struct ofono_atom *__ofono_modem_add_atom_offline(struct ofono_modem *modem,
					enum ofono_atom_type type,
					void (*destruct)(struct ofono_atom *),
					void *data)
{
	struct ofono_atom *atom;

	atom = __ofono_modem_add_atom(modem, type, destruct, data);

	atom->modem_state = MODEM_STATE_AWARE;

	return atom;
}

void *__ofono_atom_get_data(struct ofono_atom *atom)
{
	return atom->data;
}

const char *__ofono_atom_get_path(struct ofono_atom *atom)
{
	return atom->modem->path;
}

struct ofono_modem *__ofono_atom_get_modem(struct ofono_atom *atom)
{
	return atom->modem;
}

static void radio_status_change(struct ofono_modem *modem,
	enum radio_status old_status, enum radio_status new_status)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_devinfo *info;

	ofono_debug("%s old status : %d  new status : %d", __func__, old_status, new_status);

	/* radio state depends on modem state somehow */
	if (modem->powered == FALSE
		|| modem->modem_state < MODEM_STATE_ALIVE) {
		new_status = RADIO_STATUS_UNAVAILABLE;
	} else if (modem->emergency == TRUE) {
		new_status = RADIO_STATUS_EMERGENCY_ONLY;
	}

	if (new_status != modem->radio_status)
		modem->radio_status = new_status;

	if (old_status != new_status) {
		struct ofono_atom *atom;
		GSList *l;

		info = __ofono_atom_find(OFONO_ATOM_TYPE_DEVINFO, modem);
		if (info != NULL && new_status != RADIO_STATUS_UNAVAILABLE
			&& old_status == RADIO_STATUS_UNAVAILABLE)
			query_manufacturer(info);

		ofono_dbus_signal_property_changed(conn, modem->path,
					OFONO_MODEM_INTERFACE,
					"RadioState",
					DBUS_TYPE_UINT32, &modem->radio_status);


		for (l = modem->atoms; l; l = l->next) {
			atom = l->data;

			if (atom != NULL && atom->radio_state_change != NULL)
				atom->radio_state_change(new_status, atom);
		}
	}
}

static void call_watches(struct ofono_atom *atom,
				enum ofono_atom_watch_condition cond)
{
	struct ofono_modem *modem = atom->modem;
	GSList *atom_watches = modem->atom_watches->items;
	GSList *l;
	struct atom_watch *watch;
	ofono_atom_watch_func notify;

	for (l = atom_watches; l; l = l->next) {
		watch = l->data;

		if (watch->type != atom->type)
			continue;

		notify = watch->item.notify;
		notify(atom, cond, watch->item.notify_data);
	}
}

void __ofono_atom_register(struct ofono_atom *atom,
			void (*unregister)(struct ofono_atom *))
{
	if (unregister == NULL)
		return;

	atom->unregister = unregister;

	call_watches(atom, OFONO_ATOM_WATCH_CONDITION_REGISTERED);
}

void __ofono_atom_unregister(struct ofono_atom *atom)
{
	if (atom->unregister == NULL)
		return;

	call_watches(atom, OFONO_ATOM_WATCH_CONDITION_UNREGISTERED);

	atom->unregister(atom);
	atom->unregister = NULL;
}

gboolean __ofono_atom_get_registered(struct ofono_atom *atom)
{
	return atom->unregister ? TRUE : FALSE;
}

void __ofono_atom_setup_dispatcher(struct ofono_atom *atom,
				void (*dispatch)(int command_id, void *data))
{
	atom->dispatch = dispatch;
}

void __ofono_atom_add_sim_state_watch(struct ofono_atom *atom,
				void (*sim_state_change)(int sim_state, void *data))
{
	atom->sim_state_change = sim_state_change;
}

void __ofono_atom_add_radio_state_watch(struct ofono_atom *atom,
				void (*radio_state_change)(int radio_state, void *data))
{
	atom->radio_state_change = radio_state_change;
}

unsigned int __ofono_modem_add_atom_watch(struct ofono_modem *modem,
					enum ofono_atom_type type,
					ofono_atom_watch_func notify,
					void *data, ofono_destroy_func destroy)
{
	struct atom_watch *watch;
	unsigned int id;
	GSList *l;
	struct ofono_atom *atom;

	if (notify == NULL)
		return 0;

	watch = g_new0(struct atom_watch, 1);

	watch->type = type;
	watch->item.notify = notify;
	watch->item.destroy = destroy;
	watch->item.notify_data = data;

	id = __ofono_watchlist_add_item(modem->atom_watches,
					(struct ofono_watchlist_item *)watch);

	for (l = modem->atoms; l; l = l->next) {
		atom = l->data;

		if (atom->type != type || atom->unregister == NULL)
			continue;

		notify(atom, OFONO_ATOM_WATCH_CONDITION_REGISTERED, data);
	}

	return id;
}

gboolean __ofono_modem_remove_atom_watch(struct ofono_modem *modem,
						unsigned int id)
{
	return __ofono_watchlist_remove_item(modem->atom_watches, id);
}

struct ofono_atom *__ofono_modem_find_atom(struct ofono_modem *modem,
						enum ofono_atom_type type)
{
	GSList *l;
	struct ofono_atom *atom;

	if (modem == NULL)
		return NULL;

	for (l = modem->atoms; l; l = l->next) {
		atom = l->data;

		if (atom->type == type && atom->unregister != NULL)
			return atom;
	}

	return NULL;
}

void __ofono_modem_foreach_atom(struct ofono_modem *modem,
				enum ofono_atom_type type,
				ofono_atom_func callback, void *data)
{
	GSList *l;
	struct ofono_atom *atom;

	if (modem == NULL)
		return;

	for (l = modem->atoms; l; l = l->next) {
		atom = l->data;

		if (atom->type != type)
			continue;

		callback(atom, data);
	}
}

void __ofono_modem_foreach_registered_atom(struct ofono_modem *modem,
						enum ofono_atom_type type,
						ofono_atom_func callback,
						void *data)
{
	GSList *l;
	struct ofono_atom *atom;

	if (modem == NULL)
		return;

	for (l = modem->atoms; l; l = l->next) {
		atom = l->data;

		if (atom->type != type)
			continue;

		if (atom->unregister == NULL)
			continue;

		callback(atom, data);
	}
}

void __ofono_atom_free(struct ofono_atom *atom)
{
	struct ofono_modem *modem = atom->modem;

	modem->atoms = g_slist_remove(modem->atoms, atom);

	__ofono_atom_unregister(atom);

	if (atom->destruct)
		atom->destruct(atom);

	g_free(atom);
}

static void flush_atoms(struct ofono_modem *modem, enum modem_state new_state)
{
	GSList *cur;
	GSList *prev;
	GSList *tmp;

	DBG("");

	prev = NULL;
	cur = modem->atoms;

	while (cur) {
		struct ofono_atom *atom = cur->data;

		if (atom->modem_state <= new_state) {
			prev = cur;
			cur = cur->next;
			continue;
		}

		__ofono_atom_unregister(atom);

		if (atom->destruct)
			atom->destruct(atom);

		g_free(atom);

		if (prev)
			prev->next = cur->next;
		else
			modem->atoms = cur->next;

		tmp = cur;
		cur = cur->next;
		g_slist_free_1(tmp);
	}
}

static void notify_online_watches(struct ofono_modem *modem)
{
	struct ofono_watchlist_item *item;
	GSList *l;
	ofono_modem_online_notify_func notify;

	if (modem->online_watches == NULL)
		return;

	for (l = modem->online_watches->items; l; l = l->next) {
		item = l->data;
		notify = item->notify;
		notify(modem, modem->online, item->notify_data);
	}
}

static void notify_powered_watches(struct ofono_modem *modem)
{
	struct ofono_watchlist_item *item;
	GSList *l;
	ofono_modem_powered_notify_func notify;

	if (modem->powered_watches == NULL)
		return;

	for (l = modem->powered_watches->items; l; l = l->next) {
		item = l->data;
		notify = item->notify;
		notify(modem, modem->powered, item->notify_data);
	}
}

static void modem_change_state(struct ofono_modem *modem,
				enum modem_state new_state)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem_driver const *driver = modem->driver;
	enum modem_state old_state = modem->modem_state;

	ofono_debug("%s, old state: %d, new state: %d", __func__, old_state, new_state);

	if (old_state == new_state)
		return;

	modem->modem_state = new_state;

	if (old_state > new_state)
		flush_atoms(modem, new_state);

	switch (new_state) {
	case MODEM_STATE_POWER_OFF:
		modem->call_ids = 0;
		break;

	case MODEM_STATE_AWARE:
		break;

	case MODEM_STATE_ALIVE:
		/* batch initialization */
		if (driver->pre_sim)
			driver->pre_sim(modem);

		if (driver->post_sim)
			driver->post_sim(modem);

		if (driver->post_online)
			driver->post_online(modem);

		break;
	}

	ofono_dbus_signal_property_changed(conn, modem->path,
			OFONO_MODEM_INTERFACE,
			"ModemState",
			DBUS_TYPE_UINT32, &modem->modem_state);
}

unsigned int __ofono_modem_add_online_watch(struct ofono_modem *modem,
					ofono_modem_online_notify_func notify,
					void *data, ofono_destroy_func destroy)
{
	struct ofono_watchlist_item *item;

	if (modem == NULL || notify == NULL)
		return 0;

	item = g_new0(struct ofono_watchlist_item, 1);

	item->notify = notify;
	item->destroy = destroy;
	item->notify_data = data;

	return __ofono_watchlist_add_item(modem->online_watches, item);
}

void __ofono_modem_remove_online_watch(struct ofono_modem *modem,
					unsigned int id)
{
	__ofono_watchlist_remove_item(modem->online_watches, id);
}

unsigned int __ofono_modem_add_powered_watch(struct ofono_modem *modem,
					ofono_modem_powered_notify_func notify,
					void *data, ofono_destroy_func destroy)
{
	struct ofono_watchlist_item *item;

	if (modem == NULL || notify == NULL)
		return 0;

	item = g_new0(struct ofono_watchlist_item, 1);

	item->notify = notify;
	item->destroy = destroy;
	item->notify_data = data;

	return __ofono_watchlist_add_item(modem->powered_watches, item);
}

void __ofono_modem_remove_powered_watch(struct ofono_modem *modem,
					unsigned int id)
{
	__ofono_watchlist_remove_item(modem->powered_watches, id);
}

static gboolean modem_is_always_online(struct ofono_modem *modem)
{
	if (modem->driver->set_online == NULL)
		return TRUE;

	if (ofono_modem_get_boolean(modem, "AlwaysOnline") == TRUE)
		return TRUE;

	return FALSE;
}

static void common_online_cb(const struct ofono_error *error, void *data)
{
	struct ofono_modem *modem = data;
	enum radio_status old_radio_state = modem->radio_status;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR)
		return;

	modem->radio_status = RADIO_STATUS_ON;
	radio_status_change(modem, old_radio_state, RADIO_STATUS_ON);
}

static void online_cb(const struct ofono_error *error, void *data)
{
	struct ofono_modem *modem = data;
	DBusMessage *reply;

	if (error->type == OFONO_ERROR_TYPE_NO_ERROR)
		reply = dbus_message_new_method_return(modem->pending);
	else
		reply = __ofono_error_failed(modem->pending);

	__ofono_dbus_pending_reply(&modem->pending, reply);
}

static void common_offline_cb(const struct ofono_error *error, void *data)
{
	struct ofono_modem *modem = data;
	enum radio_status old_radio_state = modem->radio_status;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR)
		return;

	modem->radio_status = RADIO_STATUS_OFF;
	radio_status_change(modem, old_radio_state, RADIO_STATUS_OFF);
}

static void offline_cb(const struct ofono_error *error, void *data)
{
	struct ofono_modem *modem = data;
	DBusMessage *reply;

	if (error->type == OFONO_ERROR_TYPE_NO_ERROR)
		reply = dbus_message_new_method_return(modem->pending);
	else
		reply = __ofono_error_failed(modem->pending);

	__ofono_dbus_pending_reply(&modem->pending, reply);
}

static void set_radio_power(struct ofono_modem *modem, ofono_bool_t new_online,
	void (*callback)(const struct ofono_error *error, void *data))
{
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem_driver const *driver = modem->driver;

	driver->set_online(modem, new_online, callback, modem);

	if (new_online == modem->online)
		return;

	modem->online = new_online;

	g_key_file_set_boolean(modem->settings, SETTINGS_GROUP,
							"Online",
							new_online);
	storage_sync(SETTINGS_KEY, SETTINGS_STORE, modem->settings);

	ofono_dbus_signal_property_changed(conn, modem->path,
						OFONO_MODEM_INTERFACE,
						"Online", DBUS_TYPE_BOOLEAN,
						&modem->online);

	notify_online_watches(modem);
}

static void modem_activity_info_query_cb(const struct ofono_error *error,
						int activity_info[], unsigned int length,
						void *data)
{
	struct ofono_modem *modem = data;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter args;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		DBG("Error during modem access activity info query");

		reply = __ofono_error_failed(modem->pending);
		__ofono_dbus_pending_reply(&modem->pending, reply);

		return;
	}

	reply = dbus_message_new_method_return(modem->pending);
	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					DBUS_TYPE_INT32_AS_STRING, &args);

	dbus_message_iter_append_fixed_array(&args,
                                     DBUS_TYPE_INT32,
                                     &activity_info,
                                     length);

	dbus_message_iter_close_container(&iter, &args);

	__ofono_dbus_pending_reply(&modem->pending, reply);
}

static void modem_enable_cb(const struct ofono_error *error, void *data)
{
	struct ofono_modem *modem = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		ofono_error("Error occus when enabling modem.");

		reply = __ofono_error_failed(modem->pending);
		__ofono_dbus_pending_reply(&modem->pending, reply);

		return;
	}

	if (modem->modem_state < MODEM_STATE_ALIVE)
		modem_change_state(modem, MODEM_STATE_ALIVE);

	reply = dbus_message_new_method_return(modem->pending);

	__ofono_dbus_pending_reply(&modem->pending, reply);

	if (g_key_file_has_group(modem->settings, SETTINGS_GROUP)
		&& g_key_file_has_key(modem->settings, SETTINGS_GROUP,
				"Online", NULL)) {
		modem->online = g_key_file_get_boolean(modem->settings, SETTINGS_GROUP,
							"Online", NULL);
	}

	set_radio_power(modem, modem->online,
			modem->online ? common_online_cb : common_offline_cb);
}

static void modem_disable_cb(const struct ofono_error *error, void *data)
{
	struct ofono_modem *modem = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		ofono_error("Error occus when disabling modem.");

		reply = __ofono_error_failed(modem->pending);
		__ofono_dbus_pending_reply(&modem->pending, reply);

		return;
	}

	if (modem->modem_state > MODEM_STATE_AWARE)
		modem_change_state(modem, MODEM_STATE_AWARE);

	reply = dbus_message_new_method_return(modem->pending);

	__ofono_dbus_pending_reply(&modem->pending, reply);
}

static void modem_status_query_cb(const struct ofono_error *error,
						int status, void *data)
{
	struct ofono_modem *modem = data;
	DBusMessage *reply;
	DBusMessageIter iter;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		ofono_error("Error occurs when querying modem status.");

		if (modem->pending) {
			reply = __ofono_error_failed(modem->pending);
			__ofono_dbus_pending_reply(&modem->pending, reply);
		}

		/* likely, some modems don't support modem state query */
		modem_change_state(modem, MODEM_STATE_ALIVE);
		set_radio_power(modem, modem->online,
				modem->online ? common_online_cb : common_offline_cb);

		return;
	}

	if (modem->pending) {
		reply = dbus_message_new_method_return(modem->pending);
		dbus_message_iter_init_append(reply, &iter);

		dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &status);

		__ofono_dbus_pending_reply(&modem->pending, reply);
	}

	modem_change_state(modem, status ? MODEM_STATE_ALIVE : MODEM_STATE_AWARE);

	if (status) {
		set_radio_power(modem, modem->online,
				modem->online ? common_online_cb : common_offline_cb);
	}
}

static void sim_state_watch(enum ofono_sim_state new_state, void *user)
{
	struct ofono_atom *atom;
	struct ofono_modem *modem;
	GSList *l;

	ofono_info("modem - %s, sim state = %d", __func__, new_state);

	modem = user;
	if (modem == NULL)
		return;

	for (l = modem->atoms; l; l = l->next) {
		atom = l->data;

		if (atom != NULL && atom->sim_state_change != NULL)
			atom->sim_state_change(new_state, atom);
	}
}

static DBusMessage *set_property_online(struct ofono_modem *modem,
					DBusMessage *msg,
					DBusMessageIter *var)
{
	ofono_bool_t online;

	if (modem->powered == FALSE
		|| modem->radio_status == RADIO_STATUS_UNAVAILABLE)
		return __ofono_error_not_available(msg);

	if (dbus_message_iter_get_arg_type(var) != DBUS_TYPE_BOOLEAN)
		return __ofono_error_invalid_args(msg);

	dbus_message_iter_get_basic(var, &online);

	if (modem->pending != NULL)
		return __ofono_error_busy(msg);

	if (modem->modem_state != MODEM_STATE_ALIVE)
		return __ofono_error_not_allowed(msg);

	if (modem->online == online)
		return dbus_message_new_method_return(msg);

	if (ofono_modem_get_emergency_mode(modem) == TRUE)
		return __ofono_error_emergency_active(msg);

	if (modem_is_always_online(modem) == TRUE) {
		if (online)
			return dbus_message_new_method_return(msg);
		else
			return __ofono_error_not_implemented(msg);
	}

	modem->pending = dbus_message_ref(msg);

	set_radio_power(modem, online, online ? online_cb : offline_cb);

	return NULL;
}

ofono_bool_t ofono_modem_get_online(struct ofono_modem *modem)
{
	if (modem == NULL)
		return FALSE;

	return modem->online;
}

void __ofono_modem_append_properties(struct ofono_modem *modem,
						DBusMessageIter *dict)
{
	char **interfaces;
	char **features;
	int i;
	GSList *l;
	struct ofono_devinfo *info;
	dbus_bool_t emergency = ofono_modem_get_emergency_mode(modem);
	const char *strtype;
	const char *system_path;

	ofono_dbus_dict_append(dict, "Online", DBUS_TYPE_BOOLEAN,
				&modem->online);

	ofono_dbus_dict_append(dict, "Powered", DBUS_TYPE_BOOLEAN,
				&modem->powered);

	ofono_dbus_dict_append(dict, "Lockdown", DBUS_TYPE_BOOLEAN,
				&modem->lockdown);

	ofono_dbus_dict_append(dict, "Emergency", DBUS_TYPE_BOOLEAN,
				&emergency);

	ofono_dbus_dict_append(dict, "ModemState", DBUS_TYPE_UINT32,
				&modem->modem_state);

	ofono_dbus_dict_append(dict, "RadioState", DBUS_TYPE_UINT32,
				&modem->radio_status);

	info = __ofono_atom_find(OFONO_ATOM_TYPE_DEVINFO, modem);
	if (info) {
		if (info->manufacturer)
			ofono_dbus_dict_append(dict, "Manufacturer",
						DBUS_TYPE_STRING,
						&info->manufacturer);

		if (info->model)
			ofono_dbus_dict_append(dict, "Model", DBUS_TYPE_STRING,
						&info->model);

		if (info->revision)
			ofono_dbus_dict_append(dict, "Revision",
						DBUS_TYPE_STRING,
						&info->revision);

		if (info->serial)
			ofono_dbus_dict_append(dict, "Serial",
						DBUS_TYPE_STRING,
						&info->serial);

		if (info->svn)
			ofono_dbus_dict_append(dict, "SoftwareVersionNumber",
						DBUS_TYPE_STRING,
						&info->svn);
	}

	system_path = ofono_modem_get_string(modem, "SystemPath");
	if (system_path)
		ofono_dbus_dict_append(dict, "SystemPath", DBUS_TYPE_STRING,
					&system_path);

	interfaces = g_new0(char *, g_slist_length(modem->interface_list) + 1);
	for (i = 0, l = modem->interface_list; l; l = l->next, i++)
		interfaces[i] = l->data;
	ofono_dbus_dict_append_array(dict, "Interfaces", DBUS_TYPE_STRING,
					&interfaces);
	g_free(interfaces);

	features = g_new0(char *, g_slist_length(modem->feature_list) + 1);
	for (i = 0, l = modem->feature_list; l; l = l->next, i++)
		features[i] = l->data;
	ofono_dbus_dict_append_array(dict, "Features", DBUS_TYPE_STRING,
					&features);
	g_free(features);

	if (modem->name)
		ofono_dbus_dict_append(dict, "Name", DBUS_TYPE_STRING,
					&modem->name);

	strtype = modem_type_to_string(modem->driver->modem_type);
	ofono_dbus_dict_append(dict, "Type", DBUS_TYPE_STRING, &strtype);
}

static DBusMessage *modem_get_properties(DBusConnection *conn,
						DBusMessage *msg, void *data)
{
	struct ofono_modem *modem = data;
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter dict;

	reply = dbus_message_new_method_return(msg);
	if (reply == NULL)
		return NULL;

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					OFONO_PROPERTIES_ARRAY_SIGNATURE,
					&dict);
	__ofono_modem_append_properties(modem, &dict);
	dbus_message_iter_close_container(&iter, &dict);

	return reply;
}

static int set_powered(struct ofono_modem *modem, ofono_bool_t powered)
{
	const struct ofono_modem_driver *driver = modem->driver;
	int err = -EINVAL;

	if (modem->powered_pending == powered)
		return -EALREADY;

	/* Remove the atoms even if the driver is no longer available */
	if (powered == FALSE)
		modem_change_state(modem, MODEM_STATE_POWER_OFF);

	modem->powered_pending = powered;

	if (driver == NULL)
		return -EINVAL;

	if (powered == TRUE) {
		if (driver->enable)
			err = driver->enable(modem);
	} else {
		if (driver->disable)
			err = driver->disable(modem);
	}

	if (err == 0) {
		modem->powered = powered;
		notify_powered_watches(modem);
	} else if (err != -EINPROGRESS)
		modem->powered_pending = modem->powered;

	return err;
}

static void lockdown_remove(struct ofono_modem *modem)
{
	DBusConnection *conn = ofono_dbus_get_connection();

	if (modem->lock_watch) {
		g_dbus_remove_watch(conn, modem->lock_watch);
		modem->lock_watch = 0;
	}

	g_free(modem->lock_owner);
	modem->lock_owner = NULL;

	modem->lockdown = FALSE;
}

static gboolean set_powered_timeout(gpointer user)
{
	struct ofono_modem *modem = user;

	DBG("modem: %p", modem);

	modem->timeout = 0;

	if (modem->powered_pending == FALSE) {
		DBusConnection *conn = ofono_dbus_get_connection();
		dbus_bool_t powered = FALSE;

		modem->powered = FALSE;
		notify_powered_watches(modem);

		ofono_dbus_signal_property_changed(conn, modem->path,
						OFONO_MODEM_INTERFACE,
						"Powered", DBUS_TYPE_BOOLEAN,
						&powered);
	} else {
		modem->powered_pending = modem->powered;
	}

	if (modem->pending != NULL) {
		DBusMessage *reply;

		reply = __ofono_error_timed_out(modem->pending);
		__ofono_dbus_pending_reply(&modem->pending, reply);

		if (modem->lockdown)
			lockdown_remove(modem);
	}

	return FALSE;
}

static void lockdown_disconnect(DBusConnection *conn, void *user_data)
{
	struct ofono_modem *modem = user_data;

	DBG("");

	ofono_dbus_signal_property_changed(conn, modem->path,
					OFONO_MODEM_INTERFACE,
					"Lockdown", DBUS_TYPE_BOOLEAN,
					&modem->lockdown);

	modem->lock_watch = 0;
	lockdown_remove(modem);
}

static DBusMessage *set_property_lockdown(struct ofono_modem *modem,
					DBusMessage *msg,
					DBusMessageIter *var)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	ofono_bool_t lockdown;
	dbus_bool_t powered;
	const char *caller;
	int err;

	if (dbus_message_iter_get_arg_type(var) != DBUS_TYPE_BOOLEAN)
		return __ofono_error_invalid_args(msg);

	dbus_message_iter_get_basic(var, &lockdown);

	if (modem->pending != NULL)
		return __ofono_error_busy(msg);

	caller = dbus_message_get_sender(msg);

	if (modem->lockdown && g_strcmp0(caller, modem->lock_owner))
		return __ofono_error_access_denied(msg);

	if (modem->lockdown == lockdown)
		return dbus_message_new_method_return(msg);

	if (lockdown == FALSE) {
		lockdown_remove(modem);
		goto done;
	}

	if (ofono_modem_get_emergency_mode(modem) == TRUE)
		return __ofono_error_emergency_active(msg);

	modem->lock_owner = g_strdup(caller);

	modem->lock_watch = g_dbus_add_disconnect_watch(conn,
				modem->lock_owner, lockdown_disconnect,
				modem, NULL);

	if (modem->lock_watch == 0) {
		g_free(modem->lock_owner);
		modem->lock_owner = NULL;

		return __ofono_error_failed(msg);
	}

	modem->lockdown = lockdown;

	if (modem->powered == FALSE)
		goto done;

	err = set_powered(modem, FALSE);
	if (err < 0) {
		if (err != -EINPROGRESS) {
			lockdown_remove(modem);
			return __ofono_error_failed(msg);
		}

		modem->pending = dbus_message_ref(msg);
		modem->timeout = g_timeout_add_seconds(modem->timeout_hint,
						set_powered_timeout, modem);
		return NULL;
	}

	powered = FALSE;
	ofono_dbus_signal_property_changed(conn, modem->path,
					OFONO_MODEM_INTERFACE,
					"Powered", DBUS_TYPE_BOOLEAN,
					&powered);

done:
	g_dbus_send_reply(conn, msg, DBUS_TYPE_INVALID);

	ofono_dbus_signal_property_changed(conn, modem->path,
					OFONO_MODEM_INTERFACE,
					"Lockdown", DBUS_TYPE_BOOLEAN,
					&lockdown);

	return NULL;
}

static DBusMessage *modem_set_property(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct ofono_modem *modem = data;
	DBusMessageIter iter, var;
	const char *name;

	if (dbus_message_iter_init(msg, &iter) == FALSE)
		return __ofono_error_invalid_args(msg);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING)
		return __ofono_error_invalid_args(msg);

	dbus_message_iter_get_basic(&iter, &name);
	dbus_message_iter_next(&iter);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT)
		return __ofono_error_invalid_args(msg);

	if (powering_down == TRUE)
		return __ofono_error_failed(msg);

	dbus_message_iter_recurse(&iter, &var);

	if (g_str_equal(name, "Online"))
		return set_property_online(modem, msg, &var);

	if (g_str_equal(name, "Powered") == TRUE) {
		ofono_bool_t powered;
		int err;

		if (dbus_message_iter_get_arg_type(&var) != DBUS_TYPE_BOOLEAN)
			return __ofono_error_invalid_args(msg);

		dbus_message_iter_get_basic(&var, &powered);

		if (modem->pending != NULL)
			return __ofono_error_busy(msg);

		if (modem->powered == powered)
			return dbus_message_new_method_return(msg);

		if (ofono_modem_get_emergency_mode(modem) == TRUE)
			return __ofono_error_emergency_active(msg);

		if (modem->lockdown)
			return __ofono_error_access_denied(msg);

		if (!powered)
			__ofono_sim_clear_cached_pins(modem->sim);

		err = set_powered(modem, powered);
		if (err < 0) {
			if (err != -EINPROGRESS)
				return __ofono_error_failed(msg);

			modem->pending = dbus_message_ref(msg);
			modem->timeout = g_timeout_add_seconds(
						modem->timeout_hint,
						set_powered_timeout, modem);
			return NULL;
		}

		g_dbus_send_reply(conn, msg, DBUS_TYPE_INVALID);

		ofono_dbus_signal_property_changed(conn, modem->path,
						OFONO_MODEM_INTERFACE,
						"Powered", DBUS_TYPE_BOOLEAN,
						&powered);

		if (powered) {
			if (modem->driver->query_modem_status != NULL) {
				/* for rilmodem, query modem status once RIL connected */
				modem->driver->query_modem_status(modem, modem_status_query_cb, modem);
			} else {
				/* for atmodem, set modem state as alive directly */
				modem_change_state(modem, MODEM_STATE_ALIVE);
			}
		} else {
			modem_change_state(modem, MODEM_STATE_POWER_OFF);
		}

		return NULL;
	}

	if (g_str_equal(name, "Lockdown"))
		return set_property_lockdown(modem, msg, &var);

	return __ofono_error_invalid_args(msg);
}

static DBusMessage *modem_get_activity_info(DBusConnection *conn, DBusMessage *msg,
					void *data)
{
	struct ofono_modem *modem = data;

	if (modem->driver->query_activity_info == NULL)
		return __ofono_error_not_implemented(msg);

	if (modem->pending)
		return __ofono_error_busy(msg);

	modem->pending = dbus_message_ref(msg);
	modem->driver->query_activity_info(modem, modem_activity_info_query_cb, modem);

	return NULL;
}

static DBusMessage *modem_enable_or_disable(struct ofono_modem *modem, ofono_bool_t enable,
					DBusConnection *conn, DBusMessage *msg)
{
	if (modem->driver->enable_modem == NULL)
		return __ofono_error_not_implemented(msg);

	if (modem->pending)
		return __ofono_error_busy(msg);

	if (modem->modem_state <= MODEM_STATE_POWER_OFF)
		return __ofono_error_not_allowed(msg);

	if (enable && modem->modem_state >= MODEM_STATE_ALIVE)
		return __ofono_error_not_allowed(msg);

	if (!enable && modem->modem_state <= MODEM_STATE_AWARE)
		return __ofono_error_not_allowed(msg);

	modem->pending = dbus_message_ref(msg);
	modem->driver->enable_modem(modem, enable,
		enable ? modem_enable_cb : modem_disable_cb, modem);

	return NULL;
}

static DBusMessage *modem_enable(DBusConnection *conn, DBusMessage *msg,
				void *data)
{
	struct ofono_modem *modem = data;

	return modem_enable_or_disable(modem, TRUE, conn, msg);
}

static DBusMessage *modem_disable(DBusConnection *conn, DBusMessage *msg,
				void *data)
{
	struct ofono_modem *modem = data;

	return modem_enable_or_disable(modem, FALSE, conn, msg);
}

static DBusMessage *modem_get_status(DBusConnection *conn, DBusMessage *msg,
					void *data)
{
	struct ofono_modem *modem = data;

	if (modem->driver->query_modem_status == NULL)
		return __ofono_error_not_implemented(msg);

	if (modem->pending)
		return __ofono_error_busy(msg);

	modem->pending = dbus_message_ref(msg);
	modem->driver->query_modem_status(modem, modem_status_query_cb, modem);

	return NULL;
}

static void modem_invoke_oem_request_raw_cb(const struct ofono_error *error,
				unsigned char *resp, int len, void *data)
{
	struct ofono_modem *modem = data;
	DBusMessageIter iter, array;
	DBusMessage *reply;
	int i;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		ofono_error("Error occurred during modem invoke oem request raw");
		reply = __ofono_error_failed(modem->pending);
		__ofono_dbus_pending_reply(&modem->pending, reply);
		return;
	}

	reply = dbus_message_new_method_return(modem->pending);

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					DBUS_TYPE_BYTE_AS_STRING, &array);

	for (i = 0; i < len; i++)
		dbus_message_iter_append_basic(&array, DBUS_TYPE_BYTE, &resp[i]);

	dbus_message_iter_close_container(&iter, &array);

	__ofono_dbus_pending_reply(&modem->pending, reply);
}

static DBusMessage *modem_invoke_oem_request_raw(DBusConnection *conn,
				DBusMessage *msg, void *data)
{
	struct ofono_modem *modem = data;
	DBusMessageIter iter, array;
	unsigned char *oem_req;
	int req_len;

	if (modem->driver->request_oem_raw == NULL)
		return __ofono_error_not_implemented(msg);

	if (modem->pending)
		return __ofono_error_busy(msg);

	if (dbus_message_iter_init(msg, &iter) == FALSE)
		return __ofono_error_invalid_args(msg);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY)
		return __ofono_error_invalid_args(msg);

	dbus_message_iter_recurse(&iter, &array);

	if (dbus_message_iter_get_arg_type(&array) != DBUS_TYPE_BYTE)
		return __ofono_error_invalid_args(msg);

	dbus_message_iter_get_fixed_array(&array, &oem_req, &req_len);

	if (req_len == 0)
		return __ofono_error_invalid_args(msg);

	modem->pending = dbus_message_ref(msg);
	modem->driver->request_oem_raw(modem, oem_req, req_len,
				modem_invoke_oem_request_raw_cb, modem);

	return NULL;
}

static void modem_invoke_oem_request_strings_cb(const struct ofono_error *error,
				char **resp, int len, void *data)
{
	struct ofono_modem *modem = data;
	DBusMessageIter iter, array;
	DBusMessage *reply;
	int i;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		ofono_error("Error occurred during modem invoke oem request strings");
		reply = __ofono_error_failed(modem->pending);
		__ofono_dbus_pending_reply(&modem->pending, reply);
		return;
	}

	reply = dbus_message_new_method_return(modem->pending);

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					DBUS_TYPE_STRING_AS_STRING, &array);

	for (i = 0; i < len; i++)
		dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &resp[i]);

	dbus_message_iter_close_container(&iter, &array);

	__ofono_dbus_pending_reply(&modem->pending, reply);
}

static DBusMessage *modem_invoke_oem_request_strings(DBusConnection *conn,
				DBusMessage *msg, void *data)
{
	struct ofono_modem *modem = data;
	DBusMessageIter iter, entry;
	char *oem_req[DEFAULT_OEM_REQ_STRING_MAX_LEN];
	int req_len;

	if (modem->driver->request_oem_strings == NULL)
		return __ofono_error_not_implemented(msg);

	if (modem->pending)
		return __ofono_error_busy(msg);

	if (dbus_message_iter_init(msg, &iter) == FALSE)
		return __ofono_error_invalid_args(msg);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY)
		return __ofono_error_invalid_args(msg);

	dbus_message_iter_recurse(&iter, &entry);

	req_len = 0;
	while (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_STRING) {
		if (req_len == DEFAULT_OEM_REQ_STRING_MAX_LEN)
			return __ofono_error_invalid_args(msg);

		dbus_message_iter_get_basic(&entry, &oem_req[req_len++]);
		dbus_message_iter_next(&entry);
	}

	modem->pending = dbus_message_ref(msg);
	modem->driver->request_oem_strings(modem, oem_req, req_len,
				 modem_invoke_oem_request_strings_cb, modem);

	return NULL;
}

static DBusMessage *modem_handle_command(DBusConnection *conn,
				DBusMessage *msg, void *data)
{
	struct ofono_modem *modem = data;
	struct ofono_atom *atom = NULL;
	DBusMessage *reply;
	int atom_id, command_id;

	reply = dbus_message_new_method_return(msg);
	if (reply == NULL)
		return NULL;

	if (dbus_message_get_args(msg, NULL,
				DBUS_TYPE_INT32, &atom_id,
				DBUS_TYPE_INT32, &command_id,
				DBUS_TYPE_INVALID) == FALSE)
		return __ofono_error_invalid_args(msg);

	atom = __ofono_modem_find_atom(modem, atom_id);
	if (atom != NULL && atom->dispatch != NULL) {
		ofono_debug("dispatch command to atom : %d with command : %d", atom_id, command_id);
		atom->dispatch(command_id, atom);
	}

	return reply;
}

static const GDBusMethodTable modem_methods[] = {
	{ GDBUS_METHOD("GetProperties",
			NULL, GDBUS_ARGS({ "properties", "a{sv}" }),
			modem_get_properties) },
	{ GDBUS_ASYNC_METHOD("SetProperty",
			GDBUS_ARGS({ "property", "s" }, { "value", "v" }),
			NULL, modem_set_property) },
	{ GDBUS_ASYNC_METHOD("GetModemActivityInfo",
			NULL, GDBUS_ARGS({ "activityinfo", "ai" }),
			modem_get_activity_info) },
	{ GDBUS_ASYNC_METHOD("EnableModem",
			NULL, NULL,
			modem_enable) },
	{ GDBUS_ASYNC_METHOD("DisableModem",
			NULL, NULL,
			modem_disable) },
	{ GDBUS_ASYNC_METHOD("GetModemStatus",
			NULL, GDBUS_ARGS({ "status", "i" }),
			modem_get_status) },
	{ GDBUS_ASYNC_METHOD("OemRequestRaw",
			GDBUS_ARGS({ "request", "ay" }),
			GDBUS_ARGS({ "response", "ay" }),
			modem_invoke_oem_request_raw) },
	{ GDBUS_ASYNC_METHOD("OemRequestStrings",
			GDBUS_ARGS({ "request", "as" }),
			GDBUS_ARGS({ "response", "as" }),
			modem_invoke_oem_request_strings) },
	{ GDBUS_METHOD("HandleCommand",
			GDBUS_ARGS({ "atom", "i" }, { "command", "i" }),
			NULL, modem_handle_command) },
	{ }
};

static const GDBusSignalTable modem_signals[] = {
	{ GDBUS_SIGNAL("PropertyChanged",
			GDBUS_ARGS({ "name", "s" }, { "value", "v" })) },
	{ GDBUS_SIGNAL("ModemRestart", NULL) },
	{ GDBUS_SIGNAL("OemHookIndication",
			GDBUS_ARGS({ "response", "ay" })) },
	{ GDBUS_SIGNAL("DeviceInfoChanged", NULL) },
	{ }
};

void ofono_modem_set_powered(struct ofono_modem *modem, ofono_bool_t powered)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	dbus_bool_t dbus_powered = powered;

	if (modem->timeout > 0) {
		g_source_remove(modem->timeout);
		modem->timeout = 0;
	}

	if (modem->powered_pending != modem->powered &&
						modem->pending != NULL) {
		DBusMessage *reply;

		if (powered == modem->powered_pending)
			reply = dbus_message_new_method_return(modem->pending);
		else
			reply = __ofono_error_failed(modem->pending);

		__ofono_dbus_pending_reply(&modem->pending, reply);
	}

	modem->powered_pending = powered;

	if (modem->powered == powered)
		goto out;

	modem->powered = powered;
	notify_powered_watches(modem);

	if (modem->lockdown)
		ofono_dbus_signal_property_changed(conn, modem->path,
					OFONO_MODEM_INTERFACE,
					"Lockdown", DBUS_TYPE_BOOLEAN,
					&modem->lockdown);

	if (modem->driver == NULL) {
		ofono_error("Calling ofono_modem_set_powered on a"
				"modem with no driver is not valid, "
				"please fix the modem driver.");
		return;
	}

	ofono_dbus_signal_property_changed(conn, modem->path,
					OFONO_MODEM_INTERFACE,
					"Powered", DBUS_TYPE_BOOLEAN,
					&dbus_powered);

	if (powered) {
		if (modem->driver->query_modem_status != NULL) {
			/* for rilmodem, query modem status once RIL connected */
			modem->driver->query_modem_status(modem, modem_status_query_cb, modem);
		} else {
			/* for atmodem, set modem state as alive directly */
			modem_change_state(modem, MODEM_STATE_ALIVE);
		}
	} else {
		modem_change_state(modem, MODEM_STATE_POWER_OFF);
	}

out:
	if (powering_down && powered == FALSE) {
		modems_remaining -= 1;

		ofono_warn("modems_remaining : %d .", modems_remaining);
		if (modems_remaining == 0)
			__ofono_exit();
	}
}

void ofono_modem_restart(struct ofono_modem *modem)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	DBusMessage *signal;

	signal = dbus_message_new_signal(modem->path, OFONO_MODEM_INTERFACE,
					"ModemRestart");

	if (signal == NULL)
		return;

	g_dbus_send_message(conn, signal);
}

ofono_bool_t ofono_modem_get_powered(struct ofono_modem *modem)
{
	if (modem == NULL)
		return FALSE;

	return modem->powered;
}

void ofono_oem_hook_raw(struct ofono_modem *modem, unsigned char *response, int len)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	DBusMessageIter iter, array;
	DBusMessage *signal;
	int i;

	signal = dbus_message_new_signal(modem->path, OFONO_MODEM_INTERFACE,
					"OemHookIndication");

	if (signal == NULL)
		return;

	dbus_message_iter_init_append(signal, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					DBUS_TYPE_BYTE_AS_STRING, &array);

	for (i = 0; i < len; i++)
		dbus_message_iter_append_basic(&array, DBUS_TYPE_BYTE, &response[i]);

	dbus_message_iter_close_container(&iter, &array);

	g_dbus_send_message(conn, signal);
}

static gboolean trigger_interface_update(void *data)
{
	struct ofono_modem *modem = data;
	DBusConnection *conn = ofono_dbus_get_connection();
	char **interfaces;
	char **features;
	GSList *l;
	int i;

	interfaces = g_new0(char *, g_slist_length(modem->interface_list) + 1);
	for (i = 0, l = modem->interface_list; l; l = l->next, i++)
		interfaces[i] = l->data;
	ofono_dbus_signal_array_property_changed(conn, modem->path,
						OFONO_MODEM_INTERFACE,
						"Interfaces", DBUS_TYPE_STRING,
						&interfaces);
	g_free(interfaces);

	features = g_new0(char *, g_slist_length(modem->feature_list) + 1);
	for (i = 0, l = modem->feature_list; l; l = l->next, i++)
		features[i] = l->data;
	ofono_dbus_signal_array_property_changed(conn, modem->path,
						OFONO_MODEM_INTERFACE,
						"Features", DBUS_TYPE_STRING,
						&features);
	g_free(features);

	modem->interface_update = 0;

	return FALSE;
}

static const struct {
	const char *interface;
	const char *feature;
} feature_map[] = {
	{ OFONO_NETWORK_REGISTRATION_INTERFACE,		"net"	},
	{ OFONO_RADIO_SETTINGS_INTERFACE,		"rat"	},
	{ OFONO_CELL_BROADCAST_INTERFACE,		"cbs"	},
	{ OFONO_MESSAGE_MANAGER_INTERFACE,		"sms"	},
	{ OFONO_SIM_MANAGER_INTERFACE,			"sim"	},
	{ OFONO_STK_INTERFACE,				"stk"	},
	{ OFONO_SUPPLEMENTARY_SERVICES_INTERFACE,	"ussd"	},
	{ OFONO_CONNECTION_MANAGER_INTERFACE,		"gprs"	},
	{ OFONO_TEXT_TELEPHONY_INTERFACE,		"tty"	},
	{ OFONO_LOCATION_REPORTING_INTERFACE,		"gps"	},
	{ },
};

static const char *get_feature(const char *interface)
{
	int i;

	for (i = 0; feature_map[i].interface; i++) {
		if (strcmp(feature_map[i].interface, interface) == 0)
			return feature_map[i].feature;
	}

	return NULL;
}

void ofono_modem_add_interface(struct ofono_modem *modem,
				const char *interface)
{
	const char *feature;

	modem->interface_list = g_slist_prepend(modem->interface_list,
						g_strdup(interface));

	feature = get_feature(interface);
	if (feature)
		modem->feature_list = g_slist_prepend(modem->feature_list,
							g_strdup(feature));

	if (modem->interface_update != 0)
		return;

	modem->interface_update = g_idle_add(trigger_interface_update, modem);
}

void ofono_modem_remove_interface(struct ofono_modem *modem,
				const char *interface)
{
	GSList *found;
	const char *feature;

	found = g_slist_find_custom(modem->interface_list, interface,
						(GCompareFunc) strcmp);
	if (found == NULL) {
		ofono_error("Interface %s not found on the interface_list",
				interface);
		return;
	}

	g_free(found->data);
	modem->interface_list = g_slist_remove(modem->interface_list,
						found->data);

	feature = get_feature(interface);
	if (feature) {
		found = g_slist_find_custom(modem->feature_list, feature,
						(GCompareFunc) strcmp);
		if (found) {
			g_free(found->data);
			modem->feature_list =
				g_slist_remove(modem->feature_list,
						found->data);
		}
	}

	if (modem->interface_update != 0)
		return;

	modem->interface_update = g_idle_add(trigger_interface_update, modem);
}

void ofono_query_device_info_done(struct ofono_devinfo *info)
{
	struct ofono_modem *modem = __ofono_atom_get_modem(info->atom);
	DBusConnection *conn = ofono_dbus_get_connection();
	DBusMessage *signal;

	signal = dbus_message_new_signal(modem->path, OFONO_MODEM_INTERFACE,
					"DeviceInfoChanged");

	if (signal == NULL)
		return;

	g_dbus_send_message(conn, signal);
}

static void query_svn_cb(const struct ofono_error *error,
				const char *svn, void *user)
{
	struct ofono_devinfo *info = user;
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(info->atom);

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR)
		goto out;

	info->svn = g_strdup(svn);

	ofono_dbus_signal_property_changed(conn, path, OFONO_MODEM_INTERFACE,
			"SoftwareVersionNumber", DBUS_TYPE_STRING, &info->svn);

out:
	ofono_query_device_info_done(info);
}

static void query_svn(struct ofono_devinfo *info)
{
	if (info->driver->query_svn == NULL)
		return;

	info->driver->query_svn(info, query_svn_cb, info);
}

static void query_serial_cb(const struct ofono_error *error,
				const char *serial, void *user)
{
	struct ofono_devinfo *info = user;
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(info->atom);

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR)
		goto out;

	info->serial = g_strdup(serial);

	ofono_dbus_signal_property_changed(conn, path,
						OFONO_MODEM_INTERFACE,
						"Serial", DBUS_TYPE_STRING,
						&info->serial);
out:
	query_svn(info);
}

static void query_serial(struct ofono_devinfo *info)
{
	if (info->driver->query_serial == NULL)
		return;

	info->driver->query_serial(info, query_serial_cb, info);
}

static void query_revision_cb(const struct ofono_error *error,
				const char *revision, void *user)
{
	struct ofono_devinfo *info = user;
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(info->atom);

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR)
		goto out;

	info->revision = g_strdup(revision);

	ofono_dbus_signal_property_changed(conn, path,
						OFONO_MODEM_INTERFACE,
						"Revision", DBUS_TYPE_STRING,
						&info->revision);

out:
	query_serial(info);
}

static void query_revision(struct ofono_devinfo *info)
{
	if (info->driver->query_revision == NULL) {
		query_serial(info);
		return;
	}

	info->driver->query_revision(info, query_revision_cb, info);
}

static void query_model_cb(const struct ofono_error *error,
				const char *model, void *user)
{
	struct ofono_devinfo *info = user;
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(info->atom);

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR)
		goto out;

	info->model = g_strdup(model);

	ofono_dbus_signal_property_changed(conn, path,
						OFONO_MODEM_INTERFACE,
						"Model", DBUS_TYPE_STRING,
						&info->model);

out:
	query_revision(info);
}

static void query_model(struct ofono_devinfo *info)
{
	if (info->driver->query_model == NULL) {
		/* If model is not supported, don't bother querying revision */
		query_serial(info);
		return;
	}

	info->driver->query_model(info, query_model_cb, info);
}

static void query_manufacturer_cb(const struct ofono_error *error,
					const char *manufacturer, void *user)
{
	struct ofono_devinfo *info = user;
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(info->atom);

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR)
		goto out;

	info->manufacturer = g_strdup(manufacturer);

	ofono_dbus_signal_property_changed(conn, path,
						OFONO_MODEM_INTERFACE,
						"Manufacturer",
						DBUS_TYPE_STRING,
						&info->manufacturer);

out:
	query_model(info);
}

static gboolean query_manufacturer(gpointer user)
{
	struct ofono_devinfo *info = user;

	if (info->driver->query_manufacturer == NULL) {
		query_model(info);
		return FALSE;
	}

	info->driver->query_manufacturer(info, query_manufacturer_cb, info);

	return FALSE;
}

static void attr_template(struct ofono_emulator *em,
				struct ofono_emulator_request *req,
				const char *attr)
{
	struct ofono_error result;

	if (attr == NULL)
		attr = "Unknown";

	result.error = 0;

	switch (ofono_emulator_request_get_type(req)) {
	case OFONO_EMULATOR_REQUEST_TYPE_COMMAND_ONLY:
		ofono_emulator_send_info(em, attr, TRUE);
		result.type = OFONO_ERROR_TYPE_NO_ERROR;
		ofono_emulator_send_final(em, &result);
		break;
	case OFONO_EMULATOR_REQUEST_TYPE_SUPPORT:
		result.type = OFONO_ERROR_TYPE_NO_ERROR;
		ofono_emulator_send_final(em, &result);
		break;
	default:
		result.type = OFONO_ERROR_TYPE_FAILURE;
		ofono_emulator_send_final(em, &result);
	};
}

static void gmi_cb(struct ofono_emulator *em,
			struct ofono_emulator_request *req, void *userdata)
{
	struct ofono_devinfo *info = userdata;

	attr_template(em, req, info->manufacturer);
}

static void gmm_cb(struct ofono_emulator *em,
			struct ofono_emulator_request *req, void *userdata)
{
	struct ofono_devinfo *info = userdata;

	attr_template(em, req, info->model);
}

static void gmr_cb(struct ofono_emulator *em,
			struct ofono_emulator_request *req, void *userdata)
{
	struct ofono_devinfo *info = userdata;

	attr_template(em, req, info->revision);
}

static void gcap_cb(struct ofono_emulator *em,
			struct ofono_emulator_request *req, void *userdata)
{
	attr_template(em, req, "+GCAP: +CGSM");
}

static void dun_watch(struct ofono_atom *atom,
			enum ofono_atom_watch_condition cond, void *data)
{
	struct ofono_emulator *em = __ofono_atom_get_data(atom);

	if (cond == OFONO_ATOM_WATCH_CONDITION_UNREGISTERED)
		return;

	ofono_emulator_add_handler(em, "+GMI", gmi_cb, data, NULL);
	ofono_emulator_add_handler(em, "+GMM", gmm_cb, data, NULL);
	ofono_emulator_add_handler(em, "+GMR", gmr_cb, data, NULL);
	ofono_emulator_add_handler(em, "+GCAP", gcap_cb, data, NULL);
}

int ofono_devinfo_driver_register(const struct ofono_devinfo_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	if (d->probe == NULL)
		return -EINVAL;

	g_devinfo_drivers = g_slist_prepend(g_devinfo_drivers, (void *) d);

	return 0;
}

void ofono_devinfo_driver_unregister(const struct ofono_devinfo_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	g_devinfo_drivers = g_slist_remove(g_devinfo_drivers, (void *) d);
}

static void devinfo_remove(struct ofono_atom *atom)
{
	struct ofono_devinfo *info = __ofono_atom_get_data(atom);
	DBG("atom: %p", atom);

	if (info == NULL)
		return;

	if (info->driver == NULL)
		return;

	if (info->driver->remove)
		info->driver->remove(info);

	g_free(info);
}

struct ofono_devinfo *ofono_devinfo_create(struct ofono_modem *modem,
							unsigned int vendor,
							const char *driver,
							void *data)
{
	struct ofono_devinfo *info;
	GSList *l;

	info = g_new0(struct ofono_devinfo, 1);

	info->atom = __ofono_modem_add_atom(modem, OFONO_ATOM_TYPE_DEVINFO,
						devinfo_remove, info);

	for (l = g_devinfo_drivers; l; l = l->next) {
		const struct ofono_devinfo_driver *drv = l->data;

		if (g_strcmp0(drv->name, driver))
			continue;

		if (drv->probe(info, vendor, data) < 0)
			continue;

		info->driver = drv;
		break;
	}

	return info;
}

static void devinfo_unregister(struct ofono_atom *atom)
{
	struct ofono_devinfo *info = __ofono_atom_get_data(atom);

	g_free(info->manufacturer);
	info->manufacturer = NULL;

	g_free(info->model);
	info->model = NULL;

	g_free(info->revision);
	info->revision = NULL;

	g_free(info->serial);
	info->serial = NULL;

	g_free(info->svn);
	info->svn = NULL;
}

void ofono_devinfo_register(struct ofono_devinfo *info)
{
	struct ofono_modem *modem = __ofono_atom_get_modem(info->atom);

	__ofono_atom_register(info->atom, devinfo_unregister);

	info->dun_watch = __ofono_modem_add_atom_watch(modem,
						OFONO_ATOM_TYPE_EMULATOR_DUN,
						dun_watch, info, NULL);

	query_manufacturer(info);
}

void ofono_devinfo_remove(struct ofono_devinfo *info)
{
	__ofono_atom_free(info->atom);
}

void ofono_devinfo_set_data(struct ofono_devinfo *info, void *data)
{
	info->driver_data = data;
}

void *ofono_devinfo_get_data(struct ofono_devinfo *info)
{
	return info->driver_data;
}

struct ofono_modem *ofono_devinfo_get_modem(struct ofono_devinfo *info)
{
	return __ofono_atom_get_modem(info->atom);
}

static void unregister_property(gpointer data)
{
	struct modem_property *property = data;

	DBG("property %p", property);

	g_free(property->value);
	g_free(property);
}

static int set_modem_property(struct ofono_modem *modem, const char *name,
				enum property_type type, const void *value)
{
	struct modem_property *property;

	DBG("modem %p property %s", modem, name);

	if (type != PROPERTY_TYPE_STRING &&
			type != PROPERTY_TYPE_INTEGER &&
			type != PROPERTY_TYPE_BOOLEAN)
		return -EINVAL;

	property = g_try_new0(struct modem_property, 1);
	if (property == NULL)
		return -ENOMEM;

	property->type = type;

	switch (type) {
	case PROPERTY_TYPE_STRING:
		property->value = g_strdup((const char *) value);
		break;
	case PROPERTY_TYPE_INTEGER:
		property->value = g_memdup2(value, sizeof(int));
		break;
	case PROPERTY_TYPE_BOOLEAN:
		property->value = g_memdup2(value, sizeof(ofono_bool_t));
		break;
	default:
		break;
	}

	g_hash_table_replace(modem->properties, g_strdup(name), property);

	return 0;
}

static gboolean get_modem_property(struct ofono_modem *modem, const char *name,
					enum property_type type,
					void *value)
{
	struct modem_property *property;

	DBG("modem %p property %s", modem, name);

	property = g_hash_table_lookup(modem->properties, name);

	if (property == NULL)
		return FALSE;

	if (property->type != type)
		return FALSE;

	switch (property->type) {
	case PROPERTY_TYPE_STRING:
		*((const char **) value) = property->value;
		return TRUE;
	case PROPERTY_TYPE_INTEGER:
		memcpy(value, property->value, sizeof(int));
		return TRUE;
	case PROPERTY_TYPE_BOOLEAN:
		memcpy(value, property->value, sizeof(ofono_bool_t));
		return TRUE;
	default:
		return FALSE;
	}
}

int ofono_modem_set_string(struct ofono_modem *modem,
				const char *key, const char *value)
{
	return set_modem_property(modem, key, PROPERTY_TYPE_STRING, value);
}

int ofono_modem_set_integer(struct ofono_modem *modem,
				const char *key, int value)
{
	return set_modem_property(modem, key, PROPERTY_TYPE_INTEGER, &value);
}

int ofono_modem_set_boolean(struct ofono_modem *modem,
				const char *key, ofono_bool_t value)
{
	return set_modem_property(modem, key, PROPERTY_TYPE_BOOLEAN, &value);
}

const char *ofono_modem_get_string(struct ofono_modem *modem, const char *key)
{
	const char *value;

	if (get_modem_property(modem, key,
				PROPERTY_TYPE_STRING, &value) == FALSE)
		return NULL;

	return value;
}

int ofono_modem_get_integer(struct ofono_modem *modem, const char *key)
{
	int value;

	if (get_modem_property(modem, key,
				PROPERTY_TYPE_INTEGER, &value) == FALSE)
		return 0;

	return value;
}

ofono_bool_t ofono_modem_get_boolean(struct ofono_modem *modem, const char *key)
{
	ofono_bool_t value;

	if (get_modem_property(modem, key,
				PROPERTY_TYPE_BOOLEAN, &value) == FALSE)
		return FALSE;

	return value;
}

void ofono_modem_set_powered_timeout_hint(struct ofono_modem *modem,
							unsigned int seconds)
{
	modem->timeout_hint = seconds;
}

void ofono_modem_set_name(struct ofono_modem *modem, const char *name)
{
	if (modem->name)
		g_free(modem->name);

	modem->name = g_strdup(name);

	if (modem->driver) {
		DBusConnection *conn = ofono_dbus_get_connection();

		ofono_dbus_signal_property_changed(conn, modem->path,
						OFONO_MODEM_INTERFACE,
						"Name", DBUS_TYPE_STRING,
						&modem->name);
	}
}

void ofono_modem_set_driver(struct ofono_modem *modem, const char *type)
{
	DBG("type: %s", type);

	if (modem->driver)
		return;

	if (strlen(type) > 16)
		return;

	g_free(modem->driver_type);
	modem->driver_type = g_strdup(type);
}

struct ofono_modem *ofono_modem_create(const char *name, const char *type)
{
	struct ofono_modem *modem;
	char path[128];

	DBG("name: %s, type: %s", name, type);

	if (strlen(type) > 16)
		return NULL;

	if (name && strlen(name) > 64)
		return NULL;

	if (name == NULL)
		snprintf(path, sizeof(path), "/%s_%d", type, next_modem_id);
	else
		snprintf(path, sizeof(path), "/%s", name);

	if (!dbus_validate_path(path, NULL))
		return NULL;

	modem = g_try_new0(struct ofono_modem, 1);

	if (modem == NULL)
		return modem;

	modem->path = g_strdup(path);
	modem->driver_type = g_strdup(type);
	modem->properties = g_hash_table_new_full(g_str_hash, g_str_equal,
						g_free, unregister_property);
	modem->timeout_hint = DEFAULT_POWERED_TIMEOUT;

	g_modem_list = g_slist_prepend(g_modem_list, modem);

	if (name == NULL)
		next_modem_id += 1;

	return modem;
}

static void sim_watch(struct ofono_atom *atom,
			enum ofono_atom_watch_condition cond, void *data)
{
	struct ofono_modem *modem = data;

	if (cond == OFONO_ATOM_WATCH_CONDITION_UNREGISTERED) {
		modem->sim_ready_watch = 0;
		return;
	}

	modem->sim = __ofono_atom_get_data(atom);
	modem->sim_ready_watch = ofono_sim_add_state_watch(modem->sim,
							sim_state_watch,
							modem, NULL);
}

void __ofono_modemwatch_init(void)
{
	g_modemwatches = __ofono_watchlist_new(g_free);
}

void __ofono_modemwatch_cleanup(void)
{
	__ofono_watchlist_free(g_modemwatches);
}

unsigned int __ofono_modemwatch_add(ofono_modemwatch_cb_t cb, void *user,
					ofono_destroy_func destroy)
{
	struct ofono_watchlist_item *watch;

	if (cb == NULL)
		return 0;

	watch = g_new0(struct ofono_watchlist_item, 1);

	watch->notify = cb;
	watch->destroy = destroy;
	watch->notify_data = user;

	return __ofono_watchlist_add_item(g_modemwatches, watch);
}

gboolean __ofono_modemwatch_remove(unsigned int id)
{
	return __ofono_watchlist_remove_item(g_modemwatches, id);
}

static void call_modemwatches(struct ofono_modem *modem, gboolean added)
{
	GSList *l;
	struct ofono_watchlist_item *watch;
	ofono_modemwatch_cb_t notify;

	DBG("%p added:%d", modem, added);

	for (l = g_modemwatches->items; l; l = l->next) {
		watch = l->data;

		notify = watch->notify;
		notify(modem, added, watch->notify_data);
	}
}

static void emit_modem_added(struct ofono_modem *modem)
{
	DBusMessage *signal;
	DBusMessageIter iter;
	DBusMessageIter dict;
	const char *path;

	DBG("%p", modem);

	signal = dbus_message_new_signal(OFONO_MANAGER_PATH,
						OFONO_MANAGER_INTERFACE,
						"ModemAdded");

	if (signal == NULL)
		return;

	dbus_message_iter_init_append(signal, &iter);

	path = modem->path;
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH, &path);
	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					OFONO_PROPERTIES_ARRAY_SIGNATURE,
					&dict);
	__ofono_modem_append_properties(modem, &dict);
	dbus_message_iter_close_container(&iter, &dict);

	g_dbus_send_message(ofono_dbus_get_connection(), signal);
}

ofono_bool_t ofono_modem_is_registered(struct ofono_modem *modem)
{
	if (modem == NULL)
		return FALSE;

	if (modem->driver == NULL)
		return FALSE;

	return TRUE;
}

int ofono_modem_register(struct ofono_modem *modem)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	GSList *l;

	DBG("%p", modem);

	if (modem == NULL)
		return -EINVAL;

	if (powering_down == TRUE)
		return -EBUSY;

	if (modem->driver != NULL)
		return -EALREADY;

	for (l = g_driver_list; l; l = l->next) {
		const struct ofono_modem_driver *drv = l->data;

		if (g_strcmp0(drv->name, modem->driver_type))
			continue;

		if (drv->probe(modem) < 0)
			continue;

		modem->driver = drv;
		break;
	}

	if (modem->driver == NULL)
		return -ENODEV;

	if (!g_dbus_register_interface(conn, modem->path,
					OFONO_MODEM_INTERFACE,
					modem_methods, modem_signals, NULL,
					modem, NULL)) {
		ofono_error("Modem register failed on path %s", modem->path);

		if (modem->driver->remove)
			modem->driver->remove(modem);

		modem->driver = NULL;

		return -EIO;
	}

	g_free(modem->driver_type);
	modem->driver_type = NULL;

	modem->atom_watches = __ofono_watchlist_new(g_free);
	modem->online_watches = __ofono_watchlist_new(g_free);
	modem->powered_watches = __ofono_watchlist_new(g_free);

	emit_modem_added(modem);
	call_modemwatches(modem, TRUE);

	modem->sim_watch = __ofono_modem_add_atom_watch(modem,
					OFONO_ATOM_TYPE_SIM,
					sim_watch, modem, NULL);

	modem_load_settings(modem);

	return 0;
}

static void emit_modem_removed(struct ofono_modem *modem)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = modem->path;

	DBG("%p", modem);

	g_dbus_emit_signal(conn, OFONO_MANAGER_PATH, OFONO_MANAGER_INTERFACE,
				"ModemRemoved", DBUS_TYPE_OBJECT_PATH, &path,
				DBUS_TYPE_INVALID);
}

static void modem_unregister(struct ofono_modem *modem)
{
	DBusConnection *conn = ofono_dbus_get_connection();

	DBG("%p", modem);

	if (modem->powered == TRUE)
		set_powered(modem, FALSE);

	__ofono_watchlist_free(modem->atom_watches);
	modem->atom_watches = NULL;

	__ofono_watchlist_free(modem->online_watches);
	modem->online_watches = NULL;

	__ofono_watchlist_free(modem->powered_watches);
	modem->powered_watches = NULL;

	modem->sim_watch = 0;
	modem->sim_ready_watch = 0;

	g_slist_free_full(modem->interface_list, g_free);
	modem->interface_list = NULL;

	g_slist_free_full(modem->feature_list, g_free);
	modem->feature_list = NULL;

	if (modem->timeout) {
		g_source_remove(modem->timeout);
		modem->timeout = 0;
	}

	if (modem->pending) {
		dbus_message_unref(modem->pending);
		modem->pending = NULL;
	}

	if (modem->interface_update) {
		g_source_remove(modem->interface_update);
		modem->interface_update = 0;
	}

	if (modem->lock_watch) {
		lockdown_remove(modem);

		ofono_dbus_signal_property_changed(conn, modem->path,
					OFONO_MODEM_INTERFACE,
					"Lockdown", DBUS_TYPE_BOOLEAN,
					&modem->lockdown);
	}

	modem_close_settings(modem);

	g_dbus_unregister_interface(conn, modem->path, OFONO_MODEM_INTERFACE);

	if (modem->driver && modem->driver->remove)
		modem->driver->remove(modem);

	g_hash_table_destroy(modem->properties);
	modem->properties = NULL;

	modem->driver = NULL;

	emit_modem_removed(modem);
	call_modemwatches(modem, FALSE);
}

void ofono_modem_remove(struct ofono_modem *modem)
{
	DBG("%p", modem);

	if (modem == NULL)
		return;

	if (modem->driver)
		modem_unregister(modem);

	g_modem_list = g_slist_remove(g_modem_list, modem);

	g_free(modem->driver_type);
	g_free(modem->name);
	g_free(modem->path);
	g_free(modem);
}

void ofono_modem_reset(struct ofono_modem *modem)
{
	int err;

	DBG("%p", modem);

	if (modem->pending) {
		DBusMessage *reply = __ofono_error_failed(modem->pending);
		__ofono_dbus_pending_reply(&modem->pending, reply);
	}

	ofono_modem_set_powered(modem, FALSE);

	err = set_powered(modem, TRUE);
	ofono_debug("%s , err : %d", __func__, err);
}

void __ofono_modem_sim_reset(struct ofono_modem *modem)
{
	DBG("%p", modem);
}

int ofono_modem_driver_register(const struct ofono_modem_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	if (d->probe == NULL)
		return -EINVAL;

	g_driver_list = g_slist_prepend(g_driver_list, (void *) d);

	return 0;
}

void ofono_modem_driver_unregister(const struct ofono_modem_driver *d)
{
	GSList *l;
	struct ofono_modem *modem;

	DBG("driver: %p, name: %s", d, d->name);

	g_driver_list = g_slist_remove(g_driver_list, (void *) d);

	for (l = g_modem_list; l; l = l->next) {
		modem = l->data;

		if (modem->driver != d)
			continue;

		modem_unregister(modem);
	}
}

void __ofono_modem_shutdown(void)
{
	struct ofono_modem *modem;
	GSList *l;

	powering_down = TRUE;

	for (l = g_modem_list; l; l = l->next) {
		modem = l->data;

		if (modem->driver == NULL)
			continue;

		if (modem->powered == FALSE && modem->powered_pending == FALSE)
			continue;

		if (set_powered(modem, FALSE) == -EINPROGRESS)
			modems_remaining += 1;
	}

	if (modems_remaining == 0)
		__ofono_exit();
}

void __ofono_modem_foreach(ofono_modem_foreach_func func, void *userdata)
{
	struct ofono_modem *modem;
	GSList *l;

	for (l = g_modem_list; l; l = l->next) {
		modem = l->data;
		func(modem, userdata);
	}
}

struct ofono_modem *ofono_modem_find(ofono_modem_compare_cb_t func,
					void *user_data)
{
	struct ofono_modem *modem;
	GSList *l;

	for (l = g_modem_list; l; l = l->next) {
		modem = l->data;

		if (func(modem, user_data) == TRUE)
			return modem;
	}

	return NULL;
}

ofono_bool_t ofono_modem_get_emergency_mode(struct ofono_modem *modem)
{
	return modem->emergency != 0;
}

void __ofono_modem_inc_emergency_mode(struct ofono_modem *modem)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	dbus_bool_t emergency = TRUE;
	enum radio_status old_radio_status = modem->radio_status;

	if (++modem->emergency > 1)
		return;

	ofono_dbus_signal_property_changed(conn, modem->path,
						OFONO_MODEM_INTERFACE,
						"Emergency", DBUS_TYPE_BOOLEAN,
						&emergency);

	modem->radio_status = RADIO_STATUS_EMERGENCY_ONLY;
	radio_status_change(modem, old_radio_status, RADIO_STATUS_EMERGENCY_ONLY);
}

void __ofono_modem_dec_emergency_mode(struct ofono_modem *modem)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	dbus_bool_t emergency = FALSE;
	enum radio_status old_radio_status = modem->radio_status;

	if (modem->emergency == 0) {
		ofono_error("emergency mode is already deactivated!!!");
		return;
	}

	if (modem->emergency > 1)
		goto out;

	ofono_dbus_signal_property_changed(conn, modem->path,
						OFONO_MODEM_INTERFACE,
						"Emergency", DBUS_TYPE_BOOLEAN,
						&emergency);

	modem->radio_status = RADIO_STATUS_EMERGENCY_ONLY;
	radio_status_change(modem, old_radio_status, RADIO_STATUS_EMERGENCY_ONLY);

out:
	modem->emergency--;
}

void ofono_modem_process_radio_state(struct ofono_modem *modem, int radio_state)
{
	enum radio_status old_radio_state = modem->radio_status;

	switch (radio_state) {
	case 0:
		modem->radio_status = RADIO_STATUS_OFF;

		/* if radio state doesn't match user setting, sync it again */
		if (modem->online)
			set_radio_power(modem, TRUE, common_online_cb);
		break;
	case 1:
		modem->radio_status = RADIO_STATUS_UNAVAILABLE;
		break;
	case 10:
		modem->radio_status = RADIO_STATUS_ON;

		/* if radio state doesn't match user setting, sync it again */
		if (!modem->online)
			set_radio_power(modem, FALSE, common_offline_cb);
		break;
	}

	radio_status_change(modem, old_radio_state, modem->radio_status);
}
