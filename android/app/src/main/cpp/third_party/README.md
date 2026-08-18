# Vendored SDL2 dependencies (Android, arm64-v8a)

Sauerbraten's desktop build links against SDL2, SDL2_image, and SDL2_mixer
(headers at `../../../../../../../src/include`, snapshotted from SDL 2.0.12
for the Windows build). Those APIs are stable across the whole SDL2 line, so
for Android these are vendored as source at newer, actively-maintained
releases with solid modern-NDK/Clang support, rather than pinning to 2.0.12.

## What's vendored, and why these versions

| Library | Path | Tag |
|---|---|---|
| SDL2 | `SDL2/` | `release-2.30.11` |
| SDL2_image | `SDL2_image/` | `release-2.8.2` |
| SDL2_mixer | `SDL2_mixer/` | `release-2.8.1` |

All three are zlib-licensed, same as Sauerbraten itself — no licensing
interaction with `../../../../../NOTICE.md`'s GPLv2 note (that's only about
the OpenXR VR glue code).

## Codec scope: JPG+PNG, OGG+WAV only, zero extra codec source

Determined by grepping the actual asset packages (`packages/` has 5557 JPG +
838 PNG images, 125 OGG + 89 WAV audio files, nothing else), and confirming
the engine only calls the generic format-sniffing dispatchers
(`IMG_LoadTyped_RW`/`IMG_Load` in `src/engine/texture.cpp:1357,1362`;
`Mix_LoadMUSType_RW`/`Mix_LoadMUS`/`Mix_LoadWAV_RW`/`Mix_LoadWAV` in
`src/engine/sound.cpp:265-317`) — so as long as the libraries are *built*
with JPG/PNG/OGG/WAV support, no engine code needs to change.

The good news: modern SDL2_image and SDL2_mixer bundle **stb_image** and
**stb_vorbis** (single-header, public-domain, part of their own source
tree already) as an alternative to linking external libjpeg/libpng/
libogg/libvorbis. Their `Android.mk` defaults already select exactly this:

- SDL2_image: `USE_STBIMAGE ?= true` → PNG+JPG via `src/IMG_stb.c` +
  `src/stb_image.h`. All the options that need external source
  (`SUPPORT_JPG`/`SUPPORT_PNG`/`SUPPORT_WEBP`/`SUPPORT_AVIF`/`SUPPORT_JXL`,
  which point at empty `external/jpeg`, `external/libpng`, etc.
  placeholder dirs) already default to `false` upstream — nothing to
  override.
- SDL2_mixer: `SUPPORT_OGG_STB ?= true` → OGG via `src/codecs/stb_vorbis.h`
  (bundled) and `SUPPORT_WAV ?= true` → WAV is built into SDL_mixer core,
  no extra lib. **This one required overriding four upstream defaults**
  that are `true` but point at empty `external/` placeholders (would have
  broken the build): `SUPPORT_GME`, `SUPPORT_WAVPACK`,
  `SUPPORT_MP3_MINIMP3`, `SUPPORT_FLAC_DRFLAC` are all forced to `false`
  in `Android.mk` (see below), alongside the already-false-by-default
  `SUPPORT_OGG` (Tremor), `SUPPORT_MP3_MPG123`, `SUPPORT_MOD_XMP`,
  `SUPPORT_MID_TIMIDITY`, `SUPPORT_FLAC_LIBFLAC`.

**zlib** is not vendored — it's provided by the NDK sysroot as a system
library (`-lz`), which is what SDL2_image's PNG path would link against
if `SUPPORT_PNG` (libpng) were ever turned on later; stb_image doesn't need
it for decoding.

Net result: **no libjpeg/libpng/libogg/libvorbis source was vendored at
all** — every codec needed is already self-contained inside the SDL2_image/
SDL2_mixer trees.

## Build quirks hit and fixed

