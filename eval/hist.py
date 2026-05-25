#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
#
# Parse bpftrace hist()/lhist() output and emit p50/p99/p999/max as CSV.

import re, sys
from pathlib import Path

SUF = {"": 1, "K": 1_000, "M": 1_000_000, "G": 1_000_000_000}
BUCKET = re.compile(r"^\[(\d+)([KMG]?),\s*(\d+)([KMG]?)\)\s+(\d+)")
HEAD = re.compile(r"^@(\w+):\s*$")
SUMMARY = re.compile(r"max=(\d+)\s+e1=(\d+)\s+e2=(\d+)")


def percentile(buckets, q):
    total = sum(c for _, _, c in buckets)
    if not total:
        return 0
    cum = 0
    for lo, hi, c in buckets:
        cum += c
        if cum >= q * total:
            return (lo + hi) // 2
    return buckets[-1][1]


def main(path):
    text = Path(path).read_text(encoding="utf-8", errors="replace")
    sections, cur = {}, None
    for line in text.splitlines():
        m = HEAD.match(line)
        if m:
            cur = m.group(1); sections[cur] = []; continue
        if cur is not None:
            if line.strip() == "" and sections[cur]:
                cur = None
            else:
                sections[cur].append(line)

    s = SUMMARY.search(text)
    e1 = int(s.group(2)) if s else 0
    e2 = int(s.group(3)) if s else 0
    mx = int(s.group(1)) if s else 0

    print("metric,p50_ns,p99_ns,p999_ns,max_ns,count,e1,e2")
    for name, lines in sections.items():
        bs = []
        for line in lines:
            m = BUCKET.search(line)
            if m:
                bs.append((int(m.group(1)) * SUF[m.group(2)],
                           int(m.group(3)) * SUF[m.group(4)],
                           int(m.group(5))))
        if bs:
            n = sum(c for _, _, c in bs)
            print(f"{name},{percentile(bs, .5)},{percentile(bs, .99)},{percentile(bs, .999)},{mx},{n},{e1},{e2}")


if __name__ == "__main__":
    main(sys.argv[1])
