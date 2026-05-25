#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# Aggregate raw.tsv from C2 / similar binary-outcome experiments into
# Wilson 95% confidence intervals per N.
#
# Input columns (tab-separated, header on line 1):
#   N  iter  status  ...
# where status is one of {MATCH, NOMATCH}.

import math
import sys
from collections import defaultdict


def wilson_ci(successes: int, total: int, z: float = 1.96) -> tuple[float, float, float]:
    if total == 0:
        return (0.0, 0.0, 0.0)
    p = successes / total
    denom = 1 + z * z / total
    center = (p + z * z / (2 * total)) / denom
    margin = z * math.sqrt(p * (1 - p) / total + z * z / (4 * total * total)) / denom
    return (p, max(0.0, center - margin), min(1.0, center + margin))


def main(path: str) -> int:
    by_n: dict[int, list[bool]] = defaultdict(list)
    with open(path, encoding="utf-8") as f:
        header = f.readline()
        if not header.strip().startswith("N\titer"):
            print(f"unexpected header: {header!r}", file=sys.stderr)
            return 1
        for line in f:
            cols = line.rstrip("\n").split("\t")
            if len(cols) < 3:
                continue
            n = int(cols[0])
            status = cols[2]
            by_n[n].append(status == "MATCH")

    print("N,iters,match,rate,wilson_lo,wilson_hi")
    for n in sorted(by_n):
        outcomes = by_n[n]
        m = sum(outcomes)
        rate, lo, hi = wilson_ci(m, len(outcomes))
        print(f"{n},{len(outcomes)},{m},{rate:.4f},{lo:.4f},{hi:.4f}")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("usage: wilson.py raw.tsv", file=sys.stderr)
        sys.exit(2)
    sys.exit(main(sys.argv[1]))
