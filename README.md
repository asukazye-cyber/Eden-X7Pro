# Eden-X7Pro

Reproducible Android builds for a POCO X7 Pro with a Dimensity 8400-Ultra and
Mali-G720 MC7. V0 is a clean upstream benchmark. V1 applies a small,
source-reviewed Android preset while preserving Eden's normal settings UI.

## Build

Run **Build Eden X7Pro V2 Experimental** from the repository's Actions tab. The workflow
clones `eden-emulator/mirror` with submodules, installs the Android NDK 27.2,
CMake 3.22.1 and Java 17, applies the V2 patches, then invokes Eden's supported
Android build script. The produced APK is retained as an Actions artifact for
30 days. After a successful V2 build, **Publish Eden X7Pro V2 APK** turns that
verified artifact into the `v0.2.0` prerelease, with a direct APK download.

## Versions

- **V0:** upstream `optimized` Android flavor, with no code changes.
- **V1:** V0 plus these fresh-install defaults for this X7 Pro-targeted APK:
  - 3 Vulkan pipeline workers;
  - asynchronous GPU emulation enabled;
  - asynchronous shader compilation enabled.
- **V2 Experimental:** V1 plus asynchronous presentation for fresh installs,
  and a renderer change gated to a proprietary ARM **Mali-G720** driver. When
  `VK_EXT_descriptor_buffer` and buffer-device address support are exposed, V2
  uses a 3 MiB per-frame descriptor-ring budget instead of Eden's generic 2 MiB
  mobile-tiler budget. Across eight frames in flight this reserves at most an
  additional 8 MiB of host-visible memory and can avoid a descriptor-ring
  exhaustion stall in descriptor-heavy scenes. It has no effect on drivers that
  do not expose that path.

V2 does not install or replace a GPU driver, raise clocks, or disable Eden's
ARM synchronization safeguards. Every changed setting remains visible and
reversible in Eden's Android settings. It is an experimental renderer profile,
not a promise of a particular frame rate or graphics compatibility. Test it at
1x first, then use 1.25x only if the same demanding scene remains stable.

No game files, keys, firmware, saves, mods or proprietary content belong in
this repository.

The `profiles/` and `benchmarks/` files record the initial X7 Pro test plan;
they do not silently alter Eden settings.

Eden is GPL-3.0-or-later. This repository contains only its own build wrapper
and documentation; the workflow obtains the upstream source at build time.
