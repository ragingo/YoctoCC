#!/bin/bash

# 並列テストフレームワーク
# バックグラウンドジョブを使用して高速化
#
# 使い方:
#   bash run_tests_parallel.sh              # 全テスト実行
#   bash run_tests_parallel.sh arith        # ファイル名に "arith" を含むテストのみ
#   bash run_tests_parallel.sh arith pointer # 複数フィルタ (OR)

set -o pipefail

# テスト実行タイムアウト（秒）
TEST_TIMEOUT=${TEST_TIMEOUT:-10}

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
CASES_DIR="$TEST_DIR/cases"

# Makefile ターゲット用（相対パス）
COMPILER_TARGET="build/yoctocc"
TEST_HELPER_TARGET="build/test_helper.o"

# 実行用（絶対パス）
COMPILER="$PROJECT_ROOT/$COMPILER_TARGET"
TEST_HELPER_O="$PROJECT_ROOT/$TEST_HELPER_TARGET"

# テスト用 C コンパイラ（環境変数 CC で上書き可能）
TEST_CC=${CC:-gcc}

# 並列数（CPU コア数、nproc がなければ 4 にフォールバック）
PARALLEL_JOBS=${PARALLEL_JOBS:-$(nproc 2>/dev/null || echo 4)}

# テストフィルタ（コマンドライン引数）
TEST_FILTERS=("$@")

# 一時ディレクトリ
WORK_DIR=$(mktemp -d)

