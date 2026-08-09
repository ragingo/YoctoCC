#ifndef TEST_HELPER_H
#define TEST_HELPER_H

// --- Syscall を使った低レベルヘルパー（-nostdlib 環境用）---
// ASSERT: 失敗しても続行し、結果を stderr に出力する
void ASSERT(int expected, int actual);
int printf(const char *fmt, ...);
int memcmp(const void *s1, const void *s2, unsigned long n);
int strcmp(const char *s1, const char *s2);

static int static_fn() { return 5; }

#endif // TEST_HELPER_H
