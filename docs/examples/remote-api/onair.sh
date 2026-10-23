#!/bin/sh
# curl examples for the OnAir remote API (docs/REMOTE-API.md).  Usage: TOKEN=... ./onair.sh
# The token: View > Data feeds > Remote control API > Copy token.
BASE=${BASE:-http://127.0.0.1:8765/api/v1}
H="Authorization: Bearer ${TOKEN:?set TOKEN}"
J="Content-Type: application/json"

curl -s -H "$H" "$BASE/status"; echo
curl -s -H "$H" "$BASE/modes" | head -c 300; echo
curl -s -H "$H" -H "$J" -d '{"mhz":162.55}' "$BASE/frequency"; echo
curl -s -H "$H" -H "$J" -d '{"db":30}' "$BASE/gain"; echo
curl -s -H "$H" -H "$J" -d '{"level":0.4}' "$BASE/volume"; echo
curl -s -H "$H" -H "$J" -d '{"on":false}' "$BASE/mute"; echo
curl -s -H "$H" -H "$J" -d '{}' "$BASE/start"; echo
curl -s -H "$H" "$BASE/spectrum?bins=64" | head -c 300; echo
curl -s -H "$H" "$BASE/memories" | head -c 300; echo
# decoded messages: first call gives the cursor ("next"), then ask since=<next>
curl -s -H "$H" "$BASE/events"; echo
curl -s -H "$H" -H "$J" -d '{"kind":"iq"}' "$BASE/recording/start"; echo
curl -s -H "$H" -H "$J" -d '{}' "$BASE/recording/stop"; echo
curl -s -H "$H" -H "$J" -d '{}' "$BASE/stop"; echo
