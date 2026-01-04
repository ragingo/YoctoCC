# コンパイラ設定（環境変数で上書き可能）
CXX ?= g++
CC ?= gcc
MODE ?= debug
PROFILE ?= 0

# C++23 フラグ（Apple Clang は -std=c++2b が必要な場合あり）
CXX_STD ?= -std=c++23

ifeq ($(MODE), release)
	CXXFLAGS := $(CXX_STD) -O3 -DNDEBUG -Wall -Wextra -I./include -c
else
	CXXFLAGS := $(CXX_STD) -g -O0 -Wall -Wextra -I./include -c
endif

ifeq ($(PROFILE), 1)
	CXXFLAGS += -pg
	LDFLAGS := $(LDFLAGS) -pg
endif

INPUT ?=

BUILD_DIR := build
SRC_DIR := .
COMPILER := $(BUILD_DIR)/yoctocc
ASM := $(BUILD_DIR)/program.s
OBJ := $(BUILD_DIR)/program.o
BIN := $(BUILD_DIR)/program

# テスト用オブジェクト
TEST_HELPER_C := test/test_helper.c
TEST_HELPER_O := $(BUILD_DIR)/test_helper.o

# C++ ソースファイル (サブディレクトリも含む)
SRCS := $(shell find $(SRC_DIR) -name "*.cpp" -type f)
# ヘッダーファイル (コンパイル時の依存関係用)
HEADERS := $(wildcard include/*.hpp include/**/*.hpp)
# オブジェクトファイル
OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/%.o)
# 依存関係ファイル
DEPS := $(OBJS:.o=.d)

.PHONY: all clean run compile test help

all: $(COMPILER)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# オブジェクトファイルのコンパイル (自動依存関係生成)
$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -o $@ $<

# テスト用ヘルパーのコンパイル (C 言語、C23/C2X 対応)
$(TEST_HELPER_O): $(TEST_HELPER_C) | $(BUILD_DIR)
	$(CC) -std=c2x -O2 -c -o $@ $<

# コンパイラ実行ファイルのリンク
$(COMPILER): $(OBJS) | $(BUILD_DIR)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

$(ASM): $(COMPILER)
	@if [ -z "$(INPUT)" ]; then \
		echo "Error: INPUT variable is required. Use: make INPUT=filename.txt"; \
		exit 1; \
	fi
	@echo "Compiling $(INPUT) to assembly..."
	./$(COMPILER) $(INPUT)
	@echo "Generated assembly file: $(ASM)"

# GCC でアセンブル
$(OBJ): $(ASM)
	$(CC) -c -o $@ $<

# yoctocc が生成したコード + テストヘルパーをリンク（nostdlib で独自の _start を使用）
$(BIN): $(OBJ) $(TEST_HELPER_O)
	$(CC) -nostdlib -no-pie -o $@ $^

# 依存関係ファイルのインクルード
-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)

run: $(COMPILER)
	./$(COMPILER)

compile: $(ASM)

test:
	@echo "Running parallel test suite..."
	@bash test/run_tests_parallel.sh

test-serial:
	@echo "Running serial test suite..."
	@bash test/run_tests.sh

rebuild: clean all
	@echo "Rebuild complete"

profile: clean
	$(MAKE) PROFILE=1
	@rm -f gmon.out
	./$(COMPILER) test/test1.txt
	@gprof ./$(COMPILER) gmon.out | tee profile.log

help:
	@echo "Available targets:"
	@echo "  all         - Build the compiler (default)"
	@echo "  compile     - Compile input file to assembly (INPUT=filename.txt)"
	@echo "  run         - Run the compiler"
	@echo "  test        - Run test suite (parallel, default)"
	@echo "  test-serial - Run test suite (sequential)"
	@echo "  profile     - Profile compiler with gprof"
	@echo "  rebuild     - Clean and rebuild"
	@echo "  clean       - Remove build directory"
	@echo "  help        - Show this help message"
	@echo ""
	@echo "Build mode (default: debug):"
	@echo "  make MODE=debug       - Debug build (-g -O0)"
	@echo "  make MODE=release     - Release build (-O3 -DNDEBUG)"
	@echo ""
	@echo "Compiler selection (default: g++):"
	@echo "  make CXX=clang++      - Use Clang"
	@echo "  make CXX=g++          - Use GCC"
	@echo ""
	@echo "C++ standard (default: -std=c++23):"
	@echo "  make CXX_STD=-std=c++2b  - For older Clang versions"

