#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.

set -eEuo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
exec "${here}/../common/docker-build.sh" "${here}"
