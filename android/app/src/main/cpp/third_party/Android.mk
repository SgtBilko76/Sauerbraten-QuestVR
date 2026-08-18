# SDL2_mixer/Android.mk's codec flags default several codecs to true
# (GME, WavPack, MP3-via-minimp3, FLAC-via-dr_flac) that need external/
# source trees we haven't vendored (packages/ only uses OGG+WAV, per the
# project plan's Phase 2 scope). These are plain `?=` in SDL2_mixer's own
# Android.mk, so defining them here first (before all-subdir-makefiles
# reaches it) wins. OGG_STB (stb_vorbis, bundled, header-only) and WAV
# (built into SDL_mixer core) are the only codecs this build enables.
SUPPORT_WAV := true
SUPPORT_OGG_STB := true
SUPPORT_FLAC_DRFLAC := false
SUPPORT_FLAC_LIBFLAC := false
SUPPORT_OGG := false
SUPPORT_MP3_MINIMP3 := false
SUPPORT_MP3_MPG123 := false
SUPPORT_WAVPACK := false
SUPPORT_GME := false
SUPPORT_MOD_XMP := false
SUPPORT_MID_TIMIDITY := false

# Standard SDL2 multi-library pattern (matches SDL2's own
# android-project/app/jni/Android.mk sample): each direct child directory's
# Android.mk is included via the build system's own subdir-recursion, which
# -- unlike a manual sequence of `include $(LOCAL_PATH)/X/Android.mk` lines
# -- correctly manages LOCAL_PATH/my-dir context around each library's own
# $(call import-module,...) calls (e.g. SDL2/Android.mk's cpufeatures
# import, which otherwise leaks a stale LOCAL_PATH into whatever is
# included next).
include $(call all-subdir-makefiles)
