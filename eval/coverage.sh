#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
. "$(dirname "${BASH_SOURCE[0]}")/common.sh"
require_root

POLICY=${1:-tcb}
D=$(new_run_dir coverage)
write_meta "$D"

mode=oot
[[ -d $SYSFS ]] && ! lsmod | grep -qE '^ima_rtmr ' && mode=intree
echo "$mode" >"$D/mode.txt"

[[ $mode == oot ]] && load_module

if [[ -w /sys/kernel/security/ima/policy && $POLICY == critical_data ]]; then
    echo "measure func=CRITICAL_DATA" >/sys/kernel/security/ima/policy || true
fi

for _ in $(seq 1 50); do /bin/true; done
stress-ng --exec 8 --timeout 10s >/dev/null 2>&1 || true
sleep 2

snapshot "$D"
lines=$(wc -l <"$D/ima.log")
skip=$(cat "$D/skip_count.txt" 2>/dev/null || echo 0)
ext=$(cat "$D/extended_count.txt" 2>/dev/null || echo 0)
dis=$(cat "$D/disabled.txt" 2>/dev/null || echo 0)

cat >"$D/coverage.json" <<EOF
{
  "mode": "$mode",
  "policy": "$POLICY",
  "ima_log_lines": $lines,
  "skip_count": $skip,
  "extended_count": $ext,
  "post_load_lines": $((lines - skip)),
  "disabled": $dis
}
EOF

if verify "$D"; then echo MATCH >"$D/status"; else echo NOMATCH >"$D/status"; fi
cat "$D/coverage.json"; cat "$D/status"
