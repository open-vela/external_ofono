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
#include <stdlib.h>
#include <errno.h>

#include <glib.h>
#include <gdbus.h>

#include "ofono.h"

#include "common.h"
#include "util.h"

#define CALL_SETTINGS_FLAG_CACHED 0x1

static GSList *g_drivers = NULL;

/* 27.007 Section 7.7 */
enum clir_status {
	CLIR_STATUS_NOT_PROVISIONED =		0,
	CLIR_STATUS_PROVISIONED_PERMANENT =	1,
	CLIR_STATUS_UNKNOWN =			2,
	CLIR_STATUS_TEMPORARY_RESTRICTED =	3,
	CLIR_STATUS_TEMPORARY_ALLOWED =		4
};

/* 27.007 Section 7.6 */
enum clip_status {
	CLIP_STATUS_NOT_PROVISIONED =		0,
	CLIP_STATUS_PROVISIONED =		1,
	CLIP_STATUS_UNKNOWN =			2
};

/* 27.007 Section 7.30 */
enum cnap_status {
	CNAP_STATUS_NOT_PROVISIONED =		0,
	CNAP_STATUS_PROVISIONED =		1,
	CNAP_STATUS_UNKNOWN =			2
};

/* 27.007 Section 7.8 */
enum colp_status {
	COLP_STATUS_NOT_PROVISIONED =		0,
	COLP_STATUS_PROVISIONED =		1,
	COLP_STATUS_UNKNOWN =			2
};

/* 27.007 Section 7.9 */
enum cdip_status {
	CDIP_STATUS_NOT_PROVISIONED =		0,
	CDIP_STATUS_PROVISIONED =		1,
	CDIP_STATUS_UNKNOWN =			2
};

/* This is not defined in 27.007, but presumably the same as CLIP/COLP */
enum colr_status {
	COLR_STATUS_NOT_PROVISIONED =		0,
	COLR_STATUS_PROVISIONED =		1,
	COLR_STATUS_UNKNOWN =			2
};

enum call_setting_type {
	CALL_SETTING_TYPE_CLIP = 0,
	CALL_SETTING_TYPE_CNAP,
	CALL_SETTING_TYPE_CDIP,
	CALL_SETTING_TYPE_COLP,
	CALL_SETTING_TYPE_COLR,
	CALL_SETTING_TYPE_CLIR,
	CALL_SETTING_TYPE_CW
};

struct ofono_call_settings {
	int clir;
	int colr;
	int clip;
	int cnap;
	int cdip;
	int colp;
	int clir_setting;
	int cw;
	int flags;
	DBusMessage *pending;
	int ss_req_type;
	int ss_req_cls;
	enum call_setting_type ss_setting;
	struct ofono_ussd *ussd;
	unsigned int ussd_watch;
	const struct ofono_call_settings_driver *driver;
	void *driver_data;
	struct ofono_atom *atom;
	GQueue *cs_queue;
	struct ofono_netreg *netreg;
	unsigned int netreg_watch;
};

static const char *cs_support_pending_list[] = { "SetCallWaiting",
						 "GetCallWaiting", NULL };

static DBusMessage *cs_pop_message_from_queue(DBusConnection *connection,
					      DBusMessage *msg, void *data);
static DBusMessage *cs_push_message_to_queue(DBusConnection *connection,
					     DBusMessage *msg, void *data);

static const char *clip_status_to_string(int status)
{
	switch (status) {
	case CLIP_STATUS_NOT_PROVISIONED:
		return "disabled";
	case CLIP_STATUS_PROVISIONED:
		return "enabled";
	}

	return "unknown";
}

static const char *cdip_status_to_string(int status)
{
	switch (status) {
	case CDIP_STATUS_NOT_PROVISIONED:
		return "disabled";
	case CDIP_STATUS_PROVISIONED:
		return "enabled";
	}

	return "unknown";
}

static const char *cnap_status_to_string(int status)
{
	switch (status) {
	case CNAP_STATUS_NOT_PROVISIONED:
		return "disabled";
	case CNAP_STATUS_PROVISIONED:
		return "enabled";
	}

	return "unknown";
}

static const char *colp_status_to_string(int status)
{
	switch (status) {
	case COLP_STATUS_NOT_PROVISIONED:
		return "disabled";
	case COLP_STATUS_PROVISIONED:
		return "enabled";
	}

	return "unknown";
}

static const char *colr_status_to_string(int status)
{
	switch (status) {
	case COLR_STATUS_NOT_PROVISIONED:
		return "disabled";
	case COLR_STATUS_PROVISIONED:
		return "enabled";
	}

	return "unknown";
}

static const char *hide_callerid_to_string(int status)
{
	switch (status) {
	case OFONO_CLIR_OPTION_DEFAULT:
		return "default";
	case OFONO_CLIR_OPTION_INVOCATION:
		return "enabled";
	case OFONO_CLIR_OPTION_SUPPRESSION:
		return "disabled";
	}

	return "default";
}

static const char *clir_status_to_string(int status)
{
	switch (status) {
	case CLIR_STATUS_NOT_PROVISIONED:
		return "disabled";
	case CLIR_STATUS_PROVISIONED_PERMANENT:
		return "permanent";
	case CLIR_STATUS_TEMPORARY_RESTRICTED:
		return "on";
	case CLIR_STATUS_TEMPORARY_ALLOWED:
		return "off";
	}

	return "unknown";
}

static void set_clir_network(struct ofono_call_settings *cs, int clir)
{
	DBusConnection *conn;
	const char *path;
	const char *str;

	if (cs->clir == clir)
		return;

	cs->clir = clir;

	conn = ofono_dbus_get_connection();
	path = __ofono_atom_get_path(cs->atom);

	str = clir_status_to_string(clir);

	ofono_dbus_signal_property_changed(conn, path,
						OFONO_CALL_SETTINGS_INTERFACE,
						"CallingLineRestriction",
						DBUS_TYPE_STRING, &str);
}

static void set_clir_override(struct ofono_call_settings *cs, int override)
{
	DBusConnection *conn;
	const char *path;
	const char *str;

	if (cs->clir_setting == override)
		return;

	cs->clir_setting = override;

	conn = ofono_dbus_get_connection();
	path = __ofono_atom_get_path(cs->atom);

	str = hide_callerid_to_string(override);

	ofono_dbus_signal_property_changed(conn, path,
						OFONO_CALL_SETTINGS_INTERFACE,
						"HideCallerId",
						DBUS_TYPE_STRING, &str);
}

static void set_cdip(struct ofono_call_settings *cs, int cdip)
{
	DBusConnection *conn;
	const char *path;
	const char *str;

	if (cs->cdip == cdip)
		return;

	cs->cdip = cdip;

	conn = ofono_dbus_get_connection();
	path = __ofono_atom_get_path(cs->atom);

	str = cdip_status_to_string(cdip);

	ofono_dbus_signal_property_changed(conn, path,
						OFONO_CALL_SETTINGS_INTERFACE,
						"CalledLinePresentation",
						DBUS_TYPE_STRING, &str);
}

static void set_clip(struct ofono_call_settings *cs, int clip)
{
	DBusConnection *conn;
	const char *path;
	const char *str;

	if (cs->clip == clip)
		return;

	cs->clip = clip;

	conn = ofono_dbus_get_connection();
	path = __ofono_atom_get_path(cs->atom);

	str = clip_status_to_string(clip);

	ofono_dbus_signal_property_changed(conn, path,
						OFONO_CALL_SETTINGS_INTERFACE,
						"CallingLinePresentation",
						DBUS_TYPE_STRING, &str);
}

