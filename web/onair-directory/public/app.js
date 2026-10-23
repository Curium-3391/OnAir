// OnAir directory page: list, search, band filter and map pins from /api/v1/servers. Every server value is put in the page with
// textContent (never innerHTML), links only for http(s) urls the Worker already validated. No analytics, no cookies.
const $ = (id) => document.getElementById(id);
const listEl = $("list"), countEl = $("count");
let servers = [], selected = null, map = null, markers = new Map(), timer = null;

function fmtHz(hz) { return hz >= 1e9 ? (hz / 1e9).toFixed(2) + " GHz" : hz >= 1e6 ? (hz / 1e6).toFixed(hz % 1e6 ? 3 : 0).replace(/\.?0+$/, "") + " MHz" : (hz / 1e3) + " kHz"; }
function coverage(ranges) { return (ranges || []).map((r) => fmtHz(r[0]) + " to " + fmtHz(r[1])).join(", ") || "not stated"; }
function el(tag, cls, text) { const e = document.createElement(tag); if (cls) e.className = cls; if (text !== undefined) e.textContent = text; return e; }
function safeHref(u) { try { const x = new URL(u); return x.protocol === "https:" || x.protocol === "http:" ? x.href : null; } catch { return null; } }

function render() {
  listEl.replaceChildren();
  countEl.textContent = servers.length + (servers.length === 1 ? " server" : " servers");
  if (!servers.length) { listEl.append(el("div", "empty", "No servers match. Servers appear here when their owners switch on the directory in OnAir Web.")); }
  for (const s of servers) {
    const c = el("div", "card" + (s.id === selected ? " sel" : ""));
    c.tabIndex = 0;
    const h = el("h3");
    const dot = el("span", "dot" + (s.reachable === true ? " ok" : s.reachable === false ? " bad" : ""));
    dot.title = s.reachable === true ? "Reachable (checked by the directory)" : s.reachable === false ? "Not reachable from the directory" : "Not checked";
    h.append(dot, document.createTextNode(s.name));
    c.append(h);
    c.append(el("div", "meta", `${s.users} of ${s.capacity} listening` + (s.radio ? " · " + s.radio : "") + (s.location ? " · " + s.location : "")));
    c.append(el("div", "meta", coverage(s.freq)));
    if (s.description) c.append(el("p", "", s.description));
    const acts = el("div", "acts");
    const href = safeHref(s.url);
    if (href) { const a = el("a", "", "Open"); a.href = href; a.target = "_blank"; a.rel = "noopener noreferrer nofollow"; acts.append(a); }
    const rb = el("button", "", "Report"); rb.type = "button"; rb.addEventListener("click", (e) => { e.stopPropagation(); openReport(s); }); acts.append(rb);
    if (s.contact) acts.append(el("span", "meta", s.contact));
    c.append(acts);
    const pick = () => select(s.id, true);
    c.addEventListener("click", pick);
    c.addEventListener("keydown", (e) => { if (e.key === "Enter") pick(); });
    listEl.append(c);
  }
  drawPins();
}

function select(id, fly) {
  selected = id;
  const s = servers.find((x) => x.id === id);
  if (fly && s && s.lat !== null && map) map.flyTo({ center: [s.lon, s.lat], zoom: Math.max(map.getZoom(), 6) });
  render();
}

function drawPins() {
  if (!map) return;
  const keep = new Set();
  for (const s of servers) {
    if (s.lat === null || s.lat === undefined) continue;
    keep.add(s.id);
    let m = markers.get(s.id);
    if (!m) {
      const b = document.createElement("button");
      b.className = "pin"; b.type = "button";
      b.addEventListener("click", () => select(s.id, false));
      m = new maplibregl.Marker({ element: b }).setLngLat([s.lon, s.lat]).addTo(map);
      markers.set(s.id, m);
    }
    m.setLngLat([s.lon, s.lat]);
    const b = m.getElement();
    b.title = s.name;
    b.setAttribute("aria-label", s.name);
    b.classList.toggle("sel", s.id === selected);
  }
  for (const [id, m] of markers) if (!keep.has(id)) { m.remove(); markers.delete(id); }
}

function initMap() {
  if (typeof maplibregl === "undefined") return;
  map = new maplibregl.Map({
    container: "map", center: [15, 40], zoom: 2.2, attributionControl: { compact: true },
    style: { version: 8, sources: { osm: { type: "raster", tiles: ["https://tile.openstreetmap.org/{z}/{x}/{y}.png"], tileSize: 256, maxzoom: 19, attribution: "© OpenStreetMap" } },
      layers: [{ id: "osm", type: "raster", source: "osm", paint: { "raster-saturation": -0.5, "raster-brightness-max": matchMedia("(prefers-color-scheme: dark)").matches ? 0.85 : 1 } }] },
  });
  map.on("load", drawPins);
}

async function load() {
  const p = new URLSearchParams();
  if ($("q").value.trim()) p.set("q", $("q").value.trim());
  if ($("band").value) p.set("band", $("band").value);
  if ($("free").checked) p.set("free", "1");
  try {
    const r = await fetch("/api/v1/servers?" + p);
    const j = await r.json();
    servers = j.ok ? j.servers : [];
  } catch { servers = []; countEl.textContent = "directory not reachable"; return; }
  render();
}
function later() { clearTimeout(timer); timer = setTimeout(load, 250); }

const rep = $("rep");
let repId = null;
function openReport(s) { repId = s.id; $("repname").textContent = s.name; $("repmsg").textContent = ""; $("reptext").value = ""; rep.showModal(); }
$("repcancel").addEventListener("click", () => rep.close());
$("repform").addEventListener("submit", async (e) => {
  e.preventDefault();
  try {
    const r = await fetch("/api/v1/report", { method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify({ id: repId, reason: $("repreason").value, text: $("reptext").value }) });
    const j = await r.json();
    $("repmsg").textContent = j.ok ? "Thank you. The report was sent." : "Not sent: " + (j.error || "error");
    if (j.ok) setTimeout(() => rep.close(), 1200);
  } catch { $("repmsg").textContent = "Not sent: network error"; }
});

$("q").addEventListener("input", later);
$("band").addEventListener("change", load);
$("free").addEventListener("change", load);
initMap();
load();
setInterval(load, 60000);
