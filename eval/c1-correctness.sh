#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# C1: Basic correctness — verify that the IMA log replays into the live RTMR[2].
# Runs each workload W1, W2 (small N), W4 once and confirms validate.py succeeds.

SCRIPT_NAME=c1
. "$(dirname "${BASH_SOURCE[0]}")/lib/common.sh"
require_root

DIR=$(init_run_dir c1)
write_meta "$DIR"
log "results dir: $DIR"

load_module

# Workload runners
w1_serial_exec() { for _ in $(seq 1 500); do /bin/true; done; }
w2_parallel_exec() { stress-ng --exec 16 --exec-ops 2000 --timeout 30s >/dev/null 2>&1 || true; }
w3_docker_burst() {
    command -v docker >/dev/null 2>&1 || { log "docker not available, skipping W3"; return; }
    seq 1 8 | xargs -P 8 -I _ docker run --rm alpine echo hello >/dev/null 2>&1 || true
}
w4_kernel_build_smoke() {
    # A lightweight stand-in: compile /usr/src kernel headers' samples if available, else fall back to make in a small project.
    cd /tmp
    rm -rf c1-build-smoke && mkdir c1-build-smoke && cd c1-build-smoke
    cat >hello.c <<'EOF'
#include <stdio.h>
int main(void) { puts("hi"); return 0; }
EOF
    for _ in $(seq 1 100); do cc hello.c -o hello && ./hello >/dev/null; done
}

for w in w1_serial_exec w2_parallel_exec w3_docker_burst w4_kernel_build_smoke; do
    log "running workload: $w"
    workload_start=$(date +%s%N)
    $w
    workload_end=$(date +%s%N)

    # Settle: wait for worker queue to drain.
    sleep 2

    snap_dir="$DIR/$w"
    mkdir -p "$snap_dir"
    snapshot_state "$snap_dir"

    log "verifying $w with validate.py"
    if (cd "$snap_dir" && python3 "$REPO_ROOT/validate.py" \
            "$(cat initial.txt 2>/dev/null || true)" \
            "$(cat skip_count.txt 2>/dev/null || echo 0)" \
            --rtmr ./rtmr2.bin \
            --log ./ima.log \
            --sysfs "$IMA_RTMR_SYSFS" \
            >"validate.out" 2>&1); then
        status=MATCH
    else
        status=NOMATCH
    fi
    echo "$workload_start $workload_end $status" >>"$DIR/summary.txt"
    log "$w -> $status"
done

log "summary:"
cat "$DIR/summary.txt" | tee -a "$DIR/c1.log"

if grep -q NOMATCH "$DIR/summary.txt"; then
    log "C1 FAILED: at least one workload did not match"
    exit 2
fi

log "C1 PASS"