1. **Windows command-line length limit.** The first standalone build attempt
   used a deeply-relative `NDK_OUT`/`NDK_LIBS_OUT` (nine `../` segments back
   up to the drive root) — with ~150 SDL2 source files, the resulting link
   command line blew past Windows' `CreateProcess` argument limit and failed
   silently-ish (`process_begin: CreateProcess(...)` with no further detail).
   Fix: always pass a short **absolute** `NDK_OUT`/`NDK_LIBS_OUT` when
   invoking `ndk-build` directly (see the command below) — this is purely a
   standalone-test-invocation concern, not something `Android.mk` itself
   needs to handle, since Gradle's own `externalNativeBuild` picks its own
   (already-absolute) output paths.
2. **`LOCAL_PATH` pollution across `include`s.** `SDL2/Android.mk` ends with
   `$(call import-module,android/cpufeatures)`, which does **not** restore
   `LOCAL_PATH` afterward — a manual `include $(LOCAL_PATH)/SDL2_image/Android.mk`
   line right after including SDL2's ended up resolving against the NDK's
   cpufeatures directory instead of `third_party/`. Fixed by switching to
   the same pattern SDL2's own `android-project/app/jni/Android.mk` sample
   uses: `include $(call all-subdir-makefiles)`, which lets the build
   system manage per-file `LOCAL_PATH`/`my-dir` context correctly instead of
   manual sequential `include`s. This file (`Android.mk` in this directory)
   is intentionally that one line (plus the `SUPPORT_*` overrides above) —
   don't replace it with manual includes.
3. **SDL2_mixer's default codec flags assume `external/*` source that isn't
   vendored here** (see above) — fixed via explicit `:=` overrides *before*
   the `all-subdir-makefiles` include, since SDL2_mixer's own `Android.mk`
   uses `?=` (first-definition-wins), so defining them first here takes
   priority over its own defaults.

## Module names (for later `Android.mk` integration)

Each library builds both a shared and a static variant:

| Shared module | Static module | Static output filename |
|---|---|---|
| `SDL2` | `SDL2_static` | `libSDL2.a` |
| — | `SDL2_main` | `libSDL2main.a` (SDL's `main()` shim; Sauerbraten defines its own `main()` in `engine/main.cpp`, so this is likely not needed) |
| `SDL2_image` | `SDL2_image_static` | `libSDL2_image.a` |
| `SDL2_mixer` | `SDL2_mixer_static` | `libSDL2_mixer.a` |

Given the rest of this Android port statically links Sauerbraten's own code
into a single `libsauerquest.so` (see `../Android.mk`'s `sauerengine`/`enet`
static-library modules), the `*_static` variants
(`SDL2_static`/`SDL2_image_static`/`SDL2_mixer_static`) are almost certainly
the right ones to pull in via `LOCAL_STATIC_LIBRARIES` there, rather than
shipping separate `.so`s in `jniLibs`.

## Verifying this in isolation

This directory's `Android.mk`/`Application.mk` are a **standalone test
harness**, not wired into the real app build
(`android/app/src/main/cpp/Android.mk` doesn't reference `third_party/` at
all yet — that integration is a separate step). To re-verify these three
libraries still build cleanly on their own:

```sh
NDK=/c/Users/ssiew_000/AppData/Local/Android/Sdk/ndk/25.1.8937393
cd android/app/src/main/cpp/third_party
"$NDK/ndk-build.cmd" \
  NDK_PROJECT_PATH=null \
  APP_BUILD_SCRIPT="$(pwd)/Android.mk" \
  NDK_APPLICATION_MK="$(pwd)/Application.mk" \
  APP_ABI=arm64-v8a \
  NDK_OUT="E:/sq_scratch/obj" \
  NDK_LIBS_OUT="E:/sq_scratch/lib" \
  V=0 -j8 SDL2 SDL2_static SDL2_main SDL2_image SDL2_image_static SDL2_mixer SDL2_mixer_static
```

(`NDK_OUT`/`NDK_LIBS_OUT` must be short absolute paths — see quirk #1 above.
`E:/sq_scratch` is scratch space outside the repo; delete it freely.)

Confirmed working: all seven modules compile and link successfully for
arm64-v8a with this exact invocation.
