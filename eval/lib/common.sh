# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# Shared helpers for eval scripts. Source from each c*-*.sh.

set -euo pipefail

EVAL_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "$EVAL_ROOT/.." && pwd)"
RESULTS_BASE="$EVAL_ROOT/results"

# Common defaults
: "${RTMR_PATH:=/sys/class/misc/tdx_guest/measurements/rtmr2:sha384}"
: "${IMA_LOG_PATH:=/sys/kernel/security/ima/ascii_runtime_measurements_sha384}"
: "${IMA_RTMR_SYSFS:=/sys/kernel/ima_rtmr}"
: "${KO_PATH:=$REPO_ROOT/build/ima_rtmr.ko}"

require_root() {
    if [[ $EUID -ne 0 ]]; then
        echo "must run as root" >&2
        exit 1
    fi
}

log() { printf '[%s] %s\n' "${SCRIPT_NAME:-eval}" "$*" >&2; }

# Returns the directory for this run's results.
init_run_dir() {
    local test_name=$1
    local stamp
    stamp=$(date -u +%Y%m%dT%H%M%SZ)
    local dir="$RESULTS_BASE/$test_name-$stamp"
    mkdir -p "$dir"
    echo "$dir"
}

# Write meta.json with environment info into the given directory.
write_meta() {
    local dir=$1
    local meta="$dir/meta.json"
    {
        echo "{"
        printf '  "timestamp_utc": "%s",\n' "$(date -u --iso-8601=seconds)"
        printf '  "hostname": "%s",\n' "$(hostname)"
        printf '  "kernel_release": "%s",\n' "$(uname -r)"
        printf '  "kernel_version": "%s",\n' "$(uname -v | tr -d '"')"
        printf '  "cpu_model": "%s",\n' "$(awk -F': ' '/^model name/{print $2; exit}' /proc/cpuinfo)"
        printf '  "cpu_count": %s,\n' "$(nproc)"
        printf '  "microcode": "%s",\n' "$(awk -F': ' '/^microcode/{print $2; exit}' /proc/cpuinfo)"
        printf '  "tdx_module": "%s",\n' "$(read_tdx_module)"
        printf '  "module_ko_sha256": "%s",\n' "$(sha256sum "$KO_PATH" 2>/dev/null | awk '{print $1}')"
        printf '  "module_loaded": %s,\n' "$(lsmod | grep -qE '^ima_rtmr ' && echo true || echo false)"
        printf '  "ima_rtmr_sysfs_present": %s,\n' "$([[ -d $IMA_RTMR_SYSFS ]] && echo true || echo false)"
        printf '  "kallsyms_lookup": "%s",\n' "$(read_initial_safe)"
        printf '  "smt_state": "%s"\n' "$(cat /sys/devices/system/cpu/smt/control 2>/dev/null || echo unknown)"
        echo "}"
    } >"$meta"
}

read_tdx_module() {
    if [[ -r /sys/firmware/tdx/tdx_module/version ]]; then
        cat /sys/firmware/tdx/tdx_module/version
    elif [[ -r /sys/firmware/tdx ]] || [[ -d /sys/firmware/acpi/tables/data/CCEL ]]; then
        echo "tdx-detected"
    else
        echo "unknown"
    fi
}

read_initial_safe() {
    if [[ -r $IMA_RTMR_SYSFS/initial ]]; then
        cat "$IMA_RTMR_SYSFS/initial"
    else
        echo "n/a"
    fi
}

# Load module if not already loaded. Pass mr_path optionally as $1.
load_module() {
    if lsmod | grep -qE '^ima_rtmr '; then
        log "module already loaded"
        return 0
    fi
    if [[ ! -f $KO_PATH ]]; then
        log "module not found at $KO_PATH; run 'make' first"
        return 1
    fi
    if [[ -n ${1:-} ]]; then
        log "insmod $KO_PATH mr_path=$1"
        insmod "$KO_PATH" mr_path="$1"
    else
        log "insmod $KO_PATH"
        insmod "$KO_PATH"
    fi
}

unload_module() {
    if lsmod | grep -qE '^ima_rtmr '; then
        log "rmmod ima_rtmr"
        rmmod ima_rtmr || true
    fi
}

# Snapshot the IMA log and the RTMR value in a deterministic order.
# The verifier replay assumes the RTMR snapshot is taken first.
snapshot_state() {
    local dir=$1
    cp -f "$RTMR_PATH" "$dir/rtmr2.bin"
    cp -f "$IMA_LOG_PATH" "$dir/ima.log"
    if [[ -d $IMA_RTMR_SYSFS ]]; then
        for attr in initial disabled extended_count skip_count nmissed; do
            [[ -r $IMA_RTMR_SYSFS/$attr ]] && cp -f "$IMA_RTMR_SYSFS/$attr" "$dir/$attr.txt" || true
        done
    fi
}

# Pin the current shell to a CPU range. Use isolated CPUs (4..nproc-1).
pin_to_isolated() {
    local n
    n=$(nproc)
    if (( n >= 8 )); then
        taskset -cp 4-$((n - 1)) $$ >/dev/null
    fi
}
