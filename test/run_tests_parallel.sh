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
CASES_DIR="$SCRIPT_DIR/cases"

# Makefile ターゲット
COMPILER="$PROJECT_ROOT/build/yoctocc"
TEST_HELPER_O="$PROJECT_ROOT/build/test_helper.o"

# アーキテクチャ検出とツール選択
# YoctoCC は x86-64 アセンブリを出力するため、arm64 では
# x86-64 クロスコンパイラ + QEMU が必要
UNAME_M=$(uname -m)
if [ "$UNAME_M" = "aarch64" ]; then
    X86_64_CC="x86_64-linux-gnu-gcc"
    QEMU_EXEC="qemu-x86_64"
    MAKE_X86_FLAG="X86_64_CC=$X86_64_CC"
else
    # x86-64 ではネイティブの gcc をそのまま使用
    X86_64_CC="gcc"
    QEMU_EXEC=""
    MAKE_X86_FLAG=""
fi

# 並列数（CPU コア数、nproc がなければ 4 にフォールバック）
PARALLEL_JOBS=${PARALLEL_JOBS:-$(nproc 2>/dev/null || echo 4)}

# テストフィルタ（コマンドライン引数）
TEST_FILTERS=("$@")

# 一時ディレクトリ
WORK_DIR=$(mktemp -d)

