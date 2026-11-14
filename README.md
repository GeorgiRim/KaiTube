# KaiTube

<p align="center"><img src="resources/icon/icon.jpg" width="160" alt="KaiTube"></p>

Experimental native YouTube client for **Nintendo Switch 2**, by **Sank-kab**. Based on [switch-newpipe](https://github.com/mirusu400/switch-newpipe) and its dependencies. Requires KaiHEN compatible with ounce-devkit **20261114**.

## Status

Beta **0.0.1**, SDK **20261114** migration branch (awaiting console validation). The developer reports successful console testing of the uploaded beta. This repository removes unused proprietary font/demo assets from the resource bundle; that packaging change needs a fresh console test before a binary release. No release binary is currently provided.

## Features

- Public search and browsing; personalized Home, subscriptions and read-only YouTube history using locally imported cookies.
- Local history, saved videos, recent searches and playback positions.
- Quality and playback-speed selection, subtitles when retrievable, automatic decoder selection and resume/start-over.
- Sidebar navigation and menus in English, Italian, French, German, Spanish, Portuguese and Korean.

Authenticated feeds load up to 96 items and at most seven continuation pages. Account playlists and synchronized search history are not implemented. YouTube changes may break extraction or account feeds. HD performance depends on codec, decoder support and network.

## Build

On Linux, install Docker and Python 3. Clone this repository, then download the pinned public SDK and build:

```bash
bash scripts/download-sdk.sh
bash build.sh
```

Output: `output/KaiTube-beta0.0.1.nro`. Build log: `output/logs/build-native.log`. The `--public` argument remains compatible and requires no Google OAuth client. The build checks the SDK hash, translations, Switch 2 linkage, icon, author and NACP version.

Copy the NRO to `/switch/` on your SD. Keep a working backup. Existing settings and session paths are preserved.

## Account setup

Use a private desktop-browser window with only the intended YouTube account signed in. Confirm Home and subscriptions work there. Export the YouTube cookies to `/switch/kaitube_native_auth.txt` on the SD. Supported formats: Cookie header value, JSON cookie_header/cookies/array, Netscape cookies.txt. Open Settings → YouTube cookies → Import cookies, then restart KaiTube.

Cookies are login credentials. Never send them to anyone or include them in issues, screenshots or releases. They are stored unencrypted on the SD in `/switch/kaitube_native_session.json`. Delete the import file after importing. To sign out, disconnect in settings, remove the import file and restart. Existing working sessions do not need reimporting after an update.

## Player controls

| Input | Action |
|---|---|
| A | Pause/resume |
| B | Hide controls; exit player when controls are hidden |
| Y | Timeline |
| L / R | Seek −10 / +10 seconds |
| ZL / ZR | Seek −30 / +30 seconds in timeline preview |
| Stick or D-pad left/right | Move timeline preview |
| Plus | Exit confirmation |

Controls appear on pause, touch and controller interaction. B can cancel initial loading.

## Reporting issues

Include the app/SDK/KaiHEN versions, reproduction steps and a sanitized log from `/switch/KaiTube-Native/logs/`. Logs may contain video titles, searches and URLs. Do not include cookie files, session JSON or account credentials. See [SECURITY.md](SECURITY.md).

## License and attribution

See [LICENSE.MD](LICENSE.MD), [THIRD_PARTY.md](THIRD_PARTY.md) and dependency license files in vendor. Preserve their notices. Nintendo/Morisawa font files, demo Pokémon images and the external SDK archive are excluded from this repository. Nintendo system fonts are accessed through the console at runtime by Borealis. This is an unofficial project, unaffiliated with Nintendo or Google.
