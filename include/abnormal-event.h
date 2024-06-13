#ifndef __OFONO_ABNORMAL_EVENT_H
#define __OFONO_ABNORMAL_EVENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ofono/types.h>

enum ofono_abnormal_event {
	OFONO_ABNORMAL_INSIDE_MODEM = 1,
	OFONO_ABNORMAL_EF_FILE,
	OFONO_ABNORMAL_PROFILE,
	OFONO_ABNORMAL_RLF,
	OFONO_ABNORMAL_RACH_ACCESS,
	OFONO_ABNORMAL_OOS,
	OFONO_ABNORMAL_NAS_TIMEOUT,
	OFONO_ABNORMAL_SIP_TIMEOUT,
	OFONO_ABNORMAL_TIMEOUT_IN_RRC,
	OFONO_ABNORMAL_ECC_CALL_FAIL,
	OFONO_ABNORMAL_RTP_RTCP,
	OFONO_ABNORMAL_PAGING_DECODE,
	OFONO_ABNORMAL_CALL_QUALITY,
	OFONO_ABNORMAL_PDCP,
	OFONO_ABNORMAL_NAS_REJECT,
	OFONO_ABNORMAL_SIP_REJECT,
	OFONO_ABNORMAL_RRC_REJECT,
	OFONO_ABNORMAL_PING_PONG,
	OFONO_ABNORMAL_CC,
	OFONO_ABNORMAL_XCAP,
	OFONO_ABNORMAL_DATA,
	OFONO_ABNORMAL_CALL_END_REASON_FROM_SIP,

	OFONO_LIMITED_SERVICE_CAMP_EVENT = 200,
	OFONO_REDIRECT_EVENT,
	OFONO_HANDOVER_EVENT,
	OFONO_RESELECT_EVENT,
	OFONO_CSFB_EVENT,
	OFONO_SRVCC_EVENT,
	OFONO_UE_CAP_INFO,
	OFONO_UE_CAMP_CELL_INFO,
	OFONO_UE_SIM_INFO,
};

#pragma pack(1)
struct ofono_cell_quality {
	//0X7FFF will be reported if param is invalid
	int16_t rsrp;
	int16_t rsrq;
	int16_t sinr;
	int16_t rssi;
};

//ABNORMAL_EF_FILE=2
struct ofono_abnormal_ef_file {
	unsigned int sub;
	uint16_t ef_id;//ref TS 31.102
	uint8_t  sw1;//sw1 sw2 ref TS 31.102
	uint8_t  sw2;
};

//ABNORMAL_PROFILE=3
struct ofono_abnormal_profile {
	unsigned int sub;
};

//ABNORMAL_RLF=4
struct ofono_abnormal_reest {
	unsigned int sub;
	unsigned int earfcn;
	unsigned int pci;
	unsigned int band;
	struct ofono_cell_quality cell_quality;
	/*
	0:RECFG_FAILURE
	1:HO_FAILURE
	2:T310 timeout
	3:RACH_PROBLEM
	4:MAX_RETRX
	5:IP_CHECK_FAILURE
	6:SIB_READ_FAILURE
	7:SMC_FAILURE
	8:CFG_L2_FAILURE
	9:OTHER_FAILURE
	*/
	unsigned int reest_cause;
};

//ABNORMAL_RACH_ACCESS=5
struct ofono_abnormal_rach_access {
	unsigned int sub;
	unsigned int earfcn;
	unsigned int pci;
	/*
	0：RA_FAIL_CAUSE_NOMSG2
	1: RA_FAIL_CAUSE_NOMSG4
	2: RA_FAIL_CAUSE_NORARESOURCE
	*/
	unsigned int fail_reason;
};

//ABNORMAL_OOS=6
struct ofono_abnormal_oos {
	unsigned int sub;
	unsigned int earfcn;
	unsigned int pci;
	/*
	0:OOS_TYPE_S_CRIT_FAIL
	1:OOS_TYPE_RESYNC_FAIL
	2:OOS_TYPE_RESEL_FAIL
	3:OOS_TYPE_L1_ABN_IND
	4:OOS_TYPE_MORMAL_TO_OOS
	5:OOS_TYPE_OOS_DIRECTLY
	*/
	unsigned int oos_type;
};

