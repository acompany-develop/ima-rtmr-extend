#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
. "$(dirname "${BASH_SOURCE[0]}")/common.sh"
require_root

D=$(new_run_dir smoke)
write_meta "$D"
load_module

run_serial()   { for _ in $(seq 1 500); do /bin/true; done; }
run_parallel() { stress-ng --exec 16 --timeout 30s >/dev/null 2>&1 || true; }
run_docker()   { command -v docker >/dev/null && seq 1 8 | xargs -P 8 -I _ docker run --rm alpine echo hi >/dev/null 2>&1 || true; }

fail=0
for w in serial parallel docker; do
    "run_$w"
    sleep 2
    sd="$D/$w"; mkdir -p "$sd"
    snapshot "$sd"
    if verify "$sd"; then
        echo "$w MATCH" | tee -a "$D/summary.txt"
    else
        echo "$w NOMATCH" | tee -a "$D/summary.txt"
        fail=1
    fi
done

exit $fail
