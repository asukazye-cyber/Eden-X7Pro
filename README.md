<!--
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2018 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
-->
<!-- lang: en-GB -->

<h1 align="center">
  <br>
  <img src="./dist/qt_themes/default/icons/256x256/eden.png" alt="X7NX" width="200">
  <br>
  <b>X7NX</b>
  <br>
</h1>

<h4 align="center"><b>X7NX</b> is an experimental GPLv3 Android ARM64 fork of Eden for the POCO X7 Pro,
Dimensity 8400-Ultra and ARM Mali-G720 MC7.
</h4>

<p align="center">
    </a>
    <a href="https://discord.gg/HstXbPch7X">
        <img src="https://img.shields.io/discord/1367654015269339267?color=5865F2&label=Eden&logo=discord&logoColor=white"
            alt="Discord">
    </a>
    <a href="https://stt.gg/qKgFEAbH">
        <img src="https://img.shields.io/revolt/invite/qKgFEAbH?color=d61f3a&label=Stoat"
            alt="Stoat">
    </a>
</p>

<p align="center">
  <a href="#scope">Scope</a> |
  <a href="#building">Building</a> |
  <a href="#download">Download</a> |
  <a href="#support">Support</a> |
  <a href="#license">License</a>
</p>

## Scope

X7NX keeps Eden's mature emulation components while owning an Android-only build, a standalone
`org.x7nx.emulator` install package, a Dimensity all-big-core scheduler policy and an X7/Mali Vulkan
backend boundary. It is not a general-purpose Eden release and it makes no performance claim before
replay measurements are published.

See [the fork scope](docs/X7NX_FORK_SCOPE.md), [Phase 1 architecture](docs/X7NX_PHASE1_ARCHITECTURE.md)
and [Phase 2 renderer work](docs/X7NX_PHASE2_RENDERER.md).

The only supported build target is Android ARM64 on the POCO X7 Pro. The source contains no games,
keys or copyrighted game assets.

[![Packaging status](https://repology.org/badge/vertical-allrepos/eden-emulator.svg)](https://repology.org/project/eden-emulator/versions)

## Lineage and contribution

X7NX is derived from Eden and retains the required GPLv3 notices. Contributions should preserve
correctness first: unknown drivers and unvalidated Vulkan paths must use safe fallbacks.

## Documentation

We have a user manual! See our [User Handbook](./docs/user/README.md).

## Building

Use the X7NX task: `cd src/android && ./gradlew copyX7nxReleaseOutputs`.

For information on provided development tooling, see the [Tools directory](./tools)

## Download

GitHub Actions publishes the unsigned experimental APK as a workflow artifact. See
[download instructions](docs/X7NX_DOWNLOAD.md).

Save us some bandwidth! We have [mirrors available](./docs/user/ThirdParty.md#mirrors) as well.

## License

X7NX is licensed under the GPLv3 (or any later version). See [LICENSE.txt](LICENSE.txt).

## Special thanks

Super special thanks to Cloudflare for preventing the git server from blowing up.

- Yuzu
- Ryujinx
- Sudachi
- Citron
- Torzu
- Suyu
- Ryubing

And everyone who continues or had contributed to the project! <3
