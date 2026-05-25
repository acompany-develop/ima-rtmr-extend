#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only

# shellcheck source-path=SCRIPTDIR
# shellcheck source=./common.sh
. "$(dirname "${BASH_SOURCE[0]}")/common.sh"
require_root

d=$(new_run_dir syscall-bench)
write_meta "$d"

bin=$eval_dir/syscall-bench
[[ -x $bin && $bin -nt $eval_dir/syscall-bench.c ]] || cc -O2 -Wall -o "$bin" "$eval_dir/syscall-bench.c"

iters="${ITERS:-10000}"
tsc_hz="${TSC_HZ:-$(awk '/cpu MHz/{print int($4*1e6); exit}' /proc/cpuinfo)}"
: "${tsc_hz:=2500000000}"
cpu="${CPU:-4}"
intree="${INTREE:-0}"

run_one() {
    local tag=$1
    taskset -c "$cpu" "$bin" execve "$iters" >"$d/$tag-execve.raw"
    taskset -c "$cpu" "$bin" openat "$iters" /etc/hostname >"$d/$tag-openat.raw"
    taskset -c "$cpu" "$bin" read "$iters" /etc/hostname >"$d/$tag-read.raw"
    for k in execve openat read; do
        python3 "$eval_dir/syscall-stats.py" --tsc-hz "$tsc_hz" "$d/$tag-$k.raw" >"$d/$tag-$k.csv"
    done
}

unload_module
run_one baseline
load_module
run_one oot
unload_module
[[ $intree == 1 && -d $sysfs_dir ]] && run_one intree

if command -v hyperfine >/dev/null; then
    # hyperfine evaluates its argument as a shell command, so single quotes are intentional.
    # shellcheck disable=SC2016
    hyperfine --warmup 3 --runs 20 --export-csv "$d/hyperfine.csv" \
        'for i in $(seq 1 500); do /bin/true; done' || true
fi
