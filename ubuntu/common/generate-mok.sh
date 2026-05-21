#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# Generate a MOK key pair for Secure Boot signing.
#
# The certificate must NOT carry OID 1.3.6.1.4.1.2312.16.1.2
# (module-signing-only), or shim >= 15.4 will reject it for kernel
# image verification.

set -eEuo pipefail

out_dir="${1:?"usage: $0 <output-dir>"}"
mkdir -p "${out_dir}"

if [[ -f "${out_dir}/MOK.priv" ]]; then
    echo "${out_dir}/MOK.priv exists, skipping" >&2
    exit 0
fi

cnf=$(mktemp)
trap 'rm -f "${cnf}"' EXIT

cat >"${cnf}" <<'EOF'
[req]
default_bits = 4096
distinguished_name = dn
prompt = no
string_mask = utf8only
x509_extensions = ext

[dn]
CN = IMA-RTMR Kernel Signing Key

[ext]
basicConstraints = critical,CA:FALSE
keyUsage = digitalSignature
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid
EOF

openssl req -config "${cnf}" \
    -new -x509 -newkey rsa:4096 -nodes \
    -days 3650 -sha256 \
    -keyout "${out_dir}/MOK.priv" \
    -out "${out_dir}/MOK.pem"

openssl x509 -in "${out_dir}/MOK.pem" -outform DER -out "${out_dir}/MOK.der"
chmod 600 "${out_dir}/MOK.priv"
