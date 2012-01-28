#include <cogl/cogl.h>

#include "test-utils.h"

#define RED 0
#define GREEN 1
#define BLUE 2

typedef struct _TestState
{
  CoglContext *context;
  int fb_width;
  int fb_height;
} TestState;

static void
check_quadrant (TestState *state,
                int qx,
                int qy,
                guint32 expected_rgba)
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

  test_utils_check_region (x, y, width, height, expected_rgba);
}

static void
paint (TestState *state)
{
  CoglTexture2D *tex_2d;
  CoglTexture *tex;
  CoglHandle offscreen;

  tex_2d = cogl_texture_2d_new_with_size (state->context,
                                          state->fb_width,
                                          state->fb_height,
                                          COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                          NULL);
  tex = COGL_TEXTURE (tex_2d);

  offscreen = cogl_offscreen_new_to_texture (tex);

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
  cogl_translate (0.5, 0.5, 0);
  cogl_scale (-0.5, 0.5, 1);

  cogl_push_framebuffer (offscreen);

  /* Cogl should release the last reference when we call cogl_pop_framebuffer()
   */
  cogl_handle_unref (offscreen);

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

void
test_cogl_offscreen (TestUtilsGTestFixture *fixture,
                     void *data)
{
  TestUtilsSharedState *shared_state = data;
  TestState state;

  state.context = shared_state->ctx;
  state.fb_width = cogl_framebuffer_get_width (shared_state->fb);
  state.fb_height = cogl_framebuffer_get_height (shared_state->fb);

  paint (&state);

  if (g_test_verbose ())
    g_print ("OK\n");
}
