#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only

# shellcheck source-path=SCRIPTDIR
# shellcheck source=./common.sh
. "$(dirname "${BASH_SOURCE[0]}")/common.sh"
require_root

policy=${1:-tcb}
d=$(new_run_dir coverage)
write_meta "$d"

mode=oot
[[ -d $sysfs_dir ]] && ! lsmod | grep -qE '^ima_rtmr ' && mode=intree
echo "$mode" >"$d/mode.txt"

[[ $mode == oot ]] && load_module

if [[ -w /sys/kernel/security/ima/policy && $policy == critical_data ]]; then
    echo "measure func=CRITICAL_DATA" >/sys/kernel/security/ima/policy || true
fi

for _ in $(seq 1 50); do /bin/true; done
stress-ng --exec 8 --timeout 10s >/dev/null 2>&1 || true
sleep 2

snapshot "$d"
lines=$(wc -l <"$d/ima.log")
skip=$(cat "$d/skip_count.txt" 2>/dev/null || echo 0)
ext=$(cat "$d/extended_count.txt" 2>/dev/null || echo 0)
dis=$(cat "$d/disabled.txt" 2>/dev/null || echo 0)

cat >"$d/coverage.json" <<EOF
{
  "mode": "$mode",
  "policy": "$policy",
  "ima_log_lines": $lines,
  "skip_count": $skip,
  "extended_count": $ext,
  "post_load_lines": $((lines - skip)),
  "disabled": $dis
}
EOF

if verify "$d"; then echo MATCH >"$d/status"; else echo NOMATCH >"$d/status"; fi
cat "$d/coverage.json"
cat "$d/status"