//ABNORMAL_NAS_TIMEOUT=7
struct ofono_abnormal_timer_exp {
	unsigned int sub;
	/*
	0: EMM_T3402
	1: EMM_T3410
	2: EMM_T3411
	3: EMM_T3412
	4: EMM_T3417
	5: EMM_T3421
	6: EMM_T3430
	7: EMM_T3440
	100: ESM_T3480
	101: ESM_T3481
	102: ESM_T3482
	103: ESM_T3492
	*/
	unsigned int timer_id;
	unsigned int cell_quality_exist;
	struct ofono_cell_quality cell_quality;
};

//ABNORMAL_SIP_TIMEOUT=8
struct ofono_abnormal_sip_timeout {
	unsigned int sub;
	/*
	0：DEBUG_SRV_REGISTATION
	1：DEBUG_SRV_CALL
	2：DEBUG_SRV_EMG_CALL
	3：DEBUG_SRV_SMS
	4：DEBUG_SRV_MPTY
	5：DEBUG_SRV_USSI
	*/
	unsigned int srv_type;
	/*
	0：DEBUG_SIP_REGISTER
	1：DEBUG_SIP_SUBSCRIBE
	2：DEBUG_SIP_INVITE
	3：DEBUG_SIP_RE_INVITE
	4：DEBUG_SIP_PRACK
	5：DEBUG_SIP_UPDATE
	6：DEBUG_SIP_MESSAGE
	7：DEBUG_SIP_REFER
	8：DEBUG_SIP_INFO
	*/
	unsigned int sip_method;
};

//ABNORMAL_TIMEOUT_IN_RRC=9
struct ofono_abnormal_timeout_in_rrc {
	unsigned int sub;
	unsigned int earfcn;
	unsigned int pci;
	/*
	0:ERRC_T300_EST_FAIL
	1:ERRC_T301_REEST_FAIL
	2:ERRC_T304_HO_FAIL
	3.ERRC_T310_RADIO_LINK_FAIL
	4:ERRC_T311_REEST_CELL_SELECT_FAIL
	*/
	unsigned int timer;
};

//ABNORMAL_ECC_CALL_FAIL=10
struct ofono_abnormal_ecc_call_fail {
	unsigned int sub;
	/*
	cause:
	0：Other
	1: Lost covery
	2: Emergency Bearer not support by NW
	3: Emergency Bearer Establish failure
	*/
	unsigned int cause;
};

//ABNORMAL_RTP_RTCP=11
struct ofono_abnormal_rtp_rtcp {
	unsigned int sub;
	/*
	0：DEBUG_DL_RTP_TIMEOUT
	1：DEBUG_DL_RTCP_TIMEOUT
	2：DEBUG_MV_UDP_SOCKET_ERROR
	*/
	unsigned int error_type;
};

//ABNORMAL_PAGING_DECODE=12
struct ofono_abnormal_paging_decode {
	unsigned int sub;
	unsigned int earfcn;
	unsigned int pci;
	struct ofono_cell_quality cell_quality;
};

//ABNORMAL_CALL_QUALITY=13
struct ofono_abnormal_call_quality {
	unsigned int sub;
	unsigned int pkt_lost;
	unsigned int fraction_lost;//RTP downlink packet loss rate (threshold:50%)
	unsigned int jitter_buffer_size;//Jitter buffer size, indicating the delay situation
};

//ABNORMAL_PDCP=14
struct ofono_abnormal_pdcp {
	unsigned int sub;
	unsigned int dl_loss_rate;//PDCP downlink packet loss rate(threshold:20%)
	unsigned int ul_loss_rate;//PDCP uplink packet loss rate(threshold:20%)
	struct ofono_cell_quality cell_quality;
};

