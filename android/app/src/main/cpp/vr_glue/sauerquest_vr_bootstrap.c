/*
 * SauerQuest VR bootstrap.
 *
 * This is a from-scratch JNI/lifecycle driver (not adapted from QuakeQuest)
 * built on top of TBXR_Common.c/OpenXrInput.c's OpenXR session/swapchain/
 * frame loop. Phase 1 proved the toolchain end to end with a standalone
 * colored clear and no engine linkage; Phase 4 (here) calls into the real
 * Sauerbraten engine every frame via androidbridge.h's C-linkage entry
 * points, mirroring QuakeQuest's AppThreadFunction pattern: boot the
 * engine once via android_sauer_main(), then drive per-frame
 * tick/draw/endframe from this thread's own loop instead of the engine
 * owning one itself (see src/engine/main.cpp's __ANDROID__ branch).
 *
 * Both eyes still render identically (monoscopic) using the engine's
 * existing single-camera state -- real per-eye stereo matrices are
 * Phase 5's job, not this one. The goal here is just to get real engine
 * pixels (the main menu, initially) on screen and prove Phase 3's GLES3
 * fixes actually work at runtime, not just that they compile.
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <pthread.h>

#include "VrCommon.h"
#include "androidbridge.h"

void jni_shutdown(void); /* defined below; VR_Shutdown() needs it forward-declared */

/* Sauerbraten's own logging (logoutf() -> stdout, see src/engine/server.cpp)
 * goes nowhere on Android unless something redirects stdout/stderr to
 * logcat -- there is no terminal for it to inherit. Standard NDK pattern:
 * pipe stdout/stderr into a background thread that forwards each line via
 * __android_log_write. Started once, before android_sauer_main(), so init
 * logging is visible from the very first line. */
static void *StdioLogThread(void *arg)
{
    int fd = *(int *)arg;
    free(arg);
    char buf[512];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        if (buf[n - 1] == '\n') n--;
        buf[n] = '\0';
        __android_log_write(ANDROID_LOG_INFO, "SauerQuest", buf);
    }
    return NULL;
}

static void RedirectStdioToLogcat(void)
{
    int pipefd[2];
    if (pipe(pipefd) != 0) return;

    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[1]);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    int *readFd = (int *)malloc(sizeof(int));
    *readFd = pipefd[0];
    pthread_t thread;
    pthread_create(&thread, NULL, StdioLogThread, readFd);
    pthread_detach(thread);
}

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
    /* The main menu (menus.cpp's g3d_addgui() forces GUI_2D while
     * mainmenu is set, and main.cpp's renderbackground()) is rendered
     * entirely via hudmatrix.ortho() -- flat screen-space content with no
     * real per-eye perspective at all. Submitting that through the normal
     * stereo XrCompositionLayerProjection is wrong: the compositor maps
     * each eye's rendered image onto the display using that eye's own
     * declared asymmetric FOV (its distortion-mesh/reprojection metadata),
     * which only produces a correct result for content that was actually
     * rendered with a matching per-eye perspective. Flat ortho content
     * has none, so the two eyes' declared (and genuinely different, real
     * lens-driven asymmetric) FOVs each warp the *same* image
     * differently -- confirmed on-device as the actual cause of a
     * reported inability to binocularly fuse the main menu at all, even
     * with eye-to-eye camera *position* forced completely identical
     * (ruling out every other stereo-math candidate first). The
     * XrCompositionLayerQuad path below instead submits it as a real
     * flat panel positioned in stage space, shown identically to both
     * eyes regardless of per-eye FOV asymmetry -- the standard, correct
     * technique for screen-locked 2D VR content. Real gameplay
     * (gl_drawframe(), real per-eye asymmetric frustums matching the
     * declared FOV) still needs the true stereo projection layer. */
    return android_sauer_is_mainmenu() != 0;
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

/* Called by src/engine/main.cpp's setupscreen() (via androidbridge.h) once
 * TBXR_InitRenderer() below has already sized the eye buffers. */
void android_get_eye_size(int *w, int *h)
{
    *w = (int)gAppState.Width;
    *h = (int)gAppState.Height;
}

