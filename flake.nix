# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.

{
    description = "ima-rtmr-extend - IMA to RTMR bridge kernel module";

    inputs = {
        nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
        treefmt-nix.url = "github:numtide/treefmt-nix";
    };

    outputs =
        {
            self,
            nixpkgs,
            treefmt-nix,
        }:
        with builtins;
        let
            inherit (nixpkgs) lib;

            supportedSystems = [
                "x86_64-linux"
                "aarch64-linux"
            ];
            forAllSystems = lib.genAttrs supportedSystems;

            patchesDir = ./kernel/patches;
            patchVersions =
                readDir patchesDir |> attrNames |> filter (n: (readDir patchesDir).${n} == "directory");

            latestPatchVersion = patchVersions |> sort (a: b: compareVersions a b < 0) |> lib.last;

            versionKey = v: lib.replaceStrings [ "." ] [ "_" ] v;

            mkKernelPatch =
                {
                    pkgs,
                    patchVersion ? latestPatchVersion,
                }:
                import ./nix/kernel-patch.nix {
                    inherit pkgs patchVersion lib;
                };

            treefmtEval = forAllSystems (
                system: treefmt-nix.lib.evalModule nixpkgs.legacyPackages.${system} ./treefmt.nix
            );
        in
        {
            lib = { inherit mkKernelPatch; };

            nixosModules.default = import ./nix/module.nix;

            packages = forAllSystems (
                system:
                let
                    pkgs = nixpkgs.legacyPackages.${system};
                in
                patchVersions
                |> map (v: {
                    name = "kernelPatch-${versionKey v}";
                    value =
                        (mkKernelPatch {
                            inherit pkgs;
                            patchVersion = v;
                        }).patch;
                })
                |> listToAttrs
            );

            # kernel patches for nixosModules
            kernelPatches = forAllSystems (
                system:
                let
                    pkgs = nixpkgs.legacyPackages.${system};
                    byVersion =
                        patchVersions
                        |> map (v: {
                            name = versionKey v;
                            value = mkKernelPatch {
                                inherit pkgs;
                                patchVersion = v;
                            };
                        })
                        |> listToAttrs;
                in
                byVersion // { default = byVersion.${versionKey latestPatchVersion}; }
            );

            # nix fmt
            formatter = forAllSystems (system: treefmtEval.${system}.config.build.wrapper);

            # nix build --check
            checks = forAllSystems (system: {
                formatting = treefmtEval.${system}.config.build.check self;
            });

            # nix develop
            devShells = forAllSystems (
                system:
                let
                    pkgs = nixpkgs.legacyPackages.${system};
                in
                {
                    default = pkgs.mkShell {
                        nativeBuildInputs = [
                            # Build
                            pkgs.gnumake
                            pkgs.gcc

                            # Formatting
                            treefmtEval.${system}.config.build.wrapper

                            # Utilities
                            pkgs.kmod
                            pkgs.git
                        ];

                        shellHook = ''
                            export KDIR="/lib/modules/$(uname -r)/build"
                        '';
                    };
                }
            );
        };
}
