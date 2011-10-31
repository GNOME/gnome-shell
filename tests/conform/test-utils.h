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

/*
 * test_utils_check_pixel:
 * @x: x co-ordinate of the pixel to test
 * @y: y co-ordinate of the pixel to test
 * @pixel: An integer of the form 0xRRGGBBAA representing the expected
 *         pixel value
 *
 * This performs reads a pixel on the current cogl framebuffer and
 * asserts that it matches the given color. The alpha channel of the
 * color is ignored. The pixels are converted to a string and compared
 * with g_assert_cmpstr so that if the comparison fails then the
 * assert will display a meaningful message
 */
void
test_utils_check_pixel (int x, int y, guint32 expected_pixel);

/*
 * test_utils_check_pixel:
 * @x: x co-ordinate of the pixel to test
 * @y: y co-ordinate of the pixel to test
 * @pixel: An integer of the form 0xrrggbb representing the expected pixel value
 *
 * This performs reads a pixel on the current cogl framebuffer and
 * asserts that it matches the given color. The alpha channel of the
 * color is ignored. The pixels are converted to a string and compared
 * with g_assert_cmpstr so that if the comparison fails then the
 * assert will display a meaningful message
 */
void
test_utils_check_pixel_rgb (int x, int y, int r, int g, int b);

/*
 * test_utils_check_region:
 * @x: x co-ordinate of the region to test
 * @y: y co-ordinate of the region to test
 * @width: width of the region to test
 * @height: height of the region to test
 * @pixel: An integer of the form 0xrrggbb representing the expected region color
 *
 * Performs a read pixel on the specified region of the current cogl
 * framebuffer and asserts that it matches the given color. The alpha
 * channel of the color is ignored. The pixels are converted to a
 * string and compared with g_assert_cmpstr so that if the comparison
 * fails then the assert will display a meaningful message
 */
void
test_utils_check_region (int x, int y,
                         int width, int height,
                         guint32 expected_rgba);

#endif /* _TEST_UTILS_H_ */
