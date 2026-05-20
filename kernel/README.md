<!--
SPDX-License-Identifier: GPL-2.0-only
Copyright (c) 2026 Acompany Co., Ltd.
-->

# カーネルパッチ

IMA_RTMR をカーネルソースツリーに組み込むためのパッチとスクリプトです。

## 対応カーネルバージョン

| ディレクトリ | カーネル |
| --- | --- |
| `patches/6.17/` | Linux 6.17 |
| `patches/7.0/` | Linux 7.0 |

## 使い方

### パッチ適用

```sh
kernel/apply-patch.sh /path/to/linux-source 7.0
```

`src/` ディレクトリを `security/integrity/ima_rtmr` に配置し、
Kconfig と Makefile にエントリを追加します。

適用後、`make menuconfig` で Security -> Integrity -> IMA_RTMR を有効化して
ビルドしてください。

### パッチ除去

```sh
kernel/remove-patch.sh /path/to/linux-source 7.0
```

### 新しいカーネルバージョンへの対応

`patches/<version>/` に `kconfig.patch` と `makefile.patch` を用意してください。
既存のパッチは `security/integrity/Kconfig` と `security/integrity/Makefile` に
IMA_RTMR のエントリを追加するだけなので、多くの場合そのまま流用できます。
