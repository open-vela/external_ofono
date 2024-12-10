#ifndef __OFONO_DFX_H
#define __OFONO_DFX_H

#include <nuttx/config.h>
#if defined(CONFIG_DFX) && defined(CONFIG_DFX_EVENT)
#include <dfx_debug.h>
#include <dfx_event.h>
#endif
#include <syslog.h>

#define REPORTING_PERIOD 1000 * 60 * 60 * 5.5
#define NORMAL_REGISTER_DURATION 5
#define REASON_DESC_SIZE 20
#define MAX_MCC_LENGTH 3
#define MAX_MNC_LENGTH 3

#define MIWEAR_LOG_IND_BUF_SIZE 200

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

#if defined(CONFIG_DFX) && defined(CONFIG_DFX_EVENT)
#define OFONO_DFX_CALL_INFO(type, direction, media, fail_scenario, fail_reason)                    \
	do {                                                                                       \
		syslog(LOG_DEBUG, "OFONO_DFX_CALL_INFO:%d,%d,%d,%d,%s", type, direction, media,    \
		       fail_scenario, fail_reason);                                                \
		sendEventMisightF(915200010, "%s:%d,%s:%d,%s:%d,%s:%d,%s:%s", "call_type", type,   \
				  "direction", direction, "media", media, "fail_scenario",         \
				  fail_scenario, "fail_reason", fail_reason);                      \
	} while (0)

#define OFONO_DFX_SS_INFO(type, fail_reason)                                                       \
	do {                                                                                       \
		syslog(LOG_DEBUG, "OFONO_DFX_SS_INFO:%s,%s", type, fail_reason);                   \
		sendEventMisightF(915200011, "%s:%s,%s:%s", "ss_type", type, "fail_reason",        \
				  fail_reason);                                                    \
	} while (0)

#define OFONO_DFX_CALL_TIME_INFO(level0_duration, level1_duration, level2_duration,                \
				 level3_duration, level4_duration, level5_duration)                \
	do {                                                                                       \
		syslog(LOG_DEBUG, "OFONO_DFX_CALL_TIME:%d,%d,%d,%d,%d,%d", level0_duration,        \
		       level1_duration, level2_duration, level3_duration, level4_duration,         \
		       level5_duration);                                                           \
		sendEventMisightF(915200012, "%s:%d,%s:%d,%s:%d,%s:%d,%s:%d,%s:%d",                \
				  "level0_time_value", level0_duration, "level1_time_value",       \
				  level1_duration, "level2_time_value", level2_duration,           \
				  "level3_time_value", level3_duration, "level4_time_value",       \
				  level4_duration, "level5_time_value", level5_duration);          \
	} while (0)

#define OFONO_DFX_SMS_INFO(opcode, sms_type, direction, fail_flag, covered_plmn)                   \
	do {                                                                                       \
		syslog(LOG_DEBUG, "OFONO_DFX_SMS:%d,%d,%d,%d,%s", opcode, sms_type, direction,     \
		       fail_flag, covered_plmn);                                                   \
		sendEventMisightF(915200013, "%s:%d,%s:%d,%s:%d,%s:%d,%s:%s", "op_code", opcode,   \
				  "sms_type", sms_type, "direction", direction, "fail_flag",       \
				  fail_flag, "plmn", covered_plmn);                                \
	} while (0)

#define OFONO_DFX_DATA_INTERRUPTION_INFO()                                                         \
	do {                                                                                       \
		syslog(LOG_DEBUG, "OFONO_DFX:DATA_INTERRUPTION");                                  \
		sendEventMisightF(915200014, "%s:%d", "data_interruption", 1);                     \
	} while (0)

#define OFONO_DFX_DATA_ACTIVE_FAIL(cause)                                                          \
	do {                                                                                       \
		syslog(LOG_DEBUG, "OFONO_DFX:DATA_ACTIVE_FAIL:%s", cause);                         \
		sendEventMisightF(915000002, "%s:%s", "cause", cause);                             \
	} while (0)

#define OFONO_DFX_DATA_ACTIVE_DURATION(data_active_time)                                           \
	do {                                                                                       \
		syslog(LOG_DEBUG, "OFONO_DFX:DATA_ACTIVE_TIME:%d", data_active_time);              \
		sendEventMisightF(915200015, "%s:%d", "data_active_time", data_active_time);       \
	} while (0)

#define OFONO_DFX_OOS_INFO()                                                                       \
	do {                                                                                       \
		syslog(LOG_DEBUG, "OFONO_DFX:OOS_INFO");                                           \
		sendEventMisightF(915300004, "%s:%d", "oosSubId", 0);                              \
	} while (0)

