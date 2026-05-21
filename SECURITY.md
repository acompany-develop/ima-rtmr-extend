<!--
SPDX-License-Identifier: GPL-2.0-only
Copyright (c) 2026 Acompany Co., Ltd.
-->

# Security Policy

`ima-rtmr-extend` is a GPL-2.0 Linux kernel module that bridges IMA
measurements into Intel TDX Runtime Measurement Registers (RTMRs). Because it
operates in kernel space and feeds Confidential Computing attestation state,
we take security reports seriously.

## Supported Versions

| Version              | Status              |
| -------------------- | ------------------- |
| `main` branch        | Active development  |
| Latest released tag  | Security fixes      |
| Older tags           | Not supported       |

## Reporting a Vulnerability

**Do NOT open a public GitHub issue for security reports.**

Use GitHub's Private Vulnerability Reporting:

<https://github.com/acompany-develop/ima-rtmr-extend/security/advisories/new>

Please include:

- Affected commit hash or release tag
- Linux kernel version and TDX guest configuration
- Reproduction steps or proof-of-concept
- Impact assessment (e.g. forged RTMR extension, IMA log bypass, memory
  safety issue, privilege escalation)

We aim to acknowledge reports within 3 business days and provide an initial
assessment within 10 business days. The default coordinated disclosure window
is 90 days, which may be shortened or extended in agreement with the reporter.

## Disclosure Policy

Once a fix is ready we will:

1. Publish a GitHub Security Advisory.
2. Request a CVE through GitHub's CNA where applicable.
3. Release a patched version tagged on GitHub.
4. Credit the reporter unless anonymity is requested.

## Scope

In scope:

- Forged or out-of-order RTMR extensions originating from this module
- Bypass of IMA log → RTMR mapping integrity
- Memory-safety issues in the module's code paths
- Privilege escalation enabled by this module

Out of scope (please report to upstream):

- Linux IMA subsystem vulnerabilities → `security@kernel.org`
- Intel TDX module / TDX Module ABI issues → Intel PSIRT
- Generic Linux kernel vulnerabilities → `security@kernel.org`

## Coordinated Disclosure with Upstream

If a finding affects both this module and upstream components, we will
coordinate disclosure timelines with `security@kernel.org` and/or Intel PSIRT
to align with their published embargo policies.
