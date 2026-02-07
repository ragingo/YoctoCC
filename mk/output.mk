# ==================================================
#  YoctoCC コンパイラ成果物（アセンブル・リンク）
# ==================================================

include mk/compiler.mk

INPUT ?=

ASM := $(BUILD_DIR)/program.s
OBJ := $(BUILD_DIR)/program.o
BIN := $(BUILD_DIR)/program

# テスト用ヘルパー（_start 提供）
TEST_HELPER_C := test/test_helper.c
TEST_HELPER_O := $(BUILD_DIR)/test_helper.o

# YoctoCC でソースをコンパイル → アセンブリ生成
$(ASM): $(COMPILER)
	@if [ -z "$(INPUT)" ]; then \
		echo "Error: INPUT variable is required. Use: make compile INPUT=filename.c"; \
		exit 1; \
	fi
	@echo "Compiling $(INPUT) to assembly..."
	./$(COMPILER) $(INPUT)
	@echo "Generated assembly file: $(ASM)"

# アセンブル（.s → .o）
$(OBJ): $(ASM)
	$(CC) -c -o $@ $<

# テスト用ヘルパーのコンパイル（C23/C2X 対応）
$(TEST_HELPER_O): $(TEST_HELPER_C) | $(BUILD_DIR)
	$(CC) -std=c2x -O2 -c -o $@ $<

# yoctocc が生成したコード + テストヘルパーをリンク（nostdlib で独自の _start を使用）
$(BIN): $(OBJ) $(TEST_HELPER_O)
	$(CC) -nostdlib -no-pie -o $@ $^
