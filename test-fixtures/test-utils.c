#include <config.h>

#include <cogl/cogl.h>
#include <stdlib.h>

#include "test-unit.h"
#include "test-utils.h"

#define FB_WIDTH 512
#define FB_HEIGHT 512

static CoglBool cogl_test_is_verbose;

CoglContext *test_ctx;
CoglFramebuffer *test_fb;

static CoglBool
check_flags (TestFlags flags,
             CoglRenderer *renderer)
{
  if (flags & TEST_REQUIREMENT_GL &&
      cogl_renderer_get_driver (renderer) != COGL_DRIVER_GL &&
      cogl_renderer_get_driver (renderer) != COGL_DRIVER_GL3)
    {
      return FALSE;
    }

  if (flags & TEST_REQUIREMENT_NPOT &&
      !cogl_has_feature (test_ctx, COGL_FEATURE_ID_TEXTURE_NPOT))
    {
      return FALSE;
    }

  if (flags & TEST_REQUIREMENT_TEXTURE_3D &&
      !cogl_has_feature (test_ctx, COGL_FEATURE_ID_TEXTURE_3D))
    {
      return FALSE;
    }

  if (flags & TEST_REQUIREMENT_TEXTURE_RECTANGLE &&
      !cogl_has_feature (test_ctx, COGL_FEATURE_ID_TEXTURE_RECTANGLE))
    {
      return FALSE;
    }

  if (flags & TEST_REQUIREMENT_POINT_SPRITE &&
      !cogl_has_feature (test_ctx, COGL_FEATURE_ID_POINT_SPRITE))
    {
      return FALSE;
    }

  if (flags & TEST_REQUIREMENT_PER_VERTEX_POINT_SIZE &&
      !cogl_has_feature (test_ctx, COGL_FEATURE_ID_PER_VERTEX_POINT_SIZE))
    {
      return FALSE;
    }

  if (flags & TEST_REQUIREMENT_GLES2_CONTEXT &&
      !cogl_has_feature (test_ctx, COGL_FEATURE_ID_GLES2_CONTEXT))
    {
      return FALSE;
    }

  if (flags & TEST_REQUIREMENT_MAP_WRITE &&
      !cogl_has_feature (test_ctx, COGL_FEATURE_ID_MAP_BUFFER_FOR_WRITE))
    {
      return FALSE;
    }

  if (flags & TEST_REQUIREMENT_GLSL &&
      !cogl_has_feature (test_ctx, COGL_FEATURE_ID_GLSL))
    {
      return FALSE;
    }

  if (flags & TEST_REQUIREMENT_OFFSCREEN &&
      !cogl_has_feature (test_ctx, COGL_FEATURE_ID_OFFSCREEN))
    {
      return FALSE;
    }

  if (flags & TEST_REQUIREMENT_FENCE &&
      !cogl_has_feature (test_ctx, COGL_FEATURE_ID_FENCE))
    {
      return FALSE;
    }

  if (flags & TEST_KNOWN_FAILURE)
    {
      return FALSE;
    }

  return TRUE;
}

CoglBool
is_boolean_env_set (const char *variable)
{
  char *val = getenv (variable);
  CoglBool ret;

  if (!val)
    return FALSE;

  if (g_ascii_strcasecmp (val, "1") == 0 ||
      g_ascii_strcasecmp (val, "on") == 0 ||
      g_ascii_strcasecmp (val, "true") == 0)
    ret = TRUE;
  else if (g_ascii_strcasecmp (val, "0") == 0 ||
           g_ascii_strcasecmp (val, "off") == 0 ||
           g_ascii_strcasecmp (val, "false") == 0)
    ret = FALSE;
  else
    {
      g_critical ("Spurious boolean environment variable value (%s=%s)",
                  variable, val);
      ret = TRUE;
    }

  return ret;
}