static void set_cnap(struct ofono_call_settings *cs, int cnap)
{
	DBusConnection *conn;
	const char *path;
	const char *str;

	if (cs->cnap == cnap)
		return;

	cs->cnap = cnap;

	conn = ofono_dbus_get_connection();
	path = __ofono_atom_get_path(cs->atom);

	str = cnap_status_to_string(cnap);

	ofono_dbus_signal_property_changed(conn, path,
						OFONO_CALL_SETTINGS_INTERFACE,
						"CallingNamePresentation",
						DBUS_TYPE_STRING, &str);
}

static void set_colp(struct ofono_call_settings *cs, int colp)
{
	DBusConnection *conn;
	const char *path;
	const char *str;

	if (cs->colp == colp)
		return;

	cs->colp = colp;

	conn = ofono_dbus_get_connection();
	path = __ofono_atom_get_path(cs->atom);

	str = colp_status_to_string(colp);

	ofono_dbus_signal_property_changed(conn, path,
						OFONO_CALL_SETTINGS_INTERFACE,
						"ConnectedLinePresentation",
						DBUS_TYPE_STRING, &str);
}

static void set_colr(struct ofono_call_settings *cs, int colr)
{
	DBusConnection *conn;
	const char *path;
	const char *str;

	if (cs->colr == colr)
		return;

	cs->colr = colr;

	conn = ofono_dbus_get_connection();
	path = __ofono_atom_get_path(cs->atom);

	str = colr_status_to_string(colr);

	ofono_dbus_signal_property_changed(conn, path,
						OFONO_CALL_SETTINGS_INTERFACE,
						"ConnectedLineRestriction",
						DBUS_TYPE_STRING, &str);
}

static void set_cw(struct ofono_call_settings *cs, int new_cw, int mask)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(cs->atom);
	char buf[64];
	int j;
	const char *value;

	for (j = 1; j <= BEARER_CLASS_PAD; j = j << 1) {
		if ((j & mask) == 0)
			continue;

		if ((cs->cw & j) == (new_cw & j))
			continue;

		if (new_cw & j)
			value = "enabled";
		else
			value = "disabled";

		snprintf(buf, sizeof(buf), "%sCallWaiting",
				bearer_class_to_string(j));
		ofono_dbus_signal_property_changed(conn, path,
						OFONO_CALL_SETTINGS_INTERFACE,
						buf, DBUS_TYPE_STRING,
						&value);
	}

	cs->cw = new_cw;
}

static void property_append_cw_conditions(DBusMessageIter *dict,
						int conditions, int mask)
{
	int i;
	char prop[128];
	const char *value;

	for (i = 1; i <= BEARER_CLASS_PAD; i = i << 1) {
		if (!(mask & i))
			continue;

		snprintf(prop, sizeof(prop), "%sCallWaiting",
				bearer_class_to_string(i));

		if (conditions & i)
			value = "enabled";
		else
			value = "disabled";

		ofono_dbus_dict_append(dict, prop, DBUS_TYPE_STRING, &value);
	}
}

static void generate_cw_ss_query_reply(struct ofono_call_settings *cs)
{
	const char *sig = "(sa{sv})";
	const char *ss_type = ss_control_type_to_string(cs->ss_req_type);
	const char *context = "CallWaiting";
	DBusMessageIter iter;
	DBusMessageIter var;
	DBusMessageIter vstruct;
	DBusMessageIter dict;
	DBusMessage *reply;

	reply = dbus_message_new_method_return(cs->pending);

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &context);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, sig, &var);

	dbus_message_iter_open_container(&var, DBUS_TYPE_STRUCT, NULL,
						&vstruct);

	dbus_message_iter_append_basic(&vstruct, DBUS_TYPE_STRING,
					&ss_type);

	dbus_message_iter_open_container(&vstruct, DBUS_TYPE_ARRAY,
					OFONO_PROPERTIES_ARRAY_SIGNATURE,
					&dict);

	property_append_cw_conditions(&dict, cs->cw, cs->ss_req_cls);

	dbus_message_iter_close_container(&vstruct, &dict);

	dbus_message_iter_close_container(&var, &vstruct);

	dbus_message_iter_close_container(&iter, &var);

	__ofono_dbus_pending_reply(&cs->pending, reply);
}

static void cw_ss_query_callback(const struct ofono_error *error, int status,
					void *data)
{
	struct ofono_call_settings *cs = data;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		ofono_error("setting CW via SS failed");

		cs->flags &= ~CALL_SETTINGS_FLAG_CACHED;
		__ofono_dbus_pending_reply(&cs->pending,
					__ofono_error_failed(cs->pending));

		return;
	}

	set_cw(cs, status, BEARER_CLASS_VOICE);

	generate_cw_ss_query_reply(cs);
}

static void cw_ss_set_callback(const struct ofono_error *error, void *data)
{
	struct ofono_call_settings *cs = data;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		ofono_error("setting CW via SS failed with error: %s",
			telephony_error_to_str(error));
		__ofono_dbus_pending_reply(&cs->pending,
			__ofono_error_from_error(error, cs->pending));

		return;
	}

	cs->driver->cw_query(cs, BEARER_CLASS_DEFAULT,
				cw_ss_query_callback, cs);
}

static gboolean cw_ss_control(int type,
				const char *sc, const char *sia,
				const char *sib, const char *sic,
				const char *dn, DBusMessage *msg, void *data)
{
	struct ofono_call_settings *cs = data;
	DBusConnection *conn = ofono_dbus_get_connection();
	int cls = BEARER_CLASS_SS_DEFAULT;
	DBusMessage *reply;

	if (cs == NULL) {
		ofono_error("%s: Call setting instance is NULL. Initialization failed.",
			__func__);
		return FALSE;
	}

	if (strcmp(sc, "43")) {
		ofono_error("%s: Invalid service category: %s", __func__, sc);
		return FALSE;
	}

	if (__ofono_call_settings_is_busy(cs)) {
		ofono_error("%s: Call setting is currently busy.", __func__);
		reply = __ofono_error_busy(msg);
		goto error;
	}

	if (strlen(sib) || strlen(dn)) {
		ofono_error("%s, sib: %s, dn: %s", __func__, sib, dn);
		goto bad_format;
	}

	if ((type == SS_CONTROL_TYPE_QUERY && cs->driver->cw_query == NULL)) {
		ofono_error("%s: Call setting driver's 'cw_query' function is not implemented.",
			__func__);
		reply = __ofono_error_not_implemented(msg);
		goto error;
	}

	if ((type != SS_CONTROL_TYPE_QUERY && cs->driver->cw_set == NULL)) {
		ofono_error("%s: Call setting driver's 'cw_set' function is not implemented.",
			__func__);
		reply = __ofono_error_not_implemented(msg);
		goto error;
	}

	if (strlen(sia) > 0) {
		long service_code;
		char *end;

		service_code = strtoul(sia, &end, 10);

		if (end == sia) {
			ofono_error("%s: Invalid sia format - no digits found.", __func__);
			goto bad_format;
		}

		if (*end != '\0') {
			ofono_error("%s: Invalid sia format - extra non-digit characters found.",
				__func__);
			goto bad_format;
		}

		cls = mmi_service_code_to_bearer_class(service_code);
		if (cls == 0) {
			ofono_error("%s: Invalid service code - no corresponding bearer class.",
				__func__);
			goto bad_format;
		}
	}

	cs->ss_req_cls = cls;
	cs->pending = dbus_message_ref(msg);

	/* For the default case use the more readily accepted value */
	if (cls == BEARER_CLASS_SS_DEFAULT)
		cls = BEARER_CLASS_DEFAULT;

	switch (type) {
	case SS_CONTROL_TYPE_REGISTRATION:
	case SS_CONTROL_TYPE_ACTIVATION:
		cs->ss_req_type = SS_CONTROL_TYPE_ACTIVATION;
		cs->driver->cw_set(cs, 1, cls, cw_ss_set_callback, cs);
		break;

	case SS_CONTROL_TYPE_QUERY:
		cs->ss_req_type = SS_CONTROL_TYPE_QUERY;
		/*
		 * Always query the entire set, SMS not applicable
		 * according to 22.004 Appendix A, so CLASS_DEFAULT
		 * is safe to use here
		 */
		cs->driver->cw_query(cs, BEARER_CLASS_DEFAULT,
					cw_ss_query_callback, cs);
		break;

	case SS_CONTROL_TYPE_DEACTIVATION:
	case SS_CONTROL_TYPE_ERASURE:
		cs->ss_req_type = SS_CONTROL_TYPE_DEACTIVATION;
		cs->driver->cw_set(cs, 0, cls, cw_ss_set_callback, cs);
		break;
	}

	return TRUE;

bad_format:
	reply = __ofono_error_invalid_format(msg);

error:
	g_dbus_send_message(conn, reply);
	return TRUE;
}

