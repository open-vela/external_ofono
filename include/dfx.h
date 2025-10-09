#ifndef __OFONO_DFX_H
#define __OFONO_DFX_H

#include <nuttx/config.h>
#ifdef CONFIG_TELEPHONY_DFX
#include <dfx.h>
#endif
#include <syslog.h>

#define REPORTING_PERIOD 1000 * 60 * 60 * 5.5
#define NORMAL_REGISTER_DURATION 5
#define REASON_DESC_SIZE 20
#define MAX_MCC_LENGTH 3
#define MAX_MNC_LENGTH 3

#define LOG_IND_BUF_SIZE 350

#ifndef LOG_DEBUG
#define LOG_DEBUG 7
#endif

typedef enum {
	OFONO_NORMAL_CALL = 1,
	OFONO_EMERGENCY_CALL,
	OFONO_CONFERENCE_CALL,
	OFONO_CALL_TYPE_UNKNOW
} ofono_call_type;

typedef enum {
	OFONO_ORIGINATE = 1,
	OFONO_TERMINATE,
	OFONO_DIRECTION_UNKNOW
} ofono_call_direction;

typedef enum {
	OFONO_VOICE = 1,
	OFONO_VIDEO,
	OFONO_MEDIA_UNKNOW
} ofono_call_media;

typedef enum {
	OFONO_NORMAL = 0,
	OFONO_DIAL_FAIL,
	OFONO_ANSWER_FAIL,
	OFONO_HANGUP_FAIL,
	OFONO_ONGOING_FAIL,
	OFONO_CALL_UNKNOW_FAIL,
	OFONO_LISTEN_NORMAL
} ofono_call_scenario;

typedef enum {
	OFONO_CU = 1,
	OFONO_CMCC,
	OFONO_CT,
	OFONO_CBN,
	OFONO_OPERATOR_UNKNOW
} ofono_op_code;

typedef enum {
	OFONO_OTHER = 0,
	OFONO_2G,
	OFONO_3G,
	OFONO_4G
} ofono_rat_code;

typedef enum {
	OFONO_HONGKONG = 1,
	OFONO_MACAU,
	OFONO_COUNTRY_UNKNOW
} ofono_country_code;

typedef enum {
	OFONO_CS_SMS = 1,
	OFONO_IMS_SMS,
	OFONO_CBS_SMS,
	OFONO_SMS_TYPE_UNKNOW
} ofono_sms_type;

typedef enum {
	OFONO_SMS_SEND = 1,
	OFONO_SMS_RECEIVE
} ofono_sms_direction;

typedef enum {
	OFONO_SMS_NORMAL = 0,
	OFONO_SMS_FAIL
} ofono_sms_fail_scenario;

struct ofono_plmn_op_code {
	char mcc[MAX_MCC_LENGTH + 1];
	char mnc[MAX_MNC_LENGTH + 1];
	int op_code;
};

#ifdef CONFIG_TELEPHONY_DFX
#define OFONO_DFX_CALL_INFO(type, direction, media, fail_scenario, fail_reason)                    \
	do {                                                                                       \
		sendEventMisightF(961040001, "%s:%d,%s:%d,%s:%d,%s:%d,%s:%s", "call_type", type,   \
				  "direction", direction, "media", media, "fail_scenario",         \
				  fail_scenario, "fail_reason", fail_reason);                      \
	} while (0)

#define OFONO_DFX_SS_INFO(type, fail_reason, covered_plmn)                                         \
	do {                                                                                       \
		sendEventMisightF(961040401, "%s:%s,%s:%s,%s:%s", "ss_type", type, "fail_reason",  \
				  fail_reason, "covered_plmn", covered_plmn);                      \
	} while (0)

#define OFONO_DFX_CALL_TIME_INFO(level0_duration, level1_duration, level2_duration,                \
				 level3_duration, level4_duration, level5_duration)                \
	do {                                                                                       \
		sendEventMisightF(961040002, "%s:%d,%s:%d,%s:%d,%s:%d,%s:%d,%s:%d",                \
				  "level0_time_value", level0_duration, "level1_time_value",       \
				  level1_duration, "level2_time_value", level2_duration,           \
				  "level3_time_value", level3_duration, "level4_time_value",       \
				  level4_duration, "level5_time_value", level5_duration);          \
	} while (0)

