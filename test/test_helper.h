#ifndef TEST_HELPER_H
#define TEST_HELPER_H

// --- Syscall を使った低レベルヘルパー（-nostdlib 環境用）---
void ASSERT(int expected, int actual);
int printf(const char *fmt, ...);

#endif // TEST_HELPER_H
