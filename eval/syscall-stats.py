#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
#
# Convert TSC cycles from syscall-bench output to ns and summarise.

import argparse, collections, statistics, sys


def main():
    p = argparse.ArgumentParser()
    p.add_argument("input")
    p.add_argument("--tsc-hz", type=float, required=True)
    p.add_argument("--warmup", type=int, default=100)
    a = p.parse_args()

    by_kind = collections.defaultdict(list)
    with open(a.input, encoding="utf-8") as f:
        for line in f:
            parts = line.split()
            if len(parts) == 2:
                try:
                    by_kind[parts[0]].append(int(parts[1]))
                except ValueError:
                    pass

    ns_per_cycle = 1e9 / a.tsc_hz
    print("kind,samples,mean_ns,p50_ns,p95_ns,p99_ns,p999_ns,max_ns")
    for kind, vals in by_kind.items():
        if len(vals) <= a.warmup:
            continue
        ns = sorted(v * ns_per_cycle for v in vals[a.warmup:])
        n = len(ns)
        pct = lambda q: ns[min(n - 1, int(q * (n - 1)))]
        print(f"{kind},{n},{statistics.fmean(ns):.0f},"
              f"{pct(.5):.0f},{pct(.95):.0f},{pct(.99):.0f},{pct(.999):.0f},{ns[-1]:.0f}")


if __name__ == "__main__":
    sys.exit(main() or 0)
