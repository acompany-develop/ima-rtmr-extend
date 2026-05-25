#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.

import math
import sys
from collections import defaultdict


def wilson(k: int, n: int, z: float = 1.96) -> tuple[float, float, float]:
    if not n:
        return 0.0, 0.0, 0.0
    p: float = k / n
    d: float = 1 + z * z / n
    c: float = (p + z * z / (2 * n)) / d
    m: float = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / d
    return p, max(0.0, c - m), min(1.0, c + m)


def load_outcomes(path: str) -> dict[int, list[bool]]:
    by_n: dict[int, list[bool]] = defaultdict(list)
    with open(path, encoding="utf-8") as f:
        next(f)
        for line in f:
            cols: list[str] = line.rstrip("\n").split("\t")
            if len(cols) >= 3:
                by_n[int(cols[0])].append(cols[2] == "MATCH")
    return by_n


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: wilson.py raw.tsv", file=sys.stderr)
        return 2
    by_n: dict[int, list[bool]] = load_outcomes(sys.argv[1])
    print("n,iters,match,rate,lo,hi")
    for n in sorted(by_n):
        outs: list[bool] = by_n[n]
        k: int = sum(outs)
        rate, lo, hi = wilson(k, len(outs))
        print(f"{n},{len(outs)},{k},{rate:.4f},{lo:.4f},{hi:.4f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
