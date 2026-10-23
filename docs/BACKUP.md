# Backup and restore (Tools > Backup and restore)

One `.onairbackup` file (a plain zip, entries stored) with: settings and bookmarks, memories and frequency lists, presets (`presets.json`, `*.onairpreset`), scheduler/automations, ham logbook (ADIF), layouts, DAB/TV/FM/scanner results, custom station lists, and the names of installed codecs (never the codec files).

- **Secrets** (voice keys, QRZ / Club Log / eQSL and any key, token or password setting) go in only with "Include secrets" and a passphrase (8+ characters). They are encrypted in `secrets.enc` with the same scheme as the voice key store: PBKDF2-HMAC-SHA256 (200000 rounds) -> AES-256-CTR + HMAC-SHA256 (encrypt-then-MAC, separate keys), from `dect2::meshcrypto`. Never logged, never in diagnostic bundles, never in automatic backups.
- **Restore**: open the file, see what is inside, tick items, choose Merge (keep existing, add missing) or Replace, dry run, then restore. A safety backup (no secrets) is saved in `<data>/backups/` first. Files are written to a temporary name and renamed. Restart OnAir right after a restore (a running OnAir would save its old settings over them).
- **Extractor**: refuses `..`, absolute and drive paths, backslashes, symlinked parents, compressed/encrypted/zip64 entries, duplicate or overlapping entries, bad CRCs, entries not in the manifest, and sizes over the limits (512 MB per entry, 2 GB total, 20000 entries).
- **Automatic**: once a week, newest N kept, `<data>/backups/onair-auto-*.onairbackup`.
- Code: `core/src/backup/backup.cpp`, `app/panels/backup_ui.cpp`; test `tests/backup/test_backup.cpp`.