static void generate_ss_query_reply(struct ofono_call_settings *cs,
					const char *context, const char *value)
{
	const char *sig = "(ss)";
	const char *ss_type = ss_control_type_to_string(cs->ss_req_type);
	DBusMessageIter iter;
	DBusMessageIter var;
	DBusMessageIter vstruct;
	DBusMessage *reply;

	reply = dbus_message_new_method_return(cs->pending);

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &context);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, sig, &var);

	dbus_message_iter_open_container(&var, DBUS_TYPE_STRUCT, NULL,
						&vstruct);

	dbus_message_iter_append_basic(&vstruct, DBUS_TYPE_STRING,
					&ss_type);

	dbus_message_iter_append_basic(&vstruct, DBUS_TYPE_STRING, &value);

	dbus_message_iter_close_container(&var, &vstruct);

	dbus_message_iter_close_container(&iter, &var);

	__ofono_dbus_pending_reply(&cs->pending, reply);
}

static void clip_cnap_colp_colr_ss_query_cb(const struct ofono_error *error,
					int status, void *data)
{
	struct ofono_call_settings *cs = data;
	const char *context;
	const char *value;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		ofono_error("SS control query failed with error: %s",
			telephony_error_to_str(error));
		__ofono_dbus_pending_reply(&cs->pending,
			__ofono_error_from_error(error, cs->pending));

		return;
	}

	switch (cs->ss_setting) {
	case CALL_SETTING_TYPE_CLIP:
		set_clip(cs, status);
		value = clip_status_to_string(status);
		context = "CallingLinePresentation";
		break;

	case CALL_SETTING_TYPE_CNAP:
		set_cnap(cs, status);
		value = cnap_status_to_string(status);
		context = "CallingNamePresentation";
		break;


	case CALL_SETTING_TYPE_COLP:
		set_colp(cs, status);
		value = colp_status_to_string(status);
		context = "ConnectedLinePresentation";
		break;

	case CALL_SETTING_TYPE_COLR:
		set_colr(cs, status);
		value = colr_status_to_string(status);
		context = "ConnectedLineRestriction";
		break;

	default:
		__ofono_dbus_pending_reply(&cs->pending,
				__ofono_error_failed(cs->pending));
		ofono_error("Unknown type during COLR/COLP/CLIP/CNAP ss");
		return;
	};

	generate_ss_query_reply(cs, context, value);
}

static gboolean clip_cnap_colp_colr_ss(int type,
				const char *sc, const char *sia,
				const char *sib, const char *sic,
				const char *dn, DBusMessage *msg, void *data)
{
	struct ofono_call_settings *cs = data;
	DBusConnection *conn = ofono_dbus_get_connection();
	void (*query_op)(struct ofono_call_settings *cs,
				ofono_call_settings_status_cb_t cb, void *data);

	if (cs == NULL) {
		ofono_error("%s: Call setting instance is NULL. Initialization failed.",
			__func__);
		return FALSE;
	}

	if (__ofono_call_settings_is_busy(cs)) {
		ofono_error("%s: Call setting is currently busy.", __func__);
		DBusMessage *reply = __ofono_error_busy(msg);
		g_dbus_send_message(conn, reply);

		return TRUE;
	}

	if (!strcmp(sc, "30")) {
		cs->ss_setting = CALL_SETTING_TYPE_CLIP;
		query_op = cs->driver->clip_query;
	} else if (!strcmp(sc, "300")) {
		cs->ss_setting = CALL_SETTING_TYPE_CNAP;
		query_op = cs->driver->cnap_query;
	} else if (!strcmp(sc, "76")) {
		cs->ss_setting = CALL_SETTING_TYPE_COLP;
		query_op = cs->driver->colp_query;
	} else if (!strcmp(sc, "77")) {
		cs->ss_setting = CALL_SETTING_TYPE_COLR;
		query_op = cs->driver->colr_query;
	} else {
		return FALSE;
	}

	if (type != SS_CONTROL_TYPE_QUERY || strlen(sia) || strlen(sib) ||
		strlen(sic) || strlen(dn)) {
		ofono_error("%s, invalid format, type: %d, sia: %s, sib: %s, sic: %s, dn: %s",
			__func__, type, sia, sib, sic, dn);
		DBusMessage *reply = __ofono_error_invalid_format(msg);
		g_dbus_send_message(conn, reply);

		return TRUE;
	}

	if (query_op == NULL) {
		ofono_error("%s: Call setting driver is not implemented.", __func__);
		DBusMessage *reply = __ofono_error_not_implemented(msg);
		g_dbus_send_message(conn, reply);

		return TRUE;
	}

	ofono_debug("Received CLIP/CNAP/COLR/COLP query ss control");

	cs->pending = dbus_message_ref(msg);

	query_op(cs, clip_cnap_colp_colr_ss_query_cb, cs);

	return TRUE;
}

static void clir_ss_query_callback(const struct ofono_error *error,
					int override, int network, void *data)
{
	struct ofono_call_settings *cs = data;
	const char *value;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		ofono_error("clir query via SS failed with error: %s",
					telephony_error_to_str(error));
		__ofono_dbus_pending_reply(&cs->pending,
				__ofono_error_from_error(error, cs->pending));

		return;
	}

	switch (network) {
	case CLIR_STATUS_UNKNOWN:
		value = "unknown";
		break;

	case CLIR_STATUS_PROVISIONED_PERMANENT:
		value = "enabled";
		break;

	case CLIR_STATUS_NOT_PROVISIONED:
		value = "disabled";
		break;

	case CLIR_STATUS_TEMPORARY_RESTRICTED:
		if (override == OFONO_CLIR_OPTION_SUPPRESSION)
			value = "enabled";
		else
			value = "disabled";
		break;

	case CLIR_STATUS_TEMPORARY_ALLOWED:
		if (override == OFONO_CLIR_OPTION_INVOCATION)
			value = "enabled";
		else
			value = "disabled";
		break;
	default:
		value = "unknown";
	};

	generate_ss_query_reply(cs, "CallingLineRestriction", value);

	set_clir_network(cs, network);
	set_clir_override(cs, override);
}

static void clir_ss_set_callback(const struct ofono_error *error, void *data)
{
	struct ofono_call_settings *cs = data;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		ofono_error("setting clir via SS failed with error: %s",
			telephony_error_to_str(error));
		__ofono_dbus_pending_reply(&cs->pending,
			__ofono_error_from_error(error, cs->pending));

		return;
	}

	cs->driver->clir_query(cs, clir_ss_query_callback, cs);
}

