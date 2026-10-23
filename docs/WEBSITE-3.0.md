# Website content for 3.0 (nothing deployed)

The website is not in this repository. Its project is `~/onair-website-new` (built with `python3 tools/build.py`, checked with `python3 tools/check.py`, published with `sh tools/make_dist.sh && npx wrangler deploy`). This page holds the exact text to put there. Nothing here has been applied or deployed; the owner decides when, after 3.0 is released and every asset is attached to the GitHub release.

The wording follows docs/CHANGELOG-3.0.md. Regenerate that first (`python3 tools/release/gen_changelog.py`) and re-read the "Not in this build yet" section: anything listed there must not go on the site.

## 1. tools/config.json

Replace the file's content with this when 3.0 is published (the version drives the sitemap dates, the JSON-LD softwareVersion and the download page). The date is the release date, so it is a placeholder.

```json
{"site": "https://on-air.dev", "version": "3.0.0", "date": "YYYY-MM-DD"}
```

`3.0.0` is what `CMakeLists.txt` reports (`project(OnAir VERSION 3.0.0 ...)`) and so what the packages are named after. If the release assets are named `3.0` instead, use `"3.0"`; look at the real asset names first, because `tools/build.py` builds the download links from this value.

## 2. Download page (mirror-link pattern)

`tools/build.py` already builds this list from the version, each file with a `mirror` link next to it (GitHub first, `https://mirror.on-air.dev/<github address>` second). For 3.0.0 the page will show these nine files plus the checksums, in this order:

| File | Meaning | GitHub address | Mirror address |
|---|---|---|---|
| `OnAir-3.0.0-windows-x64-setup.exe` | Windows installer | `https://github.com/adswill/OnAir/releases/download/v3.0.0/OnAir-3.0.0-windows-x64-setup.exe` | `https://mirror.on-air.dev/https://github.com/adswill/OnAir/releases/download/v3.0.0/OnAir-3.0.0-windows-x64-setup.exe` |
| `OnAir-3.0.0-windows-x64.zip` | Windows portable | same pattern | same pattern |
| `OnAir-3.0.0-macos-arm64.dmg` | macOS, Apple silicon | same pattern | same pattern |
| `OnAir-3.0.0-macos-x86_64.dmg` | macOS, Intel | same pattern | same pattern |
| `onair_3.0.0_amd64.deb` | Debian / Ubuntu, x86-64 | same pattern | same pattern |
| `onair_3.0.0_arm64.deb` | Debian / Ubuntu, arm64 (Raspberry Pi) | same pattern | same pattern |
| `onair-3.0.0-linux-x86_64-portable.tar.gz` | Linux portable, x86-64 | same pattern | same pattern |
| `onair-3.0.0-linux-aarch64-portable.tar.gz` | Linux portable, arm64 | same pattern | same pattern |
| `OnAir-3.0.0-android.apk` | Android (same signing key as 0.2.8 and 0.2.9) | same pattern | same pattern |
| `SHA256SUMS` | checksums | `.../v3.0.0/SHA256SUMS` | (no mirror link on the page; check against GitHub) |

The paragraph under the list stays as it is: "If GitHub is slow or blocked where you are (for example in China), use the mirror link next to each file ... Check the download against the checksums." The tag is `v3.0.0` if the tag follows the `v0.2.9` style.

Before publishing, remove the sentence in `tools/build.py` that tells Android users to uninstall 0.2.6 first (it is old). The Raspberry Pi and Linux lines can mention the CM5 waterfall and OpenGL 3.1 fixes (CHANGELOG-3.0.md, "Radios and platforms"), as "not tried on real Pi hardware yet".

## 3. Changelog entry (tools/changelog_notes.py)

Insert this first in `NOTES`. It uses only what is merged today; add the items of "Not in this build yet" after they merge. Keep "Known limits".

