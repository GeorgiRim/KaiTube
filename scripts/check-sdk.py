#!/usr/bin/env python3
import hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
expected=json.loads((root/'sdk/build-sdk.json').read_text())
actual=hashlib.sha256((root/'sdk/ounce-devkit-20261114.tar.xz').read_bytes()).hexdigest()
if actual!=expected['sha256']:raise SystemExit('SDK errato o incompleto: riapplica il pacchetto SDK 20261114')
print('SDK archive verified:',expected['version'],actual)
