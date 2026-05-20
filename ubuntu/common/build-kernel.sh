#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# Build Ubuntu kernel .deb packages with ccache and optimized flags.
#
# Usage: build-kernel.sh <kernel-source-dir>
# Expects /ccache to be mounted as a persistent BuildKit cache.
# See docs/build-options.md for the safety analysis of each flag.

set -eEuo pipefail

kernel_src="${1:?"usage: $0 <kernel-source-dir>"}"
original_parent="$(dirname "${kernel_src}")"

export CCACHE_DIR=/ccache
export CCACHE_MAXSIZE=10G
export CCACHE_COMPRESS=true
export CCACHE_COMPRESSLEVEL=1
export CCACHE_SLOPPINESS=time_macros,include_file_mtime,include_file_ctime # safe: kernel headers do not change after `make prepare`
export PATH="/usr/lib/ccache:$PATH"

DEB_BUILD_OPTIONS="parallel=$(($(nproc) * 3 / 2))"
export DEB_BUILD_OPTIONS

cd "${kernel_src}"

# Build on /tmp (BuildKit tmpfs) when there is enough RAM, to skip overlayfs overhead.
mem_gib=$(awk '/MemTotal/{printf "%d", $2/1048576}' /proc/meminfo)
if (( mem_gib >= 64 )); then
    echo "==> ${mem_gib} GiB RAM detected (>=64 GiB): building on tmpfs"
    tmpfs_src="/tmp/kernel-build"
    cp -a "${kernel_src}" "${tmpfs_src}"
    kernel_src="${tmpfs_src}"
    cd "${kernel_src}"
else
    echo "==> ${mem_gib} GiB RAM detected (<64 GiB): building on overlayfs"
fi

# Skip fakeroot when running as root: faked-sysv serializes all
# stat/chown/chmod through a single daemon and tanks high-parallelism builds.
if (( UID == 0 )); then
    FAKEROOT=""
else
    FAKEROOT="fakeroot"
fi

${FAKEROOT} debian/rules clean

${FAKEROOT} debian/rules binary-headers binary-generic binary-perarch \
    do_skip_checks=true \
    skipdbg=true \
    do_linux_tools=false \
    do_cloud_tools=false \
    do_tools_common=false \
    do_tools_host=false

if [[ "$(dirname "${kernel_src}")" != "${original_parent}" ]]; then
    echo "==> Copying .deb packages back to ${original_parent}/"
    cp -a "$(dirname "${kernel_src}")"/*.deb "${original_parent}/"
fi