```python
 dict(tag='3.0.0', date='YYYY-MM-DD', intro='OnAir 3.0 adds ten new modes (154 in all), better weak-signal behaviour in many receivers, a calmer interface (status strip, one signal indicator, Audio pop-up, auto gain, first-run lessons), one voice monitor with keys you supply yourself, a complete remote API, OnAir Web for listening from a browser, and fixes from your reports. Nothing patented is shipped: voice codecs are plug-ins you install yourself.', groups=[
  ('New modes', [
   'NFC / RFID 13.56 MHz, SBAS, ASTERIX radar data, Argos, Sigfox, Marine VHF voice, ISDB-S, P25 Phase 2, LoRaWAN (all channels) and LoRa satellites; Horus balloons; ROS is recognised (identify only)',
   'Mode Builder (describe a digital protocol and decode it, no code) and Signal Lab (blind analysis of an unknown digital signal)',
   'Iridium voice (user-installed plug-in), ACARS, packet and pager views; TETRAPOL voice and polarity; long-range and satellite AIS']),
  ('Using OnAir', [
   'One status strip, one signal indicator for every mode, an Audio pop-up in every mode, auto gain for every radio, first-run setup with lessons, presets, a merged Scanner, Scheduler, Datasets, Backup and restore, a frequency calibration wizard',
   'Ham view as one operating screen with rig control and a safety layer; TX studio; antenna designer; propagation tool rework']),
  ('Voice', [
   'One voice monitor for DMR, P25, NXDN, dPMR, D-STAR, Yaesu, M17 and TETRA; keys you add yourself for P25 and NXDN (no key search, nothing cracked); codec plug-ins are found next to the program and in any folder you name']),
  ('Speed and power', [
   'Lower CPU in the common receivers, much lower idle wake-ups, a faster portable FFT; numbers are in the changelog on GitHub']),
  ('Network', [
   'Complete remote API with rigctl and WebSocket events; OnAir Web for listening from a browser; a LAN swarm (opt-in)']),
  ('Fixes', [
   'Raspberry Pi CM5 waterfall and OpenGL 3.1 start-up, no start on CPUs without AVX2, DAB click-in-capture slide, KiwiSDR stalls, network radio search, codec folders, DTMB labels, and more; the full issue list is in the GitHub changelog']),
  ('Known limits', [
   'New modes and measurements are verified on signals OnAir generated itself, not on air; voice needs a codec you install; translations of the new texts are machine-made and unreviewed']),
 ]),
```

(Check the exact tuple layout against the 0.2.9 entry in that file; the layout above copies it. The numbers "ten new modes (154 in all)" come from `tools/release/gen_changelog.py`; re-run it, the count changes when mode branches merge.)

## 4. Feature page text ("What is new in 3.0")

Use the "In short" list from docs/CHANGELOG-3.0.md. Suggested page text:

> **OnAir 3.0**
>
> Ten new modes: NFC / RFID, SBAS, ASTERIX, Argos, Sigfox, Marine VHF voice, ISDB-S, P25 Phase 2, LoRaWAN and LoRa satellites. A Mode Builder and a Signal Lab for protocols that have no mode yet.
>
> A calmer interface: one status strip, one signal indicator for every mode, an Audio pop-up with squelch, filters and EQ, auto gain for every radio, and a first-run setup with lessons that point at the real controls.
>
> Digital voice in one place: a voice monitor with hold, allow, mute and priority lists and per-call recordings. Keys are yours to add; OnAir never searches or cracks them. Codecs are plug-ins you install yourself.
>
> Listen from a browser with OnAir Web, drive OnAir from your own scripts with the remote API, and run a LAN swarm over several machines.
>
> Most new modes are experimental: they work on signals OnAir generates itself and need real recordings. Send yours on Discord or to the signal sample library.

Screenshots for this page are placeholders (nothing was captured): status strip, spectrum with the 3D waterfall, voice monitor, first-run setup, OnAir Web in a browser. They are named in the README comments (`<!-- SCREENSHOT: ... -->`).

## 5. "What changed" page

Build it from docs/CHANGELOG-3.0.md, section by section, in this order: In short, New modes, Better decoding (tables only once their branches are merged), Faster and lighter, Spectrum and tuning, Using OnAir, Voice, Radios and platforms, Network, KAAL, Issues from GitHub, Thanks, Known limits. Do not copy "For maintainers" or "Not in this build yet". Keep the plain tone and the "measured on generated signals" wording. The issue table can link each number to `https://github.com/adswill/OnAir/issues/<n>`.

## 6. Pages that name old facts (check before publishing)

- `modes.json`, `mode_tuning.json`, `stats.json` are extracted from `app/ui/mode_table.cpp` of the release tag (`tools/extract_modes.py`, `extract_tuning.py`, `stats.py`): re-run them at the 3.0 tag. A mode page is needed for each new mode.
- `tools/github_wiki.py` and `tools/wikidata` hold the wiki text; the wiki mirrors docs/*.md (see docs/README.md, "Added in 3.0").
- The credits page (docs/credits-for-website.md) needs the 3.0 additions: rtl_433 (ISM decoders), horusdemodlib (Horus layer 2, LGPL-2.1), tetrapol-kit read as a cross-check only, the DRA/AVS+ FFmpeg add-on pointer, and contributors @drfruct and @xeonliu.
