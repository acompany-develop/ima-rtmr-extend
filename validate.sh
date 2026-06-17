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

sysfs_dir="${SYSFS_DIR:-/sys/kernel/ima_rtmr}"
rtmr="${1:-}"
skip="${2:-}"

if [[ -z $rtmr ]]; then
    rtmr=$(cat "${sysfs_dir}/initial")
fi
if [[ -z $skip ]]; then
    skip=$(cat "${sysfs_dir}/skip_count")
fi

# Fail closed: if the module permanently disabled extension (RTMR write error,
# missing digest, ...), the RTMR has diverged from the IMA log and any "match"
# below would be meaningless. Refuse to validate.
if [[ -r ${sysfs_dir}/disabled ]]; then
    disabled=$(cat "${sysfs_dir}/disabled")
    if [[ $disabled != "0" ]]; then
        echo "extension is disabled (${sysfs_dir}/disabled=${disabled}); RTMR diverged from IMA log" >&2
        exit 1
    fi
fi

actual_rtmr="$(xxd -p /sys/class/misc/tdx_guest/measurements/rtmr2:sha384 | tr -d '\n')"

if [[ -z $actual_rtmr ]]; then
    echo "Failed to read actual RTMR[2]" >&2
    exit 1
fi

# The initial baseline and the actual RTMR must describe the same digest width.
# A mismatch means the wrong RTMR/initial pair was supplied; replaying it would
# silently never match (or match by accident), so reject up front.
if ((${#rtmr} != ${#actual_rtmr})); then
    echo "initial RTMR width ${#rtmr} does not match actual RTMR width ${#actual_rtmr}" >&2
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
    digest="${digests[$i]}"
    # Violation entries log an all-zero digest; the module extends 0xFF to match
    # the kernel's PCR invalidation, so replay 0xFF in their place.
    if [[ $digest =~ ^0+$ ]]; then
        digest="${digest//0/f}"
    fi
    rtmr=$(echo -n "${rtmr}${digest}" | xxd -r -p | sha384sum | awk '{print $1}')

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
