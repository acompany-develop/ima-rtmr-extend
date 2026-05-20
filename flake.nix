# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.

{
  description = "ima-rtmr-extend - IMA to RTMR bridge kernel module";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    treefmt-nix.url = "github:numtide/treefmt-nix";
  };

  outputs = { self, nixpkgs, treefmt-nix }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
      treefmtEval = forAllSystems (system:
        treefmt-nix.lib.evalModule nixpkgs.legacyPackages.${system} ./treefmt.nix
      );
    in {
      formatter = forAllSystems (system:
        treefmtEval.${system}.config.build.wrapper
      );

      checks = forAllSystems (system: {
        formatting = treefmtEval.${system}.config.build.check self;
      });

      devShells = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in {
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
        });
    };
}
