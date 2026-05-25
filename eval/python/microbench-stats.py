#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# Convert microbench rdtscp output to nanoseconds using a measured
# TSC frequency, then emit percentile summary.
#
# Input: one "<kind> <tsc_cycles>" per line.
# Args:
#   --tsc-hz <hz>   measured TSC frequency (Hz)
#   --warmup <n>    drop first N samples per kind (default 100)
#
# Output: CSV
#   kind,samples,mean_ns,p50_ns,p95_ns,p99_ns,p999_ns,max_ns

import argparse
import collections
import statistics
import sys


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("input")
    p.add_argument("--tsc-hz", type=float, required=True)
    p.add_argument("--warmup", type=int, default=100)
    args = p.parse_args()

    by_kind: dict[str, list[int]] = collections.defaultdict(list)
    with open(args.input, encoding="utf-8") as f:
        for line in f:
            parts = line.split()
            if len(parts) != 2:
                continue
            try:
                cycles = int(parts[1])
            except ValueError:
                continue
            by_kind[parts[0]].append(cycles)

    ns_per_cycle = 1e9 / args.tsc_hz
    print("kind,samples,mean_ns,p50_ns,p95_ns,p99_ns,p999_ns,max_ns")
    for kind, vals in by_kind.items():
        if len(vals) <= args.warmup:
            continue
        ns = sorted(v * ns_per_cycle for v in vals[args.warmup:])
        n = len(ns)
        def pct(q: float) -> float:
            idx = min(n - 1, max(0, int(q * (n - 1))))
            return ns[idx]
        print(
            f"{kind},{n},{statistics.fmean(ns):.0f},"
            f"{pct(0.50):.0f},{pct(0.95):.0f},{pct(0.99):.0f},{pct(0.999):.0f},{ns[-1]:.0f}"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
