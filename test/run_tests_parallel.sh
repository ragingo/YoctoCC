#!/bin/bash

# 並列テストフレームワーク
# バックグラウンドジョブを使用して高速化

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

# Makefile ターゲット用（相対パス）
COMPILER_TARGET="build/yoctocc"
TEST_HELPER_TARGET="build/test_helper.o"

# 実行用（絶対パス）
COMPILER="$PROJECT_ROOT/$COMPILER_TARGET"
TEST_HELPER_O="$PROJECT_ROOT/$TEST_HELPER_TARGET"

# 並列数（CPU コア数）
PARALLEL_JOBS=${PARALLEL_JOBS:-$(nproc)}

# 一時ディレクトリ
WORK_DIR=$(mktemp -d)
trap "rm -rf $WORK_DIR" EXIT

# ヘッダー表示
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}      Yoctocc テストスイート (並列)${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "並列数: $PARALLEL_JOBS"
echo ""

# 設定ファイルの確認
if [ ! -f "$CONF_FILE" ]; then
    echo -e "${RED}エラー: 設定ファイル $CONF_FILE が見つかりません${NC}"
    exit 1
fi

# コンパイラのビルド（インクリメンタルビルド）
echo -e "${YELLOW}コンパイラをビルド中...${NC}"
cd "$PROJECT_ROOT"
if ! make -s "$COMPILER_TARGET" "$TEST_HELPER_TARGET" > /dev/null 2>&1; then
    echo -e "${RED}コンパイラのビルドに失敗しました${NC}"
    exit 1
fi
echo -e "${GREEN}コンパイラのビルドが完了しました${NC}"
echo ""

# 単一テスト実行関数
run_single_test() {
    local test_num="$1"
    local expected_exit="$2"
    local test_code="$3"

    local test_work="$WORK_DIR/test_$test_num"
    mkdir -p "$test_work"

    local test_file="$test_work/test.c"
    local asm_file="$test_work/program.s"
    local obj_file="$test_work/program.o"
    local bin_file="$test_work/program"
    local result_file="$test_work/result"

    # テストコードをファイルに書き出し
    echo "$test_code" > "$test_file"

    # コンパイラ実行（出力先を直接指定）
    if ! "$COMPILER" "$test_file" "$asm_file" > "$test_work/compiler.log" 2>&1; then
        echo "FAIL compile" > "$result_file"
        return
    fi

    # アセンブル
    if ! gcc -c -o "$obj_file" "$asm_file" 2>/dev/null; then
        echo "FAIL assemble" > "$result_file"
        return
    fi

    # リンク
    if ! gcc -nostdlib -no-pie -o "$bin_file" "$obj_file" "$TEST_HELPER_O" 2>/dev/null; then
        echo "FAIL link" > "$result_file"
        return
    fi

    # 実行
    "$bin_file" > /dev/null 2>&1
    local actual_exit=$?

    if [ "$actual_exit" -eq "$expected_exit" ]; then
        echo "PASS $actual_exit" > "$result_file"
    else
        echo "FAIL result $expected_exit $actual_exit" > "$result_file"
    fi
}

# テストケースを配列に読み込み
declare -a TEST_CODES
declare -a TEST_EXPECTED
test_num=0
while IFS= read -r line || [ -n "$line" ]; do
    [[ "$line" =~ ^[[:space:]]*# ]] && continue
    [[ -z "${line// }" ]] && continue

    expected_exit=$(echo "$line" | awk '{print $1}')
    test_code=$(echo "$line" | cut -d' ' -f2-)

    test_num=$((test_num + 1))
    TEST_EXPECTED[$test_num]="$expected_exit"
    TEST_CODES[$test_num]="$test_code"
done < "$CONF_FILE"

TOTAL_TESTS=$test_num

echo -e "${YELLOW}テスト実行中... ($TOTAL_TESTS テスト)${NC}"

# 並列実行（バックグラウンドジョブ）
running=0
for i in $(seq 1 $TOTAL_TESTS); do
    run_single_test "$i" "${TEST_EXPECTED[$i]}" "${TEST_CODES[$i]}" &
    running=$((running + 1))

    # 並列数制限
    if [ $running -ge $PARALLEL_JOBS ]; then
        wait -n 2>/dev/null || true
        running=$((running - 1))
    fi
done

# 残りのジョブを待機
wait

# 結果集計
PASSED_TESTS=0
FAILED_TESTS=0

echo ""
for i in $(seq 1 $TOTAL_TESTS); do
    result_file="$WORK_DIR/test_$i/result"
    test_code="${TEST_CODES[$i]}"

    if [ -f "$result_file" ]; then
        result=$(cat "$result_file")
        status=$(echo "$result" | awk '{print $1}')

        if [ "$status" = "PASS" ]; then
            actual=$(echo "$result" | awk '{print $2}')
            echo -e "Testing #$i: ${GREEN}$test_code => $actual${NC}"
            PASSED_TESTS=$((PASSED_TESTS + 1))
        else
            reason=$(echo "$result" | awk '{print $2}')
            case "$reason" in
                compile)
                    echo -e "Testing #$i: ${RED}✗ FAILED (compile error): $test_code${NC}"
                    ;;
                assemble)
                    echo -e "Testing #$i: ${RED}✗ FAILED (assemble error): $test_code${NC}"
                    ;;
                link)
                    echo -e "Testing #$i: ${RED}✗ FAILED (link error): $test_code${NC}"
                    ;;
                result)
                    expected=$(echo "$result" | awk '{print $3}')
                    actual=$(echo "$result" | awk '{print $4}')
                    echo -e "Testing #$i: ${RED}$test_code => $expected expected, but got $actual${NC}"
                    ;;
            esac
            FAILED_TESTS=$((FAILED_TESTS + 1))
        fi
    else
        echo -e "Testing #$i: ${RED}✗ FAILED (no result)${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
done

# 結果のサマリー
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}      テスト結果サマリー${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "総テスト数:   $TOTAL_TESTS"
echo -e "${GREEN}成功:         $PASSED_TESTS${NC}"
echo -e "${RED}失敗:         $FAILED_TESTS${NC}"
echo ""

if [ "$FAILED_TESTS" -eq 0 ]; then
    echo -e "${GREEN}すべてのテストが成功しました!${NC}"
    exit 0
else
    echo -e "${RED}一部のテストが失敗しました${NC}"
    exit 1
fi
