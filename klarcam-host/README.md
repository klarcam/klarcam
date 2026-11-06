<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Lars von Thienen -->
# KlarCam filter host for OBS Studio

The OBS Studio module of KlarCam (`klarcam.dll`): it registers the "KlarCam" filter, shows
its properties and hands every frame to the KlarCam engine (`engine/klarcam-engine.exe`,
installed beside it), which processes it and hands it back within the same OBS frame. The
image processing, the face tracking and the license live in the engine, a separate
program; this module is the part that links OBS.

KlarCam is third-party software. It is not part of OBS Studio and is not made by,
affiliated with or endorsed by the OBS Project.

License: GNU General Public License, version 2 or (at your option) any later version
(`COPYING`, the complete text of version 2). The protocol headers in `protocol/` are under
the MIT license (their headers say so), so the engine can use them. How the two talk:
`PROTOCOL.md`.

## Files

| Path | What |
|---|---|
| `src/plugin-main.cpp` | module entry points |
| `src/klarcam-filter.cpp` | the filter: properties, settings, the per-frame hand-off, fade-in and fallbacks |
| `src/engine-link.cpp` | starts and supervises the engine, the pipes, requests |
| `src/filter-defaults.h` | setting keys and defaults |
| `protocol/` | the engine protocol (MIT) |
| `data/locale/en-US.ini` | the texts |
| `data/effects/klarcam-fade.effect` | the fade from the source to the processed picture |
| `tools/make-libobs-sdk.ps1` | the libobs headers and import library for the build |

## Build

Windows, Visual Studio 2022 with the C++ tools, CMake 3.28, git, and an installed OBS
Studio. `tools/make-libobs-sdk.ps1` makes the libobs headers (the obs-studio tag of the
installed version) and the import library `obs.lib` (from the installed `obs.dll`) in
`.deps/libobs-sdk`, where the build looks; `-DLIBOBS_SDK_DIR=<folder>` points it elsewhere.

```
powershell -File tools/make-libobs-sdk.ps1
cmake -S . -B build
cmake --build build --config RelWithDebInfo
cmake --install build --config RelWithDebInfo --prefix <OBS plugins folder>
```

The module needs the KlarCam engine (`engine/`) and its data (`data/`) beside it, which
the KlarCam setup installs (`%ProgramData%\obs-studio\plugins\klarcam`). Without the
engine the filter passes the video through and says so in its properties and in the OBS
log.

Rendering needs Direct3D 11 (OBS's default renderer on Windows).
