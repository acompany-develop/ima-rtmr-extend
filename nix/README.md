<!--
SPDX-License-Identifier: GPL-2.0-only
Copyright (c) 2026 Acompany Co., Ltd.
-->

# Nix Support

NixOSのカーネルに本リポジトリのモジュールを組み込み、`CONFIG_IMA_RTMR=y`でコンパイルするためのFlakeです。

`src/` と `kernel/patches/<version>/` をもとにカーネルにパッチを適用します。

## 使い方

### 推奨: NixOS モジュール

```nix
{
  inputs.ima-rtmr.url = "github:acompany-develop/ima-rtmr-extend";

  outputs = { nixpkgs, ima-rtmr, ... }: {
    nixosConfigurations.host = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      modules = [
        ima-rtmr.nixosModules.default
        {
          services.ima-rtmr = {
            enable = true;
            patchVersion = "7.0";  # "6.17" も指定可能
          };
        }
      ];
    };
  };
}
```

### `boot.kernelPatches` に直接渡す

```nix
{
  boot.kernelPatches = [
    inputs.ima-rtmr.kernelPatches.${pkgs.system}.default  # 最新版 (7.0)
    # inputs.ima-rtmr.kernelPatches.${pkgs.system}."6_17" など指定可
  ];
}
```

### パッチを確認する

```sh
nix build .#kernelPatch-7_0
less result
```
