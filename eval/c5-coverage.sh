#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# C5: in-tree vs OOT coverage and the `initial` baseline mechanism.
#
# Usage:
#   sudo eval/c5-coverage.sh [ima_policy=tcb|critical_data]
#
# What this run does:
#   1. Snapshot IMA log line count and /sys/kernel/ima_rtmr/{initial,skip_count,extended_count}
#   2. If module is loaded (OOT mode), the snapshot is taken after a short warm-up workload
#   3. Validate the snapshot with validate.py using the published `initial` baseline
#   4. Record per-policy coverage = extended_count / (ima_log_lines - skip_count)
#
# To collect the in-tree side: build the kernel with CONFIG_IMA_RTMR=y, reboot,
# and re-run this script. Results from both runs can be diffed offline.

SCRIPT_NAME=c5
. "$(dirname "${BASH_SOURCE[0]}")/lib/common.sh"
require_root

policy_arg=${1:-ima_policy=tcb}

DIR=$(init_run_dir c5)
write_meta "$DIR"
log "results dir: $DIR  policy_arg=$policy_arg"

mode=oot
if [[ -d $IMA_RTMR_SYSFS ]] && ! lsmod | grep -qE '^ima_rtmr '; then
    mode=intree
fi
echo "$mode" >"$DIR/mode.txt"
log "detected mode: $mode"

if [[ $mode == oot ]]; then
    load_module
fi

# Apply IMA policy override if requested. For runtime config we expect
# /sys/kernel/security/ima/policy to be writable, which requires CONFIG_IMA_WRITE_POLICY=y.
# If not writable, log it and continue (the caller may have set the policy via cmdline).
if [[ -w /sys/kernel/security/ima/policy ]]; then
    case $policy_arg in
        ima_policy=tcb)
            log "policy: leaving default (tcb)"
            ;;
        ima_policy=critical_data)
            log "policy: adding CRITICAL_DATA measure rule"
            echo "measure func=CRITICAL_DATA" >/sys/kernel/security/ima/policy || true
            ;;
        *)
            log "unknown policy_arg: $policy_arg"
            ;;
    esac
else
    log "policy file not writable; assuming command-line policy is in effect"
fi

# Warm-up workload to generate measurements
log "warm-up workload"
for _ in $(seq 1 50); do /bin/true; done
stress-ng --exec 8 --timeout 10s >/dev/null 2>&1 || true
sleep 2

# Snapshot
snapshot_state "$DIR"
log_lines=$(wc -l <"$DIR/ima.log")
initial=$(cat "$DIR/initial.txt" 2>/dev/null || echo "")
skip=$(cat "$DIR/skip_count.txt" 2>/dev/null || echo 0)
ext=$(cat "$DIR/extended_count.txt" 2>/dev/null || echo 0)
disabled=$(cat "$DIR/disabled.txt" 2>/dev/null || echo 0)

cat >"$DIR/coverage.json" <<EOF
{
  "mode": "$mode",
  "policy_arg": "$policy_arg",
  "ima_log_lines": $log_lines,
  "skip_count": $skip,
  "extended_count": $ext,
  "post_load_lines": $((log_lines - skip)),
  "disabled": $disabled
}
EOF

log "validating with validate.py (using sysfs initial baseline)"
if python3 "$REPO_ROOT/validate.py" \
        "$initial" "$skip" \
        --rtmr "$DIR/rtmr2.bin" \
        --log "$DIR/ima.log" \
        --sysfs "$IMA_RTMR_SYSFS" \
        >"$DIR/validate.out" 2>&1; then
    status=MATCH
else
    status=NOMATCH
fi
echo "$status" >"$DIR/validate.status"
log "validate -> $status"

log "summary:"
cat "$DIR/coverage.json" | tee -a "$DIR/c5.log"
log "C5 complete (mode=$mode policy=$policy_arg validate=$status)"
