#!/bin/bash
# Release consistency checks, run from a clean checkout (needs no build):
#   tools/release/check_versions.sh <x.y.z> [--final]
# - the version in CMakeLists.txt, the Windows resource, the macOS Info.plist, the Android version code
# - docs/CHANGELOG-<major>.<minor>.md exists (--final: it must no longer say "draft")
# - credits: tools/credits_data.tsv regenerates THIRD_PARTY.md, the in-app Credits table and the website block with no change
# - every folder of third_party/ and every vendored library is named in THIRD_PARTY.md; the Rust workspace uses no outside crates
# - the update checker points at the project's own releases
# Prints "FAIL" lines and exits 1 when something is wrong; "warn" lines do not fail the check.
set -u
V=${1:?usage: check_versions.sh <x.y.z> [--final]}; FINAL=0; [ "${2:-}" = "--final" ] && FINAL=1
cd "$(dirname "$0")/../.."
fail=0
bad() { echo "FAIL: $*"; fail=1; }
ok()  { echo "ok:   $*"; }
warn(){ echo "warn: $*"; }
case $V in [0-9]*.[0-9]*.[0-9]*) ;; *) echo "version must look like 3.0.0"; exit 2;; esac
IFS=. read -r MA MI PA <<<"$V"

# --- version strings ---
n=$(grep -cE "project\(OnAir VERSION $MA\.$MI\.$PA " CMakeLists.txt)
[ "$n" -ge 1 ] && ok "CMakeLists.txt project(OnAir VERSION $V) ($n lines)" || bad "CMakeLists.txt does not say VERSION $V"
grep -qE "^FILEVERSION $MA,$MI,$PA,0" packaging/windows/onair.rc && grep -qE "^PRODUCTVERSION $MA,$MI,$PA,0" packaging/windows/onair.rc \
  && grep -q "\"ProductVersion\", \"$V\"" packaging/windows/onair.rc && ok "Windows resource packaging/windows/onair.rc" || bad "packaging/windows/onair.rc is not $V"
grep -q "<key>CFBundleVersion</key><string>$V</string>" packaging/macos/Info.plist && ok "macOS Info.plist CFBundleVersion" || bad "packaging/macos/Info.plist CFBundleVersion is not $V"
code=$(( (MA*10000 + MI*100 + PA) * 100 + 99 ))
grep -q 'major\*10000 + minor\*100 + patch' android/app/build.gradle.kts && grep -q 'project\\(\\s\*OnAir\\s+VERSION' android/app/build.gradle.kts \
  && ok "Android versionName comes from CMakeLists.txt; stable versionCode will be $code" || bad "android/app/build.gradle.kts no longer derives the version from CMakeLists.txt"
[ "$V" = "3.0.0" ] && [ "$code" != 3000099 ] && bad "versionCode for 3.0.0 should be 3000099, formula gives $code"
grep -q 'api.github.com/repos/adswill/OnAir/releases' core/src/engine/updater.cpp && ok "update checker reads adswill/OnAir releases (drafts and prereleases are not offered as stable)" || bad "update checker feed URL changed"
grep -q 'ONAIR_VERSION' CMakeLists.txt && ok "program reports ONAIR_VERSION (defaults to the project version)" || bad "ONAIR_VERSION missing"
stale=$(grep -rlE "(^|[^0-9.])0\.2\.[0-9]+" packaging/windows packaging/macos packaging/linux 2>/dev/null | head)
[ -z "$stale" ] && ok "no 0.2.x in packaging/{windows,macos,linux}" || warn "old version strings in: $stale"
grep -q "ARG ONAIR_VERSION=" packaging/docker/Dockerfile && warn "packaging/docker/Dockerfile default ONAIR_VERSION=$(sed -n 's/^ARG ONAIR_VERSION=//p' packaging/docker/Dockerfile) (release.yml passes the real one as a build argument)"

# --- changelog ---
CL=docs/CHANGELOG-$MA.$MI.md
if [ -f "$CL" ]; then
  ok "$CL exists ($(wc -l < "$CL") lines)"
  if grep -qiE '^# .*(draft|not released)' "$CL"; then
    [ $FINAL = 1 ] && bad "$CL title still says draft / not released" || warn "$CL title still says draft / not released (fix before the release: --final fails on it)"
  fi
else
  bad "$CL is missing (release notes)"
fi

# --- credits and licences ---
if command -v python3 >/dev/null && git rev-parse --git-dir >/dev/null 2>&1; then
  python3 tools/gen_credits.py >/dev/null 2>&1 || bad "tools/gen_credits.py failed"
  if git diff --quiet -- THIRD_PARTY.md app/panels/credits_data.h docs/credits-for-website.md; then
    ok "credits: THIRD_PARTY.md, in-app Credits table and website block match tools/credits_data.tsv"
  else
    bad "credits out of date: run python3 tools/gen_credits.py and commit (rows edited in THIRD_PARTY.md only are lost from the in-app Credits); the regenerated files are left in the tree, review with git diff"
  fi
fi
for d in third_party/*/; do
  b=$(basename "$d")
  grep -qiE "$b|$(echo "$b" | tr -d '_-')" THIRD_PARTY.md || bad "third_party/$b is not named in THIRD_PARTY.md"
done
[ -z "$(grep -E '^source' rust/Cargo.lock 2>/dev/null | head -1)" ] && ok "Rust workspace uses no outside crates" || bad "rust/Cargo.lock has outside crates: list their licences in THIRD_PARTY.md"
for f in LICENSE LICENSES/GPL-2.0.txt LICENSES/OFL-1.1.txt LICENSES/MIT.txt; do [ -f "$f" ] || bad "missing $f"; done

# --- what the website needs (tools/config.json of the website project) ---
echo
echo "website: version \"$V\", date \"$(date +%Y-%m-%d)\", changelog from $CL"
echo "download files: OnAir-$V-macos-arm64.dmg OnAir-$V-macos-x86_64.dmg onair_${V}_amd64.deb onair_${V}_arm64.deb onair-$V-linux-x86_64-portable.tar.gz onair-$V-linux-aarch64-portable.tar.gz OnAir-$V-windows-x64.zip OnAir-$V-windows-x64-setup.exe OnAir-$V-android.apk SHA256SUMS"
echo "mirror links: https://mirror.on-air.dev/https://github.com/adswill/OnAir/releases/download/v$V/<file>"
[ $fail = 0 ] && echo "ALL CHECKS PASSED for $V" || echo "CHECKS FAILED for $V"
exit $fail
