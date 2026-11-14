# External SDK prerequisite

The public SDK project is [ouncebrew/ounce-devkit](https://github.com/ouncebrew/ounce-devkit), with precompiled archives in its [releases](https://github.com/ouncebrew/ounce-devkit/releases). Sources: [libounce](https://github.com/ouncebrew/libounce), [OunceVK](https://github.com/ouncebrew/ouncevk), and [kaiHEN/libkaihen](https://github.com/ouncebrew/kaihen/tree/main/libkaihen).

## Current KaiTube build pin

This beta still expects the previously supplied `ounce-sdk.tar.xz` for 20261113: libounce db3f88a3, ouncevk 791c34e, ouncehen 9451c44. Place that archive here. Scripts validate its SHA-256 against build-sdk.json and SDK.sha256.

The public devkit-20261114 release is a DIFFERENT archive: SHA-256 f24d5773b1ee1c2b19317f57e8ae54a60fcdcefe493a0e495297c241c4912f59, libounce 9382b1ec, OunceVK f5afcd27, kaiHEN eb81e191. The public 20261113f archive also differs. Simply renaming either public archive will not satisfy this build's hash/component checks. Migration requires updating and testing the toolchain integration; no such migration is claimed here.

The 20261114 release notes report a fix for GPU waits incorrectly reporting device loss, and require rebuilding programs to pick up the statically linked driver fix.

## Licenses

ounce-devkit's scripts are MIT; libounce and libkaihen carry permissive notices. OunceVK/Mesa has component-specific licenses. Preserve applicable notices and corresponding-source obligations. These public repositories do not license Nintendo/Morisawa fonts bundled independently by other libraries. The SDK archive remains outside this repository to avoid duplicate binary storage.
