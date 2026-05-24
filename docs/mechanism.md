<!--
SPDX-License-Identifier: GPL-2.0-only
Copyright (c) 2026 Acompany Co., Ltd.
-->

# 仕組み

## IMA 計測の RTMR 拡張

モジュールは `ima_add_template_entry` を kretprobe でフックします。IMA がテンプレート
エントリを追加するたびに、そのエントリのテンプレートダイジェストを FIFO キューに入れ、
ordered workqueue で tsm-mr sysfs 経由で RTMR に書き込みます。

モジュールの動作に特定の IMA ポリシーは不要です。IMA がエントリを追加すればそれを
RTMR に拡張するため、**モジュールを先にロードしてから IMA ポリシーを設定する**ことで、
ポリシー適用後の全計測を漏れなく RTMR に反映できます。
IMA ポリシーの設定方法と注意点は [IMA ポリシーの注意点](ima-policy.md) を参照してください。

## シーケンス番号による順序付け

並行計測時に kretprobe の発火順序が IMA ログの正規順序と異なる問題に対処するため、
モジュールは `ima_add_digest_entry` にも kprobe を仕掛けてシーケンス番号を発行します。
この kprobe は `ima_extend_list_mutex` 保持中に呼ばれるため、付与した番号は IMA ログの
正規順序と必ず一致します。

各 RTMR 拡張要求はこの番号を持って FIFO に入り、ワーカースレッド側のリオーダーバッファ
（既定 32 エントリ）でシーケンス番号順に並べ直された後で RTMR に書き込まれます。
`ima_add_template_entry` が失敗したエントリは欠番として記録され、`next_expected_seq` を
飛ばして次のエントリへ進みます。

kprobe の登録に失敗した場合は FIFO 順での拡張にフォールバックします（既存の挙動）。

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
| `initial` | モジュール介入前の RTMR 値（hex）。 Verifier はこれをリプレイの baseline に使うことで、`insmod` を含むロード前エントリの不可視性を回避できる |
| `disabled` | 1 なら RTMR sysfs エラー等で extend が永久停止されている。attestation の整合性は壊れていないが、以降の測定は RTMR に反映されない |
| `drops` | FIFO 溢れで欠落した測定数。0 でなければ RTMR と IMA ログの整合が破綻している可能性 |
| `nmissed` | kretprobe 取りこぼし回数。同じく 0 でなければ未反映エントリあり |
| `seq_enabled` | 1 なら kprobe ベースの順序付けが動作中、0 なら FIFO フォールバック |

これらにより Verifier は async window と恒久的な破綻を区別できます。

## カーネル組み込みビルド

カーネルソースツリーにパッチを適用して built-in としてビルドすることで、boot_aggregate
を含む全 IMA エントリを捕捉できます。built-in の場合 `module_init()` は
`device_initcall`（level 6）にマップされ、IMA の `late_initcall`（level 7）より前に
初期化されます。
