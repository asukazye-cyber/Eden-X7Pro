# Eden-X7Pro V0

Reproducible Android baseline build for a POCO X7 Pro with a Dimensity
8400-Ultra and Mali-G720 MC7. V0 deliberately builds the current upstream
**optimized** Eden Android flavor without unverified Mali driver or renderer
hacks. It exists to create a clean benchmark before targeted changes are made.

## Build

Run **Build Eden X7Pro V0** from the repository's Actions tab. The workflow
clones `eden-emulator/mirror` with submodules, installs the Android NDK 27.2,
CMake 3.22.1 and Java 17, then invokes Eden's supported Android build script.
The produced APK is retained as an Actions artifact for 30 days.

## V0 behavior

- Android flavor: `optimized`
- Build type: `Release`
- Source: current `master` of `eden-emulator/mirror` (selectable when starting
  the workflow)
- No custom GPU driver is installed or changed.
- No game files, keys, firmware, saves, mods or proprietary content belong in
  this repository.

The `profiles/` and `benchmarks/` files record the initial X7 Pro test plan;
they do not silently alter Eden settings.

Eden is GPL-3.0-or-later. This repository contains only its own build wrapper
and documentation; the workflow obtains the upstream source at build time.
