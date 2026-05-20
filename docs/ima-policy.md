<!--
SPDX-License-Identifier: GPL-2.0-only
Copyright (c) 2026 Acompany Co., Ltd.
-->

# IMA ポリシーの注意点

## ポリシーの書き込み順序

IMA ポリシーは**先にマッチしたルールが優先**されます。仮想ファイルシステムの除外ルール
(`dont_measure`) は `measure` ルールより先に書き込む必要があります。

順序を間違えると、RTMR の読み取りや IMA ログの読み取り自体が新たな計測と RTMR 拡張を
誘発し、無限ループ的にエントリが増加します。

## 推奨除外ルール

```sh
# 仮想ファイルシステムを除外（measure より先に書く）
echo "dont_measure fsmagic=0x9fa0"     | sudo tee -a /sys/kernel/security/ima/policy  # procfs
echo "dont_measure fsmagic=0x62656572" | sudo tee -a /sys/kernel/security/ima/policy  # sysfs
echo "dont_measure fsmagic=0x73636673" | sudo tee -a /sys/kernel/security/ima/policy  # securityfs
echo "dont_measure fsmagic=0x64626720" | sudo tee -a /sys/kernel/security/ima/policy  # debugfs
```

## ポリシーの不可逆性

> [!WARNING]
> IMA ポリシーは追加専用で、再起動でしかリセットできません。

ランタイムでの書き込みには `CONFIG_IMA_WRITE_POLICY=y` が必要です。
ポリシーを誤って書き込んだ場合は再起動が必要です。

## モジュールとポリシーの読み込み順

IMA_RTMR モジュールを先にロードしてからポリシーを設定することで、ポリシー適用後の
全計測を漏れなく RTMR に反映できます。

カーネル組み込み（built-in）の場合は `device_initcall`（level 6）で初期化されるため、
IMA の `late_initcall`（level 7）より先に準備が完了し、boot_aggregate を含む全エントリを
自動的に捕捉します。