# シグナルハンドリング
cleanup() {
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
echo -e "アーキテクチャ: $UNAME_M"
echo -e "並列数: $PARALLEL_JOBS"
echo -e "タイムアウト: ${TEST_TIMEOUT}s"
if [ ${#TEST_FILTERS[@]} -gt 0 ]; then
    echo -e "フィルタ: ${TEST_FILTERS[*]}"
fi
echo ""

# コンパイラ本体のビルド
echo -e "${YELLOW}コンパイラをビルド中...${NC}"
if ! (cd "$PROJECT_ROOT" && make -s build/yoctocc > /dev/null 2>&1); then
    echo -e "${RED}コンパイラのビルドに失敗しました${NC}"
    exit 1
fi
echo -e "${GREEN}コンパイラ本体のビルドが完了しました${NC}"

# テストヘルパーのビルド
echo -e "${YELLOW}テストヘルパーをビルド中...${NC}"
if ! (cd "$PROJECT_ROOT" && make -s $MAKE_X86_FLAG build/test_helper.o > /dev/null 2>&1); then
    echo -e "${RED}テストヘルパーのビルドに失敗しました${NC}"
    exit 1
fi
echo -e "${GREEN}テストヘルパーのビルドが完了しました${NC}"
echo ""

# 単一テスト実行関数
run_single_test() {
    local test_num="$1"
    local expected_exit="$2"
    local test_file="$3"

    local test_work="$WORK_DIR/test_$test_num"
    mkdir -p "$test_work"

    local asm_file="$test_work/program.s"
    local obj_file="$test_work/program.o"
    local bin_file="$test_work/program"
    local result_file="$test_work/result"

    # コンパイル（YoctoCC で .c → .s）
    if ! "$COMPILER" "$test_file" "$asm_file" > "$test_work/compiler.log" 2>&1; then
        echo "FAIL compile" > "$result_file"
        cp "$test_work/compiler.log" "$test_work/error.log"
        return
    fi

    # アセンブル
    if ! "$X86_64_CC" -c -o "$obj_file" "$asm_file" > "$test_work/assemble.log" 2>&1; then
        echo "FAIL assemble" > "$result_file"
        cp "$test_work/assemble.log" "$test_work/error.log"
        return
    fi

    # リンク
    if ! "$X86_64_CC" -nostdlib -no-pie -o "$bin_file" "$obj_file" "$TEST_HELPER_O" > "$test_work/link.log" 2>&1; then
        echo "FAIL link" > "$result_file"
        cp "$test_work/link.log" "$test_work/error.log"
        return
    fi

    # 実行（arm64 では qemu-x86_64 経由、x86-64 では直接）
    if command -v timeout > /dev/null 2>&1; then
        timeout "$TEST_TIMEOUT" $QEMU_EXEC "$bin_file" > /dev/null 2>"$test_work/stderr.log"
        local actual_exit=$?
        if [ "$actual_exit" -eq 124 ]; then
            echo "FAIL timeout" > "$result_file"
            return
        fi
    else
        $QEMU_EXEC "$bin_file" > /dev/null 2>"$test_work/stderr.log"
        local actual_exit=$?
    fi

    if [ "$actual_exit" -eq "$expected_exit" ]; then
        echo "PASS $actual_exit" > "$result_file"
    else
        echo "FAIL result $expected_exit $actual_exit" > "$result_file"
    fi
}

# テストケースを収集
declare -a TEST_FILES TEST_EXPECTED TEST_NAMES
test_num=0

if [ -d "$CASES_DIR" ]; then
    while IFS= read -r test_file; do
        if ! grep -q 'ASSERT(' "$test_file"; then
            echo -e "${YELLOW}警告: $test_file に ASSERT() がありません。スキップします${NC}"
            continue
        fi

        # フィルタチェック
        if [ ${#TEST_FILTERS[@]} -gt 0 ]; then
            _matched=0
            for _filter in "${TEST_FILTERS[@]}"; do
                if [[ "$test_file" == *"$_filter"* ]]; then
                    _matched=1
                    break
                fi
            done
            [ $_matched -eq 0 ] && continue
        fi

        test_num=$((test_num + 1))
        TEST_FILES[$test_num]="$test_file"
        TEST_EXPECTED[$test_num]=0
        TEST_NAMES[$test_num]=$(echo "$test_file" | sed "s|^$CASES_DIR/||")
    done < <(find "$CASES_DIR" -name "*.c" -type f | sort)
fi

TOTAL_TESTS=$test_num

# ケース数集計
ESTIMATED_CASES=0
for i in $(seq 1 $TOTAL_TESTS); do
    _n=$(grep -v '^\s*//' "${TEST_FILES[$i]}" | grep 'ASSERT(' | grep -cv '^void ' || true)
    ESTIMATED_CASES=$((ESTIMATED_CASES + _n))
done

echo -e "${YELLOW}テスト実行中... ($ESTIMATED_CASES ケース / $TOTAL_TESTS ファイル)${NC}"

# 並列実行
running=0
for i in $(seq 1 $TOTAL_TESTS); do
    run_single_test "$i" "${TEST_EXPECTED[$i]}" "${TEST_FILES[$i]}" &
    running=$((running + 1))
    if [ $running -ge $PARALLEL_JOBS ]; then
        wait -n 2>/dev/null || true
        running=$((running - 1))
    fi
done
wait

# ASSERT 結果の解析と表示
show_assert_details() {
    local test_num="$1"
    local test_file="$2"
    local count_file="$3"
    local skip_reason="${4:-}"
    local stderr_log="$WORK_DIR/test_$test_num/stderr.log"
    local pass=0 fail=0 skip=0

    mapfile -t assert_exprs < <(grep 'ASSERT(' "$test_file" | grep -v '^\s*//' | grep -v '^#' | grep -v '^void ')
    local total=${#assert_exprs[@]}

    [ "$total" -eq 0 ] && { echo "0 0 0" > "$count_file"; return; }

    declare -A _ar_status _ar_detail
    if [ -f "$stderr_log" ] && [ -s "$stderr_log" ]; then
        while IFS= read -r sline; do
            if [[ "$sline" =~ ^ASSERT_RESULT\ (PASS|FAIL)\ #([0-9]+)\ expected\ (-?[0-9]+)\ actual\ (-?[0-9]+) ]]; then
                _ar_status[${BASH_REMATCH[2]}]="${BASH_REMATCH[1]}"
                _ar_detail[${BASH_REMATCH[2]}]="expected: ${BASH_REMATCH[3]}, actual: ${BASH_REMATCH[4]}"
            fi
        done < "$stderr_log"
    fi

    for idx in $(seq 1 "$total"); do
        _src="${assert_exprs[$((idx-1))]}"
        _expr=$(echo "$_src" | sed 's/^[[:space:]]*ASSERT([^,]*, //' | sed 's/);[[:space:]]*$//')
        _st="${_ar_status[$idx]:-SKIP}"
        case "$_st" in
            PASS) echo -e "    ${GREEN}#$idx $_expr => ${_ar_detail[$idx]}${NC}"; pass=$((pass + 1)) ;;
            FAIL) echo -e "    ${RED}#$idx $_expr => ${_ar_detail[$idx]}${NC}"; fail=$((fail + 1)) ;;
            SKIP)
                if [ -n "$skip_reason" ]; then
                    echo -e "    ${YELLOW}#$idx $_expr => (skipped: $skip_reason)${NC}"
                else
                    echo -e "    ${YELLOW}#$idx $_expr => (skipped)${NC}"
                fi
                skip=$((skip + 1)) ;;
        esac
    done
    echo "$pass $fail $skip" > "$count_file"
}

# 結果集計
TOTAL_CASES=0 PASSED_CASES=0 FAILED_CASES=0 SKIPPED_CASES=0 FAILED_FILES=0
echo ""

for i in $(seq 1 $TOTAL_TESTS); do
    result_file="$WORK_DIR/test_$i/result"
    test_name="${TEST_NAMES[$i]}"
    test_file="${TEST_FILES[$i]}"

    if [ ! -f "$result_file" ]; then
        echo -e "${RED}[$test_name] FAILED (no result)${NC}"
        _fc=$(grep -v '^\s*//' "$test_file" 2>/dev/null | grep 'ASSERT(' | grep -cv '^void ' || echo 0)
        [ "$_fc" -eq 0 ] && _fc=1
        FAILED_CASES=$((FAILED_CASES + _fc))
        TOTAL_CASES=$((TOTAL_CASES + _fc))
        FAILED_FILES=$((FAILED_FILES + 1))
        continue
    fi

    result=$(cat "$result_file")
    status=$(echo "$result" | awk '{print $1}')

    if [ "$status" = "PASS" ]; then
        local_count_file="$WORK_DIR/test_${i}_counts"
        local_detail_file="$WORK_DIR/test_${i}_detail"
        show_assert_details "$i" "$test_file" "$local_count_file" > "$local_detail_file"
        read -r p f s < "$local_count_file"
        PASSED_CASES=$((PASSED_CASES + p))
        FAILED_CASES=$((FAILED_CASES + f))
        SKIPPED_CASES=$((SKIPPED_CASES + s))
        TOTAL_CASES=$((TOTAL_CASES + p + f + s))
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
                [ -f "$error_log" ] && [ -s "$error_log" ] && echo -e "${YELLOW}  Error details:${NC}" && sed 's/^/    /' "$error_log"
                ;;
            timeout) echo -e "${RED}[$test_name] FAILED (timeout: ${TEST_TIMEOUT}s)${NC}" ;;
            result)
                _exp=$(echo "$result" | awk '{print $3}')
                _act=$(echo "$result" | awk '{print $4}')
                echo -e "${RED}[$test_name] FAILED (exit code: expected=$_exp, actual=$_act)${NC}"
                ;;
        esac

        _skip_reason=""
        if [ "$reason" = "result" ]; then
            _act=$(echo "$result" | awk '{print $4}')
            if [ "$_act" -gt 128 ] 2>/dev/null; then
                _sig=$((_act - 128))
                case $_sig in
                    4)  _signame="SIGILL" ;; 6)  _signame="SIGABRT" ;;
                    8)  _signame="SIGFPE" ;; 9)  _signame="SIGKILL" ;;
                    11) _signame="SIGSEGV" ;; 13) _signame="SIGPIPE" ;;
                    14) _signame="SIGALRM" ;; 15) _signame="SIGTERM" ;;
                    *)  _signame="signal $_sig" ;;
                esac
                _skip_reason="program crashed with $_signame"
            else
                _skip_reason="program exited with code $_act"
            fi
        elif [ "$reason" = "timeout" ]; then
            _skip_reason="program timed out"
        elif [ "$reason" = "compile" ] || [ "$reason" = "assemble" ] || [ "$reason" = "link" ]; then
            _skip_reason="$reason failed"
        fi
        local_count_file="$WORK_DIR/test_${i}_counts"
        show_assert_details "$i" "$test_file" "$local_count_file" "$_skip_reason"
        read -r p f s < "$local_count_file"
        PASSED_CASES=$((PASSED_CASES + p))
        FAILED_CASES=$((FAILED_CASES + f))
        SKIPPED_CASES=$((SKIPPED_CASES + s))
        TOTAL_CASES=$((TOTAL_CASES + p + f + s))
        FAILED_FILES=$((FAILED_FILES + 1))
    fi