#define OFONO_DFX_SMS_INFO(opcode, sms_type, direction, fail_flag, covered_plmn)                   \
	do {                                                                                       \
		sendEventMisightF(961040301, "%s:%d,%s:%d,%s:%d,%s:%d,%s:%s", "op_code", opcode,   \
				  "sms_type", sms_type, "direction", direction, "fail_flag",       \
				  fail_flag, "plmn", covered_plmn);                                \
	} while (0)

#define OFONO_DFX_DATA_INTERRUPTION_INFO()                                                         \
	do {                                                                                       \
		sendEventMisightF(961040201, "%s:%d", "data_interruption", 1);                     \
	} while (0)

#define OFONO_DFX_DATA_ACTIVE_FAIL(cause)                                                          \
	do {                                                                                       \
		sendEventMisightF(961040202, "%s:%s", "cause", cause);                             \
	} while (0)

#define OFONO_DFX_DATA_ACTIVE_DURATION(data_active_time)                                           \
	do {                                                                                       \
		sendEventMisightF(961040203, "%s:%d", "data_active_time", data_active_time);       \
	} while (0)

#define OFONO_DFX_OOS_INFO(network_type)                                                           \
	do {                                                                                       \
		sendEventMisightF(961040101, "%s:%s", "network_type", network_type);               \
	} while (0)

#define OFONO_DFX_OOS_DURATION_INFO(cs_oos, ps_oos)                                                \
	do {                                                                                       \
		sendEventMisightF(961040102, "%s:%d,%s:%d", "cs_oos", cs_oos, "ps_oos", ps_oos);   \
	} while (0)

#define OFONO_DFX_ROAMING_INFO(roaming_country_code, covered_plmn)                                 \
	do {                                                                                       \
		sendEventMisightF(961040103, "%s:%d,%s:%s", "roaming_country_code",                \
				  roaming_country_code, "plmn", covered_plmn);                     \
	} while (0)

#define OFONO_DFX_BAND_INFO(band, count)                                                           \
	do {                                                                                       \
		sendEventMisightF(961040104, "%s:%d,%s:%d", "band_value", band, "band_count",      \
				  count);                                                          \
	} while (0)

#define OFONO_DFX_SIGNAL_LEVEL_DURATION(level0_duration, level1_duration, level2_duration,         \
					level3_duration, level4_duration, level5_duration)         \
	do {                                                                                       \
		sendEventMisightF(961040105, "%s:%d,%s:%d,%s:%d,%s:%d,%s:%d,%s:%d", "level0_time", \
				  level0_duration, "level1_time", level1_duration, "level2_time",  \
				  level2_duration, "level3_time", level3_duration, "level4_time",  \
				  level4_duration, "level5_time", level5_duration);                \
	} while (0)

#define OFONO_DFX_RAT_DURATION(unknow_rat_duration, rat_2g_duration, rat_3g_duration,              \
			       rat_4g_duration)                                                    \
	do {                                                                                       \
		sendEventMisightF(961040106, "%s:%d,%s:%d,%s:%d", "2g_time", rat_2g_duration,      \
				  "3g_time", rat_3g_duration, "4g_time", rat_4g_duration);         \
	} while (0)

#define OFONO_DFX_IMS_DURATION(ims_duration)                                                       \
	do {                                                                                       \
		sendEventMisightF(961040107, "%s:%d", "volte_time", ims_duration);                 \
	} while (0)

#define OFONO_DFX_MODEM_DURATION_INFO(modem_deactive_duration, modem_active_duration)              \
	do {                                                                                       \
		sendEventMisightF(961040108, "%s:%d,%s:%d", "modem_on_time",                       \
				  modem_deactive_duration, "modem_off_time",                       \
				  modem_active_duration);                                          \
	} while (0)

