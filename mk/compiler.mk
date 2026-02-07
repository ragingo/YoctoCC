# ==================================================
#  YoctoCC コンパイラ本体のビルド
# ==================================================

ifndef _COMPILER_MK
_COMPILER_MK := 1

include mk/common.mk

# C++ ソースファイル（サブディレクトリも含む）
SRCS := $(shell find $(SRC_DIR) -name "*.cpp" -type f)
OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

# コンパイラ実行ファイル
COMPILER := $(BUILD_DIR)/yoctocc

# オブジェクトファイルのコンパイル（自動依存関係生成）
$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -o $@ $<

# コンパイラ実行ファイルのリンク
$(COMPILER): $(OBJS) | $(BUILD_DIR)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

# 依存関係ファイルのインクルード
-include $(DEPS)

endif # _COMPILER_MK