#define OFONO_DFX_OOS_DURATION_INFO(oos_duration)                                                  \
	do {                                                                                       \
		syslog(LOG_DEBUG, "OFONO_DFX:OOS_DURATION_INFO:%d", oos_duration);                 \
		sendEventMisightF(915300005, "%s:%d", "oos_time", oos_duration);                   \
	} while (0)

#define OFONO_DFX_ROAMING_INFO(roaming_country_code)                                               \
	do {                                                                                       \
		syslog(LOG_DEBUG, "OFONO_DFX_ROAMING:%d", roaming_country_code);                   \
		sendEventMisightF(915300006, "%s:%d", "roaming_country_code",                      \
				  roaming_country_code);                                           \
	} while (0)

#define OFONO_DFX_BAND_INFO(band)                                                                  \
	do {                                                                                       \
		syslog(LOG_DEBUG, "OFONO_DFX_BAND_INFO:%d", band);                                 \
		sendEventMisightF(915300007, "%s:%d", "band_value", band);                         \
	} while (0)

#define OFONO_DFX_SIGNAL_LEVEL_DURATION(level0_duration, level1_duration, level2_duration,         \
					level3_duration, level4_duration, level5_duration)         \
	do {                                                                                       \
		syslog(LOG_DEBUG, "OFONO_DFX_SIGNAL_LEVEL:%d,%d,%d,%d,%d,%d", level0_duration,     \
		       level1_duration, level2_duration, level3_duration, level4_duration,         \
		       level5_duration);                                                           \
		sendEventMisightF(915200008, "%s:%d,%s:%d,%s:%d,%s:%d,%s:%d,%s:%d", "level0_time", \
				  level0_duration, "level1_time", level1_duration, "level2_time",  \
				  level2_duration, "level3_time", level3_duration, "level4_time",  \
				  level4_duration, "level5_time", level5_duration);                \
	} while (0)

#define OFONO_DFX_RAT_DURATION(unknow_rat_duration, rat_2g_duration, rat_3g_duration,              \
			       rat_4g_duration)                                                    \
	do {                                                                                       \
		syslog(LOG_DEBUG, "OFONO_DFX_RAT:%d,%d,%d,%d", unknow_rat_duration,                \
		       rat_2g_duration, rat_3g_duration, rat_4g_duration);                         \
		sendEventMisightF(915200009, "%s:%d,%s:%d,%s:%d", "2g_time", rat_2g_duration,      \
				  "3g_time", rat_3g_duration, "4g_time", rat_4g_duration);         \
	} while (0)

#define OFONO_DFX_IMS_DURATION(ims_duration)                                                       \
	do {                                                                                       \
		syslog(LOG_DEBUG, "OFONO_DFX_IMS:%d", ims_duration);                               \
		sendEventMisightF(915200010, "%s:%d", "volte_time", ims_duration);                 \
	} while (0)

#define OFONO_DFX_MODEM_DURATION_INFO(modem_deactive_duration, modem_active_duration)              \
	do {                                                                                       \
		syslog(LOG_DEBUG, "OFONO_DFX_MODEM:%d,%d", modem_deactive_duration,                \
		       modem_active_duration);                                                     \
		sendEventMisightF(915200011, "%s:%d,%s:%d", "modem_on_time",                       \
				  modem_deactive_duration, "modem_off_time",                       \
				  modem_active_duration);                                          \
	} while (0)

#elif defined(CONFIG_OFONO_DATA_LOG_OVER_MIWEAR)

#define REPORT_DATA_LOG(format, ...)                                                               \
	do {                                                                                       \
		char miwear_buf[MIWEAR_LOG_IND_BUF_SIZE];                                          \
		memset(miwear_buf, 0, sizeof(miwear_buf));                                         \
		sprintf(miwear_buf, format, __VA_ARGS__);                                          \
		__ofono_manager_data_log(miwear_buf);                                              \
	} while (0)

#define OFONO_DFX_CALL_INFO(type, direction, media, fail_scenario, fail_reason)                    \
	REPORT_DATA_LOG("%s,%d,%d,%d,%d,%s", "CALL_INFO", type, direction, media, fail_scenario,   \
			fail_reason)

#define OFONO_DFX_SS_INFO(type, fail_reason)                                                       \
	REPORT_DATA_LOG("%s,%s,%s", "SS_INFO", type, fail_reason)

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

#define OFONO_DFX_OOS_INFO() REPORT_DATA_LOG("%s,%s,%d", "OOS_INFO", "915300004", 0)

