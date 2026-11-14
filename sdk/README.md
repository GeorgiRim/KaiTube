# SDK 20261114

Run `bash scripts/download-sdk.sh` from the repository root. It downloads the official [ounce-devkit 20261114 release](https://github.com/ouncebrew/ounce-devkit/releases/tag/devkit-20261114) and verifies the pinned SHA-256. Alternatively place `ounce-devkit-20261114.tar.xz` in this directory yourself.

SHA-256: `f24d5773b1ee1c2b19317f57e8ae54a60fcdcefe493a0e495297c241c4912f59`

Verified archive metadata:
- ounce-devkit 20261114
- libounce 9382b1ec48ac
- ouncevk f5afcd27a98d
- kaihen eb81e1912586

The version marker is now `ounce-devkit-version.txt`; build scripts and Docker image tags have been updated accordingly. A change in archive hash discards the old app build directory automatically. Docker SDK/portlib layers are rebuilt as necessary.

The release fixes premature GPU-wait completion reporting device loss. Rebuild programs to pick up the statically linked driver fix. This branch awaits a full target build and console test; the upgrade is not claimed to be console-validated yet.

Public sources: [ounce-devkit](https://github.com/ouncebrew/ounce-devkit), [libounce](https://github.com/ouncebrew/libounce), [OunceVK](https://github.com/ouncebrew/ouncevk), [libkaihen](https://github.com/ouncebrew/kaihen/tree/main/libkaihen). Scripts are MIT, libounce/libkaihen carry permissive notices and OunceVK/Mesa has component-specific licenses. Preserve applicable license/source obligations. Nintendo/Morisawa font copies remain excluded.
