# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.

FROM ubuntu:26.04 AS build

SHELL ["/bin/bash", "-o", "pipefail", "-c"]
ENV DEBIAN_FRONTEND=noninteractive

# Mainline RC kernels have no Ubuntu packaging (the unstable tree and the
# mainline PPA lag behind), so this variant builds from the kernel.org
# tarball with make bindeb-pkg instead of debian/rules.
ARG MAINLINE_VERSION=7.2-rc7
ARG PATCH_VERSION=7.2

RUN apt-get update -qq && apt-get install -y -qq \
    build-essential bc kmod cpio flex bison rsync git wget curl \
    debhelper dpkg-dev fakeroot pkg-config \
    libelf-dev libssl-dev libdw-dev \
    python3-dev dwarves zstd lz4 xz-utils gawk \
    ccache && \
    rm -rf /var/lib/apt/lists/*

# Seed .config from the Ubuntu generic kernel (config ships in linux-modules-*).
RUN apt-get update -qq > /dev/null && \
    apt-get install -y -qq --no-install-recommends linux-image-generic 2>&1 | tail -2 && \
    ls /boot/config-*

WORKDIR /build

# Phase 1: Fetch kernel source (cached independently of local src changes)
RUN curl -fsSL "https://git.kernel.org/torvalds/t/linux-${MAINLINE_VERSION}.tar.gz" \
        -o linux.tar.gz && \
    tar -xzf linux.tar.gz && rm linux.tar.gz

COPY src/ /ima-rtmr-extend/src/
COPY kernel/ /ima-rtmr-extend/kernel/

# Phase 2: Apply patches and configure from the Ubuntu generic config
RUN <<EOF
    set -eEuo pipefail
    bash /ima-rtmr-extend/kernel/apply-patch.sh "/build/linux-${MAINLINE_VERSION}" "${PATCH_VERSION}"
    cd "/build/linux-${MAINLINE_VERSION}"
    cp /boot/config-* .config
    # The Ubuntu config references Canonical-only key material; DEBUG_INFO
    # would triple the build time and .deb size for no runtime difference.
    scripts/config --set-str SYSTEM_TRUSTED_KEYS "" \
                   --set-str SYSTEM_REVOCATION_KEYS "" \
                   --disable DEBUG_INFO \
                   --enable DEBUG_INFO_NONE \
                   --set-str LOCALVERSION "+imrtmr1" \
                   --enable IMA_RTMR
    make olddefconfig
    grep -q "^CONFIG_IMA_RTMR=y" .config
    grep -q "^CONFIG_TSM_MEASUREMENTS=y" .config
EOF

# Phase 3: Build kernel .deb packages
RUN --mount=type=cache,target=/ccache \
    --mount=type=tmpfs,target=/tmp <<EOF
    set -eEuo pipefail
    export CCACHE_DIR=/ccache CCACHE_MAXSIZE=10G CCACHE_COMPRESS=true
    export PATH="/usr/lib/ccache:$PATH"
    cd "/build/linux-${MAINLINE_VERSION}"
    make -j"$(($(nproc) * 3 / 2))" bindeb-pkg
EOF

FROM scratch
COPY --from=build /build/*.deb /
