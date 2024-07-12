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

void ofono_handle_abnormal_event(int type_id, char *data, int data_len)
{
	size_t len;
	unsigned char *covert_data = NULL;

	ofono_debug("ofono_handle_abnormal_event,type_id=%d,data_len=%d",
		    type_id, data_len);

	covert_data = l_util_from_hexstring(data, &len);

	switch (type_id) {
	case OFONO_ABNORMAL_INSIDE_MODEM:
		ofono_debug("ofono_handle_abnormal_event,abnormal inside "
			    "modem");
		break;
	case OFONO_ABNORMAL_EF_FILE: {
		struct ofono_abnormal_ef_file *ef_file_data =
			( struct ofono_abnormal_ef_file * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,ef_id=%u,"
			    "sw1=%u,sw2=%u",
			    ef_file_data->sub, ef_file_data->ef_id,
			    ef_file_data->sw1, ef_file_data->sw2);
		break;
	}
	case OFONO_ABNORMAL_PROFILE: {
		struct ofono_abnormal_profile *profile_data =
			( struct ofono_abnormal_profile * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u",
			    profile_data->sub);
		break;
	}
	case OFONO_ABNORMAL_RLF: {
		struct ofono_abnormal_reest *reest_data =
			( struct ofono_abnormal_reest * ) covert_data;

		ofono_debug(
			"ofono_handle_abnormal_event,sub=%u,earfcn=%u,pci=%u,"
			"band=%u,rsrp=%d,rsrq=%d,sinr=%d,rssi=%d,"
			"reest_cause=%u",
			reest_data->sub, reest_data->earfcn, reest_data->pci,
			reest_data->band, reest_data->cell_quality.rsrp,
			reest_data->cell_quality.rsrq,
			reest_data->cell_quality.sinr,
			reest_data->cell_quality.rssi, reest_data->reest_cause);
		break;
	}
	case OFONO_ABNORMAL_RACH_ACCESS: {
		struct ofono_abnormal_rach_access *rach_access_data =
			( struct ofono_abnormal_rach_access * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,earfcn=%u,"
			    "pci=%u,fail_reason=%u",
			    rach_access_data->sub, rach_access_data->earfcn,
			    rach_access_data->pci,
			    rach_access_data->fail_reason);
		break;
	}
	case OFONO_ABNORMAL_OOS: {
		struct ofono_abnormal_oos *oos_data =
			( struct ofono_abnormal_oos * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,earfcn=%u,"
			    "pci=%u,oos_type=%u",
			    oos_data->sub, oos_data->earfcn, oos_data->pci,
			    oos_data->oos_type);
		break;
	}
	case OFONO_ABNORMAL_NAS_TIMEOUT: {
		struct ofono_abnormal_timer_exp *timer_exp_data =
			( struct ofono_abnormal_timer_exp * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,timer_id=%u",
			    timer_exp_data->sub, timer_exp_data->timer_id);
		if (timer_exp_data->cell_quality_exist) {
			ofono_debug("ofono_handle_abnormal_event,rsrp=%d,"
				    "rsrq=%d,sinr=%d,rssi=%d",
				    timer_exp_data->cell_quality.rsrp,
				    timer_exp_data->cell_quality.rsrq,
				    timer_exp_data->cell_quality.sinr,
				    timer_exp_data->cell_quality.rssi);
		}
		break;
	}
	case OFONO_ABNORMAL_SIP_TIMEOUT: {
		struct ofono_abnormal_sip_timeout *sip_timeout_data =
			( struct ofono_abnormal_sip_timeout * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,srv_type=%u,"
			    "sip_method=%u",
			    sip_timeout_data->sub, sip_timeout_data->srv_type,
			    sip_timeout_data->sip_method);
		break;
	}
	case OFONO_ABNORMAL_TIMEOUT_IN_RRC: {
		struct ofono_abnormal_timeout_in_rrc *sip_timeout_in_rrc_data =
			( struct ofono_abnormal_timeout_in_rrc * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,earfcn=%u,"
			    "pci=%u,timer=%u",
			    sip_timeout_in_rrc_data->sub,
			    sip_timeout_in_rrc_data->earfcn,
			    sip_timeout_in_rrc_data->pci,
			    sip_timeout_in_rrc_data->timer);
		break;
	}
	case OFONO_ABNORMAL_ECC_CALL_FAIL: {
		struct ofono_abnormal_ecc_call_fail *ecc_call_fail_data =
			( struct ofono_abnormal_ecc_call_fail * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,cause=%u",
			    ecc_call_fail_data->sub, ecc_call_fail_data->cause);
		break;
	}
	case OFONO_ABNORMAL_RTP_RTCP: {
		struct ofono_abnormal_rtp_rtcp *rtp_rtcp_data =
			( struct ofono_abnormal_rtp_rtcp * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,error_type=%u",
			    rtp_rtcp_data->sub, rtp_rtcp_data->error_type);
		break;
	}
	case OFONO_ABNORMAL_PAGING_DECODE: {
		struct ofono_abnormal_paging_decode *paging_decode_data =
			( struct ofono_abnormal_paging_decode * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,earfcn=%u,"
			    "pci=%u,rsrp=%d,rsrq=%d,sinr=%d,rssi=%d",
			    paging_decode_data->sub, paging_decode_data->earfcn,
			    paging_decode_data->pci,
			    paging_decode_data->cell_quality.rsrp,
			    paging_decode_data->cell_quality.rsrq,
			    paging_decode_data->cell_quality.sinr,
			    paging_decode_data->cell_quality.rssi);
		break;
	}
	case OFONO_ABNORMAL_CALL_QUALITY: {
		struct ofono_abnormal_call_quality *call_quality_data =
			( struct ofono_abnormal_call_quality * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,pkt_lost=%u,"
			    "fraction_lost=%u,jitter_buffer_size=%u",
			    call_quality_data->sub, call_quality_data->pkt_lost,
			    call_quality_data->fraction_lost,
			    call_quality_data->jitter_buffer_size);
		break;
	}
	case OFONO_ABNORMAL_PDCP: {
		struct ofono_abnormal_pdcp *pdcp_data =
			( struct ofono_abnormal_pdcp * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,"
			    "dl_loss_rate=%u,ul_loss_rate=%u,rsrp=%d,rsrq=%d,"
			    "sinr=%d,rssi=%d",
			    pdcp_data->sub, pdcp_data->dl_loss_rate,
			    pdcp_data->ul_loss_rate,
			    pdcp_data->cell_quality.rsrp,
			    pdcp_data->cell_quality.rsrq,
			    pdcp_data->cell_quality.sinr,
			    pdcp_data->cell_quality.rssi);
		break;
	}
	case OFONO_ABNORMAL_NAS_REJECT: {
		struct ofono_abnormal_nas_reject *nas_reject_data =
			( struct ofono_abnormal_nas_reject * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,"
			    "procedure_type=%u,reject_cause=%u",
			    nas_reject_data->sub,
			    nas_reject_data->procedure_type,
			    nas_reject_data->reject_cause);
		if (nas_reject_data->cell_quality_exist) {
			ofono_debug("ofono_handle_abnormal_event,rsrp=%d,"
				    "rsrq=%d,sinr=%d,rssi=%d",
				    nas_reject_data->cell_quality.rsrp,
				    nas_reject_data->cell_quality.rsrq,
				    nas_reject_data->cell_quality.sinr,
				    nas_reject_data->cell_quality.rssi);
		}
		break;
	}
	case OFONO_ABNORMAL_SIP_REJECT: {
		struct ofono_abnormal_sip_reject *sip_reject_data =
			( struct ofono_abnormal_sip_reject * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,srv_type=%u,"
			    "sip_method=%u,resp_code=%u",
			    sip_reject_data->sub, sip_reject_data->srv_type,
			    sip_reject_data->sip_method,
			    sip_reject_data->resp_code);
		break;
	}
	case OFONO_ABNORMAL_RRC_REJECT: {
		struct ofono_abnormal_rrc_reject *rrc_reject_data =
			( struct ofono_abnormal_rrc_reject * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,earfcn=%u,"
			    "pci=%u,error_scenario_id=%u",
			    rrc_reject_data->sub, rrc_reject_data->earfcn,
			    rrc_reject_data->pci,
			    rrc_reject_data->error_scenario_id);
		break;
	}
	case OFONO_ABNORMAL_PING_PONG: {
		struct ofono_abnormal_ping_pong *ping_pong_data =
			( struct ofono_abnormal_ping_pong * ) covert_data;

		ofono_debug(
			"ofono_handle_abnormal_event,pcell_pci=%u,"
			"pcell_earfcn=%u,pcell_rsrp=%u,ncell_pci=%u,"
			"ncell_earfcn=%u,ncell_rsrp=%u",
			ping_pong_data->pcell_pci, ping_pong_data->pcell_earfcn,
			ping_pong_data->pcell_rsrp, ping_pong_data->ncell_pci,
			ping_pong_data->ncell_earfcn,
			ping_pong_data->ncell_rsrp);
		break;
	}
	case OFONO_ABNORMAL_CC: {
		struct ofono_cc_abnormal_fail *cc_fail_data =
			( struct ofono_cc_abnormal_fail * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,cause=%u,"
			    "error_scenario_id=%u",
			    cc_fail_data->sub, cc_fail_data->cause,
			    cc_fail_data->error_scenario_id);
		break;
	}
	case OFONO_ABNORMAL_XCAP: {
		struct ofono_abnormal_xcap *xcap_data =
			( struct ofono_abnormal_xcap * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,mode=%u,"
			    "reason=%u,error_type=%u",
			    xcap_data->sub, xcap_data->mode, xcap_data->reason,
			    xcap_data->error_type);
		break;
	}
	case OFONO_ABNORMAL_DATA: {
		struct ofono_abnormal_data *data_data =
			( struct ofono_abnormal_data * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,event=%u,"
			    "rsrp=%d,rsrq=%d,sinr=%d,rssi=%d",
			    data_data->sub, data_data->event,
			    data_data->cell_quality.rsrp,
			    data_data->cell_quality.rsrq,
			    data_data->cell_quality.sinr,
			    data_data->cell_quality.rssi);

		OFONO_DFX_DATA_INTERRUPTION_INFO();
		break;
	}
	case OFONO_ABNORMAL_CALL_END_REASON_FROM_SIP: {
		struct ofono_abnormal_call_end_reason_from_sip
			*sip_end_reason_data =
				( struct ofono_abnormal_call_end_reason_from_sip
					  * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,reason_type=%u",
			    sip_end_reason_data->sub,
			    sip_end_reason_data->reason_type);
		break;
	}
	case OFONO_LIMITED_SERVICE_CAMP_EVENT: // 200
	{
		struct ofono_abnormal_limited_service *limited_service_data =
			( struct ofono_abnormal_limited_service * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,type=%u,"
			    "cause=%u",
			    limited_service_data->sub,
			    limited_service_data->type,
			    limited_service_data->cause);
		break;
	}
	case OFONO_REDIRECT_EVENT: {
		struct ofono_redirect_event *redirect_data =
			( struct ofono_redirect_event * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u",
			    redirect_data->sub);
		break;
	}
	case OFONO_HANDOVER_EVENT: {
		struct ofono_abnormal_handover_event *handover_data =
			( struct ofono_abnormal_handover_event * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u",
			    handover_data->sub);
		break;
	}
	case OFONO_RESELECT_EVENT: {
		struct ofono_abnormal_reselect_event *reselect_data =
			( struct ofono_abnormal_reselect_event * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u",
			    reselect_data->sub);
		break;
	}
	case OFONO_CSFB_EVENT: {
		struct ofono_csfb_event *csfb_data =
			( struct ofono_csfb_event * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u",
			    csfb_data->sub);
		break;
	}
	case OFONO_SRVCC_EVENT: {
		struct ofono_srvcc_event *srvcc_data =
			( struct ofono_srvcc_event * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u",
			    srvcc_data->sub);
		break;
	}
	case OFONO_UE_CAP_INFO: {
		struct ofono_ue_cap_info *ue_cap_data =
			( struct ofono_ue_cap_info * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u",
			    ue_cap_data->sub);
		for (int i = 0; i < ue_cap_data->support_band_num; i++) {
			ofono_debug("ofono_handle_abnormal_event,band[%d]=%u",
				    i, ue_cap_data->support_band_list[i]);
		}
		ofono_debug("ofono_handle_abnormal_event,category=%u",
			    ue_cap_data->category);
		break;
	}
	case OFONO_UE_CAMP_CELL_INFO: {
		struct ofono_ue_camp_cell_info *ue_camp_cell_data =
			( struct ofono_ue_camp_cell_info * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,plmn=%u,tac=%u,"
			    "cell_id=%u,band=%u,earfcn=%u,pci=%u",
			    ue_camp_cell_data->sub, ue_camp_cell_data->plmn,
			    ue_camp_cell_data->tac, ue_camp_cell_data->cell_id,
			    ue_camp_cell_data->band, ue_camp_cell_data->earfcn,
			    ue_camp_cell_data->pci);
		OFONO_DFX_BAND_INFO(ue_camp_cell_data->band);
		break;
	}
	case OFONO_UE_SIM_INFO: {
		struct ofono_ue_sim_info *ue_sim_data =
			( struct ofono_ue_sim_info * ) covert_data;

		ofono_debug("ofono_handle_abnormal_event,sub=%u,hplmn=%u",
			    ue_sim_data->sub, ue_sim_data->hplmn);
		for (int i = 0; i < ue_sim_data->ehplmn_num; i++) {
			ofono_debug("ofono_handle_abnormal_event,ehplmn[%d]=%u",
				    i, ue_sim_data->ehplmn[i]);
		}
		break;
	}
	default:
		ofono_debug("ofono_handle_abnormal_event,unknow abnormal "
			    "event");
		break;
	}
	l_free(covert_data);
}
