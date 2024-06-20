#ifndef __OFONO_DFX_H
#define __OFONO_DFX_H

#if defined(CONFIG_DFX) && defined(CONFIG_DFX_EVENT)
#include <dfx_debug.h>
#include <dfx_event.h>
#endif
#include <ofono/log.h>

#define REPORTING_PERIOD 1000*60*60*24
#define REASON_DESC_SIZE 20

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
	OFONO_CALL_UNKNOW_FAIL
} ofono_call_fail_scenario;

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

#if defined(CONFIG_DFX) && defined(CONFIG_DFX_EVENT)
#define OFONO_DFX_CALL_INFO(type, direction, media, fail_scenario, fail_reason)                \
	do {                                                                                   \
		ofono_debug("OFONO_DFX_CALL_INFO:%d,%d,%d,%d,%s", type, direction, media,      \
				fail_scenario, fail_reason);                                   \
		sendEventMisightF(915200010, "%s:%d,%s:%d,%s:%d,%s:%d,%s:%s",                  \
					"call_type", type, "direction", direction,             \
					"media", media, "fail_scenario", fail_scenario,        \
					"fail_reason", fail_reason);                           \
	} while (0)

#define OFONO_DFX_SS_INFO(type, fail_reason)                                                   \
	do {                                                                                   \
		ofono_debug("OFONO_DFX_SS_INFO:%s,%s", type, fail_reason);                     \
		sendEventMisightF(915200011, "%s:%s,%s:%s", "ss_type", type,                   \
				"fail_reason", fail_reason);                                   \
	} while (0)

#define OFONO_DFX_CALL_TIME_INFO(level0_duration, level1_duration,                             \
		level2_duration, level3_duration, level4_duration, level5_duration)            \
	do {                                                                                   \
		ofono_debug("OFONO_DFX_CALL_TIME:%d,%d,%d,%d,%d,%d", level0_duration,          \
				level1_duration, level2_duration, level3_duration,             \
				level4_duration, level5_duration);                             \
		sendEventMisightF(915200012, "%s:%d,%s:%d,%s:%d,%s:%d,%s:%d,%s:%d",            \
				"level0_time_value", level0_duration,                          \
				"level1_time_value", level1_duration,                          \
				"level2_time_value", level2_duration,                          \
				"level3_time_value", level3_duration,                          \
				"level4_time_value", level4_duration,                          \
				"level5_time_value", level5_duration);                         \
	} while (0)

#define OFONO_DFX_SMS_INFO(opcode, sms_type, direction, fail_flag)                             \
	do {                                                                                   \
		ofono_debug("OFONO_DFX_SMS:%d,%d,%d,%d", opcode, sms_type,                     \
				direction, fail_flag);                                         \
		sendEventMisightF(915200013, "%s:%d,%s:%d,%s:%d,%s:%d", "op_code", opcode,     \
				"sms_type", sms_type, "direction", direction,                  \
				"fail_flag", fail_flag);                                       \
	} while (0)

#define OFONO_DFX_DATA_INTERRUPTION_INFO()                                                     \
	do {                                                                                   \
		ofono_debug("OFONO_DFX:DATA_INTERRUPTION");                                    \
		sendEventMisightF(915200014, "%s:%d", "data_interruption", 1);                 \
	} while (0)

#define OFONO_DFX_DATA_ACTIVE_FAIL(cause)                                                      \
	do {                                                                                   \
		ofono_debug("OFONO_DFX:DATA_ACTIVE_FAIL:%s", cause);                           \
		sendEventMisightF(915000002, "%s:%s", "cause", cause);                         \
	} while (0)

#define OFONO_DFX_DATA_ACTIVE_DURATION(data_active_time)                                       \
	do {                                                                                   \
		ofono_debug("OFONO_DFX:DATA_ACTIVE_TIME:%d", data_active_time);                \
		sendEventMisightF(915200015, "%s:%d", "data_active_time", data_active_time);   \
	} while (0)

#define OFONO_DFX_OOS_INFO()                                                                   \
	do {                                                                                   \
		ofono_debug("OFONO_DFX:OOS_INFO");                                             \
		sendEventMisightF(915300004, "%s:%d", "oosSubId", 0);                          \
	} while (0)

#define OFONO_DFX_OOS_DURATION_INFO(oos_duration)                                              \
	do {                                                                                   \
		ofono_debug("OFONO_DFX:OOS_DURATION_INFO:%d", oos_duration);                   \
		sendEventMisightF(915300005, "%s:%d", "oos_time", oos_duration);               \
	} while (0)

#define OFONO_DFX_ROAMING_INFO(roaming_country_code)                                           \
	do {                                                                                   \
		ofono_debug("OFONO_DFX_ROAMING:%d", roaming_country_code);                     \
		sendEventMisightF(915300006, "%s:%d", "roaming_country_code",                  \
				roaming_country_code);                                         \
	} while (0)

#define OFONO_DFX_BAND_INFO(band)                                                              \
	do {                                                                                   \
		ofono_debug("OFONO_DFX_BAND_INFO:%d", band);                                   \
		sendEventMisightF(915300007, "%s:%d", "band_value", band);                     \
	} while (0)

