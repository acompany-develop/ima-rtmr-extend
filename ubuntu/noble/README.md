<!--
SPDX-License-Identifier: GPL-2.0-only
Copyright (c) 2026 Acompany Co., Ltd.
-->

# Ubuntu 24.04 LTS (Noble) HWE カーネル

Ubuntu 24.04 LTS のデフォルトカーネル (6.8) には tsm-mr が存在しない(RTMRそのものがない)ため、
このカーネルモジュールは動作しません。このビルドでは HWE (Hardware Enablement) カーネル 6.17 を
使用して tsm-mr を利用可能にしています。

## 注意事項

- HWE 6.17 は Noble 24.04 の DKMS パッケージ (ZFS 2.2.2 等) と互換性がないため、
  DKMS モジュールのビルドはスキップしています
- ZFS が必要な場合は HWE カーネル対応版の `zfs-dkms` を別途インストールしてください
