# Uploaded beta binary inspection

SHA-256: `a4e9e1de27b5ad8b78bf25f17263e5035c39baaedc97627561e9d28412ec6a2d`

Size: 56,078,870 bytes. NACP: KaiTube / Sank-kab / beta0.0.1.

Static inspection of the NRO byte stream and its RomFS manifest found no recognizable Google OAuth tokens/client IDs, GitHub tokens, actual SAPISID cookie assignments, complete PEM private keys, or the developer's known personal email/Ubuntu username. Session filenames and authentication code strings are expected application code, not saved sessions. PEM begin markers were library parser strings without complete private keys. Third-party maintainer email addresses and toolchain build paths are present.

The uploaded binary embeds Nintendo/Morisawa font files and unused Borealis demo assets. Redistribution permission was not established, so this binary has not been added to the repository. Source packaging has been cleaned; rebuild and test it before release.

Pattern scans cannot prove absence of every possible secret, identify all arbitrary opaque values, or replace a complete license/security review. This report applies only to the hash above.
