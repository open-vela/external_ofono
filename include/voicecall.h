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

#ifndef __OFONO_VOICECALL_H
#define __OFONO_VOICECALL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ofono/types.h>

/*
 *  category 0 means default category,condition 1 means real ecc in no sim or
 *  sim
 */
#define DEFAULT_CATEGORY_CONDITION_FOR_ECC "0,1"

struct ofono_modem;
struct ofono_netreg;
struct ofono_voicecall;

typedef void (*ofono_voicecall_cb_t)(const struct ofono_error *error,
					void *data);

/* Voice call related functionality, including ATD, ATA, +CHLD, CTFR, CLCC
 * and VTS.
 *
 * It is up to the plugin to implement polling of CLCC if the modem does
 * not support vendor extensions for call progress indication.
 */
struct ofono_voicecall_driver {
	const char *name;
	int (*probe)(struct ofono_voicecall *vc, unsigned int vendor,
			void *data);
	void (*remove)(struct ofono_voicecall *vc);

	/* According to 22.030 the dial is expected to do the following:
	 * - If an there is an existing active call(s), and the dial is
	 *   successful, the active calls are automatically put on hold.
	 *   Driver must take special care to put the call on hold before
	 *   returning from atd call.
	 *
	 * - The dial has no affect on the state of the waiting call,
	 *   if the hardware does not support this, then it is better
	 *   to return an error here.  No special handling of the
	 *   waiting call is performed by the core
	 */
	void (*dial)(struct ofono_voicecall *vc,
			const struct ofono_phone_number *number,
			enum ofono_clir_option clir, ofono_voicecall_cb_t cb,
			void *data);
	/* dials a number at a given memory location */
	void (*dial_memory)(struct ofono_voicecall *vc,
			unsigned int memory_location, ofono_voicecall_cb_t cb,
			void *data);
	/* Dials the last number again, this handles the hfp profile last number
         * dialing with the +BLDN AT command
         */
	void (*dial_last)(struct ofono_voicecall *vc, ofono_voicecall_cb_t cb, void *data);
	/* Answers an incoming call, this usually corresponds to ATA */
	void (*answer)(struct ofono_voicecall *vc,
			ofono_voicecall_cb_t cb, void *data);

	/* Hangs up active, dialing, alerting or incoming calls */
	void (*hangup_active)(struct ofono_voicecall *vc,
			ofono_voicecall_cb_t cb, void *data);
	/* Hangs up all calls except waiting calls */
	void (*hangup_all)(struct ofono_voicecall *vc,
			ofono_voicecall_cb_t cb, void *data);
	/*
	 * Holds all active calls and answers waiting call.  If there is
	 * no waiting calls, retrieves held call.  This usually
	 * corresponds to +CHLD=2
	 */
	void (*hold_all_active)(struct ofono_voicecall *vc,
			ofono_voicecall_cb_t cb, void *data);
	/* Releases all held calls, this usually corresponds to +CHLD=0*/
	void (*release_all_held)(struct ofono_voicecall *vc,
			ofono_voicecall_cb_t cb, void *data);
	/*
	 * Sets the UDUB condition on a waiting call.  This usually
	 * corresponds to +CHLD=0
	 */
	void (*set_udub)(struct ofono_voicecall *vc,
			ofono_voicecall_cb_t cb, void *data);
	/*
	 * Releases all active calls and accepts a possible waiting call.
	 * This usually corresponds to +CHLD=1
	 */
	void (*release_all_active)(struct ofono_voicecall *vc,
			ofono_voicecall_cb_t cb, void *data);
	/*
	 * Releases a specific call given by id.  This usually corresponds to
	 * +CHLD=1X.  In 3GPP this command is only guaranteed to affect active
	 * calls.  Plugins are encouraged to implement this using vendor
	 * commands that can also affect held calls
	 */
	void (*release_specific)(struct ofono_voicecall *vc, int id,
			ofono_voicecall_cb_t cb, void *data);
	/*
	 * Breaks out a party given by id from a multiparty call.  This
	 * usually corresponds to +CHLD=2X
	 */
	void (*private_chat)(struct ofono_voicecall *vc, int id,
			ofono_voicecall_cb_t cb, void *data);
	/*
	 * Joins held and active calls together into a multiparty call.  This
	 * usually corresponds to +CHLD=3
	 */
	void (*create_multiparty)(struct ofono_voicecall *vc,
			ofono_voicecall_cb_t cb, void *data);
	/*
	 * Connects two calls together and disconnects from both calls.  This
	 * usually corresponds to +CHLD=4
	 */
	void (*transfer)(struct ofono_voicecall *vc,
			ofono_voicecall_cb_t cb, void *data);
	/*
	 * Deflects an incoming or waiting call to a given number.  This
	 * usually corresponds to +CTFR
	 */
	void (*deflect)(struct ofono_voicecall *vc,
			const struct ofono_phone_number *ph,
			ofono_voicecall_cb_t cb, void *data);
	/*
	 * This is equivalent to +CHLD=2 but does not affect a possible
	 * waiting call.
	 */
	void (*swap_without_accept)(struct ofono_voicecall *vc,
			ofono_voicecall_cb_t cb, void *data);
	void (*send_tones)(struct ofono_voicecall *vc, const char *tones,
			ofono_voicecall_cb_t cb, void *data);