# シグナルハンドリング: 割り込み時にバックグラウンドジョブを停止してから後処理
cleanup() {
    # 子プロセスをすべて停止
    local pids
    pids=$(jobs -p 2>/dev/null)
    if [ -n "$pids" ]; then
        kill $pids 2>/dev/null
        wait $pids 2>/dev/null
    fi
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT
trap 'echo -e "\n${RED}中断されました${NC}"; exit 130' INT TERM

# 計測開始
START_TIME=$(date +%s%N 2>/dev/null || date +%s)

# ヘッダー表示
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}      Yoctocc テストスイート (並列)${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "Cコンパイラ: $TEST_CC"
echo -e "並列数: $PARALLEL_JOBS"
echo -e "タイムアウト: ${TEST_TIMEOUT}s"
if [ ${#TEST_FILTERS[@]} -gt 0 ]; then
    echo -e "フィルタ: ${TEST_FILTERS[*]}"
fi
echo ""

# コンパイラのビルド（インクリメンタルビルド、サブシェルで cd を隔離）
echo -e "${YELLOW}コンパイラをビルド中...${NC}"
if ! (cd "$PROJECT_ROOT" && make -s CC="$TEST_CC" "$COMPILER_TARGET" "$TEST_HELPER_TARGET" > /dev/null 2>&1); then
    echo -e "${RED}コンパイラのビルドに失敗しました${NC}"
    exit 1
fi
echo -e "${GREEN}コンパイラのビルドが完了しました${NC}"
echo ""

# 単一テスト実行関数（ファイルベース）
run_single_test() {
    local test_num="$1"
    local expected_exit="$2"
    local test_file="$3"
    local test_name="$4"

    local test_work="$WORK_DIR/test_$test_num"
    mkdir -p "$test_work"

    local asm_file="$test_work/program.s"
    local obj_file="$test_work/program.o"
    local bin_file="$test_work/program"
    local result_file="$test_work/result"

    # コンパイラ実行（出力先を直接指定）
    if ! "$COMPILER" "$test_file" "$asm_file" > "$test_work/compiler.log" 2>&1; then
        echo "FAIL compile" > "$result_file"
        cp "$test_work/compiler.log" "$test_work/error.log"
        return
    fi

    # アセンブル
    if ! "$TEST_CC" -c -o "$obj_file" "$asm_file" > "$test_work/assemble.log" 2>&1; then
        echo "FAIL assemble" > "$result_file"
        cp "$test_work/assemble.log" "$test_work/error.log"
        return
    fi

    # リンク
    if ! "$TEST_CC" -nostdlib -no-pie -o "$bin_file" "$obj_file" "$TEST_HELPER_O" > "$test_work/link.log" 2>&1; then
        echo "FAIL link" > "$result_file"
        cp "$test_work/link.log" "$test_work/error.log"
        return
    fi

    # 実行（stderr をキャプチャ、タイムアウト付き）
    if command -v timeout > /dev/null 2>&1; then
        timeout "$TEST_TIMEOUT" "$bin_file" > /dev/null 2>"$test_work/stderr.log"
        local actual_exit=$?
        if [ "$actual_exit" -eq 124 ]; then
            echo "FAIL timeout" > "$result_file"
            return
        fi
    else
        "$bin_file" > /dev/null 2>"$test_work/stderr.log"
        local actual_exit=$?
    fi

    if [ "$actual_exit" -eq "$expected_exit" ]; then
        echo "PASS $actual_exit" > "$result_file"
    else
        echo "FAIL result $expected_exit $actual_exit" > "$result_file"
    fi
}

# テストケースを配列に読み込み
declare -a TEST_FILES
declare -a TEST_EXPECTED
declare -a TEST_NAMES
test_num=0

# casesディレクトリから.cファイルを収集（ソート済み）
if [ -d "$CASES_DIR" ]; then
    while IFS= read -r test_file; do
        # ASSERT マクロを使用しているファイルのみ対象（expected=0: 成功時 return 0）
        if ! grep -q 'ASSERT(' "$test_file"; then
            echo -e "${YELLOW}警告: $test_file に ASSERT() がありません。スキップします${NC}"
            continue
        fi
        expected_exit=0

        # フィルタチェック
        if [ ${#TEST_FILTERS[@]} -gt 0 ]; then
            _matched=0
            for _filter in "${TEST_FILTERS[@]}"; do
                if [[ "$test_file" == *"$_filter"* ]]; then
                    _matched=1
                    break
                fi
            done
            if [ $_matched -eq 0 ]; then
                continue
            fi
        fi

        test_num=$((test_num + 1))
        TEST_FILES[$test_num]="$test_file"
        TEST_EXPECTED[$test_num]="$expected_exit"
        # 相対パス名をテスト名として使用
        TEST_NAMES[$test_num]=$(echo "$test_file" | sed "s|^$CASES_DIR/||")
    done < <(find "$CASES_DIR" -name "*.c" -type f | sort)
fi



TOTAL_TESTS=$test_num

# 実行前にテストケース総数を集計
ESTIMATED_CASES=0
for i in $(seq 1 $TOTAL_TESTS); do
    _n=$(grep -v '^\s*//' "${TEST_FILES[$i]}" | grep -c 'ASSERT(' || true)
    ESTIMATED_CASES=$((ESTIMATED_CASES + _n))
done

echo -e "${YELLOW}テスト実行中... ($ESTIMATED_CASES ケース / $TOTAL_TESTS ファイル)${NC}"

# 並列実行（バックグラウンドジョブ）
running=0
for i in $(seq 1 $TOTAL_TESTS); do
    run_single_test "$i" "${TEST_EXPECTED[$i]}" "${TEST_FILES[$i]}" "${TEST_NAMES[$i]}" &
    running=$((running + 1))

    # 並列数制限
    if [ $running -ge $PARALLEL_JOBS ]; then
        wait -n 2>/dev/null || true
        running=$((running - 1))
    fi
done

# 残りのジョブを待機
wait

# ASSERT ケースの詳細を解析・表示する関数
# 引数: テスト番号, テストファイルパス, カウント書き込み先ファイル
show_assert_details() {
    local test_num="$1"
    local test_file="$2"
    local count_file="$3"
    local stderr_log="$WORK_DIR/test_$test_num/stderr.log"
    local pass=0 fail=0 skip=0

    # ソースから ASSERT() 呼び出し行を抽出
    mapfile -t assert_exprs < <(grep 'ASSERT(' "$test_file" | grep -v '^\s*//' | grep -v '^#')
    local total=${#assert_exprs[@]}

    if [ "$total" -eq 0 ]; then
        echo "0 0 0" > "$count_file"
        return
    fi

    # stderr から ASSERT_RESULT を解析
    local -A _ar_status
    local -A _ar_detail
    if [ -f "$stderr_log" ] && [ -s "$stderr_log" ]; then
        while IFS= read -r sline; do
            if [[ "$sline" =~ ^ASSERT_RESULT\ (PASS|FAIL)\ #([0-9]+)\ expected\ (-?[0-9]+)\ actual\ (-?[0-9]+) ]]; then
                _ar_status[${BASH_REMATCH[2]}]="${BASH_REMATCH[1]}"
                _ar_detail[${BASH_REMATCH[2]}]="expected: ${BASH_REMATCH[3]}, actual: ${BASH_REMATCH[4]}"
            fi
        done < "$stderr_log"
    fi

    # 各 ASSERT を表示
    for idx in $(seq 1 "$total"); do
        _src="${assert_exprs[$((idx-1))]}"
        _expr=$(echo "$_src" | sed 's/^[[:space:]]*ASSERT([^,]*, //' | sed 's/);[[:space:]]*$//')

        _st="${_ar_status[$idx]:-SKIP}"
        case "$_st" in
            PASS)
                echo -e "    ${GREEN}#$idx $_expr => ${_ar_detail[$idx]}${NC}"
                pass=$((pass + 1))
                ;;
            FAIL)
                echo -e "    ${RED}#$idx $_expr => ${_ar_detail[$idx]}${NC}"
                fail=$((fail + 1))
                ;;
            SKIP)
                echo -e "    ${YELLOW}#$idx $_expr => (skipped)${NC}"
                skip=$((skip + 1))
                ;;
        esac
    done

    echo "$pass $fail $skip" > "$count_file"
}

# 結果集計
TOTAL_CASES=0
PASSED_CASES=0
FAILED_CASES=0
SKIPPED_CASES=0
FAILED_FILES=0

echo ""
for i in $(seq 1 $TOTAL_TESTS); do
    result_file="$WORK_DIR/test_$i/result"
    test_name="${TEST_NAMES[$i]}"
    test_file="${TEST_FILES[$i]}"
    expected="${TEST_EXPECTED[$i]}"

    if [ ! -f "$result_file" ]; then
        echo -e "${RED}[$test_name] FAILED (no result)${NC}"
        # ソース中の ASSERT 数を失敗としてカウント（コメント行を除外）
        _fc=$(grep -v '^\s*//' "$test_file" 2>/dev/null | grep -c 'ASSERT(' || echo 0)
        [ "$_fc" -eq 0 ] && _fc=1
        FAILED_CASES=$(( FAILED_CASES + _fc ))
        TOTAL_CASES=$(( TOTAL_CASES + _fc ))
        FAILED_FILES=$((FAILED_FILES + 1))
        continue
    fi

    result=$(cat "$result_file")
    status=$(echo "$result" | awk '{print $1}')

    if [ "$status" = "PASS" ]; then
        # ケース詳細を取得（出力はバッファリング）
        local_count_file="$WORK_DIR/test_${i}_counts"
        local_detail_file="$WORK_DIR/test_${i}_detail"
        show_assert_details "$i" "$test_file" "$local_count_file" > "$local_detail_file"
        read -r p f s < "$local_count_file"
        PASSED_CASES=$((PASSED_CASES + p))
        FAILED_CASES=$((FAILED_CASES + f))
        SKIPPED_CASES=$((SKIPPED_CASES + s))
        TOTAL_CASES=$((TOTAL_CASES + p + f + s))
        # ファイルヘッダー → 詳細の順で表示
        if [ "$f" -gt 0 ]; then
            echo -e "${RED}[$test_name] FAILED${NC}"
            FAILED_FILES=$((FAILED_FILES + 1))
        else
            echo -e "${GREEN}[$test_name]${NC}"
        fi
        cat "$local_detail_file"
    else
        reason=$(echo "$result" | awk '{print $2}')
        error_log="$WORK_DIR/test_$i/error.log"
        case "$reason" in
            compile|assemble|link)
                echo -e "${RED}[$test_name] FAILED ($reason error)${NC}"
                if [ -f "$error_log" ] && [ -s "$error_log" ]; then
                    echo -e "${YELLOW}  Error details:${NC}"
                    sed 's/^/    /' "$error_log"
                fi
                ;;
            timeout)
                echo -e "${RED}[$test_name] FAILED (timeout: ${TEST_TIMEOUT}s)${NC}"
                ;;
            result)
                local _exp _act
                _exp=$(echo "$result" | awk '{print $3}')
                _act=$(echo "$result" | awk '{print $4}')
                echo -e "${RED}[$test_name] FAILED (exit code: expected=$_exp, actual=$_act)${NC}"
                ;;
        esac

        # 失敗時もケース詳細を表示
        local_count_file="$WORK_DIR/test_${i}_counts"
        show_assert_details "$i" "$test_file" "$local_count_file"
        read -r p f s < "$local_count_file"
        PASSED_CASES=$((PASSED_CASES + p))
        FAILED_CASES=$((FAILED_CASES + f))
        SKIPPED_CASES=$((SKIPPED_CASES + s))
        TOTAL_CASES=$((TOTAL_CASES + p + f + s))
        FAILED_FILES=$((FAILED_FILES + 1))
    fi
done

# 結果のサマリー
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}      テスト結果サマリー${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "ファイル数:   $TOTAL_TESTS (失敗: $FAILED_FILES)"
echo -e "総ケース数:   $TOTAL_CASES"
echo -e "${GREEN}成功:         $PASSED_CASES${NC}"
if [ "$FAILED_CASES" -gt 0 ]; then
    echo -e "${RED}失敗:         $FAILED_CASES${NC}"
fi
if [ "$SKIPPED_CASES" -gt 0 ]; then
    echo -e "${YELLOW}スキップ:     $SKIPPED_CASES${NC}"
fi
echo ""

# 経過時間を計算
END_TIME=$(date +%s%N 2>/dev/null || date +%s)
if [ ${#END_TIME} -gt 10 ]; then
    # ナノ秒精度
    ELAPSED=$(( (END_TIME - START_TIME) / 1000000 ))
    ELAPSED_SEC=$((ELAPSED / 1000))
    ELAPSED_MS=$((ELAPSED % 1000))
    echo -e "経過時間:     ${ELAPSED_SEC}.$(printf '%03d' $ELAPSED_MS)s"
else
    # 秒精度フォールバック
    ELAPSED=$((END_TIME - START_TIME))
    echo -e "経過時間:     ${ELAPSED}s"
fi
echo ""

if [ "$FAILED_FILES" -eq 0 ]; then
    echo -e "${GREEN}すべてのテストが成功しました! ($PASSED_CASES/$TOTAL_CASES)${NC}"
    exit 0
else
    echo -e "${RED}一部のテストが失敗しました (ファイル: $FAILED_FILES/$TOTAL_TESTS, ケース: 成功 $PASSED_CASES / 失敗 $FAILED_CASES / スキップ $SKIPPED_CASES)${NC}"
    exit 1
fi
