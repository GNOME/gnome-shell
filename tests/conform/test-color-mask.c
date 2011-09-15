#include <cogl/cogl.h>

#include "test-utils.h"

#define TEX_SIZE 128

#define NUM_FBOS 3

typedef struct _TestState
{
  int width;
  int height;

  CoglHandle tex[NUM_FBOS];
  CoglFramebuffer *fbo[NUM_FBOS];
} TestState;

static void
check_pixel (int x, int y, guint8 r, guint8 g, guint8 b)
{
  guint32 pixel;
  char *screen_pixel;
  char *intended_pixel;

  cogl_read_pixels (x, y, 1, 1, COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                    (guint8 *) &pixel);

  screen_pixel = g_strdup_printf ("#%06x", GUINT32_FROM_BE (pixel) >> 8);
  intended_pixel = g_strdup_printf ("#%02x%02x%02x", r, g, b);

  g_assert_cmpstr (screen_pixel, ==, intended_pixel);

  g_free (screen_pixel);
  g_free (intended_pixel);
}

static void
paint (TestState *state)
{
  CoglColor bg;
  int i;

  cogl_set_source_color4ub (255, 255, 255, 255);

  /* We push the third framebuffer first so that later we can switch
     back to it by popping to test that that works */
  cogl_push_framebuffer (state->fbo[2]);

  cogl_push_framebuffer (state->fbo[0]);
  cogl_rectangle (-1.0, -1.0, 1.0, 1.0);
  cogl_pop_framebuffer ();

  cogl_push_framebuffer (state->fbo[1]);
  cogl_rectangle (-1.0, -1.0, 1.0, 1.0);
  cogl_pop_framebuffer ();

  /* We should now be back on the third framebuffer */
  cogl_rectangle (-1.0, -1.0, 1.0, 1.0);
  cogl_pop_framebuffer ();

  cogl_color_init_from_4ub (&bg, 128, 128, 128, 255);
  cogl_clear (&bg, COGL_BUFFER_BIT_COLOR | COGL_BUFFER_BIT_DEPTH);

  /* Render all of the textures to the screen */
  for (i = 0; i < NUM_FBOS; i++)
    {
      cogl_set_source_texture (state->tex[i]);
      cogl_rectangle (2.0f / NUM_FBOS * i - 1.0f, -1.0f,
                      2.0f / NUM_FBOS * (i + 1) - 1.0f, 1.0f);
    }

  /* Verify all of the fbos drew the right color */
  for (i = 0; i < NUM_FBOS; i++)
    {
      guint8 expected_colors[NUM_FBOS][4] =
        { { 0xff, 0x00, 0x00, 0xff },
          { 0x00, 0xff, 0x00, 0xff },
          { 0x00, 0x00, 0xff, 0xff } };

      check_pixel (state->width * (i + 0.5f) / NUM_FBOS,
                   state->height / 2,
                   expected_colors[i][0],
                   expected_colors[i][1],
                   expected_colors[i][2]);
    }
}

void
test_cogl_color_mask (TestUtilsGTestFixture *fixture,
                      void *data)
{
  TestUtilsSharedState *shared_state = data;
  TestState state;
  int i;

  state.width = cogl_framebuffer_get_width (shared_state->fb);
  state.height = cogl_framebuffer_get_height (shared_state->fb);

  for (i = 0; i < NUM_FBOS; i++)
    {
      state.tex[i] = cogl_texture_new_with_size (128, 128,
                                                 COGL_TEXTURE_NO_ATLAS,
                                                 COGL_PIXEL_FORMAT_RGB_888);


      state.fbo[i] = cogl_offscreen_new_to_texture (state.tex[i]);
      cogl_framebuffer_set_color_mask (state.fbo[i],
                                       i == 0 ? COGL_COLOR_MASK_RED :
                                       i == 1 ? COGL_COLOR_MASK_GREEN :
                                       COGL_COLOR_MASK_BLUE);
    }

  paint (&state);

  if (g_test_verbose ())
    g_print ("OK\n");
}

