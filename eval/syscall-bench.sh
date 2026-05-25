#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
. "$(dirname "${BASH_SOURCE[0]}")/common.sh"
require_root

D=$(new_run_dir syscall-bench)
write_meta "$D"

BIN=$EVAL_DIR/syscall-bench
[[ -x $BIN && $BIN -nt $EVAL_DIR/syscall-bench.c ]] || cc -O2 -Wall -o "$BIN" "$EVAL_DIR/syscall-bench.c"

N=${N:-10000}
TSC_HZ=${TSC_HZ:-$(awk '/cpu MHz/{print int($4*1e6); exit}' /proc/cpuinfo)}
: "${TSC_HZ:=2500000000}"

run_one() {
    local tag=$1
    taskset -c "${CPU:-4}" "$BIN" execve "$N"             >"$D/$tag-execve.raw"
    taskset -c "${CPU:-4}" "$BIN" openat "$N" /etc/hostname >"$D/$tag-openat.raw"
    taskset -c "${CPU:-4}" "$BIN" read   "$N" /etc/hostname >"$D/$tag-read.raw"
    for k in execve openat read; do
        python3 "$EVAL_DIR/syscall-stats.py" --tsc-hz "$TSC_HZ" "$D/$tag-$k.raw" >"$D/$tag-$k.csv"
    done
}

unload_module; run_one baseline
load_module;   run_one oot
unload_module
[[ ${INTREE:-0} == 1 && -d $SYSFS ]] && run_one intree

if command -v hyperfine >/dev/null; then
    hyperfine --warmup 3 --runs 20 --export-csv "$D/hyperfine.csv" \
        'for i in $(seq 1 500); do /bin/true; done' || true
fi
