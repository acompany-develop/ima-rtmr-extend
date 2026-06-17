#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# Host-side unit tests for validate.py's pure replay logic. No kernel, sysfs or
# RTMR device required; everything runs against synthetic vectors. Run with:
#   python3 -m unittest discover -s tests
#   python3 tests/test_validate.py

import hashlib
import importlib.util
import tempfile
import unittest
from pathlib import Path

# validate.py lives at the repo root, next to the tests/ directory.
_VALIDATE = Path(__file__).resolve().parent.parent / "validate.py"
_spec = importlib.util.spec_from_file_location("validate", _VALIDATE)
validate = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(validate)


def _chain(baseline: bytes, digests: list[bytes]) -> bytes:
    """Reference RTMR extend chain: r = sha384(r || digest) per entry."""
    r = baseline
    for d in digests:
        r = hashlib.sha384(r + d).digest()
    return r


class ReplayFromTests(unittest.TestCase):
    def setUp(self) -> None:
        self.baseline = bytes(48)  # 48-byte (sha384) zero baseline
        self.digests = [bytes([i]) * 48 for i in range(5)]

    def test_empty_log_matches_when_baseline_equals_actual(self) -> None:
        # No entries to replay: actual already equals the baseline.
        self.assertEqual(validate.replay_from(self.baseline, [], self.baseline, 0), 0)

    def test_full_replay_from_zero(self) -> None:
        actual = _chain(self.baseline, self.digests)
        end = validate.replay_from(self.baseline, self.digests, actual, 0)
        self.assertEqual(end, len(self.digests))

    def test_replay_with_skip(self) -> None:
        # Skip the first two entries; baseline already folds them in.
        skip = 2
        baseline = _chain(bytes(48), self.digests[:skip])
        actual = _chain(baseline, self.digests[skip:])
        end = validate.replay_from(baseline, self.digests, actual, skip)
        self.assertEqual(end, len(self.digests))

    def test_partial_match_midchain(self) -> None:
        # actual matches after replaying only the first 3 of 5 entries.
        actual = _chain(self.baseline, self.digests[:3])
        end = validate.replay_from(self.baseline, self.digests, actual, 0)
        self.assertEqual(end, 3)

    def test_no_match_returns_none(self) -> None:
        actual = b"\xff" * 48
        self.assertIsNone(validate.replay_from(self.baseline, self.digests, actual, 0))

    def test_wrong_skip_does_not_match_directly(self) -> None:
        # Correct chain starts at skip=2; replaying from skip=0 must not match.
        skip = 2
        baseline = _chain(bytes(48), self.digests[:skip])
        actual = _chain(baseline, self.digests[skip:])
        # From the wrong baseline-relative start the running hash diverges.
        self.assertIsNone(validate.replay_from(baseline, self.digests, actual, 0))


class LoadLogTests(unittest.TestCase):
    def test_parse_digest_strips_alg_prefix(self) -> None:
        self.assertEqual(validate.parse_digest("sha384:00ff"), b"\x00\xff")
        self.assertEqual(validate.parse_digest("00ff"), b"\x00\xff")

    def test_load_log_extracts_second_column(self) -> None:
        # IMA ascii log format: "<pcr> <template-digest> <template> ..."
        lines = [
            "10 sha384:0011 ima-ng /path/a\n",
            "10 sha384:2233 ima-ng /path/b\n",
            "\n",  # blank line ignored
            "10 sha384:4455 ima-ng /path/c\n",
        ]
        with tempfile.NamedTemporaryFile("w", suffix=".log", delete=False) as f:
            f.writelines(lines)
            path = f.name
        try:
            digests = validate.load_log(path)
        finally:
            Path(path).unlink()
        self.assertEqual(digests, [b"\x00\x11", b"\x22\x33", b"\x44\x55"])


if __name__ == "__main__":
    unittest.main()
