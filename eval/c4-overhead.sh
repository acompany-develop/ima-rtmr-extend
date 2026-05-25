#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# C4: Syscall overhead.
#   - Build microbench.c if needed
#   - Run execve / openat / read microbenchmarks under three conditions:
#       B0 baseline (module not loaded)
#       B2 OOT module loaded
#       B4 in-tree (built-in) — requires booting an in-tree kernel; if not in-tree, skipped
#   - Also run macro: docker run echo, lightweight kernel build
#   - Per-iteration TSC samples written; post-processing converts to ns.

SCRIPT_NAME=c4
. "$(dirname "${BASH_SOURCE[0]}")/lib/common.sh"
require_root

DIR=$(init_run_dir c4)
write_meta "$DIR"
log "results dir: $DIR"

MB="$EVAL_ROOT/c/microbench"
if [[ ! -x $MB || $EVAL_ROOT/c/microbench.c -nt $MB ]]; then
    log "building microbench"
    cc -O2 -Wall -o "$MB" "$EVAL_ROOT/c/microbench.c"
fi

ITERS=${C4_ITERS:-10000}
WARMUP=${C4_WARMUP:-100}

# Try to learn TSC frequency. On TDX guests /proc/cpuinfo "tsc_khz" may be absent;
# fall back to the bootloader-reported value or 2_500_000_000 Hz.
TSC_HZ=$(dmesg 2>/dev/null | grep -m1 -oE 'tsc:[^0-9]*[0-9]+' | grep -oE '[0-9]+' | head -1)
if [[ -z $TSC_HZ ]]; then
    TSC_HZ=$(grep -m1 'cpu MHz' /proc/cpuinfo | awk '{print int($4*1000000)}')
fi
if [[ -z $TSC_HZ ]]; then
    TSC_HZ=2500000000
    log "warning: TSC freq fallback to $TSC_HZ Hz"
fi
log "TSC freq = $TSC_HZ Hz"

run_condition() {
    local label=$1
    local raw="$DIR/${label}-execve.raw"

    log "[$label] execve microbench ($ITERS iters)"
    taskset -c "${C4_CPU:-4}" "$MB" execve "$ITERS" >"$raw"
    python3 "$EVAL_ROOT/python/microbench-stats.py" \
        --tsc-hz "$TSC_HZ" --warmup "$WARMUP" "$raw" >"$DIR/${label}-execve.csv"

    log "[$label] openat microbench"
    taskset -c "${C4_CPU:-4}" "$MB" openat "$ITERS" /etc/hostname >"$DIR/${label}-openat.raw"
    python3 "$EVAL_ROOT/python/microbench-stats.py" \
        --tsc-hz "$TSC_HZ" --warmup "$WARMUP" "$DIR/${label}-openat.raw" >"$DIR/${label}-openat.csv"

    log "[$label] read microbench"
    taskset -c "${C4_CPU:-4}" "$MB" read "$ITERS" /etc/hostname >"$DIR/${label}-read.raw"
    python3 "$EVAL_ROOT/python/microbench-stats.py" \
        --tsc-hz "$TSC_HZ" --warmup "$WARMUP" "$DIR/${label}-read.raw" >"$DIR/${label}-read.csv"
}

# B0: baseline — ensure module is not loaded
unload_module
run_condition b0-baseline

# B2: OOT module loaded
load_module
run_condition b2-oot
unload_module

# B4: in-tree — detect via presence of /sys/kernel/ima_rtmr/ with our built-in (no module load)
# Operator must boot an in-tree kernel and re-run with C4_INTREE=1 to record this condition.
if [[ ${C4_INTREE:-0} == 1 ]]; then
    if [[ -d $IMA_RTMR_SYSFS ]] && ! lsmod | grep -qE '^ima_rtmr '; then
        run_condition b4-intree
    else
        log "C4_INTREE=1 but in-tree state not detected; skipping"
    fi
fi

# --- Macro: docker run echo (if docker available) ---
if command -v docker >/dev/null 2>&1; then
    log "macro: docker run alpine echo (30 reps)"
    {
        for _ in $(seq 1 30); do
            /usr/bin/time -f '%e' docker run --rm alpine echo hi 2>&1 >/dev/null | tail -1
        done
    } >"$DIR/docker-run-echo.times"
fi

# --- Macro: hyperfine on a 50-loop /bin/true (if available) ---
if command -v hyperfine >/dev/null 2>&1; then
    log "macro: hyperfine /bin/true x500"
    hyperfine --warmup 3 --runs 20 --export-csv "$DIR/hyperfine-true.csv" \
        'for i in $(seq 1 500); do /bin/true; done' || true
fi

log "C4 complete. CSV summaries in $DIR/*.csv"
