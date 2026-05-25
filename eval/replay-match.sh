#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only

# shellcheck source-path=SCRIPTDIR
# shellcheck source=./common.sh
. "$(dirname "${BASH_SOURCE[0]}")/common.sh"
require_root

D=$(new_run_dir replay-match)
write_meta "$D"
load_module

ITERS=${ITERS:-20}
DUR=${DUR:-30}
RAW=$D/raw.tsv
printf 'N\titer\tstatus\tlog_lines\textended_count\tnmissed\n' >"$RAW"

for N in 1 16 64 256; do
    for i in $(seq 1 "$ITERS"); do
        stress-ng --exec "$N" --timeout "${DUR}s" >/dev/null 2>&1 || true
        sleep 2
        sd=$(mktemp -d)
        snapshot "$sd"
        lines=$(wc -l <"$sd/ima.log")
        ext=$(cat "$sd/extended_count.txt" 2>/dev/null || echo 0)
        nm=$(cat "$sd/nmissed.txt" 2>/dev/null || echo 0)
        if verify "$sd" --no-search; then status=MATCH; else status=NOMATCH; fi
        printf '%d\t%d\t%s\t%d\t%d\t%d\n' "$N" "$i" "$status" "$lines" "$ext" "$nm" >>"$RAW"
        if [[ $status == NOMATCH ]]; then
            mv "$sd" "$D/N${N}-i${i}-NOMATCH"
        else
            rm -rf "$sd"
        fi
    done
done

python3 "$EVAL_DIR/wilson.py" "$RAW" >"$D/wilson.csv"
column -t -s $'\t' "$D/wilson.csv"
