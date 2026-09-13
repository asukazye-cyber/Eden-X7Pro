# Eden-X7Pro

Reproducible Android builds for a POCO X7 Pro with a Dimensity 8400-Ultra and
Mali-G720 MC7. V0 is a clean upstream benchmark. V1 applies a small,
source-reviewed Android preset while preserving Eden's normal settings UI.

## Build

Run **Build Eden X7Pro V1** from the repository's Actions tab. The workflow
clones `eden-emulator/mirror` with submodules, installs the Android NDK 27.2,
CMake 3.22.1 and Java 17, applies the V1 patch, then invokes Eden's supported
Android build script. The produced APK is retained as an Actions artifact for
30 days.

## Versions

- **V0:** upstream `optimized` Android flavor, with no code changes.
- **V1:** V0 plus these fresh-install defaults for this X7 Pro-targeted APK:
  - 3 Vulkan pipeline workers;
  - asynchronous GPU emulation enabled;
  - asynchronous shader compilation enabled.

V1 does not install or replace a GPU driver, and every changed setting remains
visible and reversible in Eden's Android settings. It is an initial performance
profile, not a promise of a particular frame rate or graphics compatibility.

No game files, keys, firmware, saves, mods or proprietary content belong in
this repository.

The `profiles/` and `benchmarks/` files record the initial X7 Pro test plan;
they do not silently alter Eden settings.

Eden is GPL-3.0-or-later. This repository contains only its own build wrapper
and documentation; the workflow obtains the upstream source at build time.
