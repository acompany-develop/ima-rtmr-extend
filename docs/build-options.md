<!--
SPDX-License-Identifier: GPL-2.0-only
Copyright (c) 2026 Acompany Co., Ltd.
-->

# ビルドオプションの安全性

Docker でのカーネルビルドで使用している各最適化オプションの安全性評価です。
サーバー / メインライン用途のカーネルを前提としています。

## ビルド検証スキップ

これらは全てコンパイル時の検証であり、コンパイラフラグや `.config` を変更しません。
生成されるカーネルバイナリはスキップの有無に関わらず同一です。

| フラグ | 動作 | カーネルへの影響 |
| --- | --- | --- |
| `do_skip_checks=true` | 以下のチェックを一括スキップ | なし |
| `skipabi=true` | エクスポートシンボルの ABI 互換性チェックをスキップ | なし |
| `skipmodule=true` | モジュールリスト完全性チェックをスキップ | なし |
| `skipretpoline=true` | retpoline 適用の監査をスキップ。**retpoline 自体は無効化されない** | なし |

### skipabi について

ABI チェックは Ubuntu 公式リリースで DKMS モジュールとの互換性を保証するためのもの
です。カスタムカーネルでは比較対象の ABI ベースラインが存在しないため、スキップが必要です。
DKMS モジュールはインストール時にカーネルヘッダーから再ビルドされるため影響しません。

### skipretpoline について

retpoline チェックはコンパイラが retpoline をサポートしているかの検証であり、
`CONFIG_RETPOLINE=y` によるコンパイルは維持されます。Spectre v2 緩和策は有効です。

## パッケージングスキップ

| フラグ | 動作 | 影響 |
| --- | --- | --- |
| `skipdbg=true` | `-dbgsym` `.ddeb` パッケージの生成をスキップ | BTF/BPF/bpftrace は影響なし。`crash` によるダンプ解析には dbgsym が必要 |
| `do_linux_tools=false` | perf, bpftool, cpupower 等の全ツールビルドをスキップ | `apt install linux-tools-$(uname -r)` で別途インストール可能 |
| `do_cloud_tools=false` | hv_kvp_daemon 等のクラウド/Hyper-V ツールビルドをスキップ | `apt install linux-cloud-tools-$(uname -r)` で別途インストール可能 |
| `do_tools_common=false` | `linux-tools-common` パッケージのビルドをスキップ | 同上 |
| `do_tools_host=false` | `linux-tools-host` パッケージのビルドをスキップ | 同上 |

### skipdbg と BTF の関係

`skipdbg=true` は **DWARF デバッグシンボルパッケージ**の生成のみをスキップします。
BPF プログラムが依存する BTF（BPF Type Format）は `CONFIG_DEBUG_INFO_BTF=y` によって
vmlinux に直接埋め込まれており、dbgsym パッケージとは独立です。

- `perf record` / `perf top` : 正常動作（`/proc/kallsyms` 経由）
- BPF / bpftrace / CO-RE : 正常動作（BTF は vmlinux に含まれる）
- `crash` / `drgn` : dbgsym パッケージが必要（kdump 使用時は注意）

## fakeroot スキップ

root で実行している場合（Docker / chroot 内）、`fakeroot` を使用しません。

`fakeroot` の `faked-sysv` デーモンはシングルスレッドで動作し、全プロセスの
`stat`/`chown`/`chmod` コールを IPC 経由でシリアライズします。高並列ビルド
（`-j66` 等）では、このデーモンがボトルネックとなり CPU 使用率が著しく低下します
（44 コアで 5% 程度）。

Docker コンテナ内は root で実行されるため `fakeroot` は不要であり、
スキップすることで全 CPU を活用できます。

## ccache 設定

| 設定 | リスク |
| --- | --- |
| `CCACHE_SLOPPINESS=time_macros` | `__DATE__`/`__TIME__` の値がキャッシュから返される。カーネルのタイムスタンプは `KBUILD_BUILD_TIMESTAMP` で制御されるため実害なし |
| `CCACHE_SLOPPINESS=include_file_mtime,include_file_ctime` | ヘッダ変更とコンパイルが同一秒に発生した場合の理論的競合。カーネルビルドではヘッダは `make prepare` 後に変更されないため該当しない |
| `CCACHE_COMPRESS=true` | デメリットなし |

ccache の `time_macros` 設定に起因する不具合報告はありません。

> [!NOTE]
> GCC 以外のコンパイラを使用する場合は注意が必要です。Clang の
> `-frandomize-layout-seed-file` と ccache の組み合わせで起動不能カーネルが
> 生成された事例が報告されています ([ccache#1528](https://github.com/ccache/ccache/issues/1528))。
> Ubuntu の標準ビルドでは GCC を使用するため該当しません。

## Secure Boot 署名

カスタムカーネルを Secure Boot で起動するには MOK で `vmlinuz` を署名する。

### MOK 証明書の制約

Extended Key Usage OID `1.3.6.1.4.1.2312.16.1.2`（module-signing-only）を含めてはいけない。
含めると shim >= 15.4 がカーネルイメージを拒否する。`generate-mok.sh` はこの OID を持たない証明書を生成する。

### 使い方

```bash
bash ubuntu/common/generate-mok.sh ./signing
MOK_KEY=./signing/MOK.priv MOK_CERT=./signing/MOK.pem bash ubuntu/questing/build.sh
sudo mokutil --import ./signing/MOK.der && sudo reboot
```

`MOK_KEY` / `MOK_CERT` が未設定なら署名はスキップされる。秘密鍵は BuildKit の `--secret`
として `sign-kernel.sh` に渡され、イメージレイヤーには残らない。

### カーネルモジュール

`.ko` は `CONFIG_MODULE_SIG_ALL=y` によりビルド中の一時鍵で自動署名され、公開鍵は
カーネルイメージに埋め込まれる。DKMS 等で後から作るモジュールは別途署名が必要。

### TDX 環境

MOK の登録状態は RTMR に計測されるため、鍵を変えるとアテステーション値が変わる。
TDVF の NVRAM に MOK を事前登録しておけば対話的な MokManager 操作が省ける。

## 参考資料

- [Ubuntu Kernel Build Documentation](https://canonical-kernel-docs.readthedocs-hosted.com/how-to/develop-customise/build-kernel/)
- [KernelTeam/KernelMaintenance - Ubuntu Wiki](https://wiki.ubuntu.com/KernelTeam/KernelMaintenance)
- [ccache Manual](https://ccache.dev/manual/latest.html)
- [Speeding Up Linux Kernel Builds With ccache](https://nickdesaulniers.github.io/blog/2018/06/02/speeding-up-linux-kernel-builds-with-ccache/)
- [Kernel Reproducible Builds Documentation](https://docs.kernel.org/kbuild/reproducible-builds.html)
- [How to Sign Things for Secure Boot - Ubuntu](https://ubuntu.com/blog/how-to-sign-things-for-secure-boot)
- [Ubuntu UEFI/SecureBoot - Ubuntu Wiki](https://wiki.ubuntu.com/UEFI/SecureBoot)
