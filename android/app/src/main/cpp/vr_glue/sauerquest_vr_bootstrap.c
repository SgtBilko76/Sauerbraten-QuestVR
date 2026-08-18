/*
 * SauerQuest VR bootstrap — Phase 1 ("hello triangle in headset").
 *
 * This is a from-scratch JNI/lifecycle driver (not adapted from QuakeQuest)
 * that exercises TBXR_Common.c/OpenXrInput.c's OpenXR session/swapchain/frame
 * loop in isolation, with zero Sauerbraten engine linkage. Its only job is to
 * prove the Android.mk/Gradle/manifest/OpenXR-loader toolchain works end to
 * end before any engine porting begins (see the project plan, Phase 1).
 *
 * Visual smoke test: each eye clears to a distinct color, and the left eye's
 * clear color shifts with head yaw, so a head-pose-reactive stereo image
 * confirms xrLocateViews/xrLocateSpace data is live.
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/prctl.h>

#include "VrCommon.h"

void jni_shutdown(void); /* defined below; VR_Shutdown() needs it forward-declared */

/* ---- Required by TBXR_Common.c (referenced even on code paths we never
 * take, e.g. the flat "screen layer" branch of TBXR_submitFrame) ---- */
float playerYaw = 0.0f;
float playerHeight = 0.0f;
vec3_t hmdorientation = {0, 0, 0};

ovrInputStateTrackedRemote leftTrackedRemoteState_old;
ovrInputStateTrackedRemote leftTrackedRemoteState_new;
ovrTrackedController leftRemoteTracking_new;
ovrInputStateTrackedRemote rightTrackedRemoteState_old;
ovrInputStateTrackedRemote rightTrackedRemoteState_new;
ovrTrackedController rightRemoteTracking_new;

extern int runStatus; /* defined in TBXR_Common.c, -1 while running */

/* ---- Game-specific callbacks required by TBXR_Common.c ---- */

void VR_FrameSetup(void)
{
}

bool VR_UseScreenLayer(void)
{
    return false; /* always render the real stereo projection layer */
}

float VR_GetScreenLayerDistance(void)
{
    return 4.5f; /* unused while VR_UseScreenLayer() is false */
}

void VR_SetHMDOrientation(float pitch, float yaw, float roll)
{
    hmdorientation[0] = pitch;
    hmdorientation[1] = yaw;
    hmdorientation[2] = roll;
    playerYaw = yaw;
}

void VR_SetHMDPosition(float x, float y, float z)
{
    playerHeight = y;
}

void VR_HandleControllerInput(void)
{
}

void VR_Shutdown(void)
{
    jni_shutdown();
}

/* ---- Minimal per-eye render: colored clear reacting to head yaw ---- */

static void RenderEye(int eye)
{
    /* Left eye: hue sweeps with head yaw, so turning your head visibly
     * changes the color -- confirms head tracking is live, not just that
     * a static frame is being submitted. Right eye: fixed color, so a
     * missing/wrong eye in the compositor is obvious at a glance. */
    if (eye == 0) {
        float t = fmodf(playerYaw, 360.0f) / 360.0f;
        if (t < 0.0f) t += 1.0f;
        glClearColor(t, 0.15f, 1.0f - t, 1.0f);
    } else {
        glClearColor(0.1f, 0.8f, 0.2f, 1.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT);
}

void *AppThreadFunction(void *parm)
{
    gAppThread = (ovrAppThread *)parm;

    java.Vm = gAppThread->JavaVm;
    (*java.Vm)->AttachCurrentThread(java.Vm, &java.Env, NULL);
    java.ActivityObject = gAppThread->ActivityObject;

    prctl(PR_SET_NAME, (long)"SauerQuestVRThread", 0, 0, 0);

    gAppState.MainThreadTid = gettid();

    TBXR_InitialiseOpenXR();
    TBXR_EnterVR();
    TBXR_InitRenderer();
    TBXR_InitActions();
    TBXR_WaitForSessionActive();

    ALOGV("SauerQuest VR bootstrap: entering frame loop (%dx%d per eye)",
          (int)gAppState.Width, (int)gAppState.Height);

    while (runStatus == -1) {
        TBXR_FrameSetup();

        for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
            TBXR_prepareEyeBuffer(eye);
            if (gAppState.FrameState.shouldRender) {
                RenderEye(eye);
            }
            TBXR_finishEyeBuffer(eye);
        }

        TBXR_submitFrame();
    }

    TBXR_LeaveVR();
    VR_Shutdown();
    exit(0);

    return NULL;
}

/* ---- JNI lifecycle glue ---- */

static jmethodID android_shutdown;
static JavaVM *jVM;
static jobject jniCallbackObj = 0;

void jni_shutdown(void)
{
    JNIEnv *env;
    if (((*jVM)->GetEnv(jVM, (void **)&env, JNI_VERSION_1_4)) < 0) {
        (*jVM)->AttachCurrentThread(jVM, &env, NULL);
    }
    (*env)->CallVoidMethod(env, jniCallbackObj, android_shutdown);
}

int JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv *env;
    jVM = vm;
    if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_4) != JNI_OK) {
        ALOGE("Failed JNI_OnLoad");
        return -1;
    }
    return JNI_VERSION_1_4;
}

JNIEXPORT jlong JNICALL
Java_org_sauerquest_vr_SauerQuestJNILib_onCreate(JNIEnv *env, jclass activityClass, jobject activity)
{
    ALOGV("SauerQuestJNILib::onCreate()");

    ovrAppThread *appThread = (ovrAppThread *)malloc(sizeof(ovrAppThread));
    ovrAppThread_Create(appThread, env, activity, activityClass);

    surfaceMessageQueue_Enable(&appThread->MessageQueue, true);
    srufaceMessage message;
    surfaceMessage_Init(&message, MESSAGE_ON_CREATE, MQ_WAIT_PROCESSED);
    surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);

    return (jlong)((size_t)appThread);
}