#define OFONO_DFX_ABNORMAL_EVENT_INFO(parm1, parm2, parm3, parm4, parm5, parm6)                    \
	do {                                                                                       \
		sendEventMisightF(961040501,                                                       \
				  "%s:%s,%s:%s,%s:%s,%s:%s,%s:%s,"                                 \
				  "%s:%s",                                                         \
				  "parm1", parm1, "parm2", parm2, "parm3", parm3, "parm4", parm4,  \
				  "parm5", parm5, "parm6", parm6);                                 \
	} while (0)

#define OFONO_DFX_MODEM_COMMON_EVENT_INFO(parm1, parm2, parm3, parm4, parm5, parm6)                \
	do {                                                                                       \
		sendEventMisightF(961040502,                                                       \
				  "%s:%s,%s:%u,%s:%u,%s:%u,%s:%u,"                                 \
				  "%s:%u",                                                         \
				  "parm1", parm1, "parm2", parm2, "parm3", parm3, "parm4", parm4,  \
				  "parm5", parm5, "parm6", parm6);                                 \
	} while (0)

#define OFONO_DFX_NETWORK_SIGNAL_CHANGED_COUNT(signal_changed_count, network_state_changed_count)  \
	do {                                                                                       \
		sendEventMisightF(961040109, "%s:%d,%s:%d", "signal_changed_count",                \
				  signal_changed_count, "network_state_changed_count",             \
				  network_state_changed_count);                                    \
	} while (0)

#define OFONO_DFX_IMS_STATE_CHANGED_COUNT(ims_state_changed_count)                                 \
	do {                                                                                       \
		sendEventMisightF(961040110, "%s:%d", "ims_state_changed_count",                   \
				  ims_state_changed_count);                                        \
	} while (0)

#define OFONO_DFX_CELL_INFO_CHANGED_COUNT(cellinfo_changed_count)                                  \
	do {                                                                                       \
		sendEventMisightF(961040111, "%s:%d", "cellinfo_changed_count",                    \
				  cellinfo_changed_count);                                         \
	} while (0)

#else

#define REPORT_DATA_LOG(format, ...)                                                               \
	do {                                                                                       \
		char log_buf[LOG_IND_BUF_SIZE] = { 0 };                                            \
		snprintf(log_buf, sizeof(log_buf), format, __VA_ARGS__);                           \
		__ofono_manager_data_log(log_buf);                                                 \
	} while (0)

#define OFONO_DFX_CALL_INFO(type, direction, media, fail_scenario, fail_reason)                    \
	REPORT_DATA_LOG("%s,%d,%d,%d,%d,%s", "CALL_INFO", type, direction, media, fail_scenario,   \
			fail_reason)

#define OFONO_DFX_SS_INFO(type, fail_reason, covered_plmn)                                         \
	REPORT_DATA_LOG("%s,%s,%s,%s", "SS_INFO", type, fail_reason, covered_plmn)

#define OFONO_DFX_CALL_TIME_INFO(level0_duration, level1_duration, level2_duration,                \
				 level3_duration, level4_duration, level5_duration)                \
	REPORT_DATA_LOG("%s,%d,%d,%d,%d,%d,%d", "CALL_TIME_INFO", level0_duration,                 \
			level1_duration, level2_duration, level3_duration, level4_duration,        \
			level5_duration);

#define OFONO_DFX_SMS_INFO(opcode, sms_type, direction, fail_flag, covered_plmn)                   \
	REPORT_DATA_LOG("%s,%d,%d,%d,%d,%s", "SMS_INFO", opcode, sms_type, direction, fail_flag,   \
			covered_plmn)

#define OFONO_DFX_DATA_INTERRUPTION_INFO()                                                         \
	REPORT_DATA_LOG("%s,%s,%d", "DATA_INTERRUPTION_INFO", "915200014", 1)

#define OFONO_DFX_DATA_ACTIVE_FAIL(cause)                                                          \
	REPORT_DATA_LOG("%s,%s,%s", "DATA_ACTIVE_FAIL", "915000002", cause)

