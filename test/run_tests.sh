#!/bin/bash

# 簡易的なテストフレームワーク
# tests.conf に定義されたテストケースを実行し、結果を表示する

set -o pipefail

# 色の定義
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# プロジェクトルートディレクトリ
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TEST_DIR="$SCRIPT_DIR"
CONF_FILE="$TEST_DIR/tests.conf"
BUILD_DIR="$PROJECT_ROOT/build"

# コンパイラとテストヘルパーのパス
COMPILER="$BUILD_DIR/yoctocc"
TEST_HELPER_O="$BUILD_DIR/test_helper.o"
ASM_FILE="$BUILD_DIR/program.s"
OBJ_FILE="$BUILD_DIR/program.o"
BIN_FILE="$BUILD_DIR/program"

# カウンタ
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# ヘッダー表示
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}      Yoctocc テストスイート${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# 設定ファイルの確認
if [ ! -f "$CONF_FILE" ]; then
    echo -e "${RED}エラー: 設定ファイル $CONF_FILE が見つかりません${NC}"
    exit 1
fi

# コンパイラのビルド（インクリメンタルビルド）
echo -e "${YELLOW}コンパイラをビルド中...${NC}"
cd "$PROJECT_ROOT"
if ! make -s "$COMPILER" "$TEST_HELPER_O" > /dev/null 2>&1; then
    echo -e "${RED}コンパイラのビルドに失敗しました${NC}"
    exit 1
fi
echo -e "${GREEN}コンパイラのビルドが完了しました${NC}"
echo ""

# テストケースの読み込みと実行
while IFS= read -r line || [ -n "$line" ]; do
    # コメントと空行をスキップ
    [[ "$line" =~ ^[[:space:]]*# ]] && continue
    [[ -z "${line// }" ]] && continue

    # テストケースのパース (期待値とコードを分離)
    EXPECTED_EXIT=$(echo "$line" | awk '{print $1}')
    TEST_CODE=$(echo "$line" | cut -d' ' -f2-)

    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    # テスト実行
    echo -n "Testing #$TOTAL_TESTS: "

    # コンパイラ実行（直接実行、make を経由しない）
    ERROR_LOG=$(mktemp)
    if ! echo "$TEST_CODE" | "$COMPILER" /dev/stdin > "$ERROR_LOG" 2>&1; then
        echo -e "${RED}✗ FAILED (compile error): $TEST_CODE${NC}"
        if [ -s "$ERROR_LOG" ]; then
            cat "$ERROR_LOG" | head -n 3 | sed 's/^/  /'
        fi
        rm -f "$ERROR_LOG"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        continue
    fi
    rm -f "$ERROR_LOG"

    # アセンブル & リンク（直接実行）
    if ! gcc -c -o "$OBJ_FILE" "$ASM_FILE" 2>/dev/null; then
        echo -e "${RED}✗ FAILED (assemble error): $TEST_CODE${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        continue
    fi

    if ! gcc -nostdlib -no-pie -o "$BIN_FILE" "$OBJ_FILE" "$TEST_HELPER_O" 2>/dev/null; then
        echo -e "${RED}✗ FAILED (link error): $TEST_CODE${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        continue
    fi

    # プログラム実行
    set +e
    "$BIN_FILE" > /dev/null 2>&1
    ACTUAL_EXIT=$?
    set -e

    # 結果の比較
    if [ "$ACTUAL_EXIT" -eq "$EXPECTED_EXIT" ]; then
        echo -e "${GREEN}$TEST_CODE => $ACTUAL_EXIT${NC}"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "${RED}$TEST_CODE => $EXPECTED_EXIT expected, but got $ACTUAL_EXIT${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi

done < "$CONF_FILE"

# 結果のサマリー
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}      テスト結果サマリー${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "総テスト数:   $TOTAL_TESTS"
echo -e "${GREEN}成功:         $PASSED_TESTS${NC}"
echo -e "${RED}失敗:         $FAILED_TESTS${NC}"
echo ""

# 終了コードの決定
if [ "$FAILED_TESTS" -eq 0 ]; then
    echo -e "${GREEN}すべてのテストが成功しました!${NC}"
    exit 0
else
    echo -e "${RED}一部のテストが失敗しました${NC}"
    exit 1
fi
