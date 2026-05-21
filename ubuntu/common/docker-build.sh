#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# Shared build entry point for ubuntu/<variant>/build.sh.
# Usage: docker-build.sh <variant-dir>

set -eEuo pipefail

variant_dir="${1:?"usage: $0 <variant-dir>"}"
variant_dir="$(cd "${variant_dir}" && pwd)"
project_root="$(cd "${variant_dir}/../.." && pwd)"
out_dir="${variant_dir}/out"

mkdir -p "${out_dir}"

secret_args=()
if [[ -n ${MOK_KEY:-} && -n ${MOK_CERT:-} ]]; then
    secret_args+=(--secret "id=mok-key,src=${MOK_KEY}")
    secret_args+=(--secret "id=mok-cert,src=${MOK_CERT}")
fi

docker build \
    "${secret_args[@]+"${secret_args[@]}"}" \
    -f "${variant_dir}/Dockerfile" \
    --output "type=local,dest=${out_dir}" \
    "${project_root}"

echo "Output: ${out_dir}/"
ls -lh "${out_dir}"/*.deb 2>/dev/null || echo "No .deb files found"
