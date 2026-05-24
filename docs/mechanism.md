<!--
SPDX-License-Identifier: GPL-2.0-only
Copyright (c) 2026 Acompany Co., Ltd.
-->

# 仕組み

## IMA 計測の RTMR 拡張

モジュールは `ima_add_template_entry` を kretprobe でフックします。ret_handler は
ordered workqueue を起こすだけで、実際の RTMR 書き込みは worker 側で行います。

モジュールの動作に特定の IMA ポリシーは不要です。IMA がエントリを追加すればそれを
RTMR に拡張するため、**モジュールを先にロードしてから IMA ポリシーを設定する**ことで、
ポリシー適用後の全計測を漏れなく RTMR に反映できます。
IMA ポリシーの設定方法と注意点は [IMA ポリシーの注意点](ima-policy.md) を参照してください。

## IMA ログを起点とした順序保証

順序は IMA カーネル内部の `ima_measurements` リスト（append-only RCU リスト）から
直接取得します。worker は前回 extend した位置（cursor）を保持し、`rcu_dereference` で
リスト末尾まで walk して各エントリを順に RTMR へ拡張します。

このリストへの追加は `ima_extend_list_mutex` 保持中に行われるため、リストの順序は
IMA ログの正規順序そのものです。複数 CPU で同時に計測が発生しても、walk の結果は
常にログ順となります。

kretprobe の取りこぼし（`nmissed`）は致命的にはなりません。次回の firing で worker が
未処理のエントリまで walk して追いつくため、RTMR と IMA ログのチェーンは自動的に
回復します。

`ima_measurements` のアドレスは `kallsyms_lookup_name` 経由で解決します
（`kallsyms_lookup_name` 自体も v5.7 以降 unexport されているため、`kallsyms_lookup_name`
シンボルに kprobe を当ててアドレスを取得する慣用パターンを使用）。

## RTMR リプレイ検証

`validate.sh` および `validate.py` は RTMR の現在値をスナップショットとして取得した後、
IMA の計測ログを 1 エントリずつリプレイし、ハッシュチェーンの中間値がスナップショットと
一致する地点を探します。スナップショット後に追加されたエントリは自然に無視されるため、
非同期に計測が増え続ける環境でも検証できます。

`validate.py` は Python 実装で `validate.sh` より高速です。指定した `skip` で一致しなかった
場合は全開始位置を走査して合致点を探す機能も持ちます（`--no-search` で無効化可能）。

## sysfs インターフェース

モジュール起動時に `/sys/kernel/ima_rtmr/` 以下に診断用属性を作成します。すべて RO。

| 属性 | 内容 |
|------|------|
| `initial` | モジュール介入前の RTMR 値（hex）。Verifier はこれをリプレイの baseline に使う |
| `disabled` | 1 なら RTMR 書き込みエラー等で extend が永久停止されている |
| `extended_count` | これまでに RTMR へ拡張した IMA エントリ数。IMA ログ件数との差分が現在の async window |
| `nmissed` | kretprobe 取りこぼし回数（次回 firing で自動回復するため値が増えても correctness には影響しない） |

Verifier は `IMA ログ件数 == extended_count` を確認することで「未反映 (async window 中)」と
「恒久的な破綻 (`disabled=1`)」を区別できます。

## カーネル組み込みビルド

カーネルソースツリーにパッチを適用して built-in としてビルドすることで、boot_aggregate
を含む全 IMA エントリを捕捉できます。built-in の場合 `module_init()` は
`device_initcall`（level 6）にマップされ、IMA の `late_initcall`（level 7）より前に
初期化されます。
