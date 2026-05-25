# SPDX-License-Identifier: GPL-2.0-only
# Source from each eval script.
set -euo pipefail

EVAL_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_DIR=$(cd "$EVAL_DIR/.." && pwd)

: "${RTMR_PATH:=/sys/class/misc/tdx_guest/measurements/rtmr2:sha384}"
: "${IMA_LOG:=/sys/kernel/security/ima/ascii_runtime_measurements_sha384}"
: "${SYSFS:=/sys/kernel/ima_rtmr}"
: "${KO:=$REPO_DIR/build/ima_rtmr.ko}"

require_root() { [[ $EUID -eq 0 ]] || { echo "must run as root" >&2; exit 1; }; }

new_run_dir() {
    local d="$EVAL_DIR/results/$1-$(date -u +%Y%m%dT%H%M%SZ)"
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
        printf '  "ko_sha256": "%s",\n' "$(sha256sum "$KO" 2>/dev/null | awk '{print $1}')"
        printf '  "module_loaded": %s,\n' "$(lsmod | grep -qE '^ima_rtmr ' && echo true || echo false)"
        printf '  "smt": "%s"\n' "$(cat /sys/devices/system/cpu/smt/control 2>/dev/null || echo unknown)"
        printf '}\n'
    } >"$d/meta.json"
}

load_module() {
    lsmod | grep -qE '^ima_rtmr ' && return 0
    [[ -f $KO ]] || { echo "$KO not found; run make" >&2; return 1; }
    insmod "$KO" ${1:+mr_path="$1"}
}

unload_module() {
    lsmod | grep -qE '^ima_rtmr ' && rmmod ima_rtmr || true
}

snapshot() {
    local d=$1
    cp -f "$RTMR_PATH" "$d/rtmr2.bin"
    cp -f "$IMA_LOG" "$d/ima.log"
    [[ -d $SYSFS ]] || return 0
    for attr in initial disabled extended_count skip_count nmissed; do
        [[ -r $SYSFS/$attr ]] && cp -f "$SYSFS/$attr" "$d/$attr.txt" || true
    done
}

verify() {
    local d=$1
    python3 "$REPO_DIR/validate.py" \
        "$(cat "$d/initial.txt")" "$(cat "$d/skip_count.txt")" \
        --rtmr "$d/rtmr2.bin" --log "$d/ima.log" --sysfs "$SYSFS" \
        "${@:2}" >"$d/validate.out" 2>&1
}
