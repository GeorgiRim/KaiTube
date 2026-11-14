#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
mkdir -p sdk
file=sdk/ounce-devkit-20261114.tar.xz
if [[ -f "$file" ]] && python3 scripts/check-sdk.py; then
    exit 0
fi
url=https://github.com/ouncebrew/ounce-devkit/releases/download/devkit-20261114/ounce-devkit-20261114.tar.xz
trap 'rm -f "$file.part"' EXIT
curl --fail --location --retry 3 --output "$file.part" "$url"
printf '%s  %s\n' 'f24d5773b1ee1c2b19317f57e8ae54a60fcdcefe493a0e495297c241c4912f59' "$file.part" | sha256sum --check
mv "$file.part" "$file"
python3 scripts/check-sdk.py
