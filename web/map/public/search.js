// search.js - pure track search and filter used by the page: callsign, registration, ICAO hex, MMSI, ident.

/** Ranked matches (exact 3, prefix 2, substring 1, sub-type 0.5), at most `limit`. Needs at least 2 characters. */
export function searchTracks(tracks, q, limit = 8) {
  q = String(q || "").trim().toLowerCase();
  if (q.length < 2) return [];
  const scored = [];
  for (const t of tracks) {
    const m = t.meta || {};
    const keys = [t.label, String(t.id).split(":")[1], m.registration, m.flightNumber].filter(Boolean).map((s) => String(s).toLowerCase());
    let score = 0;
    for (const k of keys) {
      if (k === q) score = Math.max(score, 3);
      else if (k.startsWith(q)) score = Math.max(score, 2);
      else if (k.includes(q)) score = Math.max(score, 1);
    }
    if (!score && (t.sub || "").toLowerCase().includes(q)) score = 0.5;
    if (score) scored.push([score, t]);
  }
  scored.sort((a, b) => b[0] - a[0] || String(a[1].label).localeCompare(String(b[1].label)));
  return scored.slice(0, limit).map((x) => x[1]);
}

/** Track passes the layer toggles `on` ({kind: bool}) and the optional maximum age in seconds. */
export function passesFilter(t, on, maxAgeSec, nowMs = Date.now()) {
  if (!on[t.kind]) return false;
  if (maxAgeSec != null && (nowMs - t.t) / 1000 > maxAgeSec) return false;
  return true;
}
