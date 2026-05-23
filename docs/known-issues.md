<!--
SPDX-License-Identifier: GPL-2.0-only
Copyright (c) 2026 Acompany Co., Ltd.
-->

# 既知の問題

## 問題 1: IMA ログの開始地点が Verifier 側からわからない

カーネルモジュールは任意のタイミングでロードできるため、IMA ログのどの地点から
RTMR[2] への拡張が開始されたのかを Verifier 側は感知できません。`insmod` 自体も
IMA 計測を誘発しますが、kretprobe が登録されるのはモジュール初期化の最後であるため、
insmod 中に生成された IMA エントリはログには存在するが RTMR には拡張されません。

カーネルに組み込む（built-in）場合はこの問題を回避でき、boot_aggregate を含む
全エントリを捕捉できます。ビルド方法は [Ubuntu カーネルビルド](../ubuntu/README.md)
を参照してください。

## 補足: 並行計測時の順序保証

複数 CPU で同時に IMA 計測が発生した場合、kretprobe の発火順序は IMA ログの正規順序と
異なることがあります。これに対処するため、本モジュールは `ima_add_digest_entry` に
kprobe を仕掛けて `ima_extend_list_mutex` 保持中にシーケンス番号を発行し、リオーダーバッファで
IMA ログ順に並べ直してから RTMR を拡張します。詳細は [仕組み](mechanism.md) を参照してください。

シーケンス用 kprobe の登録に失敗した場合（カーネル側のシンボル名変更などが原因）は
FIFO 順にフォールバックします。この場合は古い動作と同様に並行計測の順序が
入れ替わる可能性があります。

シーケンス番号が割り当てられなかったエントリ（kretprobe maxactive 超過や seq_map 溢れに
由来）はリオーダーバッファをバイパスして即時 RTMR に拡張されます。これにより hang は
防げますが、同時に到着した有効 seq エントリより先に extend されるため、ハッシュチェーンの
順序が IMA ログ順とずれて `validate.py` の MATCH が失敗することがあります。

`CONFIG_LTO_CLANG=y` カーネルでは `ima_add_digest_entry` が確定的にインライン化される
ため、ビルド時に `src/ima.h` の `#error` で拒否します。LTO カーネルを使う場合は
LTO を無効にして再ビルドしてください。
