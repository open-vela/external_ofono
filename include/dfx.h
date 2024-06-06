#ifndef __OFONO_DFX_H
#define __OFONO_DFX_H

#if defined(CONFIG_DFX) && defined(CONFIG_DFX_EVENT)
#include <dfx_debug.h>
#include <dfx_event.h>
#endif
#include <ofono/log.h>

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
		ofono_debug("%d,%d,%d,%d,%s", type, direction, media,                          \
				fail_scenario, fail_reason);                                   \
		sendEventMisightF(915200010, "%s:%d,%s:%d,%s:%d,%s:%d,%s:%s",                  \
					"call_type", type, "direction", direction,             \
					"media", media, "fail_scenario", fail_scenario,        \
					"fail_reason", fail_reason);                           \
	} while (0)
#else
#define OFONO_DFX_CALL_INFO(type, direction, media, fail_scenario, fail_reason)                \
	ofono_debug("%d,%d,%d,%d,%s", type, direction, media,                                  \
			fail_scenario, fail_reason);
#endif

#define OFONO_DFX_CALL_INFO_IF(flag, type, direction, media, fail_scenario, fail_reason)       \
	do {                                                                                   \
		if (flag) {                                                                    \
			OFONO_DFX_CALL_INFO(type, direction, media,                            \
					fail_scenario, fail_reason);                           \
		}                                                                              \
	} while (0)

#endif