#define OFONO_DFX_OOS_DURATION_INFO(oos_duration)                                                  \
	REPORT_DATA_LOG("%s,%d", "OOS_DURATION_INFO", oos_duration)

#define OFONO_DFX_ROAMING_INFO(roaming_country_code)                                               \
	REPORT_DATA_LOG("%s,%d", "ROAMING_INFO", roaming_country_code)

#define OFONO_DFX_BAND_INFO(band) REPORT_DATA_LOG("%s,%d", "BAND_INFO", band)

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

#else

#define OFONO_DFX_CALL_INFO(type, direction, media, fail_scenario, fail_reason)                    \
	syslog(LOG_DEBUG, "OFONO_DFX_CALL_INFO:%d,%d,%d,%d,%s", type, direction, media,            \
	       fail_scenario, fail_reason)

#define OFONO_DFX_SS_INFO(type, fail_reason)                                                       \
	syslog(LOG_DEBUG, "OFONO_DFX_SS_INFO:%s,%s", type, fail_reason)

#define OFONO_DFX_CALL_TIME_INFO(level0_duration, level1_duration, level2_duration,                \
				 level3_duration, level4_duration, level5_duration)                \
	syslog(LOG_DEBUG, "OFONO_DFX_CALL_TIME:%d,%d,%d,%d,%d,%d", level0_duration,                \
	       level1_duration, level2_duration, level3_duration, level4_duration,                 \
	       level5_duration);

#define OFONO_DFX_SMS_INFO(opcode, sms_type, direction, fail_flag, covered_plmn)                   \
	syslog(LOG_DEBUG, "OFONO_DFX_SMS:%d,%d,%d,%d,%s", opcode, sms_type, direction, fail_flag,  \
	       covered_plmn)

#define OFONO_DFX_DATA_INTERRUPTION_INFO() syslog(LOG_DEBUG, "OFONO_DFX:DATA_INTERRUPTION")

#define OFONO_DFX_DATA_ACTIVE_FAIL(cause) syslog(LOG_DEBUG, "OFONO_DFX:DATA_ACTIVE_FAIL:%s", cause)

#define OFONO_DFX_DATA_ACTIVE_DURATION(data_active_time)                                           \
	syslog(LOG_DEBUG, "OFONO_DFX:DATA_ACTIVE_TIME:%d", data_active_time)

#define OFONO_DFX_OOS_INFO() syslog(LOG_DEBUG, "OFONO_DFX:OOS_INFO")

#define OFONO_DFX_OOS_DURATION_INFO(oos_duration)                                                  \
	syslog(LOG_DEBUG, "OFONO_DFX:OOS_DURATION_INFO:%d", oos_duration)

#define OFONO_DFX_ROAMING_INFO(roaming_country_code)                                               \
	syslog(LOG_DEBUG, "OFONO_DFX_ROAMING:%d", roaming_country_code)

#define OFONO_DFX_BAND_INFO(band) syslog(LOG_DEBUG, "OFONO_DFX_BAND:%d", band)

#define OFONO_DFX_SIGNAL_LEVEL_DURATION(level0_duration, level1_duration, level2_duration,         \
					level3_duration, level4_duration, level5_duration)         \
	syslog(LOG_DEBUG, "OFONO_DFX_SIGNAL:%d,%d,%d,%d,%d,%d", level0_duration, level1_duration,  \
	       level2_duration, level3_duration, level4_duration, level5_duration);

#define OFONO_DFX_RAT_DURATION(unknow_rat_duration, rat_2g_duration, rat_3g_duration,              \
			       rat_4g_duration)                                                    \
	syslog(LOG_DEBUG, "OFONO_DFX_RAT:%d,%d,%d,%d", unknow_rat_duration, rat_2g_duration,       \
	       rat_3g_duration, rat_4g_duration)

#define OFONO_DFX_IMS_DURATION(ims_duration) syslog(LOG_DEBUG, "OFONO_DFX_IMS:%d", ims_duration)

#define OFONO_DFX_MODEM_DURATION_INFO(modem_deactive_duration, modem_active_duration)              \
	syslog(LOG_DEBUG, "OFONO_DFX_MODEM:%d,%d", modem_deactive_duration, modem_active_duration)

#endif

#define OFONO_DFX_CALL_INFO_IF(flag, type, direction, media, fail_scenario, fail_reason)           \
	do {                                                                                       \
		if (flag) {                                                                        \
			OFONO_DFX_CALL_INFO(type, direction, media, fail_scenario, fail_reason);   \
		}                                                                                  \
	} while (0)

void __ofono_manager_data_log(char *data);
#endif
