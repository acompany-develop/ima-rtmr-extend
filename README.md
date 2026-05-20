<!--
SPDX-License-Identifier: GPL-2.0-only
Copyright (c) 2026 Acompany Co., Ltd.
-->

# ima-rtmr-extend

IMA の測定を tsm-mr 経由で CC ランタイム測定レジスタに拡張するカーネルモジュール。

## ドキュメント

- [仕組み](docs/mechanism.md) - IMA 計測の RTMR 拡張、シーケンス番号による順序付け、リプレイ検証
- [IMA ポリシーの注意点](docs/ima-policy.md) - ポリシーの書き込み順序、仮想ファイルシステムの除外
- [既知の問題](docs/known-issues.md) - 開始地点の不可視性、並行計測時の順序保証
- [ビルドオプションの安全性](docs/build-options.md) - Docker ビルドで使用する最適化オプションの安全性評価

## 要件

- `CONFIG_TSM_MEASUREMENTS=y` (Linux 6.16+ または tsm-mr パッチ適用済みカーネル)
- `CONFIG_IMA=y`, `CONFIG_KPROBES=y`, `CONFIG_KRETPROBES=y`

## ビルド・使用

### カーネルモジュールとして使用する場合

```sh
make
sudo insmod build/ima_rtmr.ko                # 自動検出
sudo insmod build/ima_rtmr.ko mr_path=/sys/class/misc/tdx_guest/measurements/rtmr2:sha384
```

カーネルモジュールは任意のタイミングでロードできますが、ロード前の IMA エントリは RTMR に拡張されません。
詳細は[既知の問題](docs/known-issues.md#問題-1-ima-ログの開始地点が-verifier-側からわからない)を参照してください。

### カーネルに組み込む場合

```sh
# パッチ適用（v6.17 / v7.0 に対応）
kernel/apply-patch.sh /path/to/linux 7.0

# menuconfig で CONFIG_IMA_RTMR=y を有効化してビルド
make menuconfig   # Security -> Integrity -> IMA_RTMR
make

# パッチ除去
kernel/remove-patch.sh /path/to/linux 7.0
```

### Ubuntu用の .deb パッケージをビルドする場合

Docker を使ってカーネルモジュールを組み込んだ Ubuntu 向けのカーネルパッケージをビルドできます。
詳細は [ubuntu/README.md](ubuntu/README.md) を参照してください。

## 開発

Nix [flake](https://nix.dev/concepts/flakes.html) の [devShell](https://nix.dev/manual/nix/latest/command-ref/new-cli/nix3-develop) でビルド・フォーマットツール一式が揃います。

```sh
nix develop          # gcc, make, kmod, treefmt (clang-format / shfmt) が利用可能
nix fmt              # ソース全体をフォーマット
```

`.envrc` を使う場合は `direnv allow` で自動的に devShell に入れます。

## 検証

```sh
# モジュールロード・ポリシー設定後、ベースラインを取得
skip=$(sudo cat /sys/kernel/security/ima/ascii_runtime_measurements_sha384 | wc -l)
baseline=$(sudo xxd -p /sys/class/misc/tdx_guest/measurements/rtmr2:sha384 | tr -d '\n')

# IMA 計測をトリガー（バイナリ実行やファイル読み取り等）
# ...

# RTMR リプレイ検証
sudo ./validate.py "$baseline" "$skip"

# Bash実装 (遅いです)
sudo ./validate.sh "$baseline" "$skip"
```

## ライセンス

Copyright (c) 2026 Acompany Co., Ltd.

本プロジェクトは **GNU General Public License version 2 (GPL-2.0-only)** の下で
配布されています。フルテキストは [LICENSE](LICENSE) を参照してください。
個々のファイルの先頭に `SPDX-License-Identifier: GPL-2.0-only` が記載されています。