static gboolean clir_ss_control(int type,
				const char *sc, const char *sia,
				const char *sib, const char *sic,
				const char *dn, DBusMessage *msg, void *data)
{
	struct ofono_call_settings *cs = data;
	DBusConnection *conn = ofono_dbus_get_connection();

	if (cs == NULL) {
		ofono_error("%s: Call setting instance is NULL. Initialization failed.",
			__func__);
		return FALSE;
	}

	if (strcmp(sc, "31")) {
		ofono_error("%s, Invalid clir service code: %s, must be 31.",
			__func__, sc);
		return FALSE;
	}

	if (__ofono_call_settings_is_busy(cs)) {
		ofono_error("%s: Call setting is currently busy.", __func__);
		DBusMessage *reply = __ofono_error_busy(msg);
		g_dbus_send_message(conn, reply);

		return TRUE;
	}

	/* This is the temporary form of CLIR, handled in voicecalls */
	if (!strlen(sia) && !strlen(sib) & !strlen(sic) &&
			strlen(dn) && type != SS_CONTROL_TYPE_QUERY)
		return FALSE;

	if (strlen(sia) || strlen(sib) || strlen(sic) || strlen(dn)) {
		ofono_error("%s, invalid format, sia: %s, sib: %s, sic: %s, dn: %s",
			__func__, sia, sib, sic, dn);
		DBusMessage *reply = __ofono_error_invalid_format(msg);
		g_dbus_send_message(conn, reply);

		return TRUE;
	}

	if (type == SS_CONTROL_TYPE_QUERY && cs->driver->clir_query == NULL) {
		ofono_error("%s: Call setting driver's 'cw_query' function is not implemented.",
			__func__);
		DBusMessage *reply = __ofono_error_not_implemented(msg);
		g_dbus_send_message(conn, reply);

		return TRUE;
	}

	if (type != SS_CONTROL_TYPE_QUERY && cs->driver->clir_set == NULL) {
		ofono_error("%s: Call setting driver's 'clir_set' function is not implemented.",
			__func__);
		DBusMessage *reply = __ofono_error_not_implemented(msg);
		g_dbus_send_message(conn, reply);

		return TRUE;
	}

	cs->ss_setting = CALL_SETTING_TYPE_CLIR;
	cs->pending = dbus_message_ref(msg);

	switch (type) {
	case SS_CONTROL_TYPE_REGISTRATION:
	case SS_CONTROL_TYPE_ACTIVATION:
		cs->ss_req_type = SS_CONTROL_TYPE_ACTIVATION;
		cs->driver->clir_set(cs, OFONO_CLIR_OPTION_SUPPRESSION,
					clir_ss_set_callback, cs);
		break;

	case SS_CONTROL_TYPE_QUERY:
		cs->ss_req_type = SS_CONTROL_TYPE_QUERY;
		cs->driver->clir_query(cs, clir_ss_query_callback, cs);
		break;

	case SS_CONTROL_TYPE_DEACTIVATION:
	case SS_CONTROL_TYPE_ERASURE:
		cs->ss_req_type = SS_CONTROL_TYPE_DEACTIVATION;
		cs->driver->clir_set(cs, OFONO_CLIR_OPTION_INVOCATION,
					clir_ss_set_callback, cs);
		break;
	};

	return TRUE;
}

static void cs_register_ss_controls(struct ofono_call_settings *cs)
{
	__ofono_ussd_ssc_register(cs->ussd, "30", clip_cnap_colp_colr_ss,
								cs, NULL);
	__ofono_ussd_ssc_register(cs->ussd, "31", clir_ss_control, cs, NULL);
	__ofono_ussd_ssc_register(cs->ussd, "76", clip_cnap_colp_colr_ss,
								cs, NULL);
	__ofono_ussd_ssc_register(cs->ussd, "300", clip_cnap_colp_colr_ss,
								cs, NULL);

	__ofono_ussd_ssc_register(cs->ussd, "43", cw_ss_control, cs, NULL);

	if (cs->driver->colr_query != NULL)
		__ofono_ussd_ssc_register(cs->ussd, "77",
					clip_cnap_colp_colr_ss, cs, NULL);
}

static void cs_unregister_ss_controls(struct ofono_call_settings *cs)
{
	__ofono_ussd_ssc_unregister(cs->ussd, "30");
	__ofono_ussd_ssc_unregister(cs->ussd, "31");
	__ofono_ussd_ssc_unregister(cs->ussd, "76");
	__ofono_ussd_ssc_unregister(cs->ussd, "300");

	__ofono_ussd_ssc_unregister(cs->ussd, "43");

	if (cs->driver->colr_query != NULL)
		__ofono_ussd_ssc_unregister(cs->ussd, "77");
}

gboolean __ofono_call_settings_is_busy(struct ofono_call_settings *cs)
{
	return cs->pending ? TRUE : FALSE;
}

static DBusMessage *generate_get_properties_reply(struct ofono_call_settings *cs,
							DBusMessage *msg)
{
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter dict;
	const char *str;

	reply = dbus_message_new_method_return(msg);
	if (reply == NULL) {
		ofono_error("%s: Failed to allocate D-Bus reply message for call forwarding", 
			__func__);
		return __ofono_error_no_memory(msg);
	}

	dbus_message_iter_init_append(reply, &iter);

	dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					OFONO_PROPERTIES_ARRAY_SIGNATURE,
					&dict);

	str = clip_status_to_string(cs->clip);
	ofono_dbus_dict_append(&dict, "CallingLinePresentation",
				DBUS_TYPE_STRING, &str);

	str = cnap_status_to_string(cs->cnap);
	ofono_dbus_dict_append(&dict, "CallingNamePresentation",
				DBUS_TYPE_STRING, &str);

	str = colp_status_to_string(cs->colp);
	ofono_dbus_dict_append(&dict, "ConnectedLinePresentation",
				DBUS_TYPE_STRING, &str);

	str = colr_status_to_string(cs->colr);
	ofono_dbus_dict_append(&dict, "ConnectedLineRestriction",
				DBUS_TYPE_STRING, &str);

	str = cdip_status_to_string(cs->cdip);
	ofono_dbus_dict_append(&dict, "CalledLinePresentation",
				DBUS_TYPE_STRING, &str);

	str = clir_status_to_string(cs->clir);
	ofono_dbus_dict_append(&dict, "CallingLineRestriction",
				DBUS_TYPE_STRING, &str);

	str = hide_callerid_to_string(cs->clir_setting);
	ofono_dbus_dict_append(&dict, "HideCallerId", DBUS_TYPE_STRING, &str);

	property_append_cw_conditions(&dict, cs->cw, BEARER_CLASS_VOICE);

	dbus_message_iter_close_container(&iter, &dict);

	return reply;
}

static void cs_clir_callback(const struct ofono_error *error,
				int override_setting, int network_setting,
				void *data)
{
	struct ofono_call_settings *cs = data;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		ofono_error("query clir failed in %s", __func__);
		goto out;
	}

	set_clir_network(cs, network_setting);
	set_clir_override(cs, override_setting);

	cs->flags |= CALL_SETTINGS_FLAG_CACHED;

out:
	if (cs->pending) {
		DBusMessage *reply = generate_get_properties_reply(cs,
								cs->pending);
		__ofono_dbus_pending_reply(&cs->pending, reply);
	}
}

static void query_clir(struct ofono_call_settings *cs)
{
	if (cs->driver->clir_query == NULL) {
		if (cs->pending) {
			ofono_info("%s: Call setting is currently busy.", __func__);
			DBusMessage *reply =
				generate_get_properties_reply(cs,
								cs->pending);
			__ofono_dbus_pending_reply(&cs->pending, reply);
		}

		return;
	}

	cs->driver->clir_query(cs, cs_clir_callback, cs);
}

