#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# Convert TSC cycles from syscall-bench output to ns and summarise.

import argparse
import collections
import statistics
import sys


def load_cycles(path: str) -> dict[str, list[int]]:
    by_kind: dict[str, list[int]] = collections.defaultdict(list)
    with open(path, encoding="utf-8") as f:
        for line in f:
            parts: list[str] = line.split()
            if len(parts) == 2:
                try:
                    by_kind[parts[0]].append(int(parts[1]))
                except ValueError:
                    pass
    return by_kind


def percentile(values: list[float], q: float) -> float:
    n: int = len(values)
    return values[min(n - 1, int(q * (n - 1)))]


def summarise(
    cycles: list[int], tsc_hz: float, warmup: int
) -> tuple[int, float, float, float, float, float, float] | None:
    if len(cycles) <= warmup:
        return None
    ns_per_cycle: float = 1e9 / tsc_hz
    ns: list[float] = sorted(v * ns_per_cycle for v in cycles[warmup:])
    return (
        len(ns),
        statistics.fmean(ns),
        percentile(ns, 0.5),
        percentile(ns, 0.95),
        percentile(ns, 0.99),
        percentile(ns, 0.999),
        ns[-1],
    )


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("input")
    p.add_argument("--tsc-hz", type=float, required=True)
    p.add_argument("--warmup", type=int, default=100)
    args = p.parse_args()

    by_kind: dict[str, list[int]] = load_cycles(args.input)
    print("kind,samples,mean_ns,p50_ns,p95_ns,p99_ns,p999_ns,max_ns")
    for kind, cycles in by_kind.items():
        s = summarise(cycles, args.tsc_hz, args.warmup)
        if s is None:
            continue
        n, mean, p50, p95, p99, p999, mx = s
        print(
            f"{kind},{n},{mean:.0f},{p50:.0f},{p95:.0f},{p99:.0f},{p999:.0f},{mx:.0f}"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
