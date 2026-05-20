<!--
SPDX-License-Identifier: GPL-2.0-only
Copyright (c) 2026 Acompany Co., Ltd.
-->

# 仕組み

## IMA 計測の RTMR 拡張

モジュールは `ima_add_template_entry` を kretprobe でフックします。IMA がテンプレート
エントリを追加するたびに、そのエントリの SHA-384 テンプレートダイジェストを FIFO
キューに入れ、ordered workqueue で tsm-mr sysfs 経由で RTMR に書き込みます。

モジュールの動作に特定の IMA ポリシーは不要です。IMA がエントリを追加すればそれを
RTMR に拡張するため、**モジュールを先にロードしてから IMA ポリシーを設定する**ことで、
ポリシー適用後の全計測を漏れなく RTMR に反映できます。
IMA ポリシーの設定方法と注意点は [IMA ポリシーの注意点](ima-policy.md) を参照してください。

## RTMR リプレイ検証

`validate.sh` は RTMR[2] の現在値をスナップショットとして取得した後、IMA の SHA-384
計測ログを 1 エントリずつリプレイし、ハッシュチェーンの中間値がスナップショットと一致
する地点を探します。スナップショット後に追加されたエントリは自然に無視されるため、
非同期に計測が増え続ける環境でも検証できます。

## カーネル組み込みビルド

カーネルソースツリーにパッチを適用して built-in としてビルドすることで、boot_aggregate
を含む全 IMA エントリを捕捉できます。built-in の場合 `module_init()` は
`device_initcall`（level 6）にマップされ、IMA の `late_initcall`（level 7）より前に
初期化されます。