void
test_utils_init (TestFlags requirement_flags,
                 TestFlags known_failure_flags)
{
  static int counter = 0;
  CoglError *error = NULL;
  CoglOnscreen *onscreen = NULL;
  CoglDisplay *display;
  CoglRenderer *renderer;
  CoglBool missing_requirement;
  CoglBool known_failure;

  if (counter != 0)
    g_critical ("We don't support running more than one test at a time\n"
                "in a single test run due to the state leakage that can\n"
                "cause subsequent tests to fail.\n"
                "\n"
                "If you want to run all the tests you should run\n"
                "$ make test-report");
  counter++;

  if (is_boolean_env_set ("COGL_TEST_VERBOSE") ||
      is_boolean_env_set ("V"))
    cogl_test_is_verbose = TRUE;

  /* NB: This doesn't have any effect since commit 47444dac of glib
   * because the environment variable is read in a magic constructor
   * so it is too late to set them here */
  if (g_getenv ("G_DEBUG"))
    {
      char *debug = g_strconcat (g_getenv ("G_DEBUG"), ",fatal-warnings", NULL);
      g_setenv ("G_DEBUG", debug, TRUE);
      g_free (debug);
    }
  else
    g_setenv ("G_DEBUG", "fatal-warnings", TRUE);

  g_setenv ("COGL_X11_SYNC", "1", 0);

  test_ctx = cogl_context_new (NULL, &error);
  if (!test_ctx)
    g_critical ("Failed to create a CoglContext: %s", error->message);

  display = cogl_context_get_display (test_ctx);
  renderer = cogl_display_get_renderer (display);

  missing_requirement = !check_flags (requirement_flags, renderer);
  known_failure = !check_flags (known_failure_flags, renderer);

  if (is_boolean_env_set ("COGL_TEST_ONSCREEN"))
    {
      onscreen = cogl_onscreen_new (test_ctx, 640, 480);
      test_fb = COGL_FRAMEBUFFER (onscreen);
    }
  else
    {
      CoglOffscreen *offscreen;
      CoglTexture2D *tex = cogl_texture_2d_new_with_size (test_ctx,
                                                          FB_WIDTH, FB_HEIGHT,
                                                          COGL_PIXEL_FORMAT_ANY);
      offscreen = cogl_offscreen_new_to_texture (COGL_TEXTURE (tex));
      test_fb = COGL_FRAMEBUFFER (offscreen);
    }

  if (!cogl_framebuffer_allocate (test_fb, &error))
    g_critical ("Failed to allocate framebuffer: %s", error->message);

  if (onscreen)
    cogl_onscreen_show (onscreen);

  cogl_framebuffer_clear4f (test_fb,
                            COGL_BUFFER_BIT_COLOR |
                            COGL_BUFFER_BIT_DEPTH |
                            COGL_BUFFER_BIT_STENCIL,
                            0, 0, 0, 1);

  if (missing_requirement)
    g_print ("WARNING: Missing required feature[s] for this test\n");
  else if (known_failure)
    g_print ("WARNING: Test is known to fail\n");
}

void
test_utils_fini (void)
{
  if (test_fb)
    cogl_object_unref (test_fb);

  if (test_ctx)
    cogl_object_unref (test_ctx);
}

static CoglBool
compare_component (int a, int b)
{
  return ABS (a - b) <= 1;
}

void
test_utils_compare_pixel_and_alpha (const uint8_t *screen_pixel,
                                    uint32_t expected_pixel)
{
  /* Compare each component with a small fuzz factor */
  if (!compare_component (screen_pixel[0], expected_pixel >> 24) ||
      !compare_component (screen_pixel[1], (expected_pixel >> 16) & 0xff) ||
      !compare_component (screen_pixel[2], (expected_pixel >> 8) & 0xff) ||
      !compare_component (screen_pixel[3], (expected_pixel >> 0) & 0xff))
    {
      uint32_t screen_pixel_num = GUINT32_FROM_BE (*(uint32_t *) screen_pixel);
      char *screen_pixel_string =
        g_strdup_printf ("#%08x", screen_pixel_num);
      char *expected_pixel_string =
        g_strdup_printf ("#%08x", expected_pixel);

      g_assert_cmpstr (screen_pixel_string, ==, expected_pixel_string);

      g_free (screen_pixel_string);
      g_free (expected_pixel_string);
    }
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
test_utils_check_pixel (CoglFramebuffer *test_fb,
                        int x, int y, uint32_t expected_pixel)
{
  uint8_t pixel[4];

  cogl_framebuffer_read_pixels (test_fb,
                                x, y, 1, 1,
                                COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                pixel);

  test_utils_compare_pixel (pixel, expected_pixel);
}

void
test_utils_check_pixel_and_alpha (CoglFramebuffer *test_fb,
                                  int x, int y, uint32_t expected_pixel)
{
  uint8_t pixel[4];

  cogl_framebuffer_read_pixels (test_fb,
                                x, y, 1, 1,
                                COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                pixel);

  test_utils_compare_pixel_and_alpha (pixel, expected_pixel);
}

void
test_utils_check_pixel_rgb (CoglFramebuffer *test_fb,
                            int x, int y, int r, int g, int b)
{
  test_utils_check_pixel (test_fb, x, y, (r << 24) | (g << 16) | (b << 8));
}

void
test_utils_check_region (CoglFramebuffer *test_fb,
                         int x, int y,
                         int width, int height,
                         uint32_t expected_rgba)
{
  uint8_t *pixels, *p;

  pixels = p = g_malloc (width * height * 4);
  cogl_framebuffer_read_pixels (test_fb,
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
