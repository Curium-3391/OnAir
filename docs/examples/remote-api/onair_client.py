#!/usr/bin/env python3
"""A tiny client for the OnAir remote API (docs/REMOTE-API.md). Standard library only.

  TOKEN=... python3 onair_client.py status
  TOKEN=... python3 onair_client.py tune 162.55
  TOKEN=... python3 onair_client.py events        # prints decoded messages as they arrive
  python3 onair_client.py udp 5005                # listens for the UDP JSON lines
"""
import json, os, socket, sys, time, urllib.request

BASE = os.environ.get("BASE", "http://127.0.0.1:8765/api/v1")
TOKEN = os.environ.get("TOKEN", "")


def call(path, body=None):
    data = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(BASE + path, data=data, headers={"Authorization": "Bearer " + TOKEN})
    if data is not None:
        req.add_header("Content-Type", "application/json")
    with urllib.request.urlopen(req, timeout=5) as r:
        return json.load(r)


def events():
    cursor = call("/events")["next"]          # from now
    while True:
        r = call("/events?since=%d&max=100" % cursor)
        cursor = r["next"]
        for e in r["events"]:
            print(json.dumps(e))
        time.sleep(1)


def udp(port):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("", int(port)))
    while True:
        print(json.loads(s.recvfrom(2048)[0]))


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "status"
    if cmd == "status":
        print(call("/status"))
    elif cmd == "tune":
        print(call("/frequency", {"mhz": float(sys.argv[2])}))
    elif cmd == "start":
        print(call("/start", {}))
    elif cmd == "stop":
        print(call("/stop", {}))
    elif cmd == "events":
        events()
    elif cmd == "udp":
        udp(sys.argv[2])
    else:
        sys.exit(__doc__)
