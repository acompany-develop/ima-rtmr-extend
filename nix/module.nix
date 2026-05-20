# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.
#
# NixOS module that bakes ima_rtmr into the kernel via boot.kernelPatches.

{
    config,
    lib,
    pkgs,
    ...
}:

with builtins;
let
    cfg = config.services.ima-rtmr;

    patchesDir = ../kernel/patches;
    patchVersions =
        readDir patchesDir |> attrNames |> filter (n: (readDir patchesDir).${n} == "directory");

    latestPatchVersion = patchVersions |> sort (a: b: compareVersions a b < 0) |> lib.last;
in
{
    options.services.ima-rtmr = {
        enable = lib.mkEnableOption "IMA→RTMR in-tree kernel integration";

        patchVersion = lib.mkOption {
            type = lib.types.enum patchVersions;
            default = latestPatchVersion;
            description = ''
                Patch set targeting a specific upstream kernel version.
                Must match the running kernel's major.minor.
            '';
        };
    };

    config = lib.mkIf cfg.enable {
        boot.kernelPatches = [
            (import ./kernel-patch.nix {
                inherit pkgs lib;
                inherit (cfg) patchVersion;
            })
        ];
    };
}
