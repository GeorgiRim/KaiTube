# KaiTube v0.0.1-beta

Experimental native YouTube client for Nintendo Switch 2 by Sank-kab, built with ounce-devkit 20261114. Requires compatible KaiHEN; this is not a Switch 1 binary.

Copy `KaiTube-beta0.0.1.nro` to `/switch/` on the SD. For account access follow docs/COOKIES.md; never share cookie/session files. Decoder Auto is recommended for HD playback. Available qualities, captions and performance depend on video format, network and decoder support.

Includes sidebar UI, public search, cookie-based personalized Home/subscriptions/read-only YouTube history, local saved videos/searches/resume, quality/speed/subtitle controls and timeline seeking. Authenticated feeds are capped at 96 items/seven continuations. Account playlists and cloud search history are not implemented.

The developer reports successful console testing and fresh-cookie login. SDK migration, cleaned resources and SD save replacement handling are included. See BINARY-AUDIT.md for the exact checked binary hash and limitations.

Unofficial and experimental; not affiliated with Nintendo or Google. Corresponding source is in this repository; dependency notices and external-source requirements apply. No cookies, session files or SDK archive belong in release assets.
