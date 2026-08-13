# ==================================================
#  YoctoCC コンパイラ成果物（アセンブル・リンク）
# ==================================================

ifndef _OUTPUT_MK
_OUTPUT_MK := 1

include mk/compiler.mk

INPUT ?=

ASM := $(BUILD_DIR)/program.s
OBJ := $(BUILD_DIR)/program.o
BIN := $(BUILD_DIR)/program

# テスト用ヘルパー
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
	$(X86_64_CC) -g -c -o $@ $<

# テスト用ヘルパーのコンパイル
$(TEST_HELPER_O): $(TEST_HELPER_C) | $(BUILD_DIR)
	$(X86_64_CC) -g -std=c23 -O2 -fno-builtin -fno-stack-protector -c -o $@ $<

# yoctocc が生成したコード + テストヘルパーをリンク
$(BIN): $(OBJ) $(TEST_HELPER_O)
	$(X86_64_CC) -no-pie -o $@ $^

endif