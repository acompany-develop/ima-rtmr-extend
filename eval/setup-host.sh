#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
set -euo pipefail

[[ $EUID -eq 0 ]] || {
    echo "must run as root" >&2
    exit 1
}

for g in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    [[ -w $g ]] && echo performance >"$g" || true
done

sysctl -wq kernel.randomize_va_space=0

swapon --show --noheadings | grep -q . && swapoff -a || true

for svc in snapd packagekit ModemManager unattended-upgrades.service \
    apt-daily.timer apt-daily-upgrade.timer irqbalance; do
    systemctl is-active --quiet "$svc" 2>/dev/null && systemctl stop "$svc" || true
done

[[ -e /sys/devices/system/cpu/smt/control ]] && echo off >/sys/devices/system/cpu/smt/control || true
