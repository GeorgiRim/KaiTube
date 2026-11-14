#!/usr/bin/env python3
import json
from pathlib import Path

commands = json.loads(Path('cmake-build-switch2/compile_commands.json').read_text())
main = next(c for c in commands if c['file'].endswith('/src/main.cpp'))
assert '-D__SWITCH2__' in main.get('command', ' '.join(main.get('arguments', []))), 'Missing Switch 2 define'
link_map = Path('cmake-build-switch2/kaitube-native.map').read_text()
assert '/libounce/lib/libounce.a' in link_map, 'Bundled libounce was not linked'
assert '/libnx/lib/libnx.a' not in link_map, 'Switch 1 libnx was linked'
for lib in ('EGL', 'GLESv2', 'glapi', 'drm_nouveau', 'vulkan'):
    forbidden = f'/portlibs/switch/lib/lib{lib}.a'
    assert forbidden not in link_map, f'Switch 1 GPU library referenced: {forbidden}'
print('Build target checked: __SWITCH2__ and libounce archive.')

expected=json.loads(Path('sdk/build-sdk.json').read_text())
installed=Path('/opt/devkitpro/ounce-devkit-version.txt').read_text().splitlines()
assert installed[0]=='ounce-devkit '+expected['version'], 'Unexpected installed SDK version'
for component in expected['components']: assert component in installed, 'Unexpected SDK component: '+component
print('Installed SDK and component versions checked:', expected['version'])

# Check the actual produced NRO's embedded hbmenu presentation.
import struct
nro=Path('cmake-build-switch2/switch_newpipe.nro').read_bytes()
assert nro[0x10:0x14]==b'NRO0','Invalid NRO header'
asset=struct.unpack_from('<I',nro,0x18)[0]
assert nro[asset:asset+4]==b'ASET','Missing NRO assets'
icon_offset,icon_size,nacp_offset,nacp_size=struct.unpack_from('<QQQQ',nro,asset+8)
assert nro[asset+icon_offset:asset+icon_offset+icon_size]==Path('resources/icon/icon.jpg').read_bytes(),'Unexpected hbmenu icon'
nacp=nro[asset+nacp_offset:asset+nacp_offset+nacp_size]
def cstr(data): return data.split(b'\0',1)[0].decode('utf-8')
assert cstr(nacp[:512])=='KaiTube','Unexpected hbmenu title'
assert cstr(nacp[512:768])=='Sank-kab','Unexpected hbmenu author'
assert cstr(nacp[0x3060:0x3070])=='beta0.0.1','Unexpected hbmenu version'
print('NRO presentation checked: KaiTube / Sank-kab / beta0.0.1 and matching icon.')