JNIEXPORT void JNICALL
Java_org_sauerquest_vr_SauerQuestJNILib_onStart(JNIEnv *env, jobject obj, jlong handle, jobject obj1)
{
    ALOGV("SauerQuestJNILib::onStart()");

    jniCallbackObj = (jobject)((*env)->NewGlobalRef(env, obj1));
    jclass callbackClass = (*env)->GetObjectClass(env, jniCallbackObj);
    android_shutdown = (*env)->GetMethodID(env, callbackClass, "shutdown", "()V");

    ovrAppThread *appThread = (ovrAppThread *)((size_t)handle);
    srufaceMessage message;
    surfaceMessage_Init(&message, MESSAGE_ON_START, MQ_WAIT_PROCESSED);
    surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

JNIEXPORT void JNICALL
Java_org_sauerquest_vr_SauerQuestJNILib_onResume(JNIEnv *env, jobject obj, jlong handle)
{
    ALOGV("SauerQuestJNILib::onResume()");
    ovrAppThread *appThread = (ovrAppThread *)((size_t)handle);
    srufaceMessage message;
    surfaceMessage_Init(&message, MESSAGE_ON_RESUME, MQ_WAIT_PROCESSED);
    surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

JNIEXPORT void JNICALL
Java_org_sauerquest_vr_SauerQuestJNILib_onPause(JNIEnv *env, jobject obj, jlong handle)
{
    ALOGV("SauerQuestJNILib::onPause()");
    ovrAppThread *appThread = (ovrAppThread *)((size_t)handle);
    srufaceMessage message;
    surfaceMessage_Init(&message, MESSAGE_ON_PAUSE, MQ_WAIT_PROCESSED);
    surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

JNIEXPORT void JNICALL
Java_org_sauerquest_vr_SauerQuestJNILib_onStop(JNIEnv *env, jobject obj, jlong handle)
{
    ALOGV("SauerQuestJNILib::onStop()");
    ovrAppThread *appThread = (ovrAppThread *)((size_t)handle);
    srufaceMessage message;
    surfaceMessage_Init(&message, MESSAGE_ON_STOP, MQ_WAIT_PROCESSED);
    surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

JNIEXPORT void JNICALL
Java_org_sauerquest_vr_SauerQuestJNILib_onDestroy(JNIEnv *env, jobject obj, jlong handle)
{
    ALOGV("SauerQuestJNILib::onDestroy()");
    ovrAppThread *appThread = (ovrAppThread *)((size_t)handle);
    srufaceMessage message;
    surfaceMessage_Init(&message, MESSAGE_ON_DESTROY, MQ_WAIT_PROCESSED);
    surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
    surfaceMessageQueue_Enable(&appThread->MessageQueue, false);

    ovrAppThread_Destroy(appThread, env);
    free(appThread);
}

JNIEXPORT void JNICALL
Java_org_sauerquest_vr_SauerQuestJNILib_onSurfaceCreated(JNIEnv *env, jobject obj, jlong handle, jobject surface)
{
    ALOGV("SauerQuestJNILib::onSurfaceCreated()");
    ovrAppThread *appThread = (ovrAppThread *)((size_t)handle);

    ANativeWindow *newNativeWindow = ANativeWindow_fromSurface(env, surface);
    appThread->NativeWindow = newNativeWindow;
    srufaceMessage message;
    surfaceMessage_Init(&message, MESSAGE_ON_SURFACE_CREATED, MQ_WAIT_PROCESSED);
    surfaceMessage_SetPointerParm(&message, 0, appThread->NativeWindow);
    surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

JNIEXPORT void JNICALL
Java_org_sauerquest_vr_SauerQuestJNILib_onSurfaceChanged(JNIEnv *env, jobject obj, jlong handle, jobject surface)
{
    ALOGV("SauerQuestJNILib::onSurfaceChanged()");
    ovrAppThread *appThread = (ovrAppThread *)((size_t)handle);

    ANativeWindow *newNativeWindow = ANativeWindow_fromSurface(env, surface);
    if (newNativeWindow != appThread->NativeWindow) {
        if (appThread->NativeWindow != NULL) {
            srufaceMessage message;
            surfaceMessage_Init(&message, MESSAGE_ON_SURFACE_DESTROYED, MQ_WAIT_PROCESSED);
            surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
            ANativeWindow_release(appThread->NativeWindow);
            appThread->NativeWindow = NULL;
        }
        if (newNativeWindow != NULL) {
            appThread->NativeWindow = newNativeWindow;
            srufaceMessage message;
            surfaceMessage_Init(&message, MESSAGE_ON_SURFACE_CREATED, MQ_WAIT_PROCESSED);
            surfaceMessage_SetPointerParm(&message, 0, appThread->NativeWindow);
            surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
        }
    } else if (newNativeWindow != NULL) {
        ANativeWindow_release(newNativeWindow);
    }
}

JNIEXPORT void JNICALL
Java_org_sauerquest_vr_SauerQuestJNILib_onSurfaceDestroyed(JNIEnv *env, jobject obj, jlong handle)
{
    ALOGV("SauerQuestJNILib::onSurfaceDestroyed()");
    ovrAppThread *appThread = (ovrAppThread *)((size_t)handle);
    srufaceMessage message;
    surfaceMessage_Init(&message, MESSAGE_ON_SURFACE_DESTROYED, MQ_WAIT_PROCESSED);
    surfaceMessageQueue_PostMessage(&appThread->MessageQueue, &message);
    ANativeWindow_release(appThread->NativeWindow);
    appThread->NativeWindow = NULL;
}
