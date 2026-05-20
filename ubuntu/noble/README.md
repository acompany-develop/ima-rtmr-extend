<!--
SPDX-License-Identifier: GPL-2.0-only
Copyright (c) 2026 Acompany Co., Ltd.
-->

# Ubuntu 24.04 LTS (Noble) HWE カーネル

Ubuntu 24.04 LTS のデフォルトカーネル (6.8) には tsm-mr が存在しない(RTMRそのものがない)ため、
このカーネルモジュールは動作しません。このビルドでは HWE (Hardware Enablement) カーネル 6.17 を
使用して tsm-mr を利用可能にしています。

## 注意事項

- Noble の `linux-hwe-6.17` ソースに含まれる DKMS パッケージは全て 2024 年スナップショットで、
  HWE 6.17 カーネルヘッダに対してビルドが通りません:
  - `zfs-linux` 2.3.4 (公式サポートは Linux 6.16 まで)
  - `ipu6-drivers`, `ipu7-drivers`, `usbio-drivers`, `vision-drivers`
    (`asm/unaligned.h` 削除・`no_llseek` 削除・`vmalloc.h` 再編成に未対応)
  - `backport-iwlwifi-dkms` 11510 (現行 `timer.h` / `stddef.h` と衝突)
  - `v4l2loopback` 0.12.7 (未検証だが Noble archive 版は同様に古い)

  そのため `dkms-versions` を truncate して DKMS ビルドステージ全体をスキップします。
  Canonical は通常 HWE 投入時に DKMS を SRU で個別更新しますが、ビルド時点の Noble archive
  にそれらは含まれません。
- DKMS モジュールが必要な場合は対応バージョン (例: `zfs-dkms` 2.3.5+) を別途
  インストールしてください
