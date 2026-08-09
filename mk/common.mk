# ==================================================
#  YoctoCC - 共通設定（コンパイラ, フラグ, ディレクトリ）
# ==================================================

ifndef _COMMON_MK
_COMMON_MK := 1

# --- ビルドディレクトリ ---
BUILD_DIR ?= build

# コンパイラ設定（環境変数で上書き可能）
CXX      ?= g++
CC       ?= gcc
MODE     ?= debug
PROFILE  ?= 0

# C++26 フラグ
CXX_STD ?= -std=c++26

# アーキテクチャ検出
UNAME_M := $(shell uname -m 2>/dev/null || echo unknown)

# --- arm64 上で x86-64 バイナリを生成するためのクロスコンパイル設定 ---
# YoctoCC は x86-64 アセンブリを出力するので、アセンブル・リンクは
# x86-64 クロスツールチェインを使う必要がある
ifeq ($(UNAME_M),aarch64)
    X86_64_CC  ?= x86_64-linux-gnu-gcc
    QEMU_X86_64 ?= $(shell command -v qemu-x86_64 2>/dev/null || echo "")
    QEMU_WRAPPER = $(QEMU_X86_64)
else
    X86_64_CC  ?= $(CC)
    QEMU_X86_64 :=
    QEMU_WRAPPER :=
endif

# Clang 用の libc++ フラグ（-stdlib=libc++）
ifneq (,$(filter clang%,$(CXX)))
    CXXFLAGS += -stdlib=libc++
endif

# ビルドモード別フラグ
ifeq ($(MODE), release)
    CXXFLAGS := $(CXX_STD) -O3 -DNDEBUG -Wall -Wextra -I./include -c
else ifneq (,$(filter clang%,$(CXX)))
    CXXFLAGS := $(CXX_STD) $(CLANG_LIBCXX_FLAGS) -g -O0 -Wall -Wextra -I./include -c
else
    CXXFLAGS := $(CXX_STD) -g -O0 -Wall -Wextra -I./include -c -fcontracts -fcontract-evaluation-semantic=observe
    LDFLAGS  := -fcontracts
endif

# プロファイル
ifeq ($(PROFILE), 1)
    CXXFLAGS += -pg
    LDFLAGS  += -pg
endif

# --- ビルドディレクトリ作成 ---
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

endif # _COMMON_MK