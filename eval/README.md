<!--
SPDX-License-Identifier: GPL-2.0-only
Copyright (c) 2026 Acompany Co., Ltd.
-->

# eval/ — Empirical evaluation scripts

論文の評価章 (`C1`〜`C5`) を実機 (GCE `ito-tdx-test`, c3-standard-44, Intel TDX 有効) で実施するためのスクリプト・bpftrace・Python ヘルパ群。

## 評価項目と対応スクリプト

| 寄与 | 主張 | スクリプト |
|---|---|---|
| **C1** | TDX 上で IMA log と RTMR[2] の hash chain が一致する | `c1-correctness.sh` |
| **C2** | 並列計測下で replay 一致率 100% | `c2-parallel.sh` + `python/wilson.py` |
| **C3** | race window が p99 で sub-ms オーダ、verifier loop の anti-pattern を引き起こさない | `c3-race-window.sh` + `bpftrace/race-window.bt` |
| **C4** | syscall オーバーヘッドが production deployable な水準 | `c4-overhead.sh` + `c/microbench.c` |
| **C5** | in-tree 利得と `/sys/kernel/ima_rtmr/initial` baseline 実用性 | `c5-coverage.sh` |

## 使い方（順序）

```sh
# 1. ホスト設定の固定（要 root）
sudo eval/setup-host.sh

# 2. モジュールをロードして基本動作確認 (C1)
sudo eval/c1-correctness.sh

# 3. 並列計測下での一致率 (C2)
sudo eval/c2-parallel.sh   # ~30 分

# 4. race window 実測 (C3)
sudo eval/c3-race-window.sh   # ~10 分

# 5. オーバーヘッド (C4)
sudo eval/c4-overhead.sh   # ~20 分

# 6. in-tree 比較は別カーネルが必要 (C5)
sudo eval/c5-coverage.sh ima_policy=tcb           # OOT モード
sudo eval/c5-coverage.sh ima_policy=critical_data
# in-tree モードは別ビルドのカーネルで再起動した上で実行
```

## 出力

すべての結果は `results/<timestamp>/` 配下に保存される。各実行で:
- `meta.json`: kernel version, TDX module version, microcode rev, CPU info
- `<test>.json`: 測定生データ
- `<test>.csv`: 集計済みデータ
- `<test>.log`: 実行ログ

## 前提

- Linux kernel 6.16+ (or with `tsm-mr` patches)
- `CONFIG_TSM_MEASUREMENTS=y`
- `bpftrace`, `stress-ng`, `lmbench`, `hyperfine`, `python3.10+` with `numpy`, `scipy`
- root 権限 (kernel module ロード、bpftrace、tsm-mr sysfs 書き込み)
- 計測中は他のワークロードを停止することが望ましい
