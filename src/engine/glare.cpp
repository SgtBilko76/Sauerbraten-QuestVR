#include "engine.h"
#include "rendertarget.h"

static struct glaretexture : rendertarget
{
    bool dorender()
    {
        extern void drawglare();
        drawglare();
        return true;
    }
} glaretex;

void cleanupglare()
{
    glaretex.cleanup(true);
}

VARFP(glaresize, 6, 8, 10, cleanupglare());
#ifdef __ANDROID__
// Confirmed on-device: enabling glare (either via a persisted config
// setting, or the options menu's postfx screen) crashes intermittently
// with a NULL-pointer SIGSEGV inside gle::end(), reached via
// drawglaretex() -> glaretexture::dorender() -> drawglare() ->
// renderwater() -> flushwater() -> flushwaterstrips() -- i.e. only when
// the glare pass's own *nested* render-to-texture re-render of the scene
// includes visible water. Root cause not fully isolated (gle::'s shared
// vertex-buffer state -- attribbuf/vbo/vbooffset -- is namespace-global
// state also used successfully by every other draw call every frame, so
// this isn't a simple "always-null pointer"; likely a boundary/overflow
// condition specific to some water geometry sizes only reached through
// this nested pass). Capping the max to 0 disables the feature outright
// on this platform rather than leaving a known, reproducible crash
// reachable through a graphics options toggle.
VARP(glare, 0, 0, 0);
#else
VARP(glare, 0, 0, 1);
#endif
VARP(blurglare, 0, 4, 7);
VARP(blurglareaspect, 0, 1, 1);
VARP(blurglaresigma, 1, 50, 200);

VAR(debugglare, 0, 0, 1);

void viewglaretex()
{
    if(!glare) return;
    glaretex.debug();
}

bool glaring = false;

void drawglaretex()
{
#ifdef __ANDROID__
    // Unconditional, regardless of the glare cvar's own value -- the
    // exact same crash this cvar's capped max (above) was meant to
    // prevent recurred even with that cap in place (confirmed on-device:
    // identical gle::end() NULL-pointer backtrace through this same
    // drawglaretex() -> ... -> renderwater() chain), meaning something
    // -- most likely a persisted config value applied through a path
    // that doesn't go through this cvar's own clamping -- can still
    // reach glare=1 on this platform. Not worth root-causing that
    // specific bypass when the simplest fix is to just never let this
    // function do anything at all here.
    return;
#endif
    if(!glare) return;

    int w = 1<<glaresize, h = 1<<glaresize, blury = blurglare;
    if(blurglare && blurglareaspect)
    {
        while(h > (1<<5) && (screenw*h)/w >= (screenh*4)/3) h /= 2;
        blury = ((1 + 4*blurglare)*(screenw*h)/w + screenh*2)/(screenh*4);
        blury = clamp(blury, 1, MAXBLURRADIUS);
    }

    glaretex.render(w, h, blurglare, blurglaresigma/100.0f, blury);
}

FVAR(glaremod, 0.5f, 0.75f, 1);
FVARP(glarescale, 0, 1, 8);

void addglare()
{
    if(!glare) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    SETSHADER(screenrect);

    glBindTexture(GL_TEXTURE_2D, glaretex.rendertex);

    float g = glarescale*glaremod;
    gle::colorf(g, g, g);

    screenquad(1, 1);

    glDisable(GL_BLEND);
}
     
