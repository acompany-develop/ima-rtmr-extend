#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.

import argparse
import hashlib
import sys
from pathlib import Path

DEFAULT_RTMR = "/sys/class/misc/tdx_guest/measurements/rtmr2:sha384"
DEFAULT_LOG = "/sys/kernel/security/ima/ascii_runtime_measurements_sha384"
DEFAULT_SYSFS = "/sys/kernel/ima_rtmr"


def parse_digest(token: str) -> bytes:
    return bytes.fromhex(token.split(":", 1)[-1])


def load_log(path: str) -> list[bytes]:
    with open(path, encoding="utf-8", errors="surrogateescape") as f:
        return [parse_digest(line.split()[1]) for line in f if line.strip()]


def replay_from(
    baseline: bytes, digests: list[bytes], actual: bytes, start: int
) -> int | None:
    if baseline == actual:
        return start
    r = baseline
    for i in range(start, len(digests)):
        r = hashlib.sha384(r + digests[i]).digest()
        if r == actual:
            return i + 1
    return None


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("initial_rtmr", nargs="?", help=f"hex digest; default: read from {DEFAULT_SYSFS}/initial")
    p.add_argument("skip", nargs="?", type=int, help=f"entries to skip; default: read from {DEFAULT_SYSFS}/skip_count")
    p.add_argument("--rtmr", default=DEFAULT_RTMR)
    p.add_argument("--log", default=DEFAULT_LOG)
    p.add_argument("--sysfs", default=DEFAULT_SYSFS)
    p.add_argument("--no-search", action="store_true")
    args = p.parse_args()

    sysfs = Path(args.sysfs)
    initial_hex = args.initial_rtmr or (sysfs / "initial").read_text().strip()
    skip = args.skip if args.skip is not None else int((sysfs / "skip_count").read_text().strip())

    baseline: bytes = bytes.fromhex(initial_hex)
    with open(args.rtmr, "rb") as f:
        actual: bytes = f.read()
    digests: list[bytes] = load_log(args.log)
    total: int = len(digests)

    print(f"log entries: {total}")
    print(f"actual rtmr: {actual.hex()}")

    end = replay_from(baseline, digests, actual, skip)
    if end is not None:
        print(f"MATCH (direct): skip={skip} end={end} replayed={end - skip}")
        return 0

    print(f"no match with skip={skip}")
    if args.no_search:
        return 1

    print("scanning start indices...")
    for s in range(total):
        if s == skip:
            continue
        end = replay_from(baseline, digests, actual, s)
        if end is not None:
            print(
                f"MATCH (scan): skip={s} end={end} replayed={end - s} (delta {s - skip:+d})"
            )
            return 0

    print(f"NO MATCH for any start in [0, {total})", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
