# ==================================================
#  共通設定 (コンパイラ, フラグ, ディレクトリ)
# ==================================================

ifndef _COMMON_MK
_COMMON_MK := 1

# コンパイラ設定（環境変数で上書き可能）
CXX      ?= g++
CC       ?= gcc
MODE     ?= debug
PROFILE  ?= 0

# C++23 フラグ（Apple Clang は -std=c++2b が必要な場合あり）
CXX_STD  ?= -std=c++23

# ディレクトリ
BUILD_DIR := build
SRC_DIR   := .

# ビルドモード別フラグ
ifeq ($(MODE), release)
    CXXFLAGS := $(CXX_STD) -O3 -DNDEBUG -Wall -Wextra -I./include -c
else
    CXXFLAGS := $(CXX_STD) -g -O0 -Wall -Wextra -I./include -c
endif

# プロファイル
ifeq ($(PROFILE), 1)
    CXXFLAGS += -pg
    LDFLAGS  += -pg
endif

# ビルドディレクトリ作成
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

endif # _COMMON_MK
