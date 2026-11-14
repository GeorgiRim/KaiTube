#!/usr/bin/env bash
set -euo pipefail
cd /work
export PATH="/opt/devkitpro/devkitA64/bin:$PATH"
rm -f resources/google-client.json
echo "Account: cookie import only; OAuth disabled."
generator_src=vendor/borealis/library/lib/extern/libromfs/generator
CC=/usr/bin/gcc CXX=/usr/bin/g++ cmake -S "$generator_src" -B .build-native-generator -DCMAKE_BUILD_TYPE=Release
cmake --build .build-native-generator -j"$(nproc)"
cp .build-native-generator/libromfs-generator vendor/borealis/libromfs-generator
python3 scripts/check-sdk.py
python3 scripts/check-translations.py
python3 - <<'PYSDK'
import hashlib,shutil
from pathlib import Path
build=Path('cmake-build-switch2'); marker=build/'.sdk-sha256'
sha=hashlib.sha256(Path('sdk/ounce-devkit-20261114.tar.xz').read_bytes()).hexdigest()
if build.exists() and (not marker.exists() or marker.read_text().strip()!=sha):
    print('SDK changed: discarding the previous Switch 2 build directory')
    shutil.rmtree(build)
build.mkdir(exist_ok=True);marker.write_text(sha+'\n')
PYSDK
source /opt/devkitpro/switch2vars.sh
cmake -S . -B cmake-build-switch2 \
    -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/libounce/cmake/Switch2.cmake \
    -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON \
    -DUSE_DEKO3D=OFF -DUSE_SDL2=ON -DUSE_GLFW=OFF -DUSE_GLES2=ON -DUSE_EGL=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build cmake-build-switch2 -j"$(nproc)" --target switch_newpipe.nro
# Verify the app was compiled for Switch 2 and linked the bundled libounce.
python3 scripts/verify-build.py
mkdir -p output
cp cmake-build-switch2/switch_newpipe.nro output/KaiTube-beta0.0.2.nro
cp cmake-build-switch2/kaitube-native.map output/
cp cmake-build-switch2/switch_newpipe.elf output/KaiTube-Native.elf
cp /opt/devkitpro/ounce-devkit-version.txt output/
(cd output && sha256sum KaiTube-beta0.0.2.nro) | tee output/SHA256SUMS
