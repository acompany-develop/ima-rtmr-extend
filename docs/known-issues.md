<!--
SPDX-License-Identifier: GPL-2.0-only
Copyright (c) 2026 Acompany Co., Ltd.
-->

# 既知の問題

## 問題 1: IMA ログ書き込みと RTMR 拡張がアトミックでない

kretprobe は `ima_add_template_entry` がカーネルの mutex を解放した後に発火します。
そのため IMA ログへの書き込みと RTMR への拡張の間にタイムラグがあり、複数の CPU で
同時に IMA 計測が発生した場合、FIFO へのエンキュー順が IMA ログの正規順序と異なる
ことがあります。これは kretprobe 方式の本質的な制約であり、回避できません。

この場合、IMA ログを先頭から順にリプレイしても RTMR 値を再現できません。検証側では
順序の置換を考慮したリプレイが必要になります。`validate.sh` は単純な順序リプレイのみ
対応しているため、並行計測が多い環境では検証に失敗する可能性があります。

## 問題 2: IMA ログの開始地点が Verifier 側からわからない

カーネルモジュールは任意のタイミングでロードできるため、IMA ログのどの地点から
RTMR[2] への拡張が開始されたのかを Verifier 側は感知できません。`insmod` 自体も
IMA 計測を誘発しますが、kretprobe が登録されるのはモジュール初期化の最後であるため、
insmod 中に生成された IMA エントリはログには存在するが RTMR には拡張されません。

カーネルに組み込む（built-in）場合はこの問題を回避でき、boot_aggregate を含む
全エントリを捕捉できます。ビルド方法は [Ubuntu カーネルビルド](../ubuntu/README.md)
を参照してください。
