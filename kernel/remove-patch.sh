#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# Reverse apply-patch.sh: revert the Kconfig/Makefile patches and remove
# the symlink or copied directory.

set -eEuo pipefail

kernel_src="${1:?"usage: $0 <kernel-source-dir> [patch-version]"}"
patch_version="${2:-"7.0"}"
script_dir="$(cd "$(dirname "$0")" && pwd)"
patch_dir="${script_dir}/patches/${patch_version}"

integrity_dir="${kernel_src}/security/integrity"

if [[ ! -d ${patch_dir} ]]; then
    echo "error: patch directory not found: ${patch_dir}" >&2
    echo "available versions:" >&2
    ls "${script_dir}/patches/" >&2
    exit 1
fi

patch -R -d "${kernel_src}" -p1 <"${patch_dir}/makefile.patch"
patch -R -d "${kernel_src}" -p1 <"${patch_dir}/kconfig.patch"

if [[ -L "${integrity_dir}/ima_rtmr" ]]; then
    rm "${integrity_dir}/ima_rtmr"
    echo "removed symlink: ${integrity_dir}/ima_rtmr"
elif [[ -d "${integrity_dir}/ima_rtmr" ]]; then
    rm -rf "${integrity_dir}/ima_rtmr"
    echo "removed directory: ${integrity_dir}/ima_rtmr"
fi

echo "done."
