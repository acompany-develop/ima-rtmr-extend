# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.

FROM ubuntu:26.04 AS build

SHELL ["/bin/bash", "-o", "pipefail", "-c"]
ENV DEBIAN_FRONTEND=noninteractive

RUN sed -i 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/ubuntu.sources

RUN apt-get update -qq && apt-get install -y -qq \
    build-essential bc kmod cpio flex bison rsync git wget curl \
    debhelper dpkg-dev fakeroot pkg-config \
    libelf-dev libssl-dev libcap-dev libnuma-dev \
    libpci-dev libudev-dev libdw-dev \
    libtraceevent-dev libtracefs-dev \
    libbabeltrace-dev libperl-dev \
    libunwind-dev libopencsd-dev libnewt-dev \
    libiberty-dev binutils-dev libzstd-dev libnl-genl-3-dev \
    python3-dev python3-setuptools \
    dwarves zstd lz4 xz-utils gawk dctrl-tools \
    ccache default-jdk-headless dkms \
    sbsigntool && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Phase 1: Fetch kernel source (cached independently of local src changes)
RUN apt-get update -qq > /dev/null && \
    apt-get source -qq linux 2>&1 | tail -5

COPY src/ /ima-rtmr-extend/src/
COPY kernel/ /ima-rtmr-extend/kernel/
COPY ubuntu/common/ /ima-rtmr-extend/ubuntu/common/

# Phase 2: Apply patches, enable config, pin DKMS versions, rename kernel
RUN <<EOF
    set -eEuo pipefail
    bash /ima-rtmr-extend/kernel/apply-patch.sh /build/linux-7.* 7.0
    bash /ima-rtmr-extend/ubuntu/common/enable-ima-rtmr.sh /build/linux-7.*
    bash /ima-rtmr-extend/ubuntu/common/pin-dkms-versions.sh /build/linux-7.*
    bash /ima-rtmr-extend/ubuntu/common/rename-kernel.sh /build/linux-7.*
EOF

# Phase 3: Build kernel .deb packages
# ccache preserves compilation results across failed builds via cache mount.
RUN --mount=type=cache,target=/ccache \
    --mount=type=tmpfs,target=/tmp \
    bash /ima-rtmr-extend/ubuntu/common/build-kernel.sh /build/linux-7.*

# Phase 4: sign vmlinuz when MOK secrets are mounted.
RUN --mount=type=secret,id=mok-key \
    --mount=type=secret,id=mok-cert \
    <<EOF
    set -eEuo pipefail
    if [[ -f /run/secrets/mok-key && -f /run/secrets/mok-cert ]]; then
        for deb in /build/linux-image-unsigned-*.deb; do
            bash /ima-rtmr-extend/ubuntu/common/sign-kernel.sh \
                /run/secrets/mok-key /run/secrets/mok-cert "$deb"
        done
    fi
EOF

FROM scratch
COPY --from=build /build/*.deb /