//ABNORMAL_NAS_REJECT=15
struct ofono_abnormal_nas_reject {
	unsigned int sub;
	/*
	0: EPS_ATTACH_REJ
	1: EPS_TAU_REJ
	2: EPS_SR_REJ
	3: EPS_IDENTITY
	4: EPS_SMC_REJ
	5: EPS_AUTH_REJ
	6: EPS_MT_DETACH
	100: EPS_ESM_PDN_CONN_REJECT
	101: EPS_ESM_BEARER_MT_DEACT
	*/
	unsigned int procedure_type;//ref 24.301
	/*
	1. ATTACH/TAU/SR/MT Detach, reject_cause ref 24301
	2. EPS_IDENTITY，reject_cause
	3. SMC ref 24301
	4. EPS_AUTH_REJ
	5. ESM,reject_cause ref 24301
	6. custom Reject cause,start from 1000
	*/
	unsigned int reject_cause;
	/*
	0: Reject mesage Not Integrity Protected
	1: Reject mesage Integrity Protected
	*/
	unsigned int is_integrity;
	unsigned int cell_quality_exist;
	struct ofono_cell_quality cell_quality;
};

//ABNORMAL_SIP_REJECT=16
struct ofono_abnormal_sip_reject {
	unsigned int sub;
	/*
	0：DEBUG_SRV_REGISTATION
	1：DEBUG_SRV_CALL
	2：DEBUG_SRV_EMG_CALL
	3：DEBUG_SRV_SMS
	4：DEBUG_SRV_MPTY
	5：DEBUG_SRV_USSI
	*/
	unsigned int srv_type;
	/*
	0：DEBUG_SIP_REGISTER
	1：DEBUG_SIP_SUBSCRIBE
	2：DEBUG_SIP_INVITE
	3：DEBUG_SIP_RE_INVITE
	4：DEBUG_SIP_PRACK
	5：DEBUG_SIP_UPDATE
	6：DEBUG_SIP_MESSAGE
	7：DEBUG_SIP_REFER
	8：DEBUG_SIP_INFO
	*/
	unsigned int sip_method;
	unsigned int resp_code;
};

//ABNORMAL_RRC_REJECT=17
struct ofono_abnormal_rrc_reject {
	unsigned int sub;//0,1
	unsigned int earfcn;
	unsigned int pci;
	/*
	0:RRC_CONNECTION_REESTABLISHMENT_REJECT
	1:RRC_CONNECTION_REJECT
	*/
	unsigned int error_scenario_id;
};

//ABNORMAL_PING_PONG=18
struct ofono_abnormal_ping_pong {
	unsigned int sub;
	unsigned int pcell_pci;
	unsigned int pcell_earfcn;
	unsigned int pcell_rsrp;
	unsigned int ncell_pci;
	unsigned int ncell_earfcn;
	unsigned int ncell_rsrp;
};

//ABNORMAL_CC=19
struct ofono_cc_abnormal_fail {
      unsigned int sub;
      unsigned int cause;//ref 24.008
      unsigned int error_scenario_id;
    /*0:internal error event*/
    /*1:network send disconnect/release*/
};

