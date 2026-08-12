#include "test_helper.h"

#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int _assert_num = 0;

void ASSERT(int expected, int actual) {
    _assert_num++;

    if (expected == actual) {
        fprintf(stderr, "%s", "ASSERT_RESULT PASS #");
        fprintf(stderr, "%d", _assert_num);
        fprintf(stderr, "%s", " expected ");
        fprintf(stderr, "%d", expected);
        fprintf(stderr, "%s", " actual ");
        fprintf(stderr, "%d", actual);
        fprintf(stderr, "%s", "\n");
        return;
    }

    fprintf(stderr, "%s", "ASSERT_RESULT FAIL #");
    fprintf(stderr, "%d", _assert_num);
    fprintf(stderr, "%s", " expected ");
    fprintf(stderr, "%d", expected);
    fprintf(stderr, "%s", " actual ");
    fprintf(stderr, "%d", actual);
    fprintf(stderr, "%s", "\n");
}

static int static_fn() { return 5; }
int ext1 = 5;
int *ext2 = &ext1;
