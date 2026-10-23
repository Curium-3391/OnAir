-- OnAir directory: Cloudflare D1 (SQLite). Apply with: wrangler d1 execute onair-directory --remote --file=schema.sql
-- No listener data and no IP addresses are stored: the rate-limit table keeps a salted hash that expires within a day.

CREATE TABLE IF NOT EXISTS servers (
  id          TEXT PRIMARY KEY,          -- chosen by the server, trusted on first registration
  pub         TEXT NOT NULL,             -- Ed25519 public key (hex); every later request must be signed by it
  name        TEXT NOT NULL,
  url         TEXT NOT NULL,
  host        TEXT NOT NULL,             -- lower-case host of url (block list, one entry per host)
  loc         TEXT NOT NULL DEFAULT '',  -- what the host sent: "lat,lon" or a locator
  lat         REAL,
  lon         REAL,
  freq        TEXT NOT NULL DEFAULT '[]',-- JSON [[low_hz, high_hz], ...]
  radio       TEXT NOT NULL DEFAULT '',
  cap         INTEGER NOT NULL DEFAULT 0,
  users       INTEGER NOT NULL DEFAULT 0,
  ver         TEXT NOT NULL DEFAULT '',
  contact     TEXT NOT NULL DEFAULT '',
  descr       TEXT NOT NULL DEFAULT '',
  reach_opt   INTEGER NOT NULL DEFAULT 0, -- the host allowed the reachability check
  reach_ok    INTEGER,                    -- NULL: not checked
  reach_at    INTEGER NOT NULL DEFAULT 0,
  interval_s  INTEGER NOT NULL DEFAULT 300,
  first_seen  INTEGER NOT NULL,
  last_seen   INTEGER NOT NULL,
  last_ts     INTEGER NOT NULL,           -- newest signed timestamp accepted (replay protection)
  expires_at  INTEGER NOT NULL
);
CREATE UNIQUE INDEX IF NOT EXISTS servers_url ON servers(url);
CREATE INDEX IF NOT EXISTS servers_exp ON servers(expires_at);

CREATE TABLE IF NOT EXISTS rl (            -- fixed-window rate limits
  k    TEXT PRIMARY KEY,
  win  INTEGER NOT NULL,
  n    INTEGER NOT NULL,
  exp  INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS blocks (        -- admin block list: kind = 'host' | 'id' | 'pub'
  kind  TEXT NOT NULL,
  value TEXT NOT NULL,
  note  TEXT NOT NULL DEFAULT '',
  at    INTEGER NOT NULL,
  PRIMARY KEY (kind, value)
);

CREATE TABLE IF NOT EXISTS reports (
  n          INTEGER PRIMARY KEY AUTOINCREMENT,
  server_id  TEXT NOT NULL,
  reason     TEXT NOT NULL,
  text       TEXT NOT NULL DEFAULT '',
  at         INTEGER NOT NULL
);
