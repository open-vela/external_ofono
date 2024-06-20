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
	OFONO_TYPE_UNKNOW
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
	OFONO_UNKNOW_FAIL
} ofono_call_fail_sceanrio;

#if defined(CONFIG_DFX) && defined(CONFIG_DFX_EVENT)
#define OFONO_DFX_CALL_INFO(type, direction, media, fail_scenario, fail_reason)                \
	do {                                                                                   \
		ofono_debug("OFONO_DFX:%d,%d,%d,%d,%s", type, direction, media,                \
				fail_scenario, fail_reason);                                   \
		sendEventMisightF(915200010, "%s:%d,%s:%d,%s:%d,%s:%d,%s:%s",                  \
					"call_type", type, "direction", direction,             \
					"media", media, "fail_scenario", fail_scenario,        \
					"fail_reason", fail_reason);                           \
	} while (0)

#define OFONO_DFX_SS_INFO(type, fail_reason)                                                   \
	do {                                                                                   \
		ofono_debug("OFONO_DFX:%s,%s", type, fail_reason);                             \
		sendEventMisightF(915200011, "%s:%s,%s:%s", "ss_type", type,                   \
				"fail_reason", fail_reason);                                   \
	} while (0)

#define OFONO_DFX_CALL_TIME_INFO(level0_duration, level1_duration,                             \
		level2_duration, level3_duration, level4_duration, level5_duration)            \
	do {                                                                                   \
		ofono_debug("OFONO_DFX:%d,%d,%d,%d,%d,%d", level0_duration, level1_duration,   \
				level2_duration, level3_duration, level4_duration,             \
				level5_duration);                                              \
		sendEventMisightF(915200012, "%s:%d,%s:%d,%s:%d,%s:%d,%s:%d,%s:%d",            \
				"level0_time_value", level0_duration,                          \
				"level1_time_value", level1_duration,                          \
				"level2_time_value", level2_duration,                          \
				"level3_time_value", level3_duration,                          \
				"level4_time_value", level4_duration,                          \
				"level5_time_value", level5_duration);                         \
	} while (0)
#else
#define OFONO_DFX_CALL_INFO(type, direction, media, fail_scenario, fail_reason)                \
	ofono_debug("OFONO_DFX:%d,%d,%d,%d,%s", type, direction, media,                        \
			fail_scenario, fail_reason);

#define OFONO_DFX_SS_INFO(type, fail_reason)                                                   \
	ofono_debug("OFONO_DFX:%s,%s", type, fail_reason);

#define OFONO_DFX_CALL_TIME_INFO(level0_duration, level1_duration,                             \
		level2_duration, level3_duration, level4_duration, level5_duration)            \
		ofono_debug("OFONO_DFX:%d,%d,%d,%d,%d,%d", level0_duration, level1_duration,   \
			level2_duration, level3_duration, level4_duration, level5_duration);
#endif

#define OFONO_DFX_CALL_INFO_IF(flag, type, direction, media, fail_scenario, fail_reason)       \
	do {                                                                                   \
		if (flag) {                                                                    \
			OFONO_DFX_CALL_INFO(type, direction, media,                            \
					fail_scenario, fail_reason);                           \
		}                                                                              \
	} while (0)

#endif
