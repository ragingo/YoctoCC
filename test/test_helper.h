#ifndef TEST_HELPER_H
#define TEST_HELPER_H

// --- Syscall を使った低レベルヘルパー（-nostdlib 環境用）---
// ASSERT: 失敗しても続行し、結果を stderr に出力する
void ASSERT(int expected, int actual);
int printf(const char *fmt, ...);

#endif // TEST_HELPER_H
