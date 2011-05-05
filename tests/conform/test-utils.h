#ifndef _TEST_UTILS_H_
#define _TEST_UTILS_H_

/* This fixture structure is allocated by glib, and before running
 * each test we get a callback to initialize it.
 *
 * Actually we don't use this currently, we instead manage our own
 * TestUtilsSharedState structure which also gets passed as a private
 * data argument to the same initialization callback. The advantage of
 * allocating our own shared state structure is that we can put data
 * in it before we start running anything.
 */
typedef struct _TestUtilsGTestFixture
{
  /**/
  int dummy;
} TestUtilsGTestFixture;

/* Stuff you put in here is setup once in main() and gets passed around to
 * all test functions and fixture setup/teardown functions in the data
 * argument */
typedef struct _TestUtilsSharedState
{
  int    *argc_addr;
  char ***argv_addr;

  void (* todo_func) (TestUtilsGTestFixture *, void *data);

  CoglContext *ctx;
  CoglFramebuffer *fb;
} TestUtilsSharedState;

void
test_utils_init (TestUtilsGTestFixture *fixture,
                 const void *data);

void
test_utils_fini (TestUtilsGTestFixture *fixture,
                 const void *data);

#endif /* _TEST_UTILS_H_ */
