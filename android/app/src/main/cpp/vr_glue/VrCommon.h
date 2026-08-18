/*
 * Adapted from QuakeQuest (github.com/Team-Beef-Studios/QuakeQuest), GPLv2.
 * The original VrCommon.h pulled in DarkPlaces' mathlib.h purely for the
 * vec3_t/qboolean typedefs; this version defines them locally so the VR
 * glue layer has no dependency on any particular game engine.
 */
#if !defined(vrcommon_h)
#define vrcommon_h

#include <android/log.h>
#include <stdbool.h>

/* TBXR_Common.h's own declarations (e.g. QuatToYawPitchRoll) use vec3_t, so
 * it must be defined before that header is included. */
typedef float vec3_t[3];
typedef int qboolean;

#include "TBXR_Common.h"

#undef LOG_TAG
#define LOG_TAG "SauerQuest"

#ifndef NDEBUG
#define DEBUG 1
#endif

#define ALOGE(...) __android_log_print( ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__ )

#if DEBUG
#define ALOGV(...) __android_log_print( ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__ )
#else
#define ALOGV(...)
#endif

extern ovrInputStateTrackedRemote leftTrackedRemoteState_old;
extern ovrInputStateTrackedRemote leftTrackedRemoteState_new;
extern ovrTrackedController leftRemoteTracking_new;
extern ovrInputStateTrackedRemote rightTrackedRemoteState_old;
extern ovrInputStateTrackedRemote rightTrackedRemoteState_new;
extern ovrTrackedController rightRemoteTracking_new;

extern float playerHeight;
extern float playerYaw;

extern vec3_t hmdorientation;

#endif //vrcommon_h
