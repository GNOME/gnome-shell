#define COGL_VERSION_MIN_REQUIRED COGL_VERSION_1_0

#include <cogl/cogl.h>

#include "test-utils.h"

#define RED 0
#define GREEN 1
#define BLUE 2

typedef struct _TestState
{
  int fb_width;
  int fb_height;
} TestState;

static void
check_quadrant (TestState *state,
                int qx,
                int qy,
                uint32_t expected_rgba)
{
  /* The quadrants are all stuffed into the top right corner of the
     framebuffer */
  int x = state->fb_width * qx / 4 + state->fb_width / 2;
  int y = state->fb_height * qy / 4;
  int width = state->fb_width / 4;
  int height = state->fb_height / 4;

  /* Subtract a two-pixel gap around the edges to allow some rounding
     differences */
  x += 2;
  y += 2;
  width -= 4;
  height -= 4;

  test_utils_check_region (test_fb, x, y, width, height, expected_rgba);
}

static void
test_paint (TestState *state)
{
  CoglTexture2D *tex_2d;
  CoglTexture *tex;
  CoglOffscreen *offscreen;

  tex_2d = cogl_texture_2d_new_with_size (test_ctx,
                                          state->fb_width,
                                          state->fb_height,
                                          COGL_PIXEL_FORMAT_RGBA_8888_PRE);
  tex = COGL_TEXTURE (tex_2d);

  offscreen = cogl_offscreen_new_with_texture (tex);

  /* Set a scale and translate transform on the window framebuffer
   * before switching to the offscreen framebuffer so we can verify it
   * gets restored when we switch back
   *
   * The test is going to draw a grid of 4 colors to a texture which
   * we subsequently draw to the window with a fullscreen rectangle.
   * This transform will flip the texture left to right, scale it to a
   * quarter of the window size and slide it to the top right of the
   * window.
   */
  cogl_push_matrix ();
  cogl_translate (0.5, 0.5, 0);
  cogl_scale (-0.5, 0.5, 1);

  cogl_push_framebuffer (COGL_FRAMEBUFFER (offscreen));

  /* Cogl should release the last reference when we call cogl_pop_framebuffer()
   */
  cogl_object_unref (offscreen);

  /* Setup something other than the identity matrix for the modelview so we can
   * verify it gets restored when we call cogl_pop_framebuffer () */
  cogl_scale (2, 2, 1);

  /* red, top left */
  cogl_set_source_color4ub (0xff, 0x00, 0x00, 0xff);
  cogl_rectangle (-0.5, 0.5, 0, 0);
  /* green, top right */
  cogl_set_source_color4ub (0x00, 0xff, 0x00, 0xff);
  cogl_rectangle (0, 0.5, 0.5, 0);
  /* blue, bottom left */
  cogl_set_source_color4ub (0x00, 0x00, 0xff, 0xff);
  cogl_rectangle (-0.5, 0, 0, -0.5);
  /* white, bottom right */
  cogl_set_source_color4ub (0xff, 0xff, 0xff, 0xff);
  cogl_rectangle (0, 0, 0.5, -0.5);

  cogl_pop_framebuffer ();

  cogl_set_source_texture (tex);
  cogl_rectangle (-1, 1, 1, -1);

  cogl_object_unref (tex_2d);

  cogl_pop_matrix ();

  /* NB: The texture is drawn flipped horizontally and scaled to fit in the
   * top right corner of the window. */

  /* red, top right */
  check_quadrant (state, 1, 0, 0xff0000ff);
  /* green, top left */
  check_quadrant (state, 0, 0, 0x00ff00ff);
  /* blue, bottom right */
  check_quadrant (state, 1, 1, 0x0000ffff);
  /* white, bottom left */
  check_quadrant (state, 0, 1, 0xffffffff);
}

static void
test_flush (TestState *state)
{
  CoglTexture2D *tex_2d;
  CoglTexture *tex;
  CoglOffscreen *offscreen;
  CoglColor clear_color;
  int i;

  for (i = 0; i < 3; i++)
    {
      /* This tests that rendering to a framebuffer and then reading back
         the contents of the texture will automatically flush the
         journal */

      tex_2d = cogl_texture_2d_new_with_size (test_ctx,
                                              16, 16, /* width/height */
                                              COGL_PIXEL_FORMAT_RGBA_8888_PRE);
      tex = COGL_TEXTURE (tex_2d);

      offscreen = cogl_offscreen_new_with_texture (tex);

      cogl_push_framebuffer (COGL_FRAMEBUFFER (offscreen));

      cogl_color_init_from_4ub (&clear_color, 0, 0, 0, 255);
      cogl_clear (&clear_color, COGL_BUFFER_BIT_COLOR);

      cogl_set_source_color4ub (255, 0, 0, 255);
      cogl_rectangle (-1, -1, 1, 1);

      if (i == 0)
        /* First time check using read pixels on the offscreen */
        test_utils_check_region (COGL_FRAMEBUFFER (offscreen),
                                 1, 1, 15, 15, 0xff0000ff);
      else if (i == 1)
        {
          uint8_t data[16 * 4 * 16];
          int x, y;

          /* Second time try reading back the texture contents */
          cogl_texture_get_data (tex,
                                 COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                 16 * 4, /* rowstride */
                                 data);

          for (y = 1; y < 15; y++)
            for (x = 1; x < 15; x++)
              test_utils_compare_pixel (data + x * 4 + y * 16 * 4,
                                        0xff0000ff);
        }

      cogl_pop_framebuffer ();

      if (i == 2)
        {
          /* Third time try drawing the texture to the screen */
          cogl_set_source_texture (tex);
          cogl_rectangle (-1, -1, 1, 1);
          test_utils_check_region (test_fb,
                                   2, 2, /* x/y */
                                   state->fb_width - 4,
                                   state->fb_height - 4,
                                   0xff0000ff);
        }

      cogl_object_unref (tex_2d);
      cogl_object_unref (offscreen);
    }
}

void
test_offscreen (void)
{
  TestState state;

  state.fb_width = cogl_framebuffer_get_width (test_fb);
  state.fb_height = cogl_framebuffer_get_height (test_fb);

  /* XXX: we have to push/pop a framebuffer since this test currently
   * uses the legacy cogl_rectangle() api. */
  cogl_push_framebuffer (test_fb);
  test_paint (&state);
  test_flush (&state);
  cogl_pop_framebuffer ();

  if (cogl_test_verbose ())
    g_print ("OK\n");
}
