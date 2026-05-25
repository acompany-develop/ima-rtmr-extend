#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Source from each eval script.

set -eEuo pipefail

eval_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(cd "$eval_dir/.." && pwd)

sysfs_dir="${SYSFS_DIR:-/sys/kernel/ima_rtmr}"
rtmr_path="${RTMR_PATH:-/sys/class/misc/tdx_guest/measurements/rtmr2:sha384}"
ima_log="${IMA_LOG:-/sys/kernel/security/ima/ascii_runtime_measurements_sha384}"
ko_path="${KO_PATH:-$repo_dir/build/ima_rtmr.ko}"

require_root() { [[ $EUID -eq 0 ]] || {
    echo "must run as root" >&2
    exit 1
}; }

new_run_dir() {
    local d
    d="$eval_dir/results/$1-$(date -u +%Y%m%dT%H%M%SZ)"
    mkdir -p "$d"
    printf '%s\n' "$d"
}

write_meta() {
    local d=$1
    {
        printf '{\n'
        printf '  "ts": "%s",\n' "$(date -u --iso-8601=seconds)"
        printf '  "host": "%s",\n' "$(hostname)"
        printf '  "kernel": "%s",\n' "$(uname -r)"
        printf '  "cpu": "%s",\n' "$(awk -F': ' '/^model name/{print $2; exit}' /proc/cpuinfo)"
        printf '  "ncpu": %s,\n' "$(nproc)"
        printf '  "microcode": "%s",\n' "$(awk -F': ' '/^microcode/{print $2; exit}' /proc/cpuinfo)"
        printf '  "ko_sha256": "%s",\n' "$(sha256sum "$ko_path" 2>/dev/null | awk '{print $1}')"
        printf '  "module_loaded": %s,\n' "$(lsmod | grep -qE '^ima_rtmr ' && echo true || echo false)"
        printf '  "smt": "%s"\n' "$(cat /sys/devices/system/cpu/smt/control 2>/dev/null || echo unknown)"
        printf '}\n'
    } >"$d/meta.json"
}

# Optional $1 is mr_path; callers that auto-detect omit it.
# shellcheck disable=SC2120
load_module() {
    lsmod | grep -qE '^ima_rtmr ' && return 0
    [[ -f $ko_path ]] || {
        echo "$ko_path not found; run make" >&2
        return 1
    }
    insmod "$ko_path" ${1:+mr_path="$1"}
}

unload_module() {
    lsmod | grep -qE '^ima_rtmr ' && rmmod ima_rtmr || true
}

snapshot() {
    local d=$1
    cp -f "$rtmr_path" "$d/rtmr2.bin"
    cp -f "$ima_log" "$d/ima.log"
    [[ -d $sysfs_dir ]] || return 0
    for attr in initial disabled extended_count skip_count nmissed; do
        [[ -r $sysfs_dir/$attr ]] && cp -f "$sysfs_dir/$attr" "$d/$attr.txt" || true
    done
}

verify() {
    local d=$1
    python3 "$repo_dir/validate.py" \
        "$(cat "$d/initial.txt")" "$(cat "$d/skip_count.txt")" \
        --rtmr "$d/rtmr2.bin" --log "$d/ima.log" --sysfs "$sysfs_dir" \
        "${@:2}" >"$d/validate.out" 2>&1
}
