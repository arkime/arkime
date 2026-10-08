#!/usr/bin/env python3
"""Compare Arkime zeeklog plugin output with real Zeek output.

Runs zeek and arkime-capture (with zeeklog.so) over a set of pcaps, then for
every Zeek log path reports which columns we never fill, how often each
column agrees, and which logs we don't produce at all.

Example (from the tests directory):
    ../capture/plugins/zeeklog/compare.py --out /tmp/zeekcmp pcap/dns-*.pcap pcap/http-*.pcap

Use --reuse to skip re-running zeek/capture for pcaps already processed.
"""

import argparse
import collections
import json
import os
import re
import subprocess
import sys

CONN_KEY = ("id.orig_h", "id.orig_p", "id.resp_h", "id.resp_p")
LOG_KEYS = {
    "x509": ("certificate.serial",),
    "dhcp": ("mac",),
    "dns": CONN_KEY + ("trans_id", "query"),
}
# Values that mean the same thing on both sides
NORMALIZE = {
    ("conn", "service"): lambda v: ",".join(sorted(x for x in v.split(",") if not x.startswith("-"))),
    ("ssl", "version"): lambda v: re.sub(r"[^a-z0-9]", "", v.lower()),
    ("x509", "certificate.serial"): lambda v: v.lower().lstrip("0"),
}
SET_TYPES = re.compile(r"^(set|vector)\[")


def read_log(path):
    """Return (fields, types, rows) for a Zeek TSV log, rows are dicts, '-' is None."""
    fields, types, rows = [], [], []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("#fields"):
                fields = line.split("\t")[1:]
            elif line.startswith("#types"):
                types = line.split("\t")[1:]
            elif line.startswith("#"):
                continue
            elif line:
                vals = line.split("\t")
                rows.append({k: (None if v == "-" else v) for k, v in zip(fields, vals)})
    return fields, types, rows


def unescape(v):
    return re.sub(r"\\x([0-9a-fA-F]{2})", lambda m: chr(int(m.group(1), 16)), v)


def norm(log, col, typ, v):
    if v is None:
        return None
    v = unescape(v)
    if v == "(empty)":
        v = ""
    f = NORMALIZE.get((log, col))
    if f:
        v = f(v)
    if SET_TYPES.match(typ or ""):
        return frozenset(x.strip().lower() for x in v.split(",") if x != "")
    if typ in ("time", "interval", "double"):
        try:
            return round(float(v), 3)
        except ValueError:
            return v.lower()
    if typ in ("count", "int", "port"):
        try:
            return int(v)
        except ValueError:
            return v.lower()
    return v.strip().lower()


