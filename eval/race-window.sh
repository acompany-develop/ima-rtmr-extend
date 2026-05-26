#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only

# shellcheck source-path=SCRIPTDIR
# shellcheck source=./common.sh
. "$(dirname "${BASH_SOURCE[0]}")/common.sh"
require_root
command -v bpftrace >/dev/null || {
    echo "bpftrace not installed" >&2
    exit 1
}

d=$(new_run_dir race-window)
write_meta "$d"
load_module

dur="${DUR:-60}"

# Kill a process group rooted at $1, falling back to TERM/KILL escalation.
kill_pgroup() {
    local pid=$1
    local pgid
    pgid=$(ps -o pgid= -p "$pid" 2>/dev/null | tr -d ' ') || true
    if [[ -n $pgid ]]; then
        kill -TERM "-$pgid" 2>/dev/null || true
        sleep 1
        kill -KILL "-$pgid" 2>/dev/null || true
    fi
}

run() {
    local name=$1 cmd=$2
    bpftrace "$eval_dir/race-window.bt" >"$d/$name.bpf" 2>&1 &
    local bp=$!
    sleep 1
    # setsid puts the workload in its own process group so we can signal it as a unit.
    setsid bash -c "$cmd" </dev/null >/dev/null 2>&1 &
    local wp=$!
    sleep "$dur"
    kill_pgroup "$wp"
    kill -INT "$bp" 2>/dev/null || true
    wait "$bp" 2>/dev/null || true
    wait "$wp" 2>/dev/null || true
}

run serial 'while true; do /bin/true; done'
run par16 "stress-ng --exec 16 --timeout ${dur}s"
run par64 "stress-ng --exec 64 --timeout ${dur}s"
command -v docker >/dev/null && run docker 'while true; do docker run --rm alpine echo hi >/dev/null 2>&1 || break; done'

for f in "$d"/*.bpf; do
    python3 "$eval_dir/hist.py" "$f" >"${f%.bpf}.csv" || true
done

for attr in disabled extended_count skip_count nmissed; do
    [[ -r $sysfs_dir/$attr ]] && printf '%-20s %s\n' "$attr" "$(cat "$sysfs_dir/$attr")"
done >"$d/sysfs.txt"
