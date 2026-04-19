# ==================================================
#  YoctoCC - メイン Makefile
# ==================================================
#
#  mk/common.mk    共通設定（コンパイラ, フラグ, ディレクトリ）
#  mk/compiler.mk  YoctoCC コンパイラ本体のビルド
#  mk/output.mk    コンパイラ成果物のアセンブル・リンク
#  mk/test.mk      テスト
# ==================================================

.DEFAULT_GOAL := all

include mk/output.mk
include mk/test.mk

.PHONY: all clean run compile execute debug test rebuild profile help format format-check lint lint-fix lint-report

# --- デフォルトターゲット ---
all: $(COMPILER)

# --- ソースをアセンブリにコンパイル ---
compile: $(ASM)

# --- コンパイラを実行 ---
# 使用例: make run ARGS="input.c output.s"
run: $(COMPILER)
	./$(COMPILER) $(ARGS)

# --- コンパイル → アセンブル → リンク → 実行 ---
# 使用例: make execute INPUT=test/cases/arith.c
execute: $(BIN)
	./$(BIN)

# --- コンパイル → アセンブル → リンク → GDB でデバッグ ---
# 使用例: make debug INPUT=test/cases/debug.c
debug: $(BIN)
	gdb -q ./$(BIN)

# --- クリーン ---
clean:
	rm -rf $(BUILD_DIR)

# --- リビルド ---
rebuild: clean all
	@echo "Rebuild complete"

# --- プロファイル ---
# 使用例: cat test.c | make profile
profile: clean
	$(MAKE) PROFILE=1 all
	@rm -f gmon.out
	./$(COMPILER) /dev/stdin /dev/null
	@gprof ./$(COMPILER) gmon.out | tee profile.log

# --- ヘルプ ---
help:
	@echo "Available targets:"
	@echo "  all         - Build the compiler (default)"
	@echo "  compile     - Compile input file to assembly (INPUT=filename.c)"
	@echo "  run         - Run the compiler"
	@echo "  execute     - Compile, assemble, link, and run (INPUT=filename.c)"
	@echo "  debug       - Compile, assemble, link, and debug with GDB (INPUT=filename.c)"
	@echo "  test        - Run test suite"
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
	@echo "C++ standard (default: -std=c++26):"
	@echo "  make CXX_STD=-std=c++23  - For older compiler versions"
	@echo ""
	@echo "Formatting:"
	@echo "  make format           - Format all source files with clang-format"
	@echo "  make format-check     - Check formatting (dry-run)"
	@echo ""
	@echo "Static analysis:"
	@echo "  make lint             - Run clang-tidy on all source files"
	@echo "  make lint-fix         - Run clang-tidy with auto-fix"
	@echo "  make lint-report      - Run clang-tidy and export YAML report"

# --- フォーマット ---
FORMAT_FILES := $(shell find src include -name '*.cpp' -o -name '*.hpp') main.cpp

format:
	clang-format -i $(FORMAT_FILES)
	@echo "Formatted $$(echo $(FORMAT_FILES) | wc -w) files"

format-check:
	clang-format --dry-run --Werror $(FORMAT_FILES)

# --- 静的解析 ---
CLANG_TIDY ?= clang-tidy-22
LINT_FILES := $(shell find src -name '*.cpp') main.cpp

lint:
	$(CLANG_TIDY) $(LINT_FILES) -- $(CXX_STD) -I./include

lint-fix:
	$(CLANG_TIDY) -fix $(LINT_FILES) -- $(CXX_STD) -I./include

LINT_REPORT := $(BUILD_DIR)/lint-report.yaml

lint-report: | $(BUILD_DIR)
	$(CLANG_TIDY) --export-fixes=$(LINT_REPORT) $(LINT_FILES) -- $(CXX_STD) -I./include; \
	echo "Report saved to $(LINT_REPORT)"

