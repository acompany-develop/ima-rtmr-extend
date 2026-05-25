#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# C2: Parallel correctness — replay match rate under stress-ng --exec N
# for N in {1, 16, 64, 256}, 20 iterations each. Output Wilson 95% CI per N.

SCRIPT_NAME=c2
. "$(dirname "${BASH_SOURCE[0]}")/lib/common.sh"
require_root

DIR=$(init_run_dir c2)
write_meta "$DIR"
log "results dir: $DIR"

load_module

N_LIST=(1 16 64 256)
ITERS=${C2_ITERS:-20}
DURATION=${C2_DURATION:-30}

# Each line: N iter status ima_count extended_count nmissed
RAW="$DIR/raw.tsv"
printf 'N\titer\tstatus\tlog_lines\textended_count\tnmissed\n' >"$RAW"

for N in "${N_LIST[@]}"; do
    for i in $(seq 1 "$ITERS"); do
        log "N=$N iter=$i/$ITERS  (stress-ng --exec $N --timeout ${DURATION}s)"
        stress-ng --exec "$N" --timeout "${DURATION}s" >/dev/null 2>&1 || true
        sleep 2  # let the workqueue drain

        snap_dir=$(mktemp -d)
        snapshot_state "$snap_dir"

        log_lines=$(wc -l <"$snap_dir/ima.log")
        ext_count=$(cat "$snap_dir/extended_count.txt" 2>/dev/null || echo 0)
        nmissed=$(cat "$snap_dir/nmissed.txt" 2>/dev/null || echo 0)

        if python3 "$REPO_ROOT/validate.py" \
            "$(cat "$snap_dir/initial.txt")" \
            "$(cat "$snap_dir/skip_count.txt")" \
            --rtmr "$snap_dir/rtmr2.bin" \
            --log "$snap_dir/ima.log" \
            --sysfs "$IMA_RTMR_SYSFS" \
            --no-search \
            >"$snap_dir/validate.out" 2>&1; then
            status=MATCH
        else
            status=NOMATCH
        fi

        printf '%d\t%d\t%s\t%d\t%d\t%d\n' \
            "$N" "$i" "$status" "$log_lines" "$ext_count" "$nmissed" >>"$RAW"

        # Save mismatching snapshots for offline analysis
        if [[ $status == NOMATCH ]]; then
            mv "$snap_dir" "$DIR/N${N}-i${i}-NOMATCH"
        else
            rm -rf "$snap_dir"
        fi
    done
done

log "computing Wilson 95% CI per N"
python3 "$EVAL_ROOT/python/wilson.py" "$RAW" >"$DIR/wilson.csv"
log "result:"
column -t -s $'\t' "$DIR/wilson.csv" | tee -a "$DIR/c2.log"

# Pass criterion: every N has match rate == 100% (no NOMATCH rows)
if awk -F'\t' 'NR>1 && $3=="NOMATCH"{found=1} END{exit !found}' "$RAW"; then
    log "C2 FAILED: some iterations did not match"
    exit 2
fi

log "C2 PASS (100% match across all N and iterations)"
