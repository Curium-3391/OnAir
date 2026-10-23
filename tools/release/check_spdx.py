#!/usr/bin/env python3
"""Check that every tracked file has a licence and a copyright holder (REUSE.toml or its own header). No network.

  python3 tools/release/check_spdx.py [--strict] [--list-unknown]

Fails (exit 1) when
  - a tracked file is matched by no [[annotations]] entry of REUSE.toml and has no SPDX-License-Identifier of its own;
  - a licence id used anywhere has no text in LICENSES/<id>.txt (ids listed in tools/release/spdx_pending.txt are
    reported as pending and do not fail, unless --strict);
  - REUSE.toml itself is malformed.
With --strict the identifier LicenseRef-Unknown also fails. Run from the repository root; used by CI.
"""
import os, re, subprocess, sys

try:
    import tomllib
except ImportError:   # Python < 3.11
    tomllib = None

SPDX_RE = re.compile(r'SPDX-License-Identifier:\s*([^\n*]+?)\s*(?:\*/|-->|$)', re.M)

def glob_to_re(pat):
    """REUSE.toml globs: * = anything but /, ** = anything including /, \\ escapes."""
    out, i = '', 0
    while i < len(pat):
        c = pat[i]
        if c == '*':
            if pat[i:i + 2] == '**':
                i += 2
                if pat[i:i + 1] == '/':
                    out += '(?:.*/)?'
                    i += 1
                else:
                    out += '.*'
                continue
            out += '[^/]*'
        elif c == '\\' and i + 1 < len(pat):
            out += re.escape(pat[i + 1]); i += 1
        else:
            out += re.escape(c)
        i += 1
    return re.compile('^' + out + '$')

def tracked():
    r = subprocess.run(['git', 'ls-files'], capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit('git ls-files failed')
    return [l for l in r.stdout.split('\n') if l]

def main():
    strict = '--strict' in sys.argv
    if tomllib is None:
        sys.exit('needs Python 3.11 or newer (tomllib)')
    with open('REUSE.toml', 'rb') as f:
        cfg = tomllib.load(f)
    ann = []
    for a in cfg.get('annotations', []):
        paths = a['path'] if isinstance(a['path'], list) else [a['path']]
        lic = a['SPDX-License-Identifier']
        cr = a['SPDX-FileCopyrightText']
        if not lic or not cr:
            sys.exit('annotation without licence or copyright: %r' % paths)
        ann.append(([glob_to_re(p) for p in paths], lic, cr, a.get('precedence', 'closest')))
    files = tracked()
    pending = set()
    if os.path.exists('tools/release/spdx_pending.txt'):
        pending = {l.strip() for l in open('tools/release/spdx_pending.txt') if l.strip() and not l.startswith('#')}
    used = {}
    uncovered, unknown = [], []
    for f in files:
        if f.startswith('LICENSES/') or not os.path.isfile(f):
            continue
        own = None
        try:
            with open(f, 'rb') as fh:
                head = fh.read(4096).decode('utf-8', 'ignore')
            m = SPDX_RE.search(head)
            if m:
                own = m.group(1).strip()
        except OSError:
            pass
        lic = None
        for pats, l, c, prec in ann:           # the last matching entry wins
            if any(p.match(f) for p in pats):
                lic = l
        if own:
            used.setdefault(own, []).append(f)
        if lic is None and own is None:
            uncovered.append(f)
            continue
        eff = lic if own is None else own
        if own is None:
            used.setdefault(eff, []).append(f)
        if 'LicenseRef-Unknown' in eff:
            unknown.append(f)
    # licence texts
    have = {n[:-4] for n in os.listdir('LICENSES') if n.endswith('.txt')}
    missing = {}
    for expr, fl in used.items():
        for i in re.split(r'\s+(?:AND|OR|WITH)\s+|[()]', expr):
            i = i.strip().rstrip('+')
            if i and i not in have:
                missing.setdefault(i, []).extend(fl)
    bad = False
    if uncovered:
        bad = True
        print('NOT COVERED (%d files):' % len(uncovered))
        for f in uncovered[:40]:
            print('  ', f)
        if len(uncovered) > 40:
            print('   ... and %d more' % (len(uncovered) - 40))
    for i, fl in sorted(missing.items()):
        if i in pending and not strict:
            print('PENDING text LICENSES/%s.txt (%d files, e.g. %s): download from spdx.org with the owner\'s OK' % (i, len(fl), fl[0]))
        else:
            bad = True
            print('MISSING text LICENSES/%s.txt (%d files, e.g. %s)' % (i, len(fl), fl[0]))
    if unknown:
        print('LicenseRef-Unknown: %d files' % len(unknown))
        if '--list-unknown' in sys.argv or strict:
            for f in unknown:
                print('  ', f)
        if strict:
            bad = True
    print('checked %d tracked files, %d licence expressions, %d annotations' % (len(files), len(used), len(ann)))
    sys.exit(1 if bad else 0)

if __name__ == '__main__':
    main()
