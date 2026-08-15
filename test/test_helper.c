#include "test_helper.h"

#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int _assert_num = 0;

void ASSERT(int expected, int actual) {
    _assert_num++;
    fprintf(stderr, "ASSERT_RESULT %s #%d expected %d actual %d\n",
            expected == actual ? "PASS" : "FAIL", _assert_num, expected, actual);
}

static int static_fn() { return 5; }
int ext1 = 5;
int *ext2 = &ext1;
int ext3 = 7;
int ext_fn1(int x) { return x; }
int ext_fn2(int x) { return x; }

int false_fn() { return 512; }
int true_fn() { return 513; }
int char_fn() { return (2<<8)+3; }
int short_fn() { return (2<<16)+5; }
