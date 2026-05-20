<!--
SPDX-License-Identifier: GPL-2.0-only
Copyright (c) 2026 Acompany Co., Ltd.
-->

# Ubuntu カーネルビルド (Docker)

IMA_RTMR を組み込んだ Ubuntu カーネル `.deb` パッケージを Docker でビルドします。

## 対応しているUbuntuバージョン

| ディレクトリ | ディストリビューション | カーネル |
| --- | --- | --- |
| `noble/` | Ubuntu 24.04 LTS (Noble) HWE | 6.17 |
| `questing/` | Ubuntu 25.10 (Questing) | 6.17 |
| `resolute/` | Ubuntu 26.04 LTS (Resolute) | 7.0 |

## 必要環境

- Docker (BuildKit 有効 / Docker 23.0+ ではデフォルト)
- 十分なディスク空間 (ビルドイメージに ~30GB)

## ビルド方法

各バリアントの `build.sh` を実行します。

```sh
# Ubuntu 26.04 (Resolute) カーネル 7.0
ubuntu/resolute/build.sh

# Ubuntu 24.04 (Noble) HWE カーネル 6.17
ubuntu/noble/build.sh

# Ubuntu 25.10 (Questing) カーネル 6.17
ubuntu/questing/build.sh
```

ビルド完了後、`.deb` パッケージは各バリアントの `out/` に出力されます。

## インストール

linux-modules-*が複数ビルドされますが、CVMで動作させる場合は不要です。

```sh
cd ubuntu/<variant>/out/
sudo apt install ./linux-image-unsigned-*-generic_*.deb ./linux-headers-*.deb ./linux-modules-[0-9]*-generic_*.deb
sudo reboot
```

## ビルドの仕組み

Dockerfile は 3 つのフェーズに分かれています。

### Phase 1: カーネルソース取得

`apt-get source` で Ubuntu のカーネルソースパッケージを取得します。

### Phase 2: パッチ適用・DKMS バージョン調整

1. `kernel/apply-patch.sh` で IMA_RTMR のソースをカーネルツリーに統合
2. `CONFIG_IMA_RTMR=y` をカーネル config annotations に追加
3. `kernel/pin-dkms-versions.sh` で DKMS パッケージバージョンをリポジトリの
   実際のバージョンに合わせる

### Phase 3: カーネルコンパイル

`fakeroot debian/rules` で `.deb` パッケージをビルドします。
