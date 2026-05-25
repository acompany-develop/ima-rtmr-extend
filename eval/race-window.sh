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

D=$(new_run_dir race-window)
write_meta "$D"
load_module

DUR=${DUR:-60}

run() {
    local name=$1 cmd=$2
    bpftrace "$EVAL_DIR/race-window.bt" >"$D/$name.bpf" 2>&1 &
    local bp=$!
    sleep 1
    bash -c "$cmd" &
    local wp=$!
    sleep "$DUR"
    kill -INT "$wp" "$bp" 2>/dev/null || true
    wait "$wp" "$bp" 2>/dev/null || true
}

run serial 'while true; do /bin/true; done'
run par16 "stress-ng --exec 16 --timeout ${DUR}s"
run par64 "stress-ng --exec 64 --timeout ${DUR}s"
command -v docker >/dev/null && run docker 'while true; do docker run --rm alpine echo hi >/dev/null 2>&1 || break; done'

for f in "$D"/*.bpf; do
    python3 "$EVAL_DIR/hist.py" "$f" >"${f%.bpf}.csv" || true
done

for attr in disabled extended_count skip_count nmissed; do
    [[ -r $SYSFS/$attr ]] && printf '%-20s %s\n' "$attr" "$(cat "$SYSFS/$attr")"
done >"$D/sysfs.txt"