void *AppThreadFunction(void *parm)
{
    gAppThread = (ovrAppThread *)parm;

    java.Vm = gAppThread->JavaVm;
    (*java.Vm)->AttachCurrentThread(java.Vm, &java.Env, NULL);
    java.ActivityObject = gAppThread->ActivityObject;

    prctl(PR_SET_NAME, (long)"SauerQuestVRThread", 0, 0, 0);

    RedirectStdioToLogcat();

    gAppState.MainThreadTid = gettid();

    TBXR_InitialiseOpenXR();
    TBXR_EnterVR();
    TBXR_InitRenderer();
    TBXR_InitActions();
    TBXR_WaitForSessionActive();

    ALOGV("SauerQuest VR bootstrap: booting engine (%dx%d per eye)",
          (int)gAppState.Width, (int)gAppState.Height);

    /* Sauerbraten's own file I/O (src/shared/stream.cpp's openfile()) uses
     * plain fopen() with paths relative to the process's cwd -- it has no
     * concept of APK assets. SauerQuestActivity.onCreate() already
     * extracted the bundled data/packages assets to internal storage
     * (getFilesDir(), same path SDL_AndroidGetInternalStoragePath()
     * returns); chdir() there now so every relative fopen() the engine is
     * about to do just works. Declared extern rather than pulling in
     * SDL.h here (this file otherwise has no SDL dependency at all). */
    {
        extern const char *SDL_AndroidGetInternalStoragePath(void);
        const char *storagePath = SDL_AndroidGetInternalStoragePath();
        if (storagePath) chdir(storagePath);
        else ALOGV("SauerQuest VR bootstrap: SDL_AndroidGetInternalStoragePath() failed");
    }

    {
        int argc = 1;
        char *argv[] = { (char *)"sauerquest" };
        android_sauer_main(argc, argv);
    }

    ALOGV("SauerQuest VR bootstrap: engine booted, entering frame loop");

    while (runStatus == -1) {
        TBXR_FrameSetup();

        android_sauer_tick();

        for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
            TBXR_prepareEyeBuffer(eye);
            if (gAppState.FrameState.shouldRender) {
                /* gAppState.Projections[eye].pose is the eye relative to
                 * HeadSpace -- a small IPD-driven offset only, NOT the
                 * player's real position in the room (eyes don't rotate
                 * independently of the head either, so only .position is
                 * used here). android_sauer_set_eye()'s contract
                 * (androidbridge.h) documents dx/dy/dz as exactly this:
                 * "eye position offset from the head". Composing in
                 * gAppState.xfStageFromHead (the player's real physical
                 * position/height in the guardian-defined stage space, via
                 * XrPosef_Multiply as TBXR_submitFrame() does for the
                 * actual compositor submission) does NOT belong here --
                 * this app has no room-scale/positional-tracking movement
                 * system yet, so the player's *real* height and stage
                 * position (easily 1.5+ real meters) would get added
                 * straight onto camera1->o after being scaled by
                 * vrworldscale, dwarfing the true ~3cm IPD offset and
                 * placing the render camera somewhere arbitrary relative
                 * to world geometry -- confirmed on-device as the actual
                 * cause of a reported inability to binocularly fuse the
                 * view (severe near-field parallax against whatever
                 * geometry the camera ended up next to/inside, not a
                 * subtle stereo-math error). */
                XrPosef xfHeadFromEye = gAppState.Projections[eye].pose;
                XrFovf fov = gAppState.Projections[eye].fov;

                /* OpenXR is Y-up/right-handed (+X right, +Y up, -Z
                 * forward). QuatToYawPitchRoll() (TBXR_Common.c) remaps
                 * its forward/right vectors as {-z,-x,y} too, but that
                 * convention is Quake/DarkPlaces's own (X=forward,
                 * Y=lateral) -- it's only used there to extract a scalar
                 * yaw/pitch/roll *angle*, which works out the same
                 * regardless of which axis is nominally "forward", so it
                 * never surfaced as a bug. Sauerbraten/Cube2's actual
                 * world axes are the opposite: X=lateral (strafe),
                 * Y=forward (verified against vecfromyawpitch() in
                 * physics.cpp -- move contributes to .y, strafe to .x).
                 * A *position* offset can't ignore that difference the
                 * way an angle can: naively reusing {-z,-x,y} here put
                 * the real ~3cm IPD offset (from OpenXR position.x, the
                 * lateral axis) onto Sauerbraten's Y (forward/back)
                 * instead of X (lateral) -- pushing one eye's camera
                 * toward the scene and the other away from it along the
                 * view axis instead of side-to-side. Confirmed on-device
                 * as the actual cause of a reported inability to
                 * binocularly fuse the view at all ("everything,
                 * uniformly" doubled -- consistent with a wrong-axis
                 * global camera-offset bug, not per-object depth error).
                 * dx now carries the lateral component onto Sauerbraten's
                 * X, dy the (near-zero, eyes don't offset forward/back)
                 * depth component onto Sauerbraten's Y. */
                float dx = -xfHeadFromEye.position.x;
                float dy = -xfHeadFromEye.position.z;
                float dz =  xfHeadFromEye.position.y;

                android_sauer_set_eye(dx, dy, dz,
                    hmdorientation[1], hmdorientation[0], hmdorientation[2],
                    tanf(fov.angleLeft), tanf(fov.angleRight), tanf(fov.angleUp), tanf(fov.angleDown));
                android_sauer_drawframe();
            }
            TBXR_finishEyeBuffer(eye);
        }

        android_sauer_endframe();

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

/* No JNI_OnLoad of our own: SDL2's SDL_android.c already defines one (it
 * registers native methods for org/libsdl/app/SDLActivity classes we don't
 * have -- those registrations harmlessly fail-and-log, per its own
 * register_methods() error handling -- and captures SDL's internal
 * mJavaVM, which its audio backend needs). Two JNI_OnLoad definitions in
 * the same .so is a link error either way, and SDL's is the one that must
 * win, so jVM is captured here from onCreate's env instead. */

/* SDL_android.c's nativeSetupJNI() (mangled per its SDL_JAVA_INTERFACE
 * macro to Java_org_libsdl_app_SDLActivity_nativeSetupJNI) sets its global
 * mActivityClass -- required by SDL_AndroidGetInternalStoragePath() and
 * friends -- from whichever class calls it. Upstream, that's Java calling
 * into org.libsdl.app.SDLActivity's own native method declaration; this
 * port has no such class, so it's called directly as a plain C function
 * below instead, passing our real Activity's class (see
 * SauerQuestActivity.getContext(), the one static method on it SDL's
 * lookups actually need). */
extern void Java_org_libsdl_app_SDLActivity_nativeSetupJNI(JNIEnv *env, jclass cls);

JNIEXPORT jlong JNICALL
Java_org_sauerquest_vr_SauerQuestJNILib_onCreate(JNIEnv *env, jclass activityClass, jobject activity)
{
    ALOGV("SauerQuestJNILib::onCreate()");

    (*env)->GetJavaVM(env, &jVM);

    jclass realActivityClass = (*env)->GetObjectClass(env, activity);
    Java_org_libsdl_app_SDLActivity_nativeSetupJNI(env, realActivityClass);

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