static void cs_cdip_callback(const struct ofono_error *error,
				int state, void *data)
{
	struct ofono_call_settings *cs = data;

	if (error->type == OFONO_ERROR_TYPE_NO_ERROR)
		set_cdip(cs, state);

	query_clir(cs);
}

static void query_cdip(struct ofono_call_settings *cs)
{
	if (cs->driver->cdip_query == NULL) {
		ofono_info("%s: Call setting driver's 'cdip_query' function is not implemented.",
			__func__);
		query_clir(cs);
		return;
	}

	cs->driver->cdip_query(cs, cs_cdip_callback, cs);
}


static void cs_cnap_callback(const struct ofono_error *error,
				int state, void *data)
{
	struct ofono_call_settings *cs = data;

	if (error->type == OFONO_ERROR_TYPE_NO_ERROR)
		set_cnap(cs, state);

	query_cdip(cs);
}

static void query_cnap(struct ofono_call_settings *cs)
{
	if (cs->driver->cnap_query == NULL) {
		query_cdip(cs);
		return;
	}

	cs->driver->cnap_query(cs, cs_cnap_callback, cs);
}

static void cs_clip_callback(const struct ofono_error *error,
				int state, void *data)
{
	struct ofono_call_settings *cs = data;

	if (error->type == OFONO_ERROR_TYPE_NO_ERROR)
		set_clip(cs, state);

	query_cnap(cs);
}

static void query_clip(struct ofono_call_settings *cs)
{
	if (cs->driver->clip_query == NULL) {
		ofono_info("%s: Call setting driver's 'clip_query' function is not implemented.",
			__func__);
		query_clir(cs);
		return;
	}

	cs->driver->clip_query(cs, cs_clip_callback, cs);
}

static void cs_colp_callback(const struct ofono_error *error,
				int state, void *data)
{
	struct ofono_call_settings *cs = data;

	if (error->type == OFONO_ERROR_TYPE_NO_ERROR)
		set_colp(cs, state);

	query_clip(cs);
}

static void query_colp(struct ofono_call_settings *cs)
{
	if (cs->driver->colp_query == NULL) {
		ofono_info("%s: Call setting driver's 'colp_query' function is not implemented.",
			__func__);
		query_clip(cs);
		return;
	}

	cs->driver->colp_query(cs, cs_colp_callback, cs);
}

static void cs_colr_callback(const struct ofono_error *error,
				int state, void *data)
{
	struct ofono_call_settings *cs = data;

	if (error->type == OFONO_ERROR_TYPE_NO_ERROR)
		set_colr(cs, state);

	query_colp(cs);
}

static void query_colr(struct ofono_call_settings *cs)
{
	if (cs->driver->colr_query == NULL) {
		ofono_info("%s: Call setting driver's 'colr_query' function is not implemented.",
			__func__);
		query_colp(cs);
		return;
	}

	cs->driver->colr_query(cs, cs_colr_callback, cs);
}

static void cs_cw_callback(const struct ofono_error *error, int status,
				void *data)
{
	struct ofono_call_settings *cs = data;

	if (error->type == OFONO_ERROR_TYPE_NO_ERROR)
		set_cw(cs, status, BEARER_CLASS_VOICE);

	query_colr(cs);
}

static void query_cw(struct ofono_call_settings *cs)
{
	if (cs->driver->cw_query == NULL) {
		ofono_info("%s: Call setting driver's 'cw_query' function is not implemented.",
			__func__);
		query_colr(cs);
		return;
	}

	cs->driver->cw_query(cs, BEARER_CLASS_DEFAULT, cs_cw_callback, cs);
}

static DBusMessage *cs_get_properties(DBusConnection *conn, DBusMessage *msg,
					void *data)
{
	struct ofono_call_settings *cs = data;

	if (__ofono_call_settings_is_busy(cs)) {
		ofono_error("%s: Call setting is currently busy.", __func__);
		return __ofono_error_busy(msg);
	}

	if (__ofono_ussd_is_busy(cs->ussd)) {
		ofono_error("%s: USSD service is currently busy.", __func__);
		return __ofono_error_busy(msg);
	}

	if (cs->flags & CALL_SETTINGS_FLAG_CACHED)
		return generate_get_properties_reply(cs, msg);

	/* Query the settings and report back */
	cs->pending = dbus_message_ref(msg);

	query_cw(cs);

	return NULL;
}

static void clir_set_query_callback(const struct ofono_error *error,
					int override_setting,
					int network_setting, void *data)
{
	struct ofono_call_settings *cs = data;
	DBusMessage *reply;

	if (!__ofono_call_settings_is_busy(cs))
		return;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		ofono_error("set clir successful, but the query was not");

		cs->flags &= ~CALL_SETTINGS_FLAG_CACHED;

		reply = __ofono_error_failed(cs->pending);
		__ofono_dbus_pending_reply(&cs->pending, reply);
		return;
	}

	reply = dbus_message_new_method_return(cs->pending);
	__ofono_dbus_pending_reply(&cs->pending, reply);

	set_clir_override(cs, override_setting);
	set_clir_network(cs, network_setting);
}

static void clir_set_callback(const struct ofono_error *error, void *data)
{
	struct ofono_call_settings *cs = data;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		ofono_error("setting clir failed");
		__ofono_dbus_pending_reply(&cs->pending,
					__ofono_error_failed(cs->pending));

		return;
	}

	/* Assume that if we have clir_set, we have clir_query */
	cs->driver->clir_query(cs, clir_set_query_callback, cs);
}

static DBusMessage *set_clir(DBusMessage *msg, struct ofono_call_settings *cs,
				const char *setting)
{
	int clir = -1;

	if (cs->driver->clir_set == NULL) {
		ofono_error("%s: Call setting driver's 'clir_set' function is not implemented.",
			__func__);
		return __ofono_error_not_implemented(msg);
	}

	if (!strcmp(setting, "default"))
		clir = CLIR_STATUS_NOT_PROVISIONED;
	else if (!strcmp(setting, "enabled"))
		clir = CLIR_STATUS_PROVISIONED_PERMANENT;
	else if (!strcmp(setting, "disabled"))
		clir = CLIR_STATUS_UNKNOWN;

	if (clir == -1) {
		ofono_error("%s: Invalid CLIR setting '%s'", __func__, setting);
		return __ofono_error_invalid_format(msg);
	}

	cs->pending = dbus_message_ref(msg);

	cs->driver->clir_set(cs, clir, clir_set_callback, cs);

	return NULL;
}

static void cw_set_query_callback(const struct ofono_error *error, int status,
				void *data)
{
	struct ofono_call_settings *cs = data;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		ofono_error("CW set succeeded, but query failed!");

		cs->flags &= ~CALL_SETTINGS_FLAG_CACHED;

		__ofono_dbus_pending_reply(&cs->pending,
					__ofono_error_failed(cs->pending));
		return;
	}

	__ofono_dbus_pending_reply(&cs->pending,
				dbus_message_new_method_return(cs->pending));

	set_cw(cs, status, BEARER_CLASS_VOICE);
}

static void cw_set_callback(const struct ofono_error *error, void *data)
{
	struct ofono_call_settings *cs = data;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		ofono_error("Error occurred during CW set");

		__ofono_dbus_pending_reply(&cs->pending,
					__ofono_error_failed(cs->pending));

		return;
	}

	cs->driver->cw_query(cs, BEARER_CLASS_DEFAULT,
				cw_set_query_callback, cs);
}

