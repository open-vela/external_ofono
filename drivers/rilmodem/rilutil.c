/*
 *
 *  oFono - Open Source Telephony
 *
 *  Copyright (C) 2008-2011  Intel Corporation. All rights reserved.
 *  Copyright (C) 2012  Canonical Ltd.
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

#include <glib.h>
#include <gril.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

#define OFONO_API_SUBJECT_TO_CHANGE
#include <ofono/log.h>
#include <ofono/types.h>

#include "common.h"
#include "rilutil.h"
#include "simutil.h"
#include "util.h"
#include "ril_constants.h"

void decode_ril_error(struct ofono_error *error, const char *final)
{
	if (!strcmp(final, "OK")) {
		error->type = OFONO_ERROR_TYPE_NO_ERROR;
		error->error = 0;
	} else {
		error->type = OFONO_ERROR_TYPE_FAILURE;
		error->error = 0;
	}
}

gchar *ril_util_get_netmask(const gchar *address)
{
	if (g_str_has_suffix(address, "/30"))
		return PREFIX_30_NETMASK;

	if (g_str_has_suffix(address, "/29"))
		return PREFIX_29_NETMASK;

	if (g_str_has_suffix(address, "/28"))
		return PREFIX_28_NETMASK;

	if (g_str_has_suffix(address, "/27"))
		return PREFIX_27_NETMASK;

	if (g_str_has_suffix(address, "/26"))
		return PREFIX_26_NETMASK;

	if (g_str_has_suffix(address, "/25"))
		return PREFIX_25_NETMASK;

	if (g_str_has_suffix(address, "/24"))
		return PREFIX_24_NETMASK;

	return NULL;
}

void ril_util_build_activate_data_call(GRil *gril, struct parcel *rilp,
		const char *apn, int type, const char *user, const char *pwd,
			int auth_method, int proto, gpointer user_data)
{
	struct cb_data *cbd = user_data;
	struct ofono_gprs_context *gc = cbd->user;
	struct ofono_modem *modem = ofono_gprs_context_get_modem(gc);
	char buf[256];
	int num_param = 7;
	int tech;
	const char *profile;
	int auth_type;

	tech = ofono_modem_get_integer(modem, "RilDataRadioTechnology");

	/*
	 * 0: CDMA 1: GSM/UMTS, 2...
	 * anything 2+ is a RadioTechnology value +2
	 */
	ofono_debug("*gc: %p activating apn: %s; curr_tech: %d", gc, apn, tech);

	parcel_init(rilp);
	parcel_w_int32(rilp, num_param);

	if (tech == RADIO_TECH_UNKNOWN) {
		ofono_error("%s: radio tech for apn: %s UNKNOWN!", __func__, apn);
		tech = 1;
	} else
		tech = tech + 2;

	sprintf(buf, "%d", tech);
	parcel_w_string(rilp, buf);

	profile = ril_util_get_apn_profile_id(type);

	parcel_w_string(rilp, profile);
	parcel_w_string(rilp, apn);
	parcel_w_string(rilp, user);
	parcel_w_string(rilp, pwd);

	/*
	 * We do the same as in $AOSP/frameworks/opt/telephony/src/java/com/
	 * android/internal/telephony/dataconnection/DataConnection.java,
	 * onConnect(), and use authentication or not depending on whether
	 * the user field is empty or not,
	 * on top of the verification for the authentication method.
	 */

	if (auth_method != OFONO_GPRS_AUTH_METHOD_NONE &&
						user[0] != '\0')
		auth_type = RIL_AUTH_BOTH;
	else
		auth_type = RIL_AUTH_NONE;

	sprintf(buf, "%d", auth_type);
	parcel_w_string(rilp, buf);

	parcel_w_string(rilp, ril_util_gprs_proto_to_ril_string(proto));

	g_ril_append_print_buf(gril, "(%d,%s,%s,***,***,%d,%s)",
					tech, profile, apn, auth_type,
					ril_util_gprs_proto_to_ril_string(proto));
}

void ril_util_build_deactivate_data_call(GRil *gril, struct parcel *rilp,
						int cid, unsigned int reason)
{
	char *cid_str = NULL;
	char *reason_str = NULL;

	cid_str = g_strdup_printf("%d", cid);
	reason_str = g_strdup_printf("%d", reason);

	parcel_init(rilp);
	parcel_w_int32(rilp, 2);
	parcel_w_string(rilp, cid_str);
	parcel_w_string(rilp, reason_str);

	g_ril_append_print_buf(gril, "(%s,%s)", cid_str, reason_str);

	g_free(cid_str);
	g_free(reason_str);
}

const char *ril_util_gprs_proto_to_ril_string(enum ofono_gprs_proto proto)
{
	switch (proto) {
	case OFONO_GPRS_PROTO_IPV6:
		return "IPV6";
	case OFONO_GPRS_PROTO_IPV4V6:
		return "IPV4V6";
	case OFONO_GPRS_PROTO_IP:
		return "IP";
	default:
		return "IPV4V6";
	}
}

int ril_util_registration_state_to_status(int reg_state)
{
	switch (reg_state) {
	case RIL_REG_STATE_NOT_REGISTERED:
	case RIL_REG_STATE_REGISTERED:
	case RIL_REG_STATE_SEARCHING:
	case RIL_REG_STATE_DENIED:
	case RIL_REG_STATE_UNKNOWN:
	case RIL_REG_STATE_ROAMING:
		break;

	case RIL_REG_STATE_EMERGENCY_REGISTED:
	case RIL_REG_STATE_EMERGENCY_NOT_REGISTERED:
	case RIL_REG_STATE_EMERGENCY_SEARCHING:
	case RIL_REG_STATE_EMERGENCY_DENIED:
	case RIL_REG_STATE_EMERGENCY_UNKNOWN:
		break;
	default:
		reg_state = NETWORK_REGISTRATION_STATUS_UNKNOWN;
	}

	return reg_state;
}

int ril_util_address_to_gprs_proto(const char *addr)
{
	int ret = -1;
	struct in_addr ipv4;
	struct in6_addr ipv6;
	char **addr_split = g_strsplit(addr, "/", 2);

	if (addr_split == NULL || g_strv_length(addr_split) == 0)
		goto done;

	if (inet_pton(AF_INET, addr_split[0], &ipv4) > 0) {
		ret = OFONO_GPRS_PROTO_IP;
		goto done;
	}

	if (inet_pton(AF_INET6, addr_split[0], &ipv6) > 0) {
		ret = OFONO_GPRS_PROTO_IPV6;
		goto done;
	}

done:
	g_strfreev(addr_split);

	return ret;
}
