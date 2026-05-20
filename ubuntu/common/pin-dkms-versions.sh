#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# Pin DKMS package versions in debian*/dkms-versions to versions actually
# available in the configured APT repositories. Required because the
# Ubuntu kernel build system fails to download .debs for versions that
# have rolled off the archive.

set -eEuo pipefail

kernel_src="${1:?"usage: $0 <kernel-source-dir>"}"

found=0
for dkms_versions in "${kernel_src}"/debian*/dkms-versions; do
    [ -f "${dkms_versions}" ] || continue
    found=1

    echo "=== Pinning DKMS versions: ${dkms_versions} ==="
    apt-get update -qq >/dev/null

    file_content=$(cat "${dkms_versions}")

    while IFS= read -r line; do
        [[ -z ${line} || ${line} == \#* ]] && continue

        src_pkg=$(echo "${line}" | awk '{print $1}')
        wanted=$(echo "${line}" | awk '{print $2}')

        available=$(apt-cache showsrc "${src_pkg}" 2>/dev/null |
            awk '/^Version:/{print $2; exit}' || true)

        if [[ -z ${available} ]]; then
            sed -i "/^${src_pkg} ${wanted}/d" "${dkms_versions}"
            echo "removed: ${src_pkg} (not in repositories)"
        elif [[ ${available} != "${wanted}" ]]; then
            sed -i "s|^${src_pkg} ${wanted}|${src_pkg} ${available}|" "${dkms_versions}"
            echo "pinned: ${src_pkg} ${wanted} -> ${available}"
        fi
    done <<<"${file_content}"
done

if [[ ${found} -eq 0 ]]; then
    echo "no dkms-versions file found (skipping)"
fi
