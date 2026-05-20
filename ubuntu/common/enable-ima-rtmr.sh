#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# Enable CONFIG_IMA_RTMR by appending a policy entry to every
# debian*/config/annotations file in the kernel source tree.

set -eEuo pipefail

kernel_src="${1:?"usage: $0 <kernel-source-dir>"}"

found=0
for annotations in "${kernel_src}"/debian*/config/annotations; do
    [ -f "${annotations}" ] || continue
    found=1
    echo 'CONFIG_IMA_RTMR                         policy<{"amd64": "y", "arm64": "y", "armhf": "y", "ppc64el": "y", "riscv64": "y", "s390x": "y"}>  note<ima_rtmr>' >>"${annotations}"
    echo "annotations: ${annotations}"
done

if [[ ${found} -eq 0 ]]; then
    echo "error: no annotations file found in ${kernel_src}/debian*/config/" >&2
    exit 1
fi
