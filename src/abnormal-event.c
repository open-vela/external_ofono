/*
 * Copyright (C) 2024 Xiaomi Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gril.h>
#include <parcel.h>

#include "common.h"
#include "gril/ril_constants.h"
#include "ofono.h"

#include <ofono/abnormal-event.h>

#define INT_STR_MAX 15
#define MAX_PARAM_SIZE 6
#define MAX_STR_PARAM_LEN 50
#define KEY_NAME "abnormal_event"

static struct ofono_ue_camp_cell_info g_camped_cell_info = {
	.sub = 0,
	.plmn = 0,
	.tac = 0,
	.cell_id = 0,
	.band = 0,
	.earfcn = 0,
	.pci = 0,
};

static void report_abnormal_info(const struct ofono_ue_camp_cell_info *info, int param_count, ...)
{
	va_list args;
	char abnormal_info[MAX_PARAM_SIZE][MAX_STR_PARAM_LEN];

	if (param_count > MAX_PARAM_SIZE) {
		ofono_error("unexpected param count");
		return;
	}

	va_start(args, param_count);
	for (int i = 0; i < param_count; i++) {
		int type = va_arg(args, int);
		switch (type) {
		case TYPE_INT: {
			if (i == 0) { // first is type_id
				int type_id = va_arg(args, int);

				snprintf(abnormal_info[i], MAX_STR_PARAM_LEN, "%d_%u_%u_%u",
					 type_id, info->tac, info->plmn, info->cell_id);
			} else {
				snprintf(abnormal_info[i], MAX_STR_PARAM_LEN, "%d",
					 va_arg(args, int));
			}
			break;
		}
		case TYPE_UNSIGNED_INT: {
			snprintf(abnormal_info[i], MAX_STR_PARAM_LEN, "%u",
				 va_arg(args, unsigned int));
			break;
		}
		case TYPE_CHAR: {
			char *str = va_arg(args, char *);
			int len = strlen(str);
			strncpy(abnormal_info[i], str, MAX_STR_PARAM_LEN - 1);
			if (len > MAX_STR_PARAM_LEN - 1) {
				abnormal_info[i][MAX_STR_PARAM_LEN - 1] = '\0';
			} else {
				abnormal_info[i][len] = '\0';
			}
			break;
		}
		default: {
			ofono_error("unexpected parm type,type=%d", type);
			break;
		}
		}
	}
	va_end(args);
	for (int i = param_count; i < MAX_PARAM_SIZE; i++) {
		strcpy(abnormal_info[i], "0");
	}
	OFONO_DFX_ABNORMAL_EVENT_INFO(abnormal_info[0], abnormal_info[1], abnormal_info[2],
				      abnormal_info[3], abnormal_info[4], abnormal_info[5]);
}

void ofono_handle_abnormal_event(struct ofono_modem *modem, int type_id, char *data, int data_len)
{
	size_t len;
	unsigned char *covert_data = NULL;
	const char *type_str;

	type_str = abnormal_event_type_to_string(type_id);
	if (data != NULL) {
		ofono_debug("%s,type=%s,type_id=%d,data_len=%d,data=%s", KEY_NAME, type_str,
			type_id, data_len, data);
	}

	covert_data = l_util_from_hexstring(data, &len);
	if (covert_data == NULL) {
		ofono_error("covert_data is NULL");
		return;
	}
	if (len * 2 != data_len) {
		l_free(covert_data);
		ofono_error("data len mismatch %zu,%d", len, data_len);
		return;
	}
	switch (type_id)
	{
	case OFONO_ABNORMAL_INSIDE_MODEM:
		ofono_debug("%s:%s", KEY_NAME, type_str);
		break;
	case OFONO_ABNORMAL_EF_FILE: {
		struct ofono_abnormal_ef_file *ef_file_data;

		if (len != sizeof(struct ofono_abnormal_ef_file)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_abnormal_ef_file));
			break;
		}
		ef_file_data = (struct ofono_abnormal_ef_file *) covert_data;

		ofono_debug("%s,sub=%u,ef_id=%u,sw1=%u,sw2=%u", KEY_NAME, ef_file_data->sub,
			    ef_file_data->ef_id, ef_file_data->sw1, ef_file_data->sw2);
		report_abnormal_info(&g_camped_cell_info, 4, TYPE_INT, type_id, TYPE_UNSIGNED_INT,
				     ef_file_data->ef_id, TYPE_UNSIGNED_INT, ef_file_data->sw1,
				     TYPE_UNSIGNED_INT, ef_file_data->sw2);
		break;
	}
	case OFONO_ABNORMAL_PROFILE: {
		struct ofono_abnormal_profile *profile_data;

		if (len != sizeof(struct ofono_abnormal_profile)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_abnormal_profile));
			break;
		}
		profile_data = (struct ofono_abnormal_profile *) covert_data;

		ofono_debug("%s,sub=%u", KEY_NAME, profile_data->sub);
		report_abnormal_info(&g_camped_cell_info, 2, TYPE_INT, type_id, TYPE_INT, 1);
		break;
	}
	case OFONO_ABNORMAL_RLF: {
		struct ofono_abnormal_reest *reest_data;

		if (len != sizeof(struct ofono_abnormal_reest)) {
			ofono_error("len mismatch %zu,%zu", len, sizeof(struct ofono_abnormal_reest));
			break;
		}
		reest_data = (struct ofono_abnormal_reest *) covert_data;
		const char *cause_str = reest_cause_to_string(reest_data->reest_cause);

		ofono_debug("%s,sub=%u,earfcn=%u,pci=%u,band=%u,rsrp=%d,rsrq=%"
			    "d,sinr=%d,rssi=%d,reest_cause=%s",
			    KEY_NAME, reest_data->sub, reest_data->earfcn, reest_data->pci,
			    reest_data->band, reest_data->cell_quality.rsrp,
			    reest_data->cell_quality.rsrq, reest_data->cell_quality.sinr,
			    reest_data->cell_quality.rssi, cause_str);
		report_abnormal_info(&g_camped_cell_info, 6, TYPE_INT, type_id, TYPE_UNSIGNED_INT,
				     reest_data->earfcn, TYPE_UNSIGNED_INT, reest_data->pci,
				     TYPE_INT, reest_data->cell_quality.rsrp, TYPE_INT,
				     reest_data->cell_quality.sinr, TYPE_UNSIGNED_INT,
				     reest_data->reest_cause);
		break;
	}
	case OFONO_ABNORMAL_RACH_ACCESS: {
		struct ofono_abnormal_rach_access *rach_access_data;

		if (len != sizeof(struct ofono_abnormal_rach_access)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_abnormal_rach_access));
			break;
		}
		rach_access_data = (struct ofono_abnormal_rach_access *) covert_data;
		const char *cause_str = rach_fail_reason_to_string(rach_access_data->fail_reason);

		ofono_debug("%s,sub=%u,earfcn=%u,pci=%u,fail_reason=%s", KEY_NAME,
			    rach_access_data->sub, rach_access_data->earfcn, rach_access_data->pci,
			    cause_str);
		report_abnormal_info(&g_camped_cell_info, 4, TYPE_INT, type_id, TYPE_UNSIGNED_INT,
				     rach_access_data->earfcn, TYPE_UNSIGNED_INT,
				     rach_access_data->pci, TYPE_UNSIGNED_INT,
				     rach_access_data->fail_reason);
		break;
	}
	case OFONO_ABNORMAL_OOS: {
		struct ofono_abnormal_oos *oos_data;

		if (len != sizeof(struct ofono_abnormal_oos)) {
			ofono_error("len mismatch %zu,%zu", len, sizeof(struct ofono_abnormal_oos));
			break;
		}
		oos_data = (struct ofono_abnormal_oos *) covert_data;
		const char *oos_type_str = oos_type_to_string(oos_data->oos_type);

		ofono_debug("%s,sub=%u,earfcn=%u,pci=%u,oos_type=%s", KEY_NAME, oos_data->sub,
			    oos_data->earfcn, oos_data->pci, oos_type_str);
		report_abnormal_info(&g_camped_cell_info, 4, TYPE_INT, type_id, TYPE_UNSIGNED_INT,
				     oos_data->earfcn, TYPE_UNSIGNED_INT, oos_data->pci,
				     TYPE_UNSIGNED_INT, oos_data->oos_type);
		break;
	}
	case OFONO_ABNORMAL_NAS_TIMEOUT: {
		struct ofono_abnormal_timer_exp *timer_exp_data;

		if (len != sizeof(struct ofono_abnormal_timer_exp)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_abnormal_timer_exp));
			break;
		}
		timer_exp_data = (struct ofono_abnormal_timer_exp *) covert_data;
		const char *timer_str = nas_timer_id_to_string(timer_exp_data->timer_id);

		ofono_debug("%s,sub=%u,timer_id=%s", KEY_NAME, timer_exp_data->sub, timer_str);
		if (timer_exp_data->cell_quality_exist) {
			ofono_debug("%s,rsrp=%d,rsrq=%d,sinr=%d,rssi=%d", KEY_NAME,
				    timer_exp_data->cell_quality.rsrp,
				    timer_exp_data->cell_quality.rsrq,
				    timer_exp_data->cell_quality.sinr,
				    timer_exp_data->cell_quality.rssi);
		}
		report_abnormal_info(&g_camped_cell_info, 6, TYPE_INT, type_id, TYPE_UNSIGNED_INT,
				     timer_exp_data->timer_id, TYPE_INT,
				     timer_exp_data->cell_quality.rsrp, TYPE_INT,
				     timer_exp_data->cell_quality.rsrq, TYPE_INT,
				     timer_exp_data->cell_quality.sinr, TYPE_INT,
				     timer_exp_data->cell_quality.rssi);
		break;
	}
	case OFONO_ABNORMAL_SIP_TIMEOUT: {
		struct ofono_abnormal_sip_timeout *sip_timeout_data;

		if (len != sizeof(struct ofono_abnormal_sip_timeout)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_abnormal_sip_timeout));
			break;
		}
		sip_timeout_data = (struct ofono_abnormal_sip_timeout *) covert_data;
		const char *srv_type_str = sip_srv_type_to_string(sip_timeout_data->srv_type);
		const char *sip_method_str = sip_method_to_string(sip_timeout_data->sip_method);

		ofono_debug("%s,sub=%u,srv_type=%s,sip_method=%s", KEY_NAME, sip_timeout_data->sub,
			    srv_type_str, sip_method_str);
		report_abnormal_info(&g_camped_cell_info, 3, TYPE_INT, type_id, TYPE_UNSIGNED_INT,
				     sip_timeout_data->srv_type, TYPE_UNSIGNED_INT,
				     sip_timeout_data->sip_method);
		break;
	}
	case OFONO_ABNORMAL_TIMEOUT_IN_RRC: {
		struct ofono_abnormal_timeout_in_rrc *timeout_in_rrc_data;

		if (len != sizeof(struct ofono_abnormal_timeout_in_rrc)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_abnormal_timeout_in_rrc));
			break;
		}

		timeout_in_rrc_data = (struct ofono_abnormal_timeout_in_rrc *) covert_data;
		const char *timer_id_str = rrc_timer_id_to_string(timeout_in_rrc_data->timer);

		ofono_debug("%s,sub=%u,earfcn=%u,pci=%u,timer=%s", KEY_NAME,
			    timeout_in_rrc_data->sub, timeout_in_rrc_data->earfcn,
			    timeout_in_rrc_data->pci, timer_id_str);
		report_abnormal_info(&g_camped_cell_info, 4, TYPE_INT, type_id, TYPE_UNSIGNED_INT,
				     timeout_in_rrc_data->earfcn, TYPE_UNSIGNED_INT,
				     timeout_in_rrc_data->pci, TYPE_UNSIGNED_INT,
				     timeout_in_rrc_data->timer);
		break;
	}
	case OFONO_ABNORMAL_ECC_CALL_FAIL: {
		struct ofono_abnormal_ecc_call_fail *ecc_call_fail_data;

		if (len != sizeof(struct ofono_abnormal_ecc_call_fail)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_abnormal_ecc_call_fail));
			break;
		}
		ecc_call_fail_data = (struct ofono_abnormal_ecc_call_fail *) covert_data;
		const char *ecall_fail_str = ecall_fail_cause_to_string(ecc_call_fail_data->cause);

		ofono_debug("%s,sub=%u,cause=%s", KEY_NAME, ecc_call_fail_data->sub,
			    ecall_fail_str);
		report_abnormal_info(&g_camped_cell_info, 2, TYPE_INT, type_id, TYPE_UNSIGNED_INT,
				     ecc_call_fail_data->cause);
		break;
	}
	case OFONO_ABNORMAL_RTP_RTCP: {
		struct ofono_abnormal_rtp_rtcp *rtp_rtcp_data;

		if (len != sizeof(struct ofono_abnormal_rtp_rtcp)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_abnormal_rtp_rtcp));
			break;
		}
		rtp_rtcp_data = (struct ofono_abnormal_rtp_rtcp *) covert_data;
		const char *rtp_error_str = rtp_rtcp_error_to_string(rtp_rtcp_data->error_type);

		ofono_debug("%s,sub=%u,error_type=%s", KEY_NAME, rtp_rtcp_data->sub, rtp_error_str);
		report_abnormal_info(&g_camped_cell_info, 2, TYPE_INT, type_id, TYPE_UNSIGNED_INT,
				     rtp_rtcp_data->error_type);
		break;
	}
	case OFONO_ABNORMAL_PAGING_DECODE: {
		struct ofono_abnormal_paging_decode *paging_decode_data;

		if (len != sizeof(struct ofono_abnormal_paging_decode)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_abnormal_paging_decode));
			break;
		}
		paging_decode_data = (struct ofono_abnormal_paging_decode *) covert_data;

		ofono_debug("%s,sub=%u,earfcn=%u,pci=%u,rsrp=%d,rsrq=%d,sinr=%"
			    "d,rssi=%d",
			    KEY_NAME, paging_decode_data->sub, paging_decode_data->earfcn,
			    paging_decode_data->pci, paging_decode_data->cell_quality.rsrp,
			    paging_decode_data->cell_quality.rsrq,
			    paging_decode_data->cell_quality.sinr,
			    paging_decode_data->cell_quality.rssi);
		report_abnormal_info(&g_camped_cell_info, 6, TYPE_INT, type_id, TYPE_UNSIGNED_INT,
				     paging_decode_data->earfcn, TYPE_UNSIGNED_INT,
				     paging_decode_data->pci, TYPE_INT,
				     paging_decode_data->cell_quality.rsrp, TYPE_INT,
				     paging_decode_data->cell_quality.sinr, TYPE_INT,
				     paging_decode_data->cell_quality.rssi);
		break;
	}
	case OFONO_ABNORMAL_CALL_QUALITY: {
		struct ofono_abnormal_call_quality *call_quality_data;

		if (len != sizeof(struct ofono_abnormal_call_quality)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_abnormal_call_quality));
			break;
		}
		call_quality_data = (struct ofono_abnormal_call_quality *) covert_data;

		ofono_debug("%s,sub=%u,pkt_lost=%u,fraction_lost=%u,jitter_"
			    "buffer_size=%u",
			    KEY_NAME, call_quality_data->sub, call_quality_data->pkt_lost,
			    call_quality_data->fraction_lost,
			    call_quality_data->jitter_buffer_size);
		report_abnormal_info(&g_camped_cell_info, 4, TYPE_INT, type_id, TYPE_UNSIGNED_INT,
				     call_quality_data->pkt_lost, TYPE_UNSIGNED_INT,
				     call_quality_data->fraction_lost, TYPE_UNSIGNED_INT,
				     call_quality_data->jitter_buffer_size);
		break;
	}
	case OFONO_ABNORMAL_PDCP: {
		struct ofono_abnormal_pdcp *pdcp_data;

		if (len != sizeof(struct ofono_abnormal_pdcp)) {
			ofono_error("len mismatch %zu,%zu", len, sizeof(struct ofono_abnormal_pdcp));
			break;
		}
		pdcp_data = (struct ofono_abnormal_pdcp *) covert_data;

		ofono_debug("%s,sub=%u,dl_loss_rate=%u,ul_loss_rate=%u,rsrp=%d,"
			    "rsrq=%d,sinr=%d,rssi=%d",
			    KEY_NAME, pdcp_data->sub, pdcp_data->dl_loss_rate,
			    pdcp_data->ul_loss_rate, pdcp_data->cell_quality.rsrp,
			    pdcp_data->cell_quality.rsrq, pdcp_data->cell_quality.sinr,
			    pdcp_data->cell_quality.rssi);
		report_abnormal_info(
			&g_camped_cell_info, 6, TYPE_INT, type_id, TYPE_UNSIGNED_INT,
			pdcp_data->dl_loss_rate, TYPE_UNSIGNED_INT, pdcp_data->ul_loss_rate,
			TYPE_INT, pdcp_data->cell_quality.rsrp, TYPE_INT,
			pdcp_data->cell_quality.rsrq, TYPE_INT, pdcp_data->cell_quality.sinr);
		break;
	}
	case OFONO_ABNORMAL_NAS_REJECT: {
		struct ofono_abnormal_nas_reject *nas_reject_data;

		if (len != sizeof(struct ofono_abnormal_nas_reject)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_abnormal_nas_reject));
			break;
		}
		nas_reject_data = (struct ofono_abnormal_nas_reject *) covert_data;
		const char *procedure_type_str =
			nas_procedure_type_to_string(nas_reject_data->procedure_type);

		ofono_debug("%s,sub=%u,procedure_type=%s,reject_cause=%u", KEY_NAME,
			    nas_reject_data->sub, procedure_type_str,
			    nas_reject_data->reject_cause);
		if (nas_reject_data->cell_quality_exist) {
			ofono_debug("%s,rsrp=%d,rsrq=%d,sinr=%d,rssi=%d", KEY_NAME,
				    nas_reject_data->cell_quality.rsrp,
				    nas_reject_data->cell_quality.rsrq,
				    nas_reject_data->cell_quality.sinr,
				    nas_reject_data->cell_quality.rssi);
		}
		report_abnormal_info(&g_camped_cell_info, 6, TYPE_INT, type_id, TYPE_UNSIGNED_INT,
				     nas_reject_data->procedure_type, TYPE_UNSIGNED_INT,
				     nas_reject_data->reject_cause, TYPE_INT,
				     nas_reject_data->cell_quality.rsrp, TYPE_INT,
				     nas_reject_data->cell_quality.rsrq, TYPE_INT,
				     nas_reject_data->cell_quality.sinr);
		break;
	}
	case OFONO_ABNORMAL_SIP_REJECT: {
		struct ofono_abnormal_sip_reject *sip_reject_data;

		if (len != sizeof(struct ofono_abnormal_sip_reject)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_abnormal_sip_reject));
			break;
		}
		sip_reject_data = (struct ofono_abnormal_sip_reject *) covert_data;
		const char *srv_type_str = sip_srv_type_to_string(sip_reject_data->srv_type);
		const char *sip_method_str = sip_method_to_string(sip_reject_data->sip_method);

		ofono_debug("%s,sub=%u,srv_type=%s,sip_method=%s,resp_code=%u", KEY_NAME,
			    sip_reject_data->sub, srv_type_str, sip_method_str,
			    sip_reject_data->resp_code);
		report_abnormal_info(&g_camped_cell_info, 4, TYPE_INT, type_id, TYPE_UNSIGNED_INT,
				     sip_reject_data->srv_type, TYPE_UNSIGNED_INT,
				     sip_reject_data->sip_method, TYPE_UNSIGNED_INT,
				     sip_reject_data->resp_code);
		break;
	}
	case OFONO_ABNORMAL_RRC_REJECT: {
		struct ofono_abnormal_rrc_reject *rrc_reject_data;

		if (len != sizeof(struct ofono_abnormal_rrc_reject)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_abnormal_rrc_reject));
			break;
		}
		rrc_reject_data = (struct ofono_abnormal_rrc_reject *) covert_data;

		ofono_debug("%s,sub=%u,earfcn=%u,pci=%u,error_scenario_id=%u", KEY_NAME,
			    rrc_reject_data->sub, rrc_reject_data->earfcn, rrc_reject_data->pci,
			    rrc_reject_data->error_scenario_id);
		report_abnormal_info(&g_camped_cell_info, 4, TYPE_INT, type_id, TYPE_UNSIGNED_INT,
				     rrc_reject_data->earfcn, TYPE_UNSIGNED_INT,
				     rrc_reject_data->pci, TYPE_UNSIGNED_INT,
				     rrc_reject_data->error_scenario_id);
		break;
	}
	case OFONO_ABNORMAL_PING_PONG: {
		struct ofono_abnormal_ping_pong *ping_pong_data;

		if (len != sizeof(struct ofono_abnormal_ping_pong)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_abnormal_ping_pong));
			break;
		}
		ping_pong_data = (struct ofono_abnormal_ping_pong *) covert_data;

		ofono_debug("%s,pcell_pci=%u,pcell_earfcn=%u,pcell_rsrp=%u,"
			    "ncell_pci=%u,ncell_earfcn=%u,ncell_rsrp=%u",
			    KEY_NAME, ping_pong_data->pcell_pci, ping_pong_data->pcell_earfcn,
			    ping_pong_data->pcell_rsrp, ping_pong_data->ncell_pci,
			    ping_pong_data->ncell_earfcn, ping_pong_data->ncell_rsrp);
		break;
	}
	case OFONO_ABNORMAL_CC: {
		struct ofono_cc_abnormal_fail *cc_fail_data;

		if (len != sizeof(struct ofono_cc_abnormal_fail)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_cc_abnormal_fail));
			break;
		}
		cc_fail_data = (struct ofono_cc_abnormal_fail *) covert_data;

		ofono_debug("%s,sub=%u,cause=%u,error_scenario_id=%u", KEY_NAME, cc_fail_data->sub,
			    cc_fail_data->cause, cc_fail_data->error_scenario_id);
		report_abnormal_info(&g_camped_cell_info, 3, TYPE_INT, type_id, TYPE_UNSIGNED_INT,
				     cc_fail_data->cause, TYPE_UNSIGNED_INT,
				     cc_fail_data->error_scenario_id);
		break;
	}
	case OFONO_ABNORMAL_XCAP: {
		struct ofono_abnormal_xcap *xcap_data;

		if (len != sizeof(struct ofono_abnormal_xcap)) {
			ofono_error("len mismatch %zu,%zu", len, sizeof(struct ofono_abnormal_xcap));
			break;
		}
		xcap_data = (struct ofono_abnormal_xcap *) covert_data;
		const char *xcap_mode_str = xcap_mode_to_string(xcap_data->mode);
		const char *xcap_reason_str = xcap_reason_to_string(xcap_data->reason);
		const char *xcap_error_str = xcap_error_to_string(xcap_data->error_type);

		ofono_debug("%s,sub=%u,mode=%s,reason=%s,error_type=%s", KEY_NAME, xcap_data->sub,
			    xcap_mode_str, xcap_reason_str, xcap_error_str);
		report_abnormal_info(&g_camped_cell_info, 4, TYPE_INT, type_id, TYPE_UNSIGNED_INT,
				     xcap_data->mode, TYPE_UNSIGNED_INT, xcap_data->reason,
				     TYPE_UNSIGNED_INT, xcap_data->error_type);
		break;
	}
	case OFONO_ABNORMAL_DATA: {
		struct ofono_abnormal_data *data_data;

		if (len != sizeof(struct ofono_abnormal_data)) {
			ofono_error("len mismatch %zu,%zu", len, sizeof(struct ofono_abnormal_data));
			break;
		}
		data_data = (struct ofono_abnormal_data *) covert_data;

		ofono_debug("%s,sub=%u,event=%u,rsrp=%d,rsrq=%d,sinr=%d,rssi=%d", KEY_NAME,
			    data_data->sub, data_data->event, data_data->cell_quality.rsrp,
			    data_data->cell_quality.rsrq, data_data->cell_quality.sinr,
			    data_data->cell_quality.rssi);
		report_abnormal_info(&g_camped_cell_info, 6, TYPE_INT, type_id, TYPE_UNSIGNED_INT,
				     data_data->event, TYPE_INT, data_data->cell_quality.rsrp,
				     TYPE_INT, data_data->cell_quality.rsrq, TYPE_INT,
				     data_data->cell_quality.sinr, TYPE_INT,
				     data_data->cell_quality.rssi);
		OFONO_DFX_DATA_INTERRUPTION_INFO();
		break;
	}
	case OFONO_ABNORMAL_CALL_END_REASON_FROM_SIP: {
		struct ofono_abnormal_call_end_reason_from_sip *sip_end_reason_data;

		if (len != sizeof(struct ofono_abnormal_call_end_reason_from_sip)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_abnormal_call_end_reason_from_sip));
			break;
		}
		sip_end_reason_data =
			(struct ofono_abnormal_call_end_reason_from_sip *) covert_data;
		const char *call_end_reason_str =
			call_end_reason_to_string(sip_end_reason_data->reason_type);

		ofono_debug("%s,sub=%u,reason_type=%s", KEY_NAME, sip_end_reason_data->sub,
			    call_end_reason_str);
		report_abnormal_info(&g_camped_cell_info, 2, TYPE_INT, type_id, TYPE_UNSIGNED_INT,
				     sip_end_reason_data->reason_type);
		break;
	}
	case OFONO_LIMITED_SERVICE_CAMP_EVENT: // 200
	{
		struct ofono_abnormal_limited_service *limited_service_data;

		if (len != sizeof(struct ofono_abnormal_limited_service)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_abnormal_limited_service));
			break;
		}
		limited_service_data = (struct ofono_abnormal_limited_service *) covert_data;
		const char *limted_cause_str = limited_cause_to_string(limited_service_data->cause);

		ofono_debug("%s,sub=%u,type=%u,cause=%s", KEY_NAME, limited_service_data->sub,
			    limited_service_data->type, limted_cause_str);
		report_abnormal_info(&g_camped_cell_info, 3, TYPE_INT, type_id, TYPE_UNSIGNED_INT,
				     limited_service_data->type, TYPE_UNSIGNED_INT,
				     limited_service_data->cause);
		break;
	}
	case OFONO_REDIRECT_EVENT: {
		struct ofono_redirect_event *redirect_data;

		if (len != sizeof(struct ofono_redirect_event)) {
			ofono_error("len mismatch %zu,%zu", len, sizeof(struct ofono_redirect_event));
			break;
		}
		redirect_data = (struct ofono_redirect_event *) covert_data;

		ofono_debug("%s,sub=%u", KEY_NAME, redirect_data->sub);
		report_abnormal_info(&g_camped_cell_info, 2, TYPE_INT, type_id, TYPE_INT, 1);
		break;
	}
	case OFONO_HANDOVER_EVENT: {
		struct ofono_abnormal_handover_event *handover_data;

		if (len != sizeof(struct ofono_abnormal_handover_event)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_abnormal_handover_event));
			break;
		}
		handover_data = (struct ofono_abnormal_handover_event *) covert_data;

		ofono_debug("%s,sub=%u", KEY_NAME, handover_data->sub);
		report_abnormal_info(&g_camped_cell_info, 2, TYPE_INT, type_id, TYPE_INT, 1);
		break;
	}
	case OFONO_RESELECT_EVENT: {
		struct ofono_abnormal_reselect_event *reselect_data;

		if (len != sizeof(struct ofono_abnormal_reselect_event)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_abnormal_reselect_event));
			break;
		}
		reselect_data = (struct ofono_abnormal_reselect_event *) covert_data;

		ofono_debug("%s,sub=%u", KEY_NAME, reselect_data->sub);
		report_abnormal_info(&g_camped_cell_info, 2, TYPE_INT, type_id, TYPE_INT, 1);
		break;
	}
	case OFONO_CSFB_EVENT: {
		struct ofono_csfb_event *csfb_data;

		if (len != sizeof(struct ofono_csfb_event)) {
			ofono_error("len mismatch %zu,%zu", len, sizeof(struct ofono_csfb_event));
			break;
		}
		csfb_data = (struct ofono_csfb_event *) covert_data;

		ofono_debug("%s,sub=%u", KEY_NAME, csfb_data->sub);
		break;
	}
	case OFONO_SRVCC_EVENT: {
		struct ofono_srvcc_event *srvcc_data;

		if (len != sizeof(struct ofono_srvcc_event)) {
			ofono_error("len mismatch %zu,%zu", len, sizeof(struct ofono_srvcc_event));
			break;
		}
		srvcc_data = (struct ofono_srvcc_event *) covert_data;

		ofono_debug("%s,sub=%u", KEY_NAME, srvcc_data->sub);
		break;
	}
	case OFONO_UE_CAP_INFO: {
		struct ofono_ue_cap_info *ue_cap_data;

		if (len != sizeof(struct ofono_ue_cap_info)) {
			ofono_error("len mismatch %zu,%zu", len, sizeof(struct ofono_ue_cap_info));
			break;
		}
		ue_cap_data = (struct ofono_ue_cap_info *) covert_data;

		ofono_debug("%s,sub=%u", KEY_NAME, ue_cap_data->sub);
		for (int i = 0; i < ue_cap_data->support_band_num; i++) {
			ofono_debug("%s,band[%d]=%u", KEY_NAME, i,
				    ue_cap_data->support_band_list[i]);
		}
		ofono_debug("%s,category=%u", KEY_NAME, ue_cap_data->category);
		break;
	}
	case OFONO_UE_CAMP_CELL_INFO: {
		struct ofono_ue_camp_cell_info *ue_camp_cell_data;

		if (len != sizeof(struct ofono_ue_camp_cell_info)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_ue_camp_cell_info));
			break;
		}
		ue_camp_cell_data = (struct ofono_ue_camp_cell_info *) covert_data;

		memcpy(&g_camped_cell_info, ue_camp_cell_data,
		       sizeof(struct ofono_ue_camp_cell_info));
		ofono_debug("%s,sub=%u,plmn=%u,tac=%u,cell_id=%u,band=%u,"
			    "earfcn=%u,pci=%u",
			    KEY_NAME, ue_camp_cell_data->sub, ue_camp_cell_data->plmn,
			    ue_camp_cell_data->tac, ue_camp_cell_data->cell_id,
			    ue_camp_cell_data->band, ue_camp_cell_data->earfcn,
			    ue_camp_cell_data->pci);
		ofono_modem_save_band_info(modem, ue_camp_cell_data->band);
		break;
	}
	case OFONO_UE_SIM_INFO: {
		struct ofono_ue_sim_info *ue_sim_data;

		if (len != sizeof(struct ofono_ue_sim_info)) {
			ofono_error("len mismatch %zu,%zu", len, sizeof(struct ofono_ue_sim_info));
			break;
		}
		ue_sim_data = (struct ofono_ue_sim_info *) covert_data;
		char *ehplmn_list = (char *) malloc(sizeof(char) * 6 * ue_sim_data->ehplmn_num + 1);

		if (!ehplmn_list) {
			ofono_error("malloc fail in %s", __func__);
		} else {
			memset(ehplmn_list, 0, sizeof(char) * 6 * ue_sim_data->ehplmn_num + 1);
			for (int i = 0; i < ue_sim_data->ehplmn_num; i++) {
				char temp[OFONO_MAX_MCC_LENGTH + OFONO_MAX_MNC_LENGTH + 1] = {
					'\0'
				};
				snprintf(temp, sizeof(temp), "%d", ue_sim_data->ehplmn[i]);
				strcat(ehplmn_list, temp);
			}
			ofono_debug("%s,sub=%u,hplmn=%u,ehplmn_list=%s", KEY_NAME, ue_sim_data->sub,
				    ue_sim_data->hplmn, ehplmn_list);
			report_abnormal_info(&g_camped_cell_info, 3, TYPE_INT, type_id,
					     TYPE_UNSIGNED_INT, ue_sim_data->hplmn, TYPE_CHAR,
					     ehplmn_list);
			free(ehplmn_list);
		}
		break;
	}
	case OFONO_MODEM_COMMON_EVENT: {
		struct ofono_modem_common_info *modem_common_data;

		if (len != sizeof(struct ofono_modem_common_info)) {
			ofono_error("len mismatch %zu,%zu", len,
				    sizeof(struct ofono_modem_common_info));
			break;
		}
		modem_common_data = (struct ofono_modem_common_info *) covert_data;
		char common_info[MAX_STR_PARAM_LEN] = { '\0' };

		snprintf(common_info, MAX_STR_PARAM_LEN, "%u_%u_%u_%u", modem_common_data->id,
			 g_camped_cell_info.tac, g_camped_cell_info.plmn,
			 g_camped_cell_info.cell_id);
		ofono_debug("%s,modem common event id=%u", KEY_NAME, modem_common_data->id);
		OFONO_DFX_MODEM_COMMON_EVENT_INFO(
			common_info, modem_common_data->parm1, modem_common_data->parm2,
			modem_common_data->parm3, modem_common_data->parm4,
			modem_common_data->parm5);
		break;
	}
	default:
		ofono_debug("%s,unknow abnormal event", KEY_NAME);
		break;
	}
	l_free(covert_data);
}
