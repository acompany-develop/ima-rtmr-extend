# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# Produce a single unified patch that, when applied to a Linux kernel source
# tree with `patch -p1`, integrates ima_rtmr as an in-tree component
# (CONFIG_IMA_RTMR=y). Equivalent to running `kernel/apply-patch.sh`.

{
    pkgs,
    lib,
    patchVersion ? "7.0",
}:

let
    src = ../src;
    patchDir = ../kernel/patches + "/${patchVersion}";

    combinedPatch =
        pkgs.runCommand "ima-rtmr-${patchVersion}.patch"
            {
                nativeBuildInputs = [ pkgs.diffutils ];
            }
            ''
                set -euo pipefail

                mkdir -p base mod/security/integrity/ima_rtmr
                cp -r ${src}/. mod/security/integrity/ima_rtmr/

                # Sorted enumeration keeps the patch hash stable across hosts.
                export LC_ALL=C
                for f in $(cd mod && find . -type f | sort); do
                    diff -uN "base/$f" "mod/$f" || test $? -eq 1
                done \
                  | sed -e 's|^--- base/\./|--- a/|' \
                        -e 's|^+++ mod/\./|+++ b/|' \
                  > new-files.patch

                cat new-files.patch \
                    ${patchDir}/kconfig.patch \
                    ${patchDir}/makefile.patch \
                  > $out
            '';
in
{
    name = "ima-rtmr-${patchVersion}";
    patch = combinedPatch;
    structuredExtraConfig = with lib.kernel; {
        IMA_RTMR = yes;
    };
}
