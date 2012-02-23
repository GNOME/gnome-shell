#ifndef _TEST_UTILS_H_
#define _TEST_UTILS_H_

typedef enum _TestRequirement
{
  TEST_REQUIREMENT_GL         = 1<<0,
  TEST_REQUIREMENT_NPOT       = 1<<2,
  TEST_REQUIREMENT_TEXTURE_3D = 1<<3
} TestRequirement;

/* For compatability since we used to use the glib gtester
 * infrastructure and all our unit tests have an entry
 * point with a first argument of this type... */
typedef struct _TestUtilsGTestFixture TestUtilsGTestFixture;

/* Stuff you put in here is setup once in main() and gets passed around to
 * all test functions and fixture setup/teardown functions in the data
 * argument */
typedef struct _TestUtilsSharedState
{
  int    *argc_addr;
  char ***argv_addr;

  CoglContext *ctx;
  CoglFramebuffer *fb;
} TestUtilsSharedState;

void
test_utils_init (TestUtilsSharedState *state,
                 TestRequirement requirements);

void
test_utils_fini (TestUtilsSharedState *state);

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

/*
 * test_utils_compare_pixel:
 * @screen_pixel: A pixel stored in memory
 * @expected_pixel: The expected RGBA value
 *
 * Compares a pixel from a buffer to an expected value. The pixels are
 * converted to a string and compared with g_assert_cmpstr so that if
 * the comparison fails then the assert will display a meaningful
 * message.
 */
void
test_utils_compare_pixel (const guint8 *screen_pixel, guint32 expected_pixel);

/*
 * test_utils_create_color_texture:
 * @context: A #CoglContext
 * @color: A color to put in the texture
 *
 * Creates a 1x1-pixel RGBA texture filled with the given color.
 */
CoglTexture *
test_utils_create_color_texture (CoglContext *context,
                                 guint32 color);

/* cogl_test_verbose:
 *
 * Queries if the user asked for verbose output or not.
 */
gboolean
cogl_test_verbose (void);

#endif /* _TEST_UTILS_H_ */
