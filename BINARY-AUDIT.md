# Validated beta candidate

File supplied by the developer: `KaiTube-cookie-fix.nro` (release name may be `KaiTube-beta0.0.1.nro`; renaming does not alter its bytes).

- SHA-256: `2e87e5ad26a92f73c0bc10a7228d6b41426ab9fb10e65e6fbc95c15c24b26dac`
- Size: 47,938,914 bytes
- NACP: KaiTube / Sank-kab / beta0.0.1
- SDK build source pin: ounce-devkit 20261114
- SD replacement-fallback diagnostic string: present

Static inspection parsed the NRO assets and RomFS directory structure and scanned the binary for recognizable OAuth tokens/client IDs, Google API keys, GitHub tokens, populated session-cookie assignments, complete PEM private keys and the developer's known personal identifiers. None were detected. PEM begin markers are library parser strings without complete private keys. Maintainer email addresses and devkitPro build paths are third-party attribution/toolchain strings.

RomFS contains application UI/XML/translations, icons, system status images, public CA certificates, sample catalog data, and DejaVu/Material font assets with notices. No imported cookie/session files, OAuth configuration or personal logs were found. The previous Nintendo/Morisawa font binaries are absent from both the resource manifest and exact-byte search of the complete NRO; Pokémon demo images are absent from the manifest.

The developer reports successful console operation and successful cookie login after fresh import. Console behavior was not independently tested by the repository-preparation assistant. Host cookie-save/rollback/recovery, captions, resume, UMP and resource checks passed; the SDK migration/fix commits also passed GitHub Actions.

These findings apply only to the hash above. Static pattern searches cannot identify every possible opaque secret or certify all licensing/security properties. Keep corresponding source, dependency sources/build scripts and applicable notices available when distributing this GPL-based binary. Do not distribute the earlier binary identified by hash a4e9e1de27b5ad8b78bf25f17263e5035c39baaedc97627561e9d28412ec6a2d: it contained the removed font/demo assets.
