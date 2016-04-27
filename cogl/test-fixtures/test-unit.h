#ifndef _TEST_UNIT_H_
#define _TEST_UNIT_H_

#include <test-fixtures/test-utils.h>

#ifdef ENABLE_UNIT_TESTS

typedef struct _CoglUnitTest
{
  const char *name;
  TestFlags requirement_flags;
  TestFlags known_failure_flags;
  void (*run) (void);
} CoglUnitTest;

#define UNIT_TEST(NAME, REQUIREMENT_FLAGS, KNOWN_FAILURE_FLAGS) \
  static void NAME (void); \
  \
  const CoglUnitTest unit_test_##NAME = \
  { #NAME, REQUIREMENT_FLAGS, KNOWN_FAILURE_FLAGS, NAME }; \
  \
  static void NAME (void)

#else /* ENABLE_UNIT_TESTS */

#define UNIT_TEST(NAME, REQUIREMENT_FLAGS, KNOWN_FAILURE_FLAGS) \
  static inline void NAME (void)

#endif /* ENABLE_UNIT_TESTS */

#endif /* _TEST_UNIT_H_ */