#define OFONO_DFX_DATA_ACTIVE_DURATION(data_active_time)                                           \
	REPORT_DATA_LOG("%s,%d", "DATA_ACTIVE_DURATION", data_active_time)

#define OFONO_DFX_OOS_INFO(network_type)                                                           \
	REPORT_DATA_LOG("%s,%s,%s", "OOS_INFO", "915300004", network_type)

#define OFONO_DFX_OOS_DURATION_INFO(cs_oos, ps_oos)                                                \
	REPORT_DATA_LOG("%s,%d,%d", "OOS_DURATION_INFO", cs_oos, ps_oos)

#define OFONO_DFX_ROAMING_INFO(roaming_country_code, covered_plmn)                                 \
	REPORT_DATA_LOG("%s,%d,%s", "ROAMING_INFO", roaming_country_code, covered_plmn)

#define OFONO_DFX_BAND_INFO(band, count) REPORT_DATA_LOG("%s,%d,%d", "BAND_INFO", band, count)

#define OFONO_DFX_SIGNAL_LEVEL_DURATION(level0_duration, level1_duration, level2_duration,         \
					level3_duration, level4_duration, level5_duration)         \
	REPORT_DATA_LOG("%s,%d,%d,%d,%d,%d,%d", "SIGNAL_LEVEL_DURATION", level0_duration,          \
			level1_duration, level2_duration, level3_duration, level4_duration,        \
			level5_duration);

#define OFONO_DFX_RAT_DURATION(unknow_rat_duration, rat_2g_duration, rat_3g_duration,              \
			       rat_4g_duration)                                                    \
	REPORT_DATA_LOG("%s,%d,%d,%d,%d", "RAT_DURATION", unknow_rat_duration, rat_2g_duration,    \
			rat_3g_duration, rat_4g_duration)

#define OFONO_DFX_IMS_DURATION(ims_duration) REPORT_DATA_LOG("%s,%d", "IMS_DURATION", ims_duration)

#define OFONO_DFX_MODEM_DURATION_INFO(modem_deactive_duration, modem_active_duration)              \
	REPORT_DATA_LOG("%s,%d,%d", "MODEM_DURATION_INFO", modem_deactive_duration,                \
			modem_active_duration)

#define OFONO_DFX_ABNORMAL_EVENT_INFO(parm1, parm2, parm3, parm4, parm5, parm6)                    \
	REPORT_DATA_LOG("%s,%s,%s,%s,%s,%s,%s", "ABNORMAL_EVENT", parm1, parm2, parm3, parm4,      \
			parm5, parm6)

#define OFONO_DFX_MODEM_COMMON_EVENT_INFO(parm1, parm2, parm3, parm4, parm5, parm6)                \
	REPORT_DATA_LOG("%s,%s,%u,%u,%u,%u,%u", "MODEM_COMMON_EVENT", parm1, parm2, parm3, parm4,  \
			parm5, parm6)

#define OFONO_DFX_NETWORK_SIGNAL_CHANGED_COUNT(signal_changed_count, network_state_changed_count)  \
	REPORT_DATA_LOG("%s,%d,%d", "NETWORK_SIGNAL_CHANGED_COUNT", signal_changed_count,          \
			network_state_changed_count)

#define OFONO_DFX_IMS_STATE_CHANGED_COUNT(ims_state_changed_count)                                 \
	REPORT_DATA_LOG("%s,%d", "IMS_STATE_CHANGED_COUNT", ims_state_changed_count)

#define OFONO_DFX_CELL_INFO_CHANGED_COUNT(cellinfo_changed_count)                                  \
	REPORT_DATA_LOG("%s,%d", "CELLINFO_CHANGED_COUNT", cellinfo_changed_count)
#endif

#define OFONO_DFX_CALL_INFO_IF(flag, type, direction, media, fail_scenario, fail_reason)           \
	do {                                                                                       \
		if (flag) {                                                                        \
			OFONO_DFX_CALL_INFO(type, direction, media, fail_scenario, fail_reason);   \
		}                                                                                  \
	} while (0)

void __ofono_manager_data_log(const char *data);
#endif
