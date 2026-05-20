#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# Validate RTMR[2] by replaying the IMA SHA-384 measurement log.
#
# Snapshot RTMR[2] before reading the IMA log so that entries added later
# are naturally ignored: replay one entry at a time and accept the first
# running hash that matches the snapshot.

set -eEuo pipefail

if ((EUID != 0)); then
    echo "This script must be run as root" >&2
    exit 1
fi

rtmr="${1:?"usage: $0 <initial_rtmr_hex> [skip_count]"}"
skip="${2:-"1"}"

actual_rtmr="$(xxd -p /sys/class/misc/tdx_guest/measurements/rtmr2:sha384 | tr -d '\n')"

if [[ -z $actual_rtmr ]]; then
    echo "Failed to read actual RTMR[2]" >&2
    exit 1
fi

readarray -t digests < <(
    tail -n +"$((skip + 1))" /sys/kernel/security/ima/ascii_runtime_measurements_sha384 | awk '{print $2}'
)

if ((${#digests[@]} == 0)); then
    echo "No IMA log entries found (after skipping $skip)" >&2
    exit 1
fi

for i in "${!digests[@]}"; do
    rtmr=$(echo -n "${rtmr}${digests[$i]}" | xxd -r -p | sha384sum | awk '{print $1}')

    if [[ $rtmr == "$actual_rtmr" ]]; then
        echo "match at entry $((i + 1 + skip)) of $((${#digests[@]} + skip)) total" >&2
        echo "calculated: ${rtmr}"
        echo "actual:     ${actual_rtmr}"
        exit 0
    fi
done

echo "no match found after replaying all ${#digests[@]} entries" >&2
echo "last calculated: ${rtmr}"
echo "actual:          ${actual_rtmr}"
exit 1
