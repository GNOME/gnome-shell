#ifndef _TEST_UTILS_H_
#define _TEST_UTILS_H_

/* We don't really care about functions that are defined without a
   header for the unit tests so we can just disable it here */
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#endif

typedef enum _TestFlags
{
  TEST_KNOWN_FAILURE             = 1<<0,
  TEST_REQUIREMENT_GL            = 1<<1,
  TEST_REQUIREMENT_NPOT          = 1<<2,
  TEST_REQUIREMENT_TEXTURE_3D    = 1<<3,
  TEST_REQUIREMENT_POINT_SPRITE  = 1<<4,
  TEST_REQUIREMENT_GLES2_CONTEXT = 1<<5,
  TEST_REQUIREMENT_MAP_WRITE     = 1<<6
} TestFlags;

extern CoglContext *ctx;
extern CoglFramebuffer *fb;

void
test_utils_init (TestFlags flags);

void
test_utils_fini (void);

/*
 * test_utils_check_pixel:
 * @framebuffer: The #CoglFramebuffer to read from
 * @x: x co-ordinate of the pixel to test
 * @y: y co-ordinate of the pixel to test
 * @pixel: An integer of the form 0xRRGGBBAA representing the expected
 *         pixel value
 *
 * This performs reads a pixel on the given cogl @framebuffer and
 * asserts that it matches the given color. The alpha channel of the
 * color is ignored. The pixels are converted to a string and compared
 * with g_assert_cmpstr so that if the comparison fails then the
 * assert will display a meaningful message
 */
void
test_utils_check_pixel (CoglFramebuffer *framebuffer,
                        int x, int y, uint32_t expected_pixel);

/*
 * test_utils_check_pixel:
 * @framebuffer: The #CoglFramebuffer to read from
 * @x: x co-ordinate of the pixel to test
 * @y: y co-ordinate of the pixel to test
 * @pixel: An integer of the form 0xrrggbb representing the expected pixel value
 *
 * This performs reads a pixel on the given cogl @framebuffer and
 * asserts that it matches the given color. The alpha channel of the
 * color is ignored. The pixels are converted to a string and compared
 * with g_assert_cmpstr so that if the comparison fails then the
 * assert will display a meaningful message
 */
void
test_utils_check_pixel_rgb (CoglFramebuffer *framebuffer,
                            int x, int y, int r, int g, int b);

/*
 * test_utils_check_region:
 * @framebuffer: The #CoglFramebuffer to read from
 * @x: x co-ordinate of the region to test
 * @y: y co-ordinate of the region to test
 * @width: width of the region to test
 * @height: height of the region to test
 * @pixel: An integer of the form 0xrrggbb representing the expected region color
 *
 * Performs a read pixel on the specified region of the given cogl
 * @framebuffer and asserts that it matches the given color. The alpha
 * channel of the color is ignored. The pixels are converted to a
 * string and compared with g_assert_cmpstr so that if the comparison
 * fails then the assert will display a meaningful message
 */
void
test_utils_check_region (CoglFramebuffer *framebuffer,
                         int x, int y,
                         int width, int height,
                         uint32_t expected_rgba);

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
test_utils_compare_pixel (const uint8_t *screen_pixel, uint32_t expected_pixel);

/*
 * test_utils_create_color_texture:
 * @context: A #CoglContext
 * @color: A color to put in the texture
 *
 * Creates a 1x1-pixel RGBA texture filled with the given color.
 */
CoglTexture *
test_utils_create_color_texture (CoglContext *context,
                                 uint32_t color);

/* cogl_test_verbose:
 *
 * Queries if the user asked for verbose output or not.
 */
CoglBool
cogl_test_verbose (void);

#endif /* _TEST_UTILS_H_ */
