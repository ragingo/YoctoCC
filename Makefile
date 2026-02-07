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

.PHONY: all clean run compile test rebuild profile help

# --- デフォルトターゲット ---
all: $(COMPILER)

# --- ソースをアセンブリにコンパイル ---
compile: $(ASM)

# --- コンパイラを実行 ---
# 使用例: make run ARGS="input.c output.s"
run: $(COMPILER)
	./$(COMPILER) $(ARGS)

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
	@echo "C++ standard (default: -std=c++23):"
	@echo "  make CXX_STD=-std=c++2b  - For older Clang versions"

