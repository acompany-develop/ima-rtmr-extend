#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# Parse bpftrace hist() / lhist() output and emit a CSV with key percentiles.
#
# Input: a file containing bpftrace END-block output that includes
#   @race_ns: hist(...)
#   @race_ns_lin: lhist(...)
# along with summary lines.
#
# Output (CSV on stdout): one row per histogram, with p50, p99, p999, max, count, and a one-line bucket sketch.

import re
import sys
from pathlib import Path


HIST_HEADER = re.compile(r"^@(\w+):\s*$")
HIST_BUCKET_LOG = re.compile(r"^\[(\d+),\s*(\d+)\)\s+(\d+)\s+\|")
HIST_BUCKET_LIN = re.compile(r"^\[(\d+)(?:K|M)?,\s*(\d+)(?:K|M)?\)\s+(\d+)\s+\|")
SUMMARY = re.compile(r"max_ns=(\d+)\s+e1=(\d+)\s+e2=(\d+)")


def parse_buckets(lines: list[str]) -> list[tuple[int, int, int]]:
    """Return [(lo, hi, count), ...]. bpftrace bucket bounds may include K/M suffixes."""
    out: list[tuple[int, int, int]] = []
    suffix_to_factor = {"": 1, "K": 1_000, "M": 1_000_000, "G": 1_000_000_000}
    pat = re.compile(r"^\[(\d+)([KMG]?),\s*(\d+)([KMG]?)\)\s+(\d+)")
    for line in lines:
        m = pat.search(line)
        if not m:
            continue
        lo = int(m.group(1)) * suffix_to_factor[m.group(2)]
        hi = int(m.group(3)) * suffix_to_factor[m.group(4)]
        cnt = int(m.group(5))
        out.append((lo, hi, cnt))
    return out


def percentile(buckets: list[tuple[int, int, int]], q: float) -> int:
    total = sum(c for _, _, c in buckets)
    if total == 0:
        return 0
    target = q * total
    cum = 0
    for lo, hi, c in buckets:
        cum += c
        if cum >= target:
            # Take the bucket midpoint as the estimated value
            return (lo + hi) // 2
    return buckets[-1][1] if buckets else 0


def main(path: str) -> int:
    text = Path(path).read_text(encoding="utf-8", errors="replace")
    sections: dict[str, list[str]] = {}
    current: str | None = None
    for line in text.splitlines():
        m = HIST_HEADER.match(line)
        if m:
            current = m.group(1)
            sections[current] = []
            continue
        if current is not None:
            if line.strip() == "" and sections[current]:
                current = None
            else:
                sections[current].append(line)

    summary = SUMMARY.search(text)
    e1 = int(summary.group(2)) if summary else 0
    e2 = int(summary.group(3)) if summary else 0
    max_ns = int(summary.group(1)) if summary else 0

    print("metric,p50_ns,p99_ns,p999_ns,max_ns,count,e1,e2")
    for name, lines in sections.items():
        buckets = parse_buckets(lines)
        if not buckets:
            continue
        p50 = percentile(buckets, 0.50)
        p99 = percentile(buckets, 0.99)
        p999 = percentile(buckets, 0.999)
        count = sum(c for _, _, c in buckets)
        print(f"{name},{p50},{p99},{p999},{max_ns},{count},{e1},{e2}")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("usage: bpf-hist-summary.py <bpftrace-output>", file=sys.stderr)
        sys.exit(2)
    sys.exit(main(sys.argv[1]))
