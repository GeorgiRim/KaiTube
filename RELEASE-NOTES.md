# KaiTube v0.0.2-beta — Home Refresh Fix

- Fixed explicit Home refresh after returning from a video: X requests a fresh feed instead of restoring the previous grid.
- Fixed category changes reusing the restored Home grid.
- Kept the existing grid/focus restoration when returning from the player.
- Updated the cookie guide with the private-window / robots.txt export procedure and closing the window without logging out.
- Version shown in hbmenu and output filename updated to beta0.0.2.

YouTube may still return the same recommendations even after a successful refresh.

Built with ounce-devkit 20261114; requires compatible KaiHEN on Nintendo Switch 2. Copy KaiTube-beta0.0.2.nro to /switch/. Keep decoder mode on Automatic for HD playback. See docs/COOKIES.md for account setup. Existing working sessions retain their paths and do not require reimporting for this update.

The developer reports successful console testing of the versioned 0.0.2 build. Static binary inspection and the uploaded checksum match; see BINARY-AUDIT.md for scope and limitations. Preserve the previous v0.0.1-beta release.