static DBusMessage *set_cw_req(DBusMessage *msg, struct ofono_call_settings *cs,
				const char *setting, int cls)
{
	int cw;

	if (cs->driver->cw_set == NULL) {
		ofono_error("%s: Call setting driver's 'cw_set' function is not implemented.",
			__func__);
		return __ofono_error_not_implemented(msg);
	}

	if (!strcmp(setting, "enabled"))
		cw = 1;
	else if (!strcmp(setting, "disabled"))
		cw = 0;
	else {
		ofono_error("%s: Invalid cw setting '%s', must be 'enabled' or 'disabled'.",
			__func__, setting);
		return __ofono_error_invalid_format(msg);
	}

	cs->pending = dbus_message_ref(msg);

	cs->driver->cw_set(cs, cw, cls, cw_set_callback, cs);

	return NULL;
}

static gboolean is_cw_property(const char *property, int mask, int *out_cls)
{
	int i;
	int len;
	const char *prefix;

	for (i = 1; i <= BEARER_CLASS_PAD; i = i << 1) {
		if ((i & mask) == 0)
			continue;

		prefix = bearer_class_to_string(i);

		len = strlen(prefix);

		if (strncmp(property, prefix, len))
			continue;

		if (!strcmp(property+len, "CallWaiting")) {
			*out_cls = i;
			return TRUE;
		}
	}

	return FALSE;
}

static DBusMessage *cs_set_property(DBusConnection *conn, DBusMessage *msg,
					void *data)
{
	struct ofono_call_settings *cs = data;
	DBusMessageIter iter;
	DBusMessageIter var;
	const char *property;
	int cls;

	if (__ofono_call_settings_is_busy(cs)) {
		ofono_error("%s: Call setting is currently busy.", __func__);
		return __ofono_error_busy(msg);
	}

	if (__ofono_ussd_is_busy(cs->ussd)) {
		ofono_error("%s: USSD service is currently busy.", __func__);
		return __ofono_error_busy(msg);
	}

	if (!dbus_message_iter_init(msg, &iter)) {
		ofono_error("%s: Invalid D-Bus message - no arguments provided.", __func__);
		return __ofono_error_invalid_args(msg);
	}

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_STRING) {
		ofono_error("%s: Invalid argument type. Expected DBUS_TYPE_STRING ('s'), "
			"but received type '%c'.", __func__, dbus_message_iter_get_arg_type(&iter));
		return __ofono_error_invalid_args(msg);
	}

	dbus_message_iter_get_basic(&iter, &property);
	dbus_message_iter_next(&iter);

	if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT) {
		ofono_error("%s: Invalid argument type. Expected DBUS_TYPE_VARIANT ('v'), "
			"but received type '%c'.", __func__, dbus_message_iter_get_arg_type(&iter));
		return __ofono_error_invalid_args(msg);
	}

	dbus_message_iter_recurse(&iter, &var);

	if (!strcmp(property, "HideCallerId")) {
		const char *setting;

		if (dbus_message_iter_get_arg_type(&var) != DBUS_TYPE_STRING) {
			ofono_error("%s: [HideCallerId] Invalid argument type. Expected DBUS_TYPE_STRING ('s'), "
				"but received type '%c'.", __func__, dbus_message_iter_get_arg_type(&var));
			return __ofono_error_invalid_args(msg);
		}

		dbus_message_iter_get_basic(&var, &setting);

		return set_clir(msg, cs, setting);
	} else if (is_cw_property(property, BEARER_CLASS_VOICE, &cls)) {
		const char *setting;

		if (dbus_message_iter_get_arg_type(&var) != DBUS_TYPE_STRING) {
			ofono_error("%s: [cw setting] Invalid argument type. Expected DBUS_TYPE_STRING ('s'), "
				"but received type '%c'.", __func__, dbus_message_iter_get_arg_type(&var));
			return __ofono_error_invalid_args(msg);
		}

		dbus_message_iter_get_basic(&var, &setting);

		return set_cw_req(msg, cs, setting, cls);
	}

	ofono_error("property %s in %s is invalid", property, __func__);
	return __ofono_error_invalid_args(msg);
}

static void get_covered_plmn(struct ofono_call_settings *cs, char *covered_plmn)
{
	const char *mcc;
	const char *mnc;

	if (cs->netreg == NULL) {
		strncpy(covered_plmn, "unknow", OFONO_MAX_MCC_LENGTH + OFONO_MAX_MNC_LENGTH + 1);
		return;
	}
	mcc = ofono_netreg_get_mcc(cs->netreg);
	mnc = ofono_netreg_get_mnc(cs->netreg);
	get_covered_plmn_from_util(covered_plmn, mcc, mnc);
}

static void set_call_waiting_cb(const struct ofono_error *error, void *data)
{
	struct ofono_call_settings *cs = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		char covered_plmn[OFONO_MAX_MCC_LENGTH + OFONO_MAX_MNC_LENGTH + 1] = { '\0' };

		get_covered_plmn(cs, covered_plmn);
		ofono_error("Error occurred during setting call waiting !");

		__ofono_dbus_pending_reply(&cs->pending, __ofono_error_failed(cs->pending));
		OFONO_DFX_SS_INFO("ss:set call waiting", "modem fail", covered_plmn);
		return;
	}

	reply = dbus_message_new_method_return(cs->pending);

	__ofono_dbus_pending_reply(&cs->pending, reply);
}

static void get_call_waiting_cb(const struct ofono_error *error,
				int status, void *data)
{
	struct ofono_call_settings *cs = data;
	DBusMessageIter iter;
	DBusMessage *reply;
	int i, value = 0;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		char covered_plmn[OFONO_MAX_MCC_LENGTH + OFONO_MAX_MNC_LENGTH + 1] = { '\0' };

		get_covered_plmn(cs, covered_plmn);

		ofono_error("Error occurs during get call waiting status !");

		if (cs->pending) {
			reply = __ofono_error_failed(cs->pending);
			__ofono_dbus_pending_reply(&cs->pending, reply);
		}
		OFONO_DFX_SS_INFO("ss:get call waiting", "modem fail", covered_plmn);

		return;
	}

	if (cs->pending) {
		for (i = 1; i <= BEARER_CLASS_PAD; i = i << 1) {
			if (!(BEARER_CLASS_VOICE & i))
				continue;

			value = status & i ? 1 : 0;
		}

		reply = dbus_message_new_method_return(cs->pending);

		dbus_message_iter_init_append(reply, &iter);
		dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &value);

		__ofono_dbus_pending_reply(&cs->pending, reply);
	}
}

static DBusMessage *cs_set_call_waiting(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct ofono_call_settings *cs = data;
	int enable;

	if (cs->driver == NULL) {
		ofono_error("%s: Call setting driver is not available.", __func__);
		return __ofono_error_not_implemented(msg);
	}

	if (cs->driver->cw_set == NULL) {
		ofono_error("%s: Call setting driver's 'cw_set' function is not implemented.",
			__func__);
		return __ofono_error_not_implemented(msg);
	}

	if (cs->pending) {
		char covered_plmn[OFONO_MAX_MCC_LENGTH + OFONO_MAX_MNC_LENGTH + 1] = { '\0' };

		get_covered_plmn(cs, covered_plmn);

		ofono_error("%s: Call setting is currently busy.", __func__);
		OFONO_DFX_SS_INFO("ss:set call waiting", "busy", covered_plmn);
		return __ofono_error_busy(msg);
	}

	if (dbus_message_get_args(msg, NULL,
				DBUS_TYPE_INT32, &enable,
				DBUS_TYPE_INVALID) == FALSE) {
		ofono_error("%s: Failed to retrieve int32 argument from D-Bus message.",
			__func__);
		return __ofono_error_invalid_args(msg);
	}

	cs->pending = dbus_message_ref(msg);

	cs->driver->cw_set(cs, enable, BEARER_CLASS_VOICE, set_call_waiting_cb, cs);

	return NULL;

}

