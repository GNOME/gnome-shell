#ifndef _TEST_UTILS_H_
#define _TEST_UTILS_H_

#include <cogl/cogl-context.h>
#include <cogl/cogl-onscreen.h>
#include <cogl/cogl-offscreen.h>
#include <cogl/cogl-texture-2d.h>
#include <cogl/cogl-primitive-texture.h>
#include <cogl/cogl-texture-2d-sliced.h>
#include <cogl/cogl-meta-texture.h>
#include <cogl/cogl-atlas-texture.h>
#include <glib.h>

/* We don't really care about functions that are defined without a
   header for the unit tests so we can just disable it here */
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#endif

typedef enum _TestFlags
{
  TEST_KNOWN_FAILURE = 1<<0,
  TEST_REQUIREMENT_GL = 1<<1,
  TEST_REQUIREMENT_NPOT = 1<<2,
  TEST_REQUIREMENT_TEXTURE_3D = 1<<3,
  TEST_REQUIREMENT_TEXTURE_RECTANGLE = 1<<4,
  TEST_REQUIREMENT_POINT_SPRITE = 1<<5,
  TEST_REQUIREMENT_GLES2_CONTEXT = 1<<6,
  TEST_REQUIREMENT_MAP_WRITE = 1<<7,
  TEST_REQUIREMENT_GLSL = 1<<8,
  TEST_REQUIREMENT_OFFSCREEN = 1<<9,
  TEST_REQUIREMENT_FENCE = 1<<10,
  TEST_REQUIREMENT_PER_VERTEX_POINT_SIZE = 1<<11
} TestFlags;

 /**
 * TestUtilsTextureFlags:
 * @TEST_UTILS_TEXTURE_NONE: No flags specified
 * @TEST_UTILS_TEXTURE_NO_AUTO_MIPMAP: Disables the automatic generation of
 *   the mipmap pyramid from the base level image whenever it is
 *   updated. The mipmaps are only generated when the texture is
 *   rendered with a mipmap filter so it should be free to leave out
 *   this flag when using other filtering modes
 * @TEST_UTILS_TEXTURE_NO_SLICING: Disables the slicing of the texture
 * @TEST_UTILS_TEXTURE_NO_ATLAS: Disables the insertion of the texture inside
 *   the texture atlas used by Cogl
 *
 * Flags to pass to the test_utils_texture_new_* family of functions.
 */
typedef enum {
  TEST_UTILS_TEXTURE_NONE           = 0,
  TEST_UTILS_TEXTURE_NO_AUTO_MIPMAP = 1 << 0,
  TEST_UTILS_TEXTURE_NO_SLICING     = 1 << 1,
  TEST_UTILS_TEXTURE_NO_ATLAS       = 1 << 2
} TestUtilsTextureFlags;

extern CoglContext *test_ctx;
extern CoglFramebuffer *test_fb;

void
test_utils_init (TestFlags requirement_flags,
                 TestFlags known_failure_flags);

void
test_utils_fini (void);

/*
 * test_utils_texture_new_with_size:
 * @context: A #CoglContext
 * @width: width of texture in pixels.
 * @height: height of texture in pixels.
 * @flags: Optional flags for the texture, or %TEST_UTILS_TEXTURE_NONE
 * @internal_format: the #CoglPixelFormat to use for the GPU storage of the
 *    texture.
 *
 * Creates a new #CoglTexture with the specified dimensions and pixel format.
 *
 * The storage for the texture is not necesarily created before this
 * function returns. The storage can be explicitly allocated using
 * cogl_texture_allocate() or preferably you can let Cogl
 * automatically allocate the storage lazily when uploading data when
 * Cogl may know more about how the texture will be used and can
 * optimize how it is allocated.
 *
 * Return value: A newly created #CoglTexture
 */
CoglTexture *
test_utils_texture_new_with_size (CoglContext *ctx,
                                  int width,
                                  int height,
                                  TestUtilsTextureFlags flags,
                                  CoglPixelFormat internal_format);

/*
 * test_utils_texture_new_from_data:
 * @context: A #CoglContext
 * @width: width of texture in pixels
 * @height: height of texture in pixels
 * @flags: Optional flags for the texture, or %TEST_UTILS_TEXTURE_NONE
 * @format: the #CoglPixelFormat the buffer is stored in in RAM
 * @internal_format: the #CoglPixelFormat that will be used for storing
 *    the buffer on the GPU. If COGL_PIXEL_FORMAT_ANY is given then a
 *    premultiplied format similar to the format of the source data will
 *    be used. The default blending equations of Cogl expect premultiplied
 *    color data; the main use of passing a non-premultiplied format here
 *    is if you have non-premultiplied source data and are going to adjust
 *    the blend mode (see cogl_material_set_blend()) or use the data for
 *    something other than straight blending.
 * @rowstride: the memory offset in bytes between the starts of
 *    scanlines in @data
 * @data: pointer the memory region where the source buffer resides
 * @error: A #CoglError to catch exceptional errors or %NULL
 *
 * Creates a new #CoglTexture based on data residing in memory.
 *
 * Return value: A newly created #CoglTexture or %NULL on failure
 */
CoglTexture *
test_utils_texture_new_from_data (CoglContext *ctx,
                                  int width,
                                  int height,
                                  TestUtilsTextureFlags flags,
                                  CoglPixelFormat format,
                                  CoglPixelFormat internal_format,
                                  int rowstride,
                                  const uint8_t *data);

/*
 * test_utils_texture_new_from_bitmap:
 * @bitmap: A #CoglBitmap pointer
 * @flags: Optional flags for the texture, or %TEST_UTILS_TEXTURE_NONE
 * @internal_format: the #CoglPixelFormat to use for the GPU storage of the
 * texture
 * @error: A #CoglError to catch exceptional errors or %NULL
 *
 * Creates a #CoglTexture from a #CoglBitmap.
 *
 * Return value: A newly created #CoglTexture or %NULL on failure
 */
CoglTexture *
test_utils_texture_new_from_bitmap (CoglBitmap *bitmap,
                                    TestUtilsTextureFlags flags,
                                    CoglPixelFormat internal_format);

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

/**
 * @framebuffer: The #CoglFramebuffer to read from
 * @x: x co-ordinate of the pixel to test
 * @y: y co-ordinate of the pixel to test
 * @pixel: An integer of the form 0xRRGGBBAA representing the expected
 *         pixel value
 *
 * This performs reads a pixel on the given cogl @framebuffer and
 * asserts that it matches the given color. The alpha channel is also
 * checked unlike with test_utils_check_pixel(). The pixels are
 * converted to a string and compared with g_assert_cmpstr so that if
 * the comparison fails then the assert will display a meaningful
 * message.
 */
void
test_utils_check_pixel_and_alpha (CoglFramebuffer *fb,
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
 * test_utils_compare_pixel_and_alpha:
 * @screen_pixel: A pixel stored in memory
 * @expected_pixel: The expected RGBA value
 *
 * Compares a pixel from a buffer to an expected value. This is
 * similar to test_utils_compare_pixel() except that it doesn't ignore
 * the alpha component.
 */
void
test_utils_compare_pixel_and_alpha (const uint8_t *screen_pixel,
                                    uint32_t expected_pixel);

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

/* test_util_is_pot:
 * @number: A number to test
 *
 * Returns whether the given integer is a power of two
 */
static inline CoglBool
test_utils_is_pot (unsigned int number)
{
  /* Make sure there is only one bit set */
  return (number & (number - 1)) == 0;
}

#endif /* _TEST_UTILS_H_ */
