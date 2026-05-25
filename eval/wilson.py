#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
import math, sys
from collections import defaultdict


def wilson(k: int, n: int, z: float = 1.96) -> tuple[float, float, float]:
    if not n:
        return 0.0, 0.0, 0.0
    p = k / n
    d = 1 + z * z / n
    c = (p + z * z / (2 * n)) / d
    m = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / d
    return p, max(0.0, c - m), min(1.0, c + m)


def main(path: str) -> int:
    by_n: dict[int, list[bool]] = defaultdict(list)
    with open(path, encoding="utf-8") as f:
        next(f)
        for line in f:
            c = line.rstrip("\n").split("\t")
            if len(c) >= 3:
                by_n[int(c[0])].append(c[2] == "MATCH")
    print("N,iters,match,rate,lo,hi")
    for n in sorted(by_n):
        outs = by_n[n]
        k = sum(outs)
        rate, lo, hi = wilson(k, len(outs))
        print(f"{n},{len(outs)},{k},{rate:.4f},{lo:.4f},{hi:.4f}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