def run(cmd, cwd=None):
    r = subprocess.run(cmd, cwd=cwd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    if r.returncode != 0:
        sys.stderr.write(f"FAILED ({r.returncode}): {' '.join(cmd)}\n{r.stderr[-2000:]}\n")
    return r.returncode == 0


def generate(args, pcap):
    stem = os.path.splitext(os.path.basename(pcap))[0]
    zdir = os.path.join(args.out, "zeek", stem)
    adir = os.path.join(args.out, "arkime", stem)
    pcap = os.path.abspath(pcap)
    if not (args.reuse and os.path.isdir(zdir)):
        os.makedirs(zdir, exist_ok=True)
        run([args.zeek, "-C", "-r", pcap] + args.zeek_args, cwd=zdir)
    if not (args.reuse and os.path.isdir(adir)):
        os.makedirs(adir, exist_ok=True)
        cmd = [os.path.abspath(args.capture), "--tests", "-c", args.config, "-n", args.node, "-r", pcap,
               "-o", "plugins=zeeklog.so", "-o", f"zeekLogDir={adir}", "-o", "zeekLogFormat=ascii"]
        for o in args.capture_option:
            cmd += ["-o", o]
        run(cmd, cwd=args.cwd)
    return stem, zdir, adir


def load_dir(d):
    logs = {}
    if not os.path.isdir(d):
        return logs
    for name in sorted(os.listdir(d)):
        if not name.endswith(".log") or name.count(".") != 1:
            continue
        path = name[:-4]
        fields, types, rows = read_log(os.path.join(d, name))
        logs[path] = (fields, dict(zip(fields, types)), rows)
    return logs


class ColStats:
    def __init__(self):
        self.both = self.equal = self.differ = self.zeek_only = self.ark_only = self.neither = 0
        self.examples = []

    def add(self, zv, av, zraw, araw, where):
        if zv is None and av is None:
            self.neither += 1
        elif zv is None:
            self.ark_only += 1
        elif av is None:
            self.zeek_only += 1
            if len(self.examples) < 3:
                self.examples.append(f"{where}: zeek={zraw!r} ours=-")
        else:
            self.both += 1
            if zv == av:
                self.equal += 1
            else:
                self.differ += 1
                if len(self.examples) < 3:
                    self.examples.append(f"{where}: zeek={zraw!r} ours={araw!r}")


def match_rows(log, zrows, arows, zfields):
    key = LOG_KEYS.get(log, CONN_KEY)
    if not all(k in zfields for k in key):
        key = CONN_KEY if all(k in zfields for k in CONN_KEY) else ("ts",)
    zg = collections.defaultdict(list)
    ag = collections.defaultdict(list)
    for r in zrows:
        zg[tuple(r.get(k) for k in key)].append(r)
    for r in arows:
        ag[tuple(r.get(k) for k in key)].append(r)
    pairs = []
    for k, zl in zg.items():
        al = ag.get(k, [])
        zl.sort(key=lambda r: float(r.get("ts") or 0))
        al.sort(key=lambda r: float(r.get("ts") or 0))
        for z, a in zip(zl, al):
            pairs.append((z, a))
    return pairs


def pct(n, d):
    return "-" if d == 0 else f"{100.0 * n / d:.0f}%"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pcaps", nargs="*")
    ap.add_argument("--out", required=True, help="work directory for zeek/ and arkime/ outputs")
    ap.add_argument("--zeek", default="zeek")
    ap.add_argument("--zeek-args", nargs="*", default=[], help="extra zeek command line args/scripts")
    ap.add_argument("--capture", default="../capture/capture")
    ap.add_argument("--config", default="config.test.ini")
    ap.add_argument("--node", default="test")
    ap.add_argument("--cwd", default=".", help="directory to run capture from (relative config paths)")
    ap.add_argument("--capture-option", action="append", default=[], help="extra -o key=value for capture")
    ap.add_argument("--reuse", action="store_true", help="don't rerun zeek/capture when output dirs exist")
    ap.add_argument("--report", help="write markdown report here instead of stdout")
    ap.add_argument("--json", help="write raw stats as json here")
    ap.add_argument("--logs", help="comma separated list of logs to report on")
    args = ap.parse_args()

    if not args.pcaps:
        zroot = os.path.join(args.out, "zeek")
        stems = sorted(os.listdir(zroot)) if os.path.isdir(zroot) else []
        runs = [(s, os.path.join(args.out, "zeek", s), os.path.join(args.out, "arkime", s)) for s in stems]
    else:
        runs = []
        for i, pcap in enumerate(args.pcaps):
            sys.stderr.write(f"\r[{i + 1}/{len(args.pcaps)}] {os.path.basename(pcap)}          ")
            runs.append(generate(args, pcap))
        sys.stderr.write("\n")

    only = set(args.logs.split(",")) if args.logs else None
    zrows_total = collections.Counter()
    arows_total = collections.Counter()
    matched = collections.Counter()
    zcols = {}
    acols = {}
    ztypes = {}
    stats = collections.defaultdict(lambda: collections.defaultdict(ColStats))
    zeek_set = collections.defaultdict(collections.Counter)
    ark_set = collections.defaultdict(collections.Counter)
    pcaps_with = collections.defaultdict(set)

    for stem, zdir, adir in runs:
        zlogs = load_dir(zdir)
        alogs = load_dir(adir)
        for log in set(zlogs) | set(alogs):
            if only and log not in only:
                continue
            zf, zt, zr = zlogs.get(log, ([], {}, []))
            af, at, ar = alogs.get(log, ([], {}, []))
            if zf:
                zcols.setdefault(log, zf)
                ztypes.setdefault(log, {}).update(zt)
            if af:
                acols.setdefault(log, af)
            zrows_total[log] += len(zr)
            arows_total[log] += len(ar)
            if zr:
                pcaps_with[log].add(stem)
            for r in zr:
                for c, v in r.items():
                    if v is not None and v != "(empty)":
                        zeek_set[log][c] += 1
            for r in ar:
                for c, v in r.items():
                    if v is not None and v != "(empty)":
                        ark_set[log][c] += 1
            if not zr or not ar:
                continue
            for z, a in match_rows(log, zr, ar, zf):
                matched[log] += 1
                for c in zf:
                    if c not in af:
                        continue
                    typ = zt.get(c)
                    stats[log][c].add(norm(log, c, typ, z.get(c)), norm(log, c, typ, a.get(c)), z.get(c), a.get(c), stem)

    out = open(args.report, "w") if args.report else sys.stdout
    logs = sorted(set(zrows_total) | set(arows_total))

    out.write("# zeeklog vs Zeek comparison\n\n")
    out.write(f"Pcaps: {len(runs)}\n\n")
    out.write("## Logs\n\n| log | zeek rows | arkime rows | matched rows | columns only in zeek | columns only in arkime |\n|---|---|---|---|---|---|\n")
    for log in logs:
        zc = set(zcols.get(log, []))
        ac = set(acols.get(log, []))
        out.write(f"| {log} | {zrows_total[log]} | {arows_total[log]} | {matched[log]} | {', '.join(sorted(zc - ac)) or '-'} | {', '.join(sorted(ac - zc)) or '-'} |\n")

    out.write("\n## Columns Zeek fills that we never fill\n\n")
    out.write("| log | column | type | zeek rows set | example |\n|---|---|---|---|---|\n")
    for log in logs:
        for c in zcols.get(log, []):
            if zeek_set[log][c] and ark_set[log][c] == 0:
                ex = stats[log][c].examples[:1] if c in stats[log] else []
                out.write(f"| {log} | {c} | {ztypes[log].get(c, '')} | {zeek_set[log][c]} | {ex[0] if ex else ''} |\n")

    for log in logs:
        if not zcols.get(log) or not acols.get(log):
            continue
        out.write(f"\n## {log}.log\n\n")
        out.write(f"pcaps with zeek rows: {len(pcaps_with[log])}, matched rows: {matched[log]}\n\n")
        out.write("| column | type | zeek set | ours set | agree | differ | zeek only | ours only | examples |\n|---|---|---|---|---|---|---|---|---|\n")
        for c in zcols[log]:
            if c not in acols[log]:
                continue
            st = stats[log][c]
            tot = matched[log]
            ex = "<br>".join(e.replace("|", "\\|") for e in st.examples)
            out.write(f"| {c} | {ztypes[log].get(c, '')} | {pct(zeek_set[log][c], zrows_total[log])} | {pct(ark_set[log][c], arows_total[log])} | "
                      f"{pct(st.equal, tot)} | {pct(st.differ, tot)} | {pct(st.zeek_only, tot)} | {pct(st.ark_only, tot)} | {ex} |\n")

    if args.json:
        with open(args.json, "w") as f:
            json.dump({
                "zeek_rows": zrows_total, "arkime_rows": arows_total, "matched": matched,
                "zeek_set": {k: dict(v) for k, v in zeek_set.items()},
                "arkime_set": {k: dict(v) for k, v in ark_set.items()},
                "stats": {log: {c: vars(s) for c, s in cols.items()} for log, cols in stats.items()},
            }, f, indent=1, default=list)


if __name__ == "__main__":
    main()
