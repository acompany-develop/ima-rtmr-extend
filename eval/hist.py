#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# Parse bpftrace hist()/lhist() output and emit p50/p99/p999/max as CSV.

import re
import sys
from pathlib import Path

SUFFIX = {"": 1, "K": 1_000, "M": 1_000_000, "G": 1_000_000_000}
BUCKET = re.compile(r"^\[(\d+)([KMG]?),\s*(\d+)([KMG]?)\)\s+(\d+)")
HEAD = re.compile(r"^@(\w+):\s*$")
SUMMARY = re.compile(r"max=(\d+)\s+e1=(\d+)\s+e2=(\d+)")


def percentile(buckets: list[tuple[int, int, int]], q: float) -> int:
    total: int = sum(c for _, _, c in buckets)
    if not total:
        return 0
    cum: int = 0
    for lo, hi, c in buckets:
        cum += c
        if cum >= q * total:
            return (lo + hi) // 2
    return buckets[-1][1]


def parse_sections(text: str) -> dict[str, list[str]]:
    sections: dict[str, list[str]] = {}
    cur: str | None = None
    for line in text.splitlines():
        m = HEAD.match(line)
        if m:
            cur = m.group(1)
            sections[cur] = []
            continue
        if cur is not None:
            if line.strip() == "" and sections[cur]:
                cur = None
            else:
                sections[cur].append(line)
    return sections


def parse_buckets(lines: list[str]) -> list[tuple[int, int, int]]:
    buckets: list[tuple[int, int, int]] = []
    for line in lines:
        m = BUCKET.search(line)
        if m:
            buckets.append(
                (
                    int(m.group(1)) * SUFFIX[m.group(2)],
                    int(m.group(3)) * SUFFIX[m.group(4)],
                    int(m.group(5)),
                )
            )
    return buckets


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: hist.py <bpftrace-output>", file=sys.stderr)
        return 2

    text: str = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
    sections: dict[str, list[str]] = parse_sections(text)

    summary = SUMMARY.search(text)
    e1: int = int(summary.group(2)) if summary else 0
    e2: int = int(summary.group(3)) if summary else 0
    max_ns: int = int(summary.group(1)) if summary else 0

    print("metric,p50_ns,p99_ns,p999_ns,max_ns,count,e1,e2")
    for name, lines in sections.items():
        buckets: list[tuple[int, int, int]] = parse_buckets(lines)
        if buckets:
            count: int = sum(c for _, _, c in buckets)
            print(
                f"{name},{percentile(buckets, 0.5)},{percentile(buckets, 0.99)},"
                f"{percentile(buckets, 0.999)},{max_ns},{count},{e1},{e2}"
            )
    return 0


if __name__ == "__main__":
    sys.exit(main())
