# Fallout 1 & 2 Community Edition — Controller Edition

This repository contains modified versions of
[Fallout Community Edition](https://github.com/alexbatalov/fallout1-ce) and
[Fallout 2 Community Edition](https://github.com/alexbatalov/fallout2-ce) with
native SDL controller support.

You must own Fallout or Fallout 2 to play. No original game data, music, maps,
art, saves, or executables are included.

## Features

- Broad Xbox, PlayStation, Nintendo, Steam Input, 8BitDo, and generic controller
  support through SDL and the bundled SDL_GameControllerDB mappings.
- Direct left-stick character movement with walk/run pressure, native collision,
  combat AP use, and L3 switching back to point-and-click control.
- Smooth right-stick camera panning, gradual character follow, and no competing
  edge-scroll jumps.
- The red movement hex stays hidden during direct controller movement. L3 or
  real mouse movement restores it; interaction and targeting cursors still work.
- A Fallout-style controller panel with remapping, settings, actions, help, and
  an on-screen keyboard.
- Borderless desktop fullscreen, Alt+Enter switching, aspect-ratio preservation,
  black letterboxing, and renderer/device-reset recovery.
- A fix for a Fallout 1 enemy-turn crash caused by an uninitialized optional
  target pointer.

See each engine's `CONTROLLER.md` for the complete control reference:

- [Fallout 1 controller guide](fallout1-ce/CONTROLLER.md)
- [Fallout 2 controller guide](fallout2-ce/CONTROLLER.md)

## Build

Each directory is a standalone CMake project. Its existing CMake configuration
resolves the required libraries. Fallout 1 uses system SDL2 on Linux; Fallout 2
uses vendored dependencies by default and supports `FALLOUT_VENDORED=OFF` for
system packages.

```powershell
cmake -S fallout1-ce -B build/fallout1 -DFALLOUT_BUILD_CONTROLLER_TESTS=ON
cmake --build build/fallout1 --config Release
ctest --test-dir build/fallout1 -C Release --output-on-failure

cmake -S fallout2-ce -B build/fallout2 -DFALLOUT_BUILD_CONTROLLER_TESTS=ON
cmake --build build/fallout2 --config Release
ctest --test-dir build/fallout2 -C Release --output-on-failure
```

The tested Windows release builds used CMake 4.4.3, LLVM-MinGW 20260908,
SDL 2.32.10, and `-fno-strict-aliasing`. The controller module requires SDL
2.26 or newer when using a system SDL package.

The `FALLOUT_CONTROLLER_SMOKE_DRIVER` option is only for isolated integration
testing with legally obtained game data. Leave it disabled in normal builds.

## Install

Follow the upstream installation instructions in each engine directory. In
brief, copy the built Community Edition executable and the generated
`gamecontrollerdb.txt` beside your legally installed game data. The executable
is a replacement engine and cannot run without those original assets.

Controller settings are stored in `controller.ini` beside the executable when
that file exists, or in SDL's per-user preferences directory otherwise. Open
the panel with Back/Share/Minus or F11. Alt+Enter toggles borderless fullscreen.

## Repository layout

- `fallout1-ce/` — complete modified Fallout 1 CE source.
- `fallout2-ce/` — complete modified Fallout 2 CE source.
- `integrate.py` — keeps the shared controller module and tests synchronized.
- `check_motion.py` and `check_camera.py` — validate traces from the opt-in live
  test driver.

## Upstream and license

The Fallout 1 tree is based on upstream commit
`0609bcfd0ec40ff0571d0f57fab2821eb461dc8b`. The Fallout 2 tree is based on
`e97087b9582f37075db347a89898887320753f8b`.

Both engines retain the upstream Sustainable Use License. Distribution must be
free of charge and for non-commercial purposes, subject to its full terms. See
[LICENSE.md](LICENSE.md), the license copies inside each engine, and third-party
license files under each engine's `third_party` directory.

Fallout is a trademark of its respective owner. This project is an unofficial
community modification and is not affiliated with or endorsed by Bethesda,
Interplay, or the upstream Community Edition maintainer.
