# YoctoCC テストフレームワーク

このディレクトリには、[chibicc](https://github.com/rui314/chibicc) スタイルのユニットテストが含まれています。

テストケースは chibicc コミット [`a817b23`](https://github.com/rui314/chibicc/commit/a817b23da3c6f39f22bc57c0a53169978d97d7fa) 時点のものと一致しています。

## テストの実行

プロジェクトルートから:

```bash
# 全テスト実行（並列）
make test

# 特定のテストのみ
bash test/run_tests_parallel.sh arith

# 複数フィルタ
bash test/run_tests_parallel.sh arith pointer

# 並列数を指定
PARALLEL_JOBS=4 make test
```

## テストケース

`test/cases/` ディレクトリに `.c` ファイルとして配置されています。

| ファイル | 内容 | ケース数 |
|---|---|---|
| `arith.c` | 算術演算・比較演算 | 26 |
| `control.c` | if / for / while / ブロック / カンマ演算子 | 13 |
| `function.c` | 関数定義・呼び出し・再帰 | 12 |
| `pointer.c` | ポインタ・配列・添字 | 32 |
| `string.c` | 文字列リテラル・エスケープシーケンス | 28 |
| `struct.c` | 構造体・アライメント・代入 | 40 |
| `union.c` | 共用体 | 7 |
| `variable.c` | 変数・sizeof・スコープ・型宣言 | 48 |
| **合計** | | **206** |

## テストの仕組み

各テストファイルは `ASSERT(expected, actual)` マクロを使用します。

```c
int main() {
    ASSERT(3, 1+2);
    ASSERT(10, 2*5);
    return 0;
}
```

- `ASSERT` は `test_helper.c` / `test_helper.h` で定義
- `-nostdlib` 環境で動作（syscall ベース）
- 失敗時は stderr にエラーを出力し、プロセスが非ゼロで終了

## ファイル構成

```
test/
├── cases/           # テストケース（.c ファイル）
├── run_tests_parallel.sh  # 並列テスト実行スクリプト
├── test_helper.c    # ASSERT マクロ実装（syscall ベース）
├── test_helper.h    # ASSERT マクロ宣言
└── README.md        # このファイル
```

## 出力例

```
========================================
      Yoctocc テストスイート (並列)
========================================
Cコンパイラ: cc
並列数: 8
タイムアウト: 10s

コンパイラをビルド中...
コンパイラのビルドが完了しました

テスト実行中... (206 ケース / 8 ファイル)

[arith.c]
    #1 0 => expected: 0, actual: 0
    #2 42 => expected: 42, actual: 42
    ...

========================================
      テスト結果サマリー
========================================
ファイル数:   8 (失敗: 0)
総ケース数:   206
成功:         206

すべてのテストが成功しました! (206/206)
```

## 終了コード

- `0` - すべてのテストが成功
- `1` - 一部またはすべてのテストが失敗
