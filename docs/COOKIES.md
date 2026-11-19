# YouTube cookie login — Firefox guide

Cookies are account-session credentials. Keep them on your own PC and SD. Never upload them to GitHub, Discord, screenshots or issue reports. For device-code sign-in and read-only subscriptions, see the [account setup instructions](../README.md#account-setup).

## 1. Prepare a clean browser session

1. On your PC open a **new Firefox private window**.
2. Visit https://www.youtube.com and sign in to **one intended account**. Select the intended YouTube channel if the account has several.
3. Complete any Google prompts and YouTube consent screens in the browser. Verify that Home and Subscriptions show the expected content.
4. After checking the account, navigate in the same tab to `https://www.youtube.com/robots.txt`; make it the only private tab. Export the cookies as described below, then close the private window **without signing out**. Do not use that exported session in the browser again. YouTube can rotate account cookies on open tabs; leaving the original tab active can make exported copies stop working. This follows [yt-dlp's YouTube cookie guidance](https://github.com/yt-dlp/yt-dlp/wiki/Extractors#exporting-youtube-cookies). It improves export stability but does not guarantee indefinite validity.

A fresh private session with a single account solved the wrong-account/session problem during developer testing. KaiTube currently sends account index 0; importing a multi-account session may select a different profile than expected.

## 2. Copy cookies without installing an extension

1. In that private `youtube.com/robots.txt` tab press **Ctrl+Shift+E** to open Firefox Network tools.
2. Reload the page with **Ctrl+R**.
3. Select the `www.youtube.com/robots.txt` document request. Check its URL and that the request carries the signed-in session cookies.
4. In **Headers → Request headers**, locate **Cookie**. Use the raw-header view if needed.
5. Copy **only the complete value** of Cookie. Do not copy the whole request, Response headers, Set-Cookie, Authorization or unrelated browser storage. Do not insert line breaks. Visual wrapping is fine; the saved value must be one line.
6. Close the private window without logging out after copying the value. Do not revisit YouTube in that exported session. Save as UTF-8 plain text named **`kaitube_native_auth.txt`**. Check that the name is not `.txt.txt`. The file should resemble `NAME=value; OTHER_NAME=value` (those are placeholders, not working credentials).

Alternatively import Netscape `cookies.txt` content or supported JSON (`cookie_header`, `cookie`, `cookies` array, or a top-level cookie array). The filename on SD is still `kaitube_native_auth.txt`. An export must include the actual signed-in YouTube session cookies, not just preferences or consent cookies. Third-party export extensions can access your account credentials; they are not required by this guide.

## 3. Import on the Switch

1. Close KaiTube before replacing files on its SD.
2. Copy the file to **`/switch/kaitube_native_auth.txt`**. This is the import file, not the NRO folder or the logs folder.
3. Launch the newly built NRO. Open **Settings → YouTube cookies → Import cookies**.
4. Wait for the explicit import-success notification. A settings row saying **Saved** only means a session exists; it does not prove that this attempt succeeded or that YouTube accepts it.
5. After successful import restart KaiTube and test Home and Subscriptions.
6. Once the saved session works, remove the import file from the SD and protect/delete any unnecessary PC copies.

KaiTube stores the active session, unencrypted, in `/switch/kaitube_native_session.json`. Temporary/recovery files may use `.tmp` or `.bak` suffixes. Never share any of these files. Application updates preserve these paths; a working session need not be reimported every time.

## Troubleshooting

| Symptom | What to do |
|---|---|
| Cannot open authentication file | Check `/switch/kaitube_native_auth.txt`, spelling, file extension and destination on the actual SD. |
| Missing SAPISID / invalid cookie header | Re-export the full Cookie request-header value from the signed-in YouTube document. Avoid multiline headers, screenshots, JSON copied incompletely or preference-only exports. |
| Cannot save session | Confirm you built the latest source and uploaded that exact NRO. The fixed build supports SD backends that cannot rename over an existing file. Check SD write access/free space; collect the `auth-save` diagnostics if it persists. Do not delete the old session as a first step. |
| YouTube rejected the saved session / `logged_out=1` | The request reached YouTube but it treated the session as signed out. Export fresh cookies using the private-window/robots.txt procedure above, close that window without logging out, import again and confirm success. This can be a cookie/session/context problem; the message alone does not establish which. |
| `logged_out=0` but Home empty/wrong account | Verify the same account/channel has the expected Home on PC. Try the private single-account method; multi-account context and unsupported response layouts can cause problems. |
| Saved row but new login does not work | Look for the explicit import result, then restart after success. Saved is not online validation. |
| FileZilla drag-and-drop does nothing | Use FileZilla's local file pane: select the file → right-click → Upload. Check its transfer queue and log. |
| The old error message still appears after rebuilding | Avoid reusing an old downloaded ZIP or old NRO. Check `grep -F "SD replacement fallback" src/common/auth_store.cpp`, build successfully, and transfer `output/KaiTube-beta0.0.1.nro`. |

For an import failure, reproduce it once and collect `app.log` from `/switch/KaiTube-Native/logs/` **before launching the app again**. A new launch rotates the current log; the earlier run can be under `logs/previous/`. Include the exact notification and steps. Review logs for personal data (titles, searches, URLs) before posting. Do not attach the authentication/session files.

## Sign out

Choose **Disconnect** in the cookie dialog, remove any remaining import file, then restart the app so previously displayed account content is cleared. This removes KaiTube's local saved session; it does not invalidate every copy of the browser's session. If cookies were exposed, invalidate the relevant Google session from your account's security settings as well.
