# YoctoCC

- Yocto : 小さい (10^-24)
- YoctoC : 翌年 (yokutoshi)
- CC : C コンパイラ
- YoctoCC : 翌年には完成できるかな？小さな C コンパイラ (2025年現在)

https://github.com/rui314/chibicc をベースに C++ で書きながら学習しているリポジトリ。

## 必要環境

- C++23 対応コンパイラ
  - g++ 14 以上
  - clang++ 17 以上（20 推奨）
  - Apple Clang 15 以上（macOS）
- GNU Make
- Linux / macOS

## 動作確認済み環境

| 環境 | OS | コンパイラ |
|------|-----|----------|
| WSL2 | Ubuntu 24.04 | g++ 14, clang++ 17/18/20 |
| Raspi 3B | Ubuntu ? | Clang (予定) |
| Mac mini M1 | macOS | Apple Clang (予定) |

## ビルド & テスト

```bash
# ビルド（デフォルト: g++）
make

# clang でビルド
make CXX=clang++ CC=clang

# Apple Clang（古いバージョン）の場合
make CXX=clang++ CC=clang CXX_STD=-std=c++2b

# テスト実行（並列）
make test

# clang でテスト
make CXX=clang++ CC=clang test

# クリーンビルド
make clean && make test

# ヘルプ
make help
```

## 使い方

```bash
# コンパイル（出力先はデフォルトで build/program.s）
./build/yoctocc source.c

# 出力先を指定
./build/yoctocc source.c output.s
```

---

## ARM64 対応（予定）

YoctoCC は将来的に ARM64 アーキテクチャのアセンブリ出力に対応予定。

### 目標

1. **マルチプラットフォーム開発**: x86_64 でも ARM64 でも YoctoCC 自体をビルド・開発可能
2. **マルチターゲット出力**: YoctoCC が x86_64 / ARM64 両方のアセンブリを生成可能

```bash
# 将来の使用イメージ
./build/yoctocc source.c                    # デフォルト（ホストアーキテクチャ）
./build/yoctocc -arch x86_64 source.c       # x86_64 アセンブリ出力
./build/yoctocc -arch arm64 source.c        # ARM64 アセンブリ出力
```

### WSL2 (x86_64) での ARM64 実行環境

ARM64 バイナリを x86_64 環境で実行するための環境構築：

```bash
# QEMU ユーザーモードエミュレータ
sudo apt update
sudo apt install -y qemu-user qemu-user-static binfmt-support

# ARM64 クロスコンパイラ（アセンブル用）
sudo apt install -y gcc-aarch64-linux-gnu
```
