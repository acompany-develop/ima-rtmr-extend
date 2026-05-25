<!--
SPDX-License-Identifier: GPL-2.0-only
Copyright (c) 2026 Acompany Co., Ltd.
-->

# eval

`ima-rtmr-extend` モジュールを TDX ホスト上で検証・計測するためのスクリプト集。
すべて `sudo` で実行する想定。

| script | 内容 |
| --- | --- |
| `setup-host.sh` | CPU governor / ASLR / SMT を計測向けに固定 (1 boot に 1 回) |
| `smoke.sh` | モジュールを load してワークロードを 1 周走らせ、`validate.py` で一致確認 |
| `replay-match.sh` | `stress-ng --exec N` を N と反復で振り、replay 一致率を Wilson 95% CI で出力 |
| `race-window.sh` | `race-window.bt` を裏で回しながらワークロードを駆動、bpftrace 出力を percentile に変換 |
| `syscall-bench.sh` | `syscall-bench.c` を使い execve / openat / read のレイテンシをモジュール有無で比較 |
| `coverage.sh` | OOT / in-tree モードで `extended_count` と IMA log の差を取り、`initial` baseline で `validate.py` を走らせる |

出力は `results/<script>-<UTCタイムスタンプ>/` に置かれる。`meta.json` に kernel / TDX module / microcode 等の環境情報を記録する。

## 前提

- Linux 6.16+ (`CONFIG_TSM_MEASUREMENTS=y`)
- `bpftrace`, `stress-ng`, root 権限
- 計測中は他のワークロードを止めること
