#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# Apply ima_rtmr integration to a kernel source tree: link (or copy) src/
# into the tree and apply Kconfig/Makefile patches so the module can be
# built as built-in (obj-y).

set -eEuo pipefail

kernel_src="${1:?"usage: $0 <kernel-source-dir> [patch-version]"}"
patch_version="${2:-"7.0"}"
script_dir="$(cd "$(dirname "$0")" && pwd)"
src_dir="$(cd "${script_dir}/../src" && pwd)"
patch_dir="${script_dir}/patches/${patch_version}"

integrity_dir="${kernel_src}/security/integrity"

if [[ ! -d "${patch_dir}" ]]; then
    echo "error: patch directory not found: ${patch_dir}" >&2
    echo "available versions:" >&2
    ls "${script_dir}/patches/" >&2
    exit 1
fi

if [[ ! -f "${integrity_dir}/Kconfig" ]]; then
    echo "error: ${integrity_dir}/Kconfig not found (is this a full kernel source tree?)" >&2
    exit 1
fi

if [[ -e "${integrity_dir}/ima_rtmr" ]]; then
    echo "error: ${integrity_dir}/ima_rtmr already exists" >&2
    exit 1
fi

if ln -sfn "${src_dir}" "${integrity_dir}/ima_rtmr" 2>/dev/null; then
    echo "symlink: ${integrity_dir}/ima_rtmr -> ${src_dir}"
else
    cp -r "${src_dir}" "${integrity_dir}/ima_rtmr"
    echo "copy: ${src_dir} -> ${integrity_dir}/ima_rtmr"
fi

patch -d "${kernel_src}" -p1 < "${patch_dir}/kconfig.patch"
patch -d "${kernel_src}" -p1 < "${patch_dir}/makefile.patch"

echo "done. enable with: make menuconfig -> Security -> Integrity -> IMA_RTMR"
