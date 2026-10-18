#ifndef RAYDOM_TEST_CHECK_H
#define RAYDOM_TEST_CHECK_H

#include <stdio.h>

static int rd_test_failures = 0;

#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
      rd_test_failures++;                                                    \
    }                                                                        \
  } while (0)

#define TEST_RESULT() (rd_test_failures ? (fprintf(stderr, "%d failure(s)\n", rd_test_failures), 1) : 0)

#endif