static DBusMessage *cs_get_call_waiting(DBusConnection *conn,
					DBusMessage *msg, void *data)
{
	struct ofono_call_settings *cs = data;

	if (cs->driver == NULL) {
		ofono_error("%s: Call setting driver is not available.", __func__);
		return __ofono_error_not_implemented(msg);
	}

	if (cs->driver->cw_query == NULL) {
		ofono_error("%s: Call setting driver's 'cw_query' function is not implemented.",
			__func__);
		return __ofono_error_not_implemented(msg);
	}

	if (cs->pending) {
		char covered_plmn[OFONO_MAX_MCC_LENGTH + OFONO_MAX_MNC_LENGTH + 1] = { '\0' };

		get_covered_plmn(cs, covered_plmn);

		ofono_error("%s: Call setting is currently busy.", __func__);
		OFONO_DFX_SS_INFO("ss:get call waiting", "busy", covered_plmn);
		return __ofono_error_busy(msg);
	}

	cs->pending = dbus_message_ref(msg);

	cs->driver->cw_query(cs, BEARER_CLASS_DEFAULT, get_call_waiting_cb, cs);

	return NULL;
}

static void set_clir_cb(const struct ofono_error *error, void *data)
{
	struct ofono_call_settings *cs = data;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		char covered_plmn[OFONO_MAX_MCC_LENGTH + OFONO_MAX_MNC_LENGTH + 1] = { '\0' };

		get_covered_plmn(cs, covered_plmn);

		ofono_error("Error occurred during setting clir !");

		__ofono_dbus_pending_reply(&cs->pending, __ofono_error_failed(cs->pending));
		OFONO_DFX_SS_INFO("ss:set clir", "modem fail", covered_plmn);
		return;
	}

	reply = dbus_message_new_method_return(cs->pending);

	__ofono_dbus_pending_reply(&cs->pending, reply);
}

static void get_clir_cb(const struct ofono_error *error,
			int override, int network, void *data)
{
	struct ofono_call_settings *cs = data;
	DBusMessageIter iter;
	DBusMessage *reply;

	if (error->type != OFONO_ERROR_TYPE_NO_ERROR) {
		char covered_plmn[OFONO_MAX_MCC_LENGTH + OFONO_MAX_MNC_LENGTH + 1] = { '\0' };

		get_covered_plmn(cs, covered_plmn);

		ofono_error("Error occurred during setting clir !");

		__ofono_dbus_pending_reply(&cs->pending, __ofono_error_failed(cs->pending));
		OFONO_DFX_SS_INFO("ss:get clir", "modem fail", covered_plmn);
		return;
	}

	reply = dbus_message_new_method_return(cs->pending);

	dbus_message_iter_init_append(reply, &iter);
	dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &override);

	__ofono_dbus_pending_reply(&cs->pending, reply);
}

static DBusMessage *cs_set_clir(DBusConnection *conn,
				DBusMessage *msg, void *data)
{
	struct ofono_call_settings *cs = data;
	int clir = -1;

	if (cs->driver == NULL) {
		ofono_error("%s: Call setting driver is not available.", __func__);
		return __ofono_error_not_implemented(msg);
	}

	if (cs->driver->clir_set == NULL) {
		ofono_error("%s: Call setting driver's 'clir_set' function is not implemented.",
			__func__);
		return __ofono_error_not_implemented(msg);
	}

	if (cs->pending) {
		char covered_plmn[OFONO_MAX_MCC_LENGTH + OFONO_MAX_MNC_LENGTH + 1] = { '\0' };

		get_covered_plmn(cs, covered_plmn);

		ofono_error("%s: Call setting is currently busy.", __func__);
		OFONO_DFX_SS_INFO("ss:set clir", "busy", covered_plmn);
		return __ofono_error_busy(msg);
	}

	if (dbus_message_get_args(msg, NULL,
				DBUS_TYPE_INT32, &clir,
				DBUS_TYPE_INVALID) == FALSE) {
		ofono_error("%s: Failed to retrieve string argument from D-Bus message.",
			__func__);
		return __ofono_error_invalid_args(msg);
	}

	if (clir < 0 || clir > 2) {
		ofono_error("%s: Invalid CLIR value '%d'.", __func__, clir);
		return __ofono_error_invalid_format(msg);
	}

	cs->pending = dbus_message_ref(msg);

	cs->driver->clir_set(cs, clir, set_clir_cb, cs);

	return NULL;
}

static DBusMessage *cs_get_clir(DBusConnection *conn,
				DBusMessage *msg, void *data)
{
	struct ofono_call_settings *cs = data;

	if (cs->driver == NULL) {
		ofono_error("%s: Call setting driver is not available.", __func__);
		return __ofono_error_not_implemented(msg);
	}

	if (cs->driver->clir_query == NULL) {
		ofono_error("%s: Call setting driver's 'clir_query' function is not implemented.",
			__func__);
		return __ofono_error_not_implemented(msg);
	}

	if (cs->pending) {
		char covered_plmn[OFONO_MAX_MCC_LENGTH + OFONO_MAX_MNC_LENGTH + 1] = { '\0' };

		get_covered_plmn(cs, covered_plmn);

		ofono_error("%s: Call setting is currently busy.", __func__);
		OFONO_DFX_SS_INFO("ss:get clir", "busy", covered_plmn);
		return __ofono_error_busy(msg);
	}

	cs->pending = dbus_message_ref(msg);

	cs->driver->clir_query(cs, get_clir_cb, cs);

	return NULL;
}

static const GDBusMethodTable cs_methods[] = {
	{ GDBUS_ASYNC_METHOD("GetProperties",
				NULL, GDBUS_ARGS({ "properties", "a{sv}" }),
				cs_get_properties) },
	{ GDBUS_ASYNC_METHOD("SetProperty",
			GDBUS_ARGS({ "property", "s" }, { "value", "v" }),
			NULL, cs_set_property) },
	{ GDBUS_ASYNC_METHOD("SetCallWaiting",
			GDBUS_ARGS({ "enable", "i" }), NULL,
			cs_set_call_waiting) },
	{ GDBUS_ASYNC_METHOD("GetCallWaiting", NULL,
			GDBUS_ARGS({ "status", "i" }),
			cs_get_call_waiting) },
	{ GDBUS_ASYNC_METHOD("SetClir",
			GDBUS_ARGS({ "status", "i" }), NULL,
			cs_set_clir) },
	{ GDBUS_ASYNC_METHOD("GetClir", NULL,
			GDBUS_ARGS({ "status", "i" }),
			cs_get_clir) },
	{ GDBUS_ASYNC_METHOD("PopMessage", NULL,
			NULL, cs_pop_message_from_queue) },
	{ GDBUS_ASYNC_METHOD("PushMessage", NULL,
			NULL, cs_push_message_to_queue) },
	{ }
};

void cs_free_pending_data(void *data)
{
	DBusMessage *msg = data;
	DBusConnection *conn = ofono_dbus_get_connection();

	g_dbus_send_message(conn, __ofono_error_not_available(msg));
	dbus_message_unref(msg);
}

static DBusMessage *cs_pop_message_from_queue(DBusConnection *connection,
					      DBusMessage *msg, void *data)
{
	struct ofono_call_settings *cs = data;
	DBusMessage *reply = NULL;
	gboolean unexpect_msg_flag = TRUE;
	const char *member_name;
	const GDBusMethodTable *method;
	DBusMessage *pending_msg = NULL;

	if (g_queue_get_length(cs->cs_queue) == 0) {
		return NULL;
	}

	pending_msg = g_queue_pop_head(cs->cs_queue);
	member_name = dbus_message_get_member(pending_msg);
	ofono_debug("%s,member_name:%s", __func__, member_name);

	if (!strcmp(member_name, "SetCallWaiting") ||
	    !strcmp(member_name, "GetCallWaiting")) {
		if (cs->pending) {
			ofono_error("%s fail as pending", __func__);
			return NULL;
		}
	}
	for (method = cs_methods; method && method->name && method->function;
	     method++) {
		if (!strcmp(method->name, member_name)) {
			reply = method->function(connection, pending_msg, cs);
			unexpect_msg_flag = FALSE;
			break;
		}
	}
	if (unexpect_msg_flag) {
		ofono_error("%s,unexpected pending message", __func__);
		reply = __ofono_error_not_supported(pending_msg);
	}
	dbus_message_unref(pending_msg);
	if (reply != NULL) {
		g_dbus_send_message(connection, reply);
		cs_pop_message_from_queue(connection, pending_msg, cs);
	}
	return NULL;
}

