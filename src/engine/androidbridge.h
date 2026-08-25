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

// Implemented in rendergl.cpp; called once per eye, immediately before
// android_sauer_drawframe(), to report that eye's real OpenXR pose/FOV
// for this frame. setcammatrix()/gl_drawframe() read these (instead of
// camera1's mouse-driven yaw/pitch/roll and the fov cvar) on Android.
//   dx/dy/dz: eye position offset from the head, in Sauerbraten's own
//     "quake style" world axes (already remapped from OpenXR's Y-up axes
//     by the caller, matching hmdorientation's convention) and in real
//     *meters*, not world units -- scaled by the vrworldscale cvar on
//     this side so it's runtime-tunable without a rebuild.
//   yaw/pitch/roll: degrees, Sauerbraten's convention (this is head
//     orientation, shared by both eyes -- eyes don't rotate
//     independently of the head in this rig, only position differs).
//   tanLeft/tanRight/tanUp/tanDown: tan() of the per-eye asymmetric FOV
//     angles from xrLocateViews, i.e. already in the form
//     matrix4::frustum() needs once multiplied by the near clip plane.
void android_sauer_set_eye(float dx, float dy, float dz,
                            float yaw, float pitch, float roll,
                            float tanLeft, float tanRight, float tanUp, float tanDown);

// Implemented in main.cpp; reports whether the engine is currently showing
// the (fully 2D/ortho -- see menus.cpp's g3d_addgui() GUI_2D forcing while
// mainmenu is set, and main.cpp's renderbackground(), also pure
// hudmatrix.ortho()) main menu rather than real 3D gameplay. The native VR
// frame loop uses this to switch TBXR's submission between a flat
// XrCompositionLayerQuad (correct for this ortho-only content -- a
// Projection layer's per-eye asymmetric FOV drives the compositor's
// distortion-mesh mapping of the rendered image, which doesn't apply
// correctly to content that was never rendered with any real per-eye
// perspective in the first place, confirmed on-device as the actual cause
// of a reported inability to binocularly fuse the main menu at all) and
// the normal stereo Projection layer used for real gameplay.
int android_sauer_is_mainmenu(void);

#ifdef __cplusplus
}
#endif

#endif
