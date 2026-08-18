// GLES3 compatibility shim for desktop-only GL entry points the engine
// calls directly (assuming they're always present, which holds for every
// desktop GL implementation but not GLES). Android-only: not part of the
// desktop build (src/Makefile's CLIENT_OBJS doesn't reference this file,
// only android/app/src/main/cpp/Android.mk does), so it's dead weight
// everywhere else rather than something requiring #ifdef discipline at
// every call site across the engine.
#ifdef __ANDROID__

#include <GLES3/gl3.h>

// glClearDepth/glDepthRange: GLES only has the float (`f`-suffixed)
// variants; desktop GL's double-precision versions have no GLES
// equivalent to call through to, so just narrow and forward. Using plain
// `double` rather than desktop GL's GLclampd (unavailable in GLES
// headers) -- extern "C" symbols link by name only, and GLclampd is
// itself just a `double` typedef on desktop, so the ABI still matches
// what the engine's call sites (declared against SDL_opengl.h) pass.
extern "C" void glClearDepth(double depth)
{
    glClearDepthf((GLfloat)depth);
}

extern "C" void glDepthRange(double nearVal, double farVal)
{
    glDepthRangef((GLfloat)nearVal, (GLfloat)farVal);
}

// glDrawBuffer(single enum) doesn't exist in GLES; glDrawBuffers(array)
// does and is a drop-in for the single-buffer case the engine uses it for.
extern "C" void glDrawBuffer(GLenum mode)
{
    GLenum bufs[1] = { mode };
    glDrawBuffers(1, bufs);
}

// glPolygonMode (desktop-only wireframe/point fill modes) and
// glGetCompressedTexImage (desktop-only compressed-texture readback) have
// no GLES equivalent at all -- GLES's rasterizer has no fill-mode toggle,
// and GLES textures are write-only from the API's perspective. Both call
// sites are editor/tooling paths (material-boundary wireframe grids,
// prefab overlays, and offline DDS generation via a console command) that
// this VR port's milestone-1 scope doesn't exercise -- no-op rather than
// porting a real substitute.
extern "C" void glPolygonMode(GLenum face, GLenum mode)
{
}

extern "C" void glGetCompressedTexImage(GLenum target, GLint level, void *img)
{
}

#endif // __ANDROID__
