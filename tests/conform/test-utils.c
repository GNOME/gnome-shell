#define COGL_ENABLE_EXPERIMENTAL_2_0_API
#include <cogl/cogl.h>
#include <stdlib.h>

#include "test-utils.h"

#define FB_WIDTH 512
#define FB_HEIGHT 512

static CoglBool cogl_test_is_verbose;

CoglContext *ctx;
CoglFramebuffer *fb;

void
test_utils_init (TestFlags flags)
{
  static int counter = 0;
  CoglError *error = NULL;
  CoglOnscreen *onscreen = NULL;
  CoglDisplay *display;
  CoglRenderer *renderer;
  CoglBool missing_requirement = FALSE;

  if (counter != 0)
    g_critical ("We don't support running more than one test at a time\n"
                "in a single test run due to the state leakage that can\n"
                "cause subsequent tests to fail.\n"
                "\n"
                "If you want to run all the tests you should run\n"
                "$ make test-report");
  counter++;

  if (g_getenv ("COGL_TEST_VERBOSE") || g_getenv ("V"))
    cogl_test_is_verbose = TRUE;

  if (g_getenv ("G_DEBUG"))
    {
      char *debug = g_strconcat (g_getenv ("G_DEBUG"), ",fatal-warnings", NULL);
      g_setenv ("G_DEBUG", debug, TRUE);
      g_free (debug);
    }
  else
    g_setenv ("G_DEBUG", "fatal-warnings", TRUE);

  g_setenv ("COGL_X11_SYNC", "1", 0);

  ctx = cogl_context_new (NULL, &error);
  if (!ctx)
    g_critical ("Failed to create a CoglContext: %s", error->message);

  display = cogl_context_get_display (ctx);
  renderer = cogl_display_get_renderer (display);

  if (flags & TEST_REQUIREMENT_GL &&
      cogl_renderer_get_driver (renderer) != COGL_DRIVER_GL &&
      cogl_renderer_get_driver (renderer) != COGL_DRIVER_GL3)
    {
      missing_requirement = TRUE;
    }

  if (flags & TEST_REQUIREMENT_NPOT &&
      !cogl_has_feature (ctx, COGL_FEATURE_ID_TEXTURE_NPOT))
    {
      missing_requirement = TRUE;
    }

  if (flags & TEST_REQUIREMENT_TEXTURE_3D &&
      !cogl_has_feature (ctx, COGL_FEATURE_ID_TEXTURE_3D))
    {
      missing_requirement = TRUE;
    }

  if (flags & TEST_REQUIREMENT_POINT_SPRITE &&
      !cogl_has_feature (ctx, COGL_FEATURE_ID_POINT_SPRITE))
    {
      missing_requirement = TRUE;
    }

  if (flags & TEST_REQUIREMENT_GLES2_CONTEXT &&
      !cogl_has_feature (ctx, COGL_FEATURE_ID_GLES2_CONTEXT))
    {
      missing_requirement = TRUE;
    }

  if (flags & TEST_REQUIREMENT_MAP_WRITE &&
      !cogl_has_feature (ctx, COGL_FEATURE_ID_MAP_BUFFER_FOR_WRITE))
    {
      missing_requirement = TRUE;
    }

  if (flags & TEST_KNOWN_FAILURE)
    {
      missing_requirement = TRUE;
    }

  if (getenv  ("COGL_TEST_ONSCREEN"))
    {
      onscreen = cogl_onscreen_new (ctx, 640, 480);
      fb = COGL_FRAMEBUFFER (onscreen);
    }
  else
    {
      CoglOffscreen *offscreen;
      CoglTexture2D *tex = cogl_texture_2d_new_with_size (ctx,
                                                          FB_WIDTH, FB_HEIGHT,
                                                          COGL_PIXEL_FORMAT_ANY,
                                                          &error);
      if (!tex)
        g_critical ("Failed to allocate texture: %s", error->message);

      offscreen = cogl_offscreen_new_to_texture (COGL_TEXTURE (tex));
      fb = COGL_FRAMEBUFFER (offscreen);
    }

  if (!cogl_framebuffer_allocate (fb, &error))
    g_critical ("Failed to allocate framebuffer: %s", error->message);

  if (onscreen)
    cogl_onscreen_show (onscreen);

  cogl_framebuffer_clear4f (fb,
                            COGL_BUFFER_BIT_COLOR |
                            COGL_BUFFER_BIT_DEPTH |
                            COGL_BUFFER_BIT_STENCIL,
                            0, 0, 0, 1);

  if (missing_requirement)
    g_print ("WARNING: Missing required feature[s] for this test\n");
}

void
test_utils_fini (void)
{
  if (fb)
    cogl_object_unref (fb);

  if (ctx)
    cogl_object_unref (ctx);
}

static CoglBool
compare_component (int a, int b)
{
  return ABS (a - b) <= 1;
}

void
test_utils_compare_pixel (const uint8_t *screen_pixel, uint32_t expected_pixel)
{
  /* Compare each component with a small fuzz factor */
  if (!compare_component (screen_pixel[0], expected_pixel >> 24) ||
      !compare_component (screen_pixel[1], (expected_pixel >> 16) & 0xff) ||
      !compare_component (screen_pixel[2], (expected_pixel >> 8) & 0xff))
    {
      uint32_t screen_pixel_num = GUINT32_FROM_BE (*(uint32_t *) screen_pixel);
      char *screen_pixel_string =
        g_strdup_printf ("#%06x", screen_pixel_num >> 8);
      char *expected_pixel_string =
        g_strdup_printf ("#%06x", expected_pixel >> 8);

      g_assert_cmpstr (screen_pixel_string, ==, expected_pixel_string);

      g_free (screen_pixel_string);
      g_free (expected_pixel_string);
    }
}

void
test_utils_check_pixel (CoglFramebuffer *fb,
                        int x, int y, uint32_t expected_pixel)
{
  uint8_t pixel[4];

  cogl_framebuffer_read_pixels (fb,
                                x, y, 1, 1,
                                COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                pixel);

  test_utils_compare_pixel (pixel, expected_pixel);
}

void
test_utils_check_pixel_rgb (CoglFramebuffer *fb,
                            int x, int y, int r, int g, int b)
{
  test_utils_check_pixel (fb, x, y, (r << 24) | (g << 16) | (b << 8));
}

void
test_utils_check_region (CoglFramebuffer *fb,
                         int x, int y,
                         int width, int height,
                         uint32_t expected_rgba)
{
  uint8_t *pixels, *p;

  pixels = p = g_malloc (width * height * 4);
  cogl_framebuffer_read_pixels (fb,
                                x,
                                y,
                                width,
                                height,
                                COGL_PIXEL_FORMAT_RGBA_8888,
                                p);

  /* Check whether the center of each division is the right color */
  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      {
        test_utils_compare_pixel (p, expected_rgba);
        p += 4;
      }

  g_free (pixels);
}

CoglTexture *
test_utils_create_color_texture (CoglContext *context,
                                 uint32_t color)
{
  CoglTexture2D *tex_2d;

  color = GUINT32_TO_BE (color);

  tex_2d = cogl_texture_2d_new_from_data (context,
                                          1, 1, /* width/height */
                                          COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                          COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                          4, /* rowstride */
                                          (uint8_t *) &color,
                                          NULL);

  return COGL_TEXTURE (tex_2d);
}

CoglBool
cogl_test_verbose (void)
{
  return cogl_test_is_verbose;
}
