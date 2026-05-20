# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2026 Acompany Co., Ltd.

{ ... }:
{
  projectRootFile = "flake.nix";

  programs.clang-format.enable = true;
  programs.shfmt = {
    enable = true;
    indent_size = 4;
  };
}
