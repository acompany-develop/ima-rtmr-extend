#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only

# shellcheck source-path=SCRIPTDIR
# shellcheck source=./common.sh
. "$(dirname "${BASH_SOURCE[0]}")/common.sh"
require_root

d=$(new_run_dir replay-match)
write_meta "$d"
load_module

iters="${ITERS:-20}"
dur="${DUR:-30}"
raw=$d/raw.tsv
printf 'n\titer\tstatus\tlog_lines\textended_count\tnmissed\n' >"$raw"

for n in 1 16 64 256; do
    for i in $(seq 1 "$iters"); do
        stress-ng --exec "$n" --timeout "${dur}s" >/dev/null 2>&1 || true
        sleep 2
        sd=$(mktemp -d)
        snapshot "$sd"
        lines=$(wc -l <"$sd/ima.log")
        ext=$(cat "$sd/extended_count.txt" 2>/dev/null || echo 0)
        nm=$(cat "$sd/nmissed.txt" 2>/dev/null || echo 0)
        if verify "$sd" --no-search; then status=MATCH; else status=NOMATCH; fi
        printf '%d\t%d\t%s\t%d\t%d\t%d\n' "$n" "$i" "$status" "$lines" "$ext" "$nm" >>"$raw"
        if [[ $status == NOMATCH ]]; then
            mv "$sd" "$d/n${n}-i${i}-nomatch"
        else
            rm -rf "$sd"
        fi
    done
done

python3 "$eval_dir/wilson.py" "$raw" >"$d/wilson.csv"
column -t -s $'\t' "$d/wilson.csv"
