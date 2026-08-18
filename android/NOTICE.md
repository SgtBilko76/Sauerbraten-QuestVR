# Licensing note for android/

Sauerbraten's own source (`../src`, `../data`) is zlib-licensed — see
`../src/readme_source.txt`.

The VR glue code under `app/src/main/cpp/vr_glue/` (`TBXR_Common.c/h`,
`OpenXrInput.c`) is adapted from
[QuakeQuest](https://github.com/Team-Beef-Studios/QuakeQuest)
(Team Beef Studios), which is **GPLv2-licensed**. Per the project owner's
explicit decision, this code is reused directly (not reimplemented from
scratch) to move faster, since it is already tested on Quest hardware.

**Consequence: as soon as this VR port is distributed to anyone else in any
form (APK or source), the combined work must comply with GPLv2** — source
availability, no closed/commercial redistribution, GPLv2 license text
retained. This is a non-issue for personal use/sideloading on your own
headset, but must be revisited before any public or commercial release.

`app/src/main/cpp/vr_glue/reference/` holds the original, unmodified
QuakeQuest source files (including `argtable3.c/h`, BSD-3-licensed, and
`QuakeQuest_OpenXR.c`, GPLv2, both currently unused/not compiled) kept for
comparison while porting; they are not part of the build
(see `Android.mk`'s `LOCAL_SRC_FILES`).

`app/src/main/cpp/openxr_sdk/` holds Khronos OpenXR-SDK headers
(Apache-2.0 / MIT), vendored via QuakeQuest's copy for convenience — these
are unmodified upstream interface headers, not GPL.