static DBusMessage *cs_push_message_to_queue(DBusConnection *connection,
					     DBusMessage *msg, void *data)
{
	struct ofono_call_settings *cs = data;
	gboolean push_flag = FALSE;
	int i = 0;

	while (cs_support_pending_list[i]) {
		if (dbus_message_is_method_call(msg,
						OFONO_CALL_SETTINGS_INTERFACE,
						cs_support_pending_list[i])) {
			ofono_debug("%s,%s", __func__,
				    cs_support_pending_list[i]);
			if (!strcmp(cs_support_pending_list[i],
				    "GetCallWaiting") ||
			    !strcmp(cs_support_pending_list[i],
				    "SetCallWaiting")) {
				if (cs->pending) {
					push_flag = TRUE;
				}
			} // used to extension
			break;
		}
		i++;
	}
	if (push_flag) {
		g_queue_push_tail(cs->cs_queue, dbus_message_ref(msg));
		ofono_debug("%s,add queue done", __func__);
		return msg;
	}
	return NULL;
}

static const GDBusSignalTable cs_signals[] = {
	{ GDBUS_SIGNAL("PropertyChanged",
			GDBUS_ARGS({ "property", "s" }, { "value", "v" })) },
	{ }
};

int ofono_call_settings_driver_register(const struct ofono_call_settings_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	if (!is_ofono_interface_supported(CALL_SETTINGS_INTERFACE)) {
		ofono_debug("%s : not support for call setting! \n", __func__);
		return 0;
	}

	if (d->probe == NULL)
		return -EINVAL;

	g_drivers = g_slist_prepend(g_drivers, (void *) d);

	return 0;
}

void ofono_call_settings_driver_unregister(const struct ofono_call_settings_driver *d)
{
	DBG("driver: %p, name: %s", d, d->name);

	g_drivers = g_slist_remove(g_drivers, (void *) d);
}

static void netreg_watch(struct ofono_atom *atom, enum ofono_atom_watch_condition cond, void *data)
{
	struct ofono_call_settings *cs = data;

	if (cond == OFONO_ATOM_WATCH_CONDITION_UNREGISTERED) {
		cs->netreg = NULL;
		return;
	}

	cs->netreg = __ofono_atom_get_data(atom);
}

static void call_settings_unregister(struct ofono_atom *atom)
{
	struct ofono_call_settings *cs = __ofono_atom_get_data(atom);
	const char *path = __ofono_atom_get_path(cs->atom);
	DBusConnection *conn = ofono_dbus_get_connection();
	struct ofono_modem *modem = __ofono_atom_get_modem(cs->atom);

	ofono_modem_remove_interface(modem, OFONO_CALL_SETTINGS_INTERFACE);
	g_dbus_unregister_interface(conn, path, OFONO_CALL_SETTINGS_INTERFACE);

	if (cs->ussd)
		cs_unregister_ss_controls(cs);

	if (cs->ussd_watch)
		__ofono_modem_remove_atom_watch(modem, cs->ussd_watch);

	g_queue_free_full(cs->cs_queue, cs_free_pending_data);
	cs->cs_queue = NULL;

	if (cs->netreg_watch) {
		__ofono_modem_remove_atom_watch(modem, cs->netreg_watch);
		cs->netreg_watch = 0;
	}
}

static void call_settings_remove(struct ofono_atom *atom)
{
	struct ofono_call_settings *cs = __ofono_atom_get_data(atom);

	DBG("atom: %p", atom);

	if (cs == NULL)
		return;

	if (cs->pending != NULL) {
		DBusMessage *reply = __ofono_error_failed(cs->pending);
		__ofono_dbus_pending_reply(&cs->pending, reply);
	}

	if (cs->driver != NULL && cs->driver->remove != NULL)
		cs->driver->remove(cs);

	g_free(cs);
}

struct ofono_call_settings *ofono_call_settings_create(struct ofono_modem *modem,
							unsigned int vendor,
							const char *driver,
							void *data)
{
	struct ofono_call_settings *cs;
	GSList *l;

	if (!is_ofono_interface_supported(CALL_SETTINGS_INTERFACE)) {
		ofono_debug("%s : not support for call setting! \n", __func__);
		return NULL;
	}

	if (driver == NULL)
		return NULL;

	cs = g_try_new0(struct ofono_call_settings, 1);

	if (cs == NULL)
		return NULL;

	/* Set all the settings to unknown state */
	cs->clip = CLIP_STATUS_UNKNOWN;
	cs->cnap = CNAP_STATUS_UNKNOWN;
	cs->clir = CLIR_STATUS_UNKNOWN;
	cs->colp = COLP_STATUS_UNKNOWN;
	cs->colr = COLR_STATUS_UNKNOWN;
	cs->atom = __ofono_modem_add_atom(modem, OFONO_ATOM_TYPE_CALL_SETTINGS,
						call_settings_remove, cs);

	for (l = g_drivers; l; l = l->next) {
		const struct ofono_call_settings_driver *drv = l->data;

		if (g_strcmp0(drv->name, driver))
			continue;

		if (drv->probe(cs, vendor, data) < 0)
			continue;

		cs->driver = drv;
		break;
	}

	return cs;
}

static void ussd_watch(struct ofono_atom *atom,
			enum ofono_atom_watch_condition cond, void *data)
{
	struct ofono_call_settings *cs = data;

	if (cond == OFONO_ATOM_WATCH_CONDITION_UNREGISTERED) {
		cs->ussd = NULL;
		return;
	}

	cs->ussd = __ofono_atom_get_data(atom);
	cs_register_ss_controls(cs);
}

void ofono_call_settings_register(struct ofono_call_settings *cs)
{
	DBusConnection *conn = ofono_dbus_get_connection();
	const char *path = __ofono_atom_get_path(cs->atom);
	struct ofono_modem *modem = __ofono_atom_get_modem(cs->atom);

	if (!g_dbus_register_interface(conn, path,
					OFONO_CALL_SETTINGS_INTERFACE,
					cs_methods, cs_signals, NULL, cs,
					NULL)) {
		ofono_error("Could not create %s interface",
				OFONO_CALL_SETTINGS_INTERFACE);

		return;
	}

	ofono_modem_add_interface(modem, OFONO_CALL_SETTINGS_INTERFACE);

	cs->ussd_watch = __ofono_modem_add_atom_watch(modem,
					OFONO_ATOM_TYPE_USSD,
					ussd_watch, cs, NULL);

	__ofono_atom_register(cs->atom, call_settings_unregister);

	cs->cs_queue = g_queue_new();

	cs->netreg_watch =
		__ofono_modem_add_atom_watch(modem, OFONO_ATOM_TYPE_NETREG, netreg_watch, cs, NULL);
}

void ofono_call_settings_remove(struct ofono_call_settings *cs)
{
	__ofono_atom_free(cs->atom);
}

void ofono_call_settings_set_data(struct ofono_call_settings *cs, void *data)
{
	cs->driver_data = data;
}

void *ofono_call_settings_get_data(struct ofono_call_settings *cs)
{
	return cs->driver_data;
}
