// Android-only C-linkage entry points bridging the OpenXR native thread
// (android/app/src/main/cpp/vr_glue/, plain C) and the C++ engine. Phase 4
// of the Quest VR port: replaces main.cpp's own for(;;) loop on Android
// with per-frame calls driven by the OpenXR frame loop instead.
#ifndef ANDROID_BRIDGE_H
#define ANDROID_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

// Implemented in main.cpp == desktop's main(), called once by the VR
// native thread (matching QuakeQuest's AppThreadFunction pattern) after
// TBXR_InitRenderer() has an EGL context already current on that thread.
// On Android, main() runs one-time init only and returns instead of
// entering its own loop (see main()'s __ANDROID__ branch) -- everything
// afterward is driven per-frame by the three functions below.
int android_sauer_main(int argc, char **argv);

// Called once per XR frame, before the per-eye render loop: game-logic
// tick only (input/physics/world update), no rendering.
void android_sauer_tick(void);

// Called once per eye, after the OpenXR swapchain image for that eye is
// bound as the current framebuffer: renders the main menu or the game
// world (whichever is active) using the engine's existing camera/proj
// state. Both eyes render identically for now (monoscopic in the
// headset) until Phase 5 wires real per-eye stereo matrices.
void android_sauer_drawframe(void);

// Called once per XR frame, after both eyes are drawn: engine-side
// end-of-frame bookkeeping (desktop's swapbuffers(), which already
// no-ops its actual SDL_GL_SwapWindow call on __ANDROID__ -- OpenXR's
// swapchain release, done separately by the native thread, is the real
// present).
void android_sauer_endframe(void);

// Implemented in vr_glue/sauerquest_vr_bootstrap.c: reports the OpenXR
// per-eye swapchain image size. Already known by the time
// android_sauer_main() runs, since TBXR_InitRenderer() completes before
// it's called.
void android_get_eye_size(int *w, int *h);

#ifdef __cplusplus
}
#endif

#endif
