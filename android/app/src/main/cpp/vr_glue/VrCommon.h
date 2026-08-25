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

/* The main menu's curved "virtual screen" (an OpenXR Quad/Cylinder
 * composition layer positioned in front of the player, not part of the
 * normal stereo scene -- see VR_UseScreenLayer() in
 * sauerquest_vr_bootstrap.c). Shared here so TBXR_Common.c's
 * layer-building code and sauerquest_vr_bootstrap.c's controller-raycast
 * pointer code (SauerQuest_UpdateMenuPointer()) agree on the same
 * geometry without duplicating it. DISTANCE is the radius the Cylinder
 * layer curves around (how far away the screen is); WIDTH/HEIGHT are its
 * physical size in meters -- deliberately explicit constants rather than
 * derived from the swapchain's own pixel aspect ratio, so the raycast
 * math doesn't need to re-read swapchain dimensions to match. */
#define SAUERQUEST_MENU_SCREEN_DISTANCE 2.2f
#define SAUERQUEST_MENU_SCREEN_WIDTH    2.8f
#define SAUERQUEST_MENU_SCREEN_HEIGHT   3.0f

/* Implemented in sauerquest_vr_bootstrap.c: the screen's pose (position
 * of the player-side axis + facing orientation), anchored once when the
 * main menu opens and held fixed in stage space until it closes and
 * reopens -- recomputing it from *live* head tracking every frame (the
 * first attempt at this) made the screen visibly chase the player's
 * head/gaze instead of staying put like a real curved monitor.
 * TBXR_Common.c's TBXR_submitFrame() (layer placement) and
 * sauerquest_vr_bootstrap.c's own controller-raycast pointer both read
 * this same cached pose so they never disagree about where the screen
 * actually is. */
XrPosef SauerQuest_GetMenuScreenPose(void);

#endif //vrcommon_h