#define OFONO_DFX_SIGNAL_LEVEL_DURATION(level0_duration, level1_duration,level2_duration,      \
		level3_duration, level4_duration, level5_duration)                             \
	do {                                                                                   \
		ofono_debug("OFONO_DFX_SIGNAL_LEVEL:%d,%d,%d,%d,%d,%d", level0_duration,       \
				level1_duration, level2_duration, level3_duration,             \
				level4_duration, level5_duration);                             \
		sendEventMisightF(915200008, "%s:%d,%s:%d,%s:%d,%s:%d,%s:%d,%s:%d",            \
				"level0_time", level0_duration,                                \
				"level1_time", level1_duration,                                \
				"level2_time", level2_duration,                                \
				"level3_time", level3_duration,                                \
				"level4_time", level4_duration,                                \
				"level5_time", level5_duration);                               \
	} while (0)

#define OFONO_DFX_RAT_DURATION(unknow_rat_duration, rat_2g_duration, rat_3g_duration,          \
		rat_4g_duration)                                                               \
	do {                                                                                   \
		ofono_debug("OFONO_DFX_RAT:%d,%d,%d,%d", unknow_rat_duration, rat_2g_duration, \
				rat_3g_duration, rat_4g_duration);                             \
		sendEventMisightF(915200009,"%s:%d,%s:%d,%s:%d", "2g_time", rat_2g_duration,   \
				"3g_time", rat_3g_duration, "4g_time", rat_4g_duration);       \
	} while (0)

#define OFONO_DFX_IMS_DURATION(ims_duration)                                                   \
	do {                                                                                   \
		ofono_debug("OFONO_DFX_IMS:%d",ims_duration);                                  \
		sendEventMisightF(915200010,"%s:%d", "volte_time", ims_duration);              \
	} while (0)

#define OFONO_DFX_MODEM_DURATION_INFO(modem_deactive_duration, modem_active_duration)          \
	do {                                                                                   \
		ofono_debug("OFONO_DFX_MODEM:%d,%d", modem_deactive_duration,                  \
				modem_active_duration);                                        \
		sendEventMisightF(915200011,"%s:%d,%s:%d", "modem_on_time",                    \
				modem_deactive_duration, "modem_off_time",                     \
				modem_active_duration);                                        \
	} while (0)

#else

#define OFONO_DFX_CALL_INFO(type, direction, media, fail_scenario, fail_reason)                \
	ofono_debug("OFONO_DFX_CALL_INFO:%d,%d,%d,%d,%s", type, direction, media,              \
			fail_scenario, fail_reason)

#define OFONO_DFX_SS_INFO(type, fail_reason)                                                   \
	ofono_debug("OFONO_DFX_SS_INFO:%s,%s", type, fail_reason)

#define OFONO_DFX_CALL_TIME_INFO(level0_duration, level1_duration,                             \
		level2_duration, level3_duration, level4_duration, level5_duration)            \
		ofono_debug("OFONO_DFX_CALL_TIME:%d,%d,%d,%d,%d,%d", level0_duration,          \
				level1_duration, level2_duration, level3_duration,             \
				level4_duration, level5_duration);

#define OFONO_DFX_SMS_INFO(opcode, sms_type, direction, fail_flag)                             \
	ofono_debug("OFONO_DFX_SMS:%d,%d,%d,%d", opcode, sms_type, direction, fail_flag)

#define OFONO_DFX_DATA_INTERRUPTION_INFO() ofono_debug("OFONO_DFX:DATA_INTERRUPTION")

#define OFONO_DFX_DATA_ACTIVE_FAIL(cause) ofono_debug("OFONO_DFX:DATA_ACTIVE_FAIL:%s", cause)

#define OFONO_DFX_DATA_ACTIVE_DURATION(data_active_time)                                       \
		ofono_debug("OFONO_DFX:DATA_ACTIVE_TIME:%d", data_active_time)

#define OFONO_DFX_OOS_INFO() ofono_debug("OFONO_DFX:OOS_INFO")

#define OFONO_DFX_OOS_DURATION_INFO(oos_duration)                                              \
	ofono_debug("OFONO_DFX:OOS_DURATION_INFO:%d", oos_duration)

#define OFONO_DFX_ROAMING_INFO(roaming_country_code)                                           \
	ofono_debug("OFONO_DFX_ROAMING:%d", roaming_country_code)

#define OFONO_DFX_BAND_INFO(band) ofono_debug("OFONO_DFX_BAND:%d", band)

#define OFONO_DFX_SIGNAL_LEVEL_DURATION(level0_duration, level1_duration,                      \
		level2_duration, level3_duration, level4_duration, level5_duration)            \
		ofono_debug("OFONO_DFX_SIGNAL:%d,%d,%d,%d,%d,%d", level0_duration,             \
				level1_duration, level2_duration, level3_duration,             \
				level4_duration, level5_duration);

#define OFONO_DFX_RAT_DURATION(unknow_rat_duration, rat_2g_duration, rat_3g_duration           \
		,rat_4g_duration)                                                              \
	ofono_debug("OFONO_DFX_RAT:%d,%d,%d,%d", unknow_rat_duration, rat_2g_duration,         \
				rat_3g_duration, rat_4g_duration)

#define OFONO_DFX_IMS_DURATION(ims_duration) ofono_debug("OFONO_DFX_IMS:%d",ims_duration)

#define OFONO_DFX_MODEM_DURATION_INFO(modem_deactive_duration, modem_active_duration)          \
	ofono_debug("OFONO_DFX_MODEM:%d,%d", modem_deactive_duration, modem_active_duration)

#endif

#define OFONO_DFX_CALL_INFO_IF(flag, type, direction, media, fail_scenario, fail_reason)       \
	do {                                                                                   \
		if (flag) {                                                                    \
			OFONO_DFX_CALL_INFO(type, direction, media,                            \
					fail_scenario, fail_reason);                           \
		}                                                                              \
	} while (0)

#endif
