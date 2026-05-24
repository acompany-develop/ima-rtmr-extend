<!--
SPDX-License-Identifier: GPL-2.0-only
Copyright (c) 2026 Acompany Co., Ltd.
-->

# 既知の問題

## 問題 1: ロード前の IMA エントリは RTMR に反映されない

モジュールがロードされる前に発生した IMA 計測は RTMR に extend されません。
これに対しては、`/sys/kernel/ima_rtmr/initial` がモジュール介入直前の RTMR 値を
保持しているため、Verifier はこれをリプレイの baseline として使うことで
ロード前エントリの影響を回避できます。

`insmod` 自体も IMA 計測を誘発しますが、これは kretprobe 登録より前に発生するため
本モジュールは捕捉できません。`initial` の値はこの insmod-由来エントリも含めた状態の
RTMR を反映しています。

カーネルに組み込む（built-in）場合はこの問題自体を回避でき、`boot_aggregate` を含む
全エントリを捕捉できます。ビルド方法は [Ubuntu カーネルビルド](../ubuntu/README.md)
を参照してください。

## 問題 2: async window 中の attestation

kretprobe 発火から RTMR 書き込みまでには workqueue を経由するため、μs〜ms の
非同期窓があります。この窓中に attestation が行われると、IMA ログには存在するが
RTMR には未反映のエントリが見える可能性があります。

`validate.sh`/`validate.py` は RTMR を先にスナップショットしてからログを読み、
ハッシュチェーンが一致した地点で停止する設計のため、async window を正しく許容します。

Verifier 側で「未反映」と「恒久的破綻」を区別したい場合は
`/sys/kernel/ima_rtmr/extended_count` と `/sys/kernel/ima_rtmr/disabled` を確認します。