//ABNORMAL_XCAP=20
struct ofono_abnormal_xcap {
	unsigned int sub;
	/*
	0：DEBUG_XCAP_MODE_DISABLE
	1：DEBUG_XCAP_MODE_ENABLE
	2：DEBUG_XCAP_MODE_QUERY
	3：DEBUG_XCAP_MODE_REGISTRATION
	4：DEBUG_XCAP_MODE_ERASURE
	*/
	unsigned int mode;
	/*
	0：DEBUG_XCAP_REASON_CDIV_ALL
	1：DEBUG_XCAP_REASON_CDIV_CONDS
	2：DEBUG_XCAP_REASON_CDIV_CFU
	3：DEBUG_XCAP_REASON_CDIV_CFB
	4：DEBUG_XCAP_REASON_CDIV_CFNR
	5：DEBUG_XCAP_REASON_CDIV_CFNR_TMR
	6：DEBUG_XCAP_REASON_CDIV_CFNRC
	7：DEBUG_XCAP_REASON_CDIV_CFNl
	8：DEBUG_XCAP_REASON_CB_ICB_ALL
	9：DEBUG_XCAP_REASON_CB_ICB_BAIC
	10：DEBUG_XCAP_REASON_CB_ICB_BICROAM
	11：DEBUG_XCAP_REASON_CB_ICB_ACR
	12：DEBUG_XCAP_REASON_CB_OCB_ALL
	13：DEBUG_XCAP_REASON_CB_OCB_BAOC
	14：DEBUG_XCAP_REASON_CB_OCB_BOCROAM
	15：DEBUG_XCAP_REASON_CB_OCB_BOIC
	16：DEBUG_XCAP_REASON_CB_OCB_BOICEXHC
	17：DEBUG_XCAP_REASON_CW
	18：DEBUG_XCAP_REASON_OIP_CLIP
	19：DEBUG_XCAP_REASON_OIR_CLIR
	20：DEBUG_XCAP_REASON_TIP_COLP
	21：DEBUG_XCAP_REASON_TIR_COLR
	*/
	unsigned int reason;
	/*
	0：DEBUG_XCAP_NET_ERROR
	1: DEBUG_XCAP_HTTP_ERROR
	2: DEBUG_XCAP_HTTP_TIMEOUT
	3: DEBUG_XCAP_GBA_ERROR
	4: DEBUG_XCAP_NO_DNS_RESULT
	5: DEBUG_XCAP_DNS_TIMEOUT
	6: DEBUG_XCAP_NO_FUNCTION
	7: DEBUG_XCAP_OTHER_ERROR
	*/
	unsigned int error_type;
};

//ABNORMAL_DATA=21
struct ofono_abnormal_data {
	unsigned int sub;
	unsigned int event;
	/*
	pdcp rate every 2s,threshold:0
	0: DEBUG_UL_DATA_INTERRUPTION
	1: DEBUG_DL_DATA_INTERRUPTION
	*/
	struct ofono_cell_quality cell_quality;
};

//OFONO_ABNORMAL_CALL_END_REASON_FROM_SIP=22
struct ofono_abnormal_call_end_reason_from_sip {
	unsigned int sub;
	/*
	2:REASON_RTP_RTCP_TIMEOUT
	3:REASON_MEDIA_BEARER_LOSS
	4:REASON_SIP_TIMEOUT_NO_ACK
	5:REASON_SIP_RESP_TIMEOUT
	6:REASON_CALL_SETUP_TIMEOUT
	7:REASON_REDIRECTION_FAILURE
	*/
	unsigned int reason_type;
};

//LIMITED_SERVICE_CAMP_EVENT=200
struct ofono_abnormal_limited_service {
	unsigned int sub;
	/*
	0: NO SERVICE
	1: NORMAL SERVICE
	2: LIMITED SERVICE
	*/
	unsigned int type;
	/*
	0: Reseved
	1: No suitable cell
	2: No SIM Insert
	3: No Cell
	*/
	unsigned int cause;
};

//REDIRECT_EVENT=201
struct ofono_redirect_event {
	unsigned int sub;
};

//HANDOVER_EVENT=202
struct ofono_abnormal_handover_event {
	unsigned int sub;
};

//RESELECT_EVENT=203
struct ofono_abnormal_reselect_event {
	unsigned int sub;
};

//CSFB_EVENT=204
struct ofono_csfb_event {
	unsigned int sub;
};

//SRVCC_EVENT=205
struct ofono_srvcc_event {
	unsigned int sub;
};

//UE_CAP_INFO =206
struct ofono_ue_cap_info {
	unsigned int sub;
	unsigned int support_band_num;
	unsigned int support_band_list[25];
	/*INTEGER (1..12)*/
	unsigned int category;
};

//UE_CAMP_CELL_INFO=207
struct ofono_ue_camp_cell_info {
	unsigned int sub;
	unsigned int plmn;
	unsigned int tac;
	unsigned int cell_id;
	unsigned int band;
	unsigned int earfcn;
	unsigned int pci;
};

//UE_SIM_INFO=208
struct ofono_ue_sim_info {
	unsigned int sub;
	unsigned int hplmn;
	unsigned int ehplmn_num;
	unsigned int ehplmn[16];
};
#pragma pack()

void ofono_handle_abnormal_event(int type_id, char* data, int data_len);

#ifdef __cplusplus
}
#endif
#endif
