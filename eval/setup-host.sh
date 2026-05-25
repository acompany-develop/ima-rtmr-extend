#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# Fix host state for measurement reproducibility.
# Run once per boot before evaluation.

set -euo pipefail

if [[ $EUID -ne 0 ]]; then
    echo "must run as root" >&2
    exit 1
fi

log() { printf '[setup-host] %s\n' "$*"; }

# --- CPU governor ---
log "Set CPU governor to performance"
for g in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    [[ -w $g ]] && echo performance >"$g" || true
done

# --- ASLR / KASLR ---
log "Disable ASLR (kernel.randomize_va_space=0)"
sysctl -wq kernel.randomize_va_space=0

# --- swap ---
if [[ $(swapon --show --noheadings | wc -l) -gt 0 ]]; then
    log "Disable swap"
    swapoff -a
fi

# --- Disable noisy services ---
for svc in snapd packagekit ModemManager unattended-upgrades.service apt-daily.timer apt-daily-upgrade.timer; do
    if systemctl is-active --quiet "$svc" 2>/dev/null; then
        log "Stop $svc"
        systemctl stop "$svc" || true
    fi
done

# --- IRQ affinity: move IRQs off CPU 0-3 so we have isolated cores for benchmarks ---
# Note: we don't enforce this on c3-standard-44 because the irqbalance may revert it;
# instead, we record the affinity and let the experiment scripts pin work to specific CPUs.
log "Stop irqbalance if running"
systemctl stop irqbalance 2>/dev/null || true

# --- Hyper-threading (SMT) ---
# Disable SMT siblings so each measurement core does not share L1/L2 with a sibling.
log "Disable SMT siblings"
if [[ -e /sys/devices/system/cpu/smt/control ]]; then
    echo off >/sys/devices/system/cpu/smt/control || true
fi

log "done"