done

# サマリー
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}      テスト結果サマリー${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "ファイル数:   $TOTAL_TESTS (失敗: $FAILED_FILES)"
echo -e "総ケース数:   $TOTAL_CASES"
echo -e "${GREEN}成功:         $PASSED_CASES${NC}"
[ "$FAILED_CASES" -gt 0 ] && echo -e "${RED}失敗:         $FAILED_CASES${NC}"
[ "$SKIPPED_CASES" -gt 0 ] && echo -e "${YELLOW}スキップ:     $SKIPPED_CASES${NC}"
echo ""

END_TIME=$(date +%s%N 2>/dev/null || date +%s)
if [ ${#END_TIME} -gt 10 ]; then
    ELAPSED=$(( (END_TIME - START_TIME) / 1000000 ))
    echo -e "経過時間:     $((ELAPSED / 1000)).$(printf '%03d' $((ELAPSED % 1000)))s"
else
    echo -e "経過時間:     $((END_TIME - START_TIME))s"
fi
echo ""

if [ "$FAILED_FILES" -eq 0 ]; then
    echo -e "${GREEN}すべてのテストが成功しました! ($PASSED_CASES/$TOTAL_CASES)${NC}"
    exit 0
else
    echo -e "${RED}一部のテストが失敗しました (ファイル: $FAILED_FILES/$TOTAL_TESTS, ケース: 成功 $PASSED_CASES / 失敗 $FAILED_CASES / スキップ $SKIPPED_CASES)${NC}"
    exit 1
fi