# INTEG-30-A log (integ-30-a, from pub/integ-30)
Order merged: android-030, android-ui-030, android-usb-030, android-modes-030, walk-df-030, web-map-030, webonair-030, webonair-ui-030, webonair-ranges-030, webonair-dir-030, lan-swarm-030. No newer webonair/android/lan/walk branches appeared. All merged, none left out.

- android-030: clean.
- android-ui-030: docs/PATHWAY.md (kept both entries).
- android-usb-030: CMakeLists dect2core (kept integ add_library list, took usb's target_sources incl. core/src/source/audio_push.cpp); build.gradle.kts (one junit + coroutines-test, comment merged); AndroidManifest (tune deep link + USB_DEVICE_ATTACHED filters both kept); MainActivity.kt (imports, onCreate takeLink + usbAttached, both functions kept); ReceiverTab.kt deleted (ui branch removed it; usb only added a no-op branch for USB/NETWORK/SOUND_CARD); PATHWAY.
- android-modes-030: PATHWAY only.
- walk-df-030: AndroidManifest stale "Not declared yet" comments removed (both permissions are declared).
- web-map-030, webonair-030: PATHWAY only. webonair-ui-030, webonair-ranges-030: clean.
- webonair-dir-030: CMakeLists ONAIR_WEB_SOURCES (web_ranges + web_directory_proto + web_directory) and test foreach (web_ranges web_directory) both kept; PATHWAY.
- lan-swarm-030: CMakeLists APP_SOURCES (added app/panels/swarm_ui.cpp to integ list), test_api_policy and test_swarm blocks both kept; PATHWAY.
PATHWAY entries were appended in conflict order (not re-sorted by date).

## Not resolved / notes
- Android: the new UI (ui/radio/RadioScreen, Discovery) does not reference usb/UsbDevicePicker.kt; the USB/network/sound-card source kinds are not yet wired into the new screens (the branch only left a placeholder in the deleted ReceiverTab). Needs a UI wiring task.
- Android versionCode/Name come from onairVersionCode; not changed by merges.
- SECURITY-FIX-REVIEW.md corrections: not applied here (no security branch merged, secfix web code not present). For whoever applies secfix-web: check what HostDecoder::policyKind()/modeId() returns for sub-modes (ft8/ft4/wspr/sstv/wefax vs family ids hfdig, hfbeacon...) or approved decoding is blocked; secfix-api: sess.required = cfg.allowLan only; test_web_safety first run may list unclassified ids; dir diff against untested-secfix-dir-030~1.

## NEEDS BUILD
Full desktop build + ctest (web_ranges, web_directory, swarm, api_policy, web_*), android gradle build + testDebugUnitTest + native jni (usb, modes, walk-df), tests/web/run.js (node), check_jni.py.
