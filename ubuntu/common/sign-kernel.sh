#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# Sign vmlinuz inside a single linux-image-unsigned-*.deb in place.
# Requires sbsigntool and dpkg-deb.

set -eEuo pipefail

mok_key="${1:?"usage: $0 <mok-privkey> <mok-cert> <deb>"}"
mok_cert="${2:?"usage: $0 <mok-privkey> <mok-cert> <deb>"}"
deb="${3:?"usage: $0 <mok-privkey> <mok-cert> <deb>"}"

tmpdir=$(mktemp -d)
trap 'rm -rf "${tmpdir}"' EXIT

dpkg-deb -R "${deb}" "${tmpdir}"
vmlinuz=$(find "${tmpdir}/boot" -name 'vmlinuz-*' -type f -print -quit)
sbsign --key "${mok_key}" --cert "${mok_cert}" "${vmlinuz}" --output "${vmlinuz}"
dpkg-deb -b "${tmpdir}" "${deb}"
