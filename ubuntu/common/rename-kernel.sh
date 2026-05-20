#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# Append a suffix to the debian changelog upload number so custom .debs
# are distinguishable from stock Ubuntu packages (6.17.0-22.22 ->
# 6.17.0-22.22+imrtmr1).
#
# The suffix goes after the upload number rather than after abinum
# because debian/rules.d/0-common-vars.mk passes abinum (the first
# word of <revision>.split(".")) to the C compiler as -DPKG_ABI=<n>,
# which must stay numeric. Affected fields:
#   abinum    = 22          (numeric, safe for PKG_ABI)
#   uploadnum = 22+imrtmr1  (string, safe for KBUILD_BUILD_VERSION)
#   uname -r  = 6.17.0-22-generic (unchanged)

set -eEuo pipefail

kernel_src="${1:?"usage: $0 <kernel-source-dir> [suffix]"}"
suffix="${2:-"+imrtmr1"}"

changelog=""
for f in "${kernel_src}"/debian.*/changelog "${kernel_src}"/debian/changelog; do
    if [[ -f "${f}" ]]; then
        changelog="${f}"
        break
    fi
done

if [[ -z "${changelog}" ]]; then
    echo "error: no debian changelog found in ${kernel_src}" >&2
    exit 1
fi

# Append the suffix before the closing parenthesis on the first changelog line:
#   "linux (6.17.0-22.22)" -> "linux (6.17.0-22.22+imrtmr1)"
sed -i "1s/)/${suffix})/" "${changelog}"
echo "renamed: $(head -1 "${changelog}")"
