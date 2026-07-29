# YoctoCC

- Yocto : 小さい (10^-24)
- YoctoC : 翌年 (yokutoshi)
- CC : C コンパイラ
- YoctoCC : 翌年には完成できるかな？小さな C コンパイラ (2025年現在)

https://github.com/rui314/chibicc をベースに C++ で書きながら学習しているリポジトリ。

## 必要環境

- C++26 対応コンパイラ
- Ubuntu 24.04

詳細は [./.devcontainer/Dockerfile](./.devcontainer/Dockerfile) を参照。

## ビルド & テスト

```bash
# ビルド（デフォルト: g++）
make

# clang でビルド
make CXX=clang++ CC=clang

# 古いコンパイラの場合（C++23 にフォールバック）
make CXX=clang++ CC=clang CXX_STD=-std=c++23

# テスト実行
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
