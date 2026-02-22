#include "test_helper.h"

#include <stdarg.h>

// ============================================================
//  syscall ヘルパー（-nostdlib 環境用）
// ============================================================

static long sys_write(int fd, const void *buf, unsigned long count) {
    long ret;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(1 /* SYS_write */), "D"(fd), "S"(buf), "d"(count)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static _Noreturn void sys_exit(int code) {
    __asm__ volatile(
        "syscall"
        :
        : "a"(60 /* SYS_exit */), "D"(code)
        : "rcx", "r11", "memory"
    );
    __builtin_unreachable();
}

// ============================================================
//  文字列ヘルパー
// ============================================================

static unsigned long my_strlen(const char *s) {
    unsigned long len = 0;
    while (s[len]) len++;
    return len;
}

static void write_str(int fd, const char *s) {
    sys_write(fd, s, my_strlen(s));
}

static void write_int(int fd, int n) {
    char buf[20];
    int i = 0;

    if (n < 0) {
        sys_write(fd, "-", 1);
        // INT_MIN 対策: unsigned に変換
        unsigned int u = (unsigned int)(-(n + 1)) + 1u;
        if (u == 0) {
            sys_write(fd, "0", 1);
            return;
        }
        while (u > 0) {
            buf[i++] = '0' + (char)(u % 10);
            u /= 10;
        }
    } else if (n == 0) {
        sys_write(fd, "0", 1);
        return;
    } else {
        while (n > 0) {
            buf[i++] = '0' + (char)(n % 10);
            n /= 10;
        }
    }

    // 逆順に出力
    for (int j = i - 1; j >= 0; j--) {
        sys_write(fd, &buf[j], 1);
    }
}

// ============================================================
//  ASSERT: 期待値と実際の値を比較し、不一致なら即終了
// ============================================================

static int _assert_num = 0;

void ASSERT(int expected, int actual) {
    _assert_num++;

    if (expected == actual) {
        write_str(2, "ASSERT_RESULT PASS #");
        write_int(2, _assert_num);
        write_str(2, " expected ");
        write_int(2, expected);
        write_str(2, " actual ");
        write_int(2, actual);
        write_str(2, "\n");
        return;
    }

    write_str(2, "ASSERT_RESULT FAIL #");
    write_int(2, _assert_num);
    write_str(2, " expected ");
    write_int(2, expected);
    write_str(2, " actual ");
    write_int(2, actual);
    write_str(2, "\n");

    sys_exit(1);
}

// ============================================================
//  最小限の printf 実装（フォーマット文字列をそのまま出力）
//  ※ %d 等のフォーマット指定子は未サポート
// ============================================================

int printf(const char *fmt, ...) {
    (void)fmt;  // va_args 未使用の警告を抑制するためのダミー
    // va_list を使わず、最低限の文字列出力のみ対応
    write_str(1, fmt);
    return 0;
}
