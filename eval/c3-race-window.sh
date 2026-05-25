#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# C3: race window distribution.
#   - Run bpftrace race-window.bt in the background.
#   - Drive workloads W1..W4 sequentially.
#   - Stop bpftrace and post-process its histogram output.

SCRIPT_NAME=c3
. "$(dirname "${BASH_SOURCE[0]}")/lib/common.sh"
require_root

if ! command -v bpftrace >/dev/null 2>&1; then
    log "bpftrace not installed"
    exit 1
fi

DIR=$(init_run_dir c3)
write_meta "$DIR"
log "results dir: $DIR"

load_module

DURATION=${C3_DURATION:-60}

run_workload() {
    local name=$1
    local cmd=$2
    log "workload: $name ($DURATION s)"
    bash -c "$cmd" &
    local pid=$!
    sleep "$DURATION"
    kill -INT "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
}

for entry in \
    "w1 'while true; do /bin/true; done'" \
    "w2-16 'stress-ng --exec 16 --timeout ${DURATION}s'" \
    "w2-64 'stress-ng --exec 64 --timeout ${DURATION}s'" \
    "w3-docker 'while true; do docker run --rm alpine echo hi >/dev/null 2>&1 || break; done'" \
    "w4-build '
        rm -rf /tmp/c3-build && mkdir /tmp/c3-build && cd /tmp/c3-build
        cat >h.c <<EOF
#include <stdio.h>
int main(void) { puts(\"x\"); return 0; }
EOF
        while true; do cc h.c -o h && ./h >/dev/null; done
    '"; do
    name=${entry%% *}
    cmd=${entry#* }
    out="$DIR/$name.bpf"
    log "starting bpftrace -> $out"
    bpftrace "$EVAL_ROOT/bpftrace/race-window.bt" >"$out" 2>&1 &
    bpf_pid=$!
    sleep 1
    run_workload "$name" "$cmd"
    sleep 2
    log "stopping bpftrace ($bpf_pid)"
    kill -INT "$bpf_pid" 2>/dev/null || true
    wait "$bpf_pid" 2>/dev/null || true
done

# Snapshot final extended_count / nmissed / disabled for the run
log "final sysfs state:"
{
    for attr in disabled extended_count skip_count nmissed; do
        if [[ -r $IMA_RTMR_SYSFS/$attr ]]; then
            printf '%-20s %s\n' "$attr" "$(cat "$IMA_RTMR_SYSFS/$attr")"
        fi
    done
} | tee -a "$DIR/sysfs-final.txt"

# Post-process bpftrace output into per-workload percentile CSVs.
log "summarising histograms"
for bpf in "$DIR"/*.bpf; do
    name=$(basename "$bpf" .bpf)
    python3 "$EVAL_ROOT/python/bpf-hist-summary.py" "$bpf" >"$DIR/$name.csv" || true
done

log "C3 complete. inspect $DIR for histograms and summaries."