	/*
	 * Initiates an IMS conference call
	 */
	void (*dial_conferece)(struct ofono_voicecall *vc, unsigned int count,
			char *numbers[], ofono_voicecall_cb_t cb, void *data);

	/*
	 * Requests the conference server to invite an additional participants
	 * to the conference.
	 */
	void (*invite_participants)(struct ofono_voicecall *vc, unsigned int count,
			char *numbers[], ofono_voicecall_cb_t cb, void *data);

	/* set custom ecc list to modem */
	void (*set_cust_ecc)(struct ofono_voicecall *vc,
			GSList *l, ofono_voicecall_cb_t cb, void *data);

	/*
	 * start/sop playing a DTMF tone.
	 */
	void (*play_dtmf)(struct ofono_voicecall *vc, int flag,
			unsigned char digit, ofono_voicecall_cb_t cb, void *data);

	/*
	 * update call duration when signal level change
	 */
	void (*update_call_duration)(struct ofono_voicecall *vc, int signal_level);
};

void ofono_voicecall_en_list_notify(struct ofono_voicecall *vc,
					char **nw_en_list);

void ofono_voicecall_notify(struct ofono_voicecall *vc,
				const struct ofono_call *call);
void ofono_voicecall_disconnected(struct ofono_voicecall *vc, int id,
				enum ofono_disconnect_reason reason,
				const struct ofono_error *error);

/*
 * For those protocols where MPTY creation happens outside of oFono's control,
 * e.g. Bluetooth Handsfree, set the hint of the MPTY call list by passing
 * in a bitmask of ids participating in the MPTY call
 */
void ofono_voicecall_mpty_hint(struct ofono_voicecall *vc, unsigned int ids);

struct ofono_modem *ofono_voicecall_get_modem(struct ofono_voicecall *vc);

int ofono_voicecall_driver_register(const struct ofono_voicecall_driver *d);
void ofono_voicecall_driver_unregister(const struct ofono_voicecall_driver *d);

struct ofono_voicecall *ofono_voicecall_create(struct ofono_modem *modem,
					unsigned int vendor,
					const char *driver, void *data);

void ofono_voicecall_register(struct ofono_voicecall *vc);
void ofono_voicecall_remove(struct ofono_voicecall *vc);

void ofono_voicecall_set_data(struct ofono_voicecall *vc, void *data);
void *ofono_voicecall_get_data(struct ofono_voicecall *vc);
int ofono_voicecall_get_next_callid(struct ofono_voicecall *vc);

void ofono_voicecall_ssn_mo_notify(struct ofono_voicecall *vc, unsigned int id,
					int code, int index);
void ofono_voicecall_ssn_mt_notify(struct ofono_voicecall *vc, unsigned int id,
					int code, int index,
					const struct ofono_phone_number *ph);
void ofono_voicecall_ringback_tone_notify(struct ofono_voicecall *vc,
					unsigned int id, int value);
ofono_bool_t ofono_voicecall_is_emergency_number(struct ofono_voicecall *vc,
					const char *number);
int ofono_voicecall_get_signal_level(struct ofono_voicecall *vc);
void ofono_voicecall_update_call_duration(struct ofono_voicecall *vc,
					struct ofono_netreg *netreg);
char *ofono_voicecall_get_last_used_mnc_mcc(char *type);
GSList *ofono_voicecall_load_cust_ecc_with_mcc_mnc(const char *mcc,
						   const char *mnc);
const char **ofono_voicecall_get_default_en_list(void);
const char **ofono_voicecall_get_default_en_list_no_sim(void);
#ifdef __cplusplus
}
#endif

#endif /* __OFONO_VOICECALL_H */
