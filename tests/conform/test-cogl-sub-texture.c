
#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <string.h>

#include "test-conform-common.h"

#define SOURCE_SIZE        32
#define SOURCE_DIVISIONS_X 2
#define SOURCE_DIVISIONS_Y 2
#define DIVISION_WIDTH     (SOURCE_SIZE / SOURCE_DIVISIONS_X)
#define DIVISION_HEIGHT    (SOURCE_SIZE / SOURCE_DIVISIONS_Y)

#define TEST_INSET         1

static const ClutterColor
corner_colors[SOURCE_DIVISIONS_X * SOURCE_DIVISIONS_Y] =
  {
    { 0xff, 0x00, 0x00, 0xff }, /* red top left */
    { 0x00, 0xff, 0x00, 0xff }, /* green top right */
    { 0x00, 0x00, 0xff, 0xff }, /* blue bottom left */
    { 0xff, 0x00, 0xff, 0xff }  /* purple bottom right */
  };

static const ClutterColor stage_color = { 0x0, 0x0, 0x0, 0xff };

typedef struct _TestState
{
  ClutterActor *stage;
  guint frame;

  CoglHandle tex;
} TestState;

static CoglHandle
create_source (void)
{
  int dx, dy;
  guchar *data = g_malloc (SOURCE_SIZE * SOURCE_SIZE * 4);

  /* Create a texture with a different coloured rectangle at each
     corner */
  for (dy = 0; dy < SOURCE_DIVISIONS_Y; dy++)
    for (dx = 0; dx < SOURCE_DIVISIONS_X; dx++)
      {
        guchar *p = (data + dy * DIVISION_HEIGHT * SOURCE_SIZE * 4 +
                     dx * DIVISION_WIDTH * 4);
        int x, y;

        for (y = 0; y < DIVISION_HEIGHT; y++)
          {
            for (x = 0; x < DIVISION_WIDTH; x++)
              {
                memcpy (p, corner_colors + dx + dy * SOURCE_DIVISIONS_X, 4);
                p += 4;
              }

            p += SOURCE_SIZE * 4 - DIVISION_WIDTH * 4;
          }
      }

  return cogl_texture_new_from_data (SOURCE_SIZE, SOURCE_SIZE,
                                     COGL_TEXTURE_NONE,
                                     COGL_PIXEL_FORMAT_RGBA_8888,
                                     COGL_PIXEL_FORMAT_ANY,
                                     SOURCE_SIZE * 4,
                                     data);
}

static void
draw_frame (TestState *state)
{
  CoglHandle sub_texture;

  /* Create a sub texture of the bottom right quarter of the texture */
  sub_texture = cogl_texture_new_from_sub_texture (state->tex,
                                                   1.0f / SOURCE_DIVISIONS_X,
                                                   1.0f / SOURCE_DIVISIONS_Y,
                                                   1.0f, 1.0f);

  /* Paint it */
  cogl_set_source_texture (sub_texture);
  cogl_rectangle (0.0f, 0.0f, DIVISION_WIDTH, DIVISION_HEIGHT);

  cogl_handle_unref (sub_texture);

  /* Repeat a sub texture of the top half of the full texture */
  sub_texture = cogl_texture_new_from_sub_texture (state->tex,
                                                   0.0f, 0.0f, 1.0f, 0.5f);
  cogl_set_source_texture (sub_texture);
  cogl_rectangle_with_texture_coords (0.0f, SOURCE_SIZE,
                                      SOURCE_SIZE * 2.0f, SOURCE_SIZE * 1.5f,
                                      0.0f, 0.0f,
                                      2.0f, 1.0f);
  cogl_handle_unref (sub_texture);

  /* Create a texture that repeats the source texture twice */
  sub_texture = cogl_texture_new_from_sub_texture (state->tex,
                                                   0.0f, 0.0f, 2.0f, 2.0f);
  cogl_set_source_texture (sub_texture);
  cogl_rectangle (0.0f, SOURCE_SIZE * 2, SOURCE_SIZE * 2, SOURCE_SIZE * 4);
  cogl_handle_unref (sub_texture);
}

static gboolean
validate_part (TestState *state,
               int xpos, int ypos,
               int width, int height,
               const ClutterColor *color)
{
  int x, y;
  gboolean pass = TRUE;
  guchar *pixels, *p;

  p = pixels = clutter_stage_read_pixels (CLUTTER_STAGE (state->stage),
                                          xpos + TEST_INSET,
                                          ypos + TEST_INSET,
                                          width - TEST_INSET - 2,
                                          height - TEST_INSET - 2);

  /* Check whether the center of each division is the right color */
  for (y = 0; y < height - TEST_INSET - 2; y++)
    for (x = 0; x < width - TEST_INSET - 2; x++)
      {
        if (p[0] != color->red ||
            p[1] != color->green ||
            p[2] != color->blue)
          pass = FALSE;

        p += 4;
      }

  return pass;
}

static CoglHandle
create_test_texture (void)
{
  CoglHandle tex;
  guint8 *data = g_malloc (256 * 256 * 4), *p = data;
  int x, y;

  /* Create a texture that is 256x256 where the red component ranges
     from 0->255 along the x axis and the green component ranges from
     0->255 along the y axis. The blue and alpha components are all
     255 */
  for (y = 0; y < 256; y++)
    for (x = 0; x < 256; x++)
      {
        *(p++) = x;
        *(p++) = y;
        *(p++) = 255;
        *(p++) = 255;
      }

  tex = cogl_texture_new_from_data (256, 256, COGL_TEXTURE_NONE,
                                    COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                    COGL_PIXEL_FORMAT_ANY,
                                    256 * 4,
                                    data);

  g_free (data);

  return tex;
}

static guint8 *
create_update_data (void)
{
  guint8 *data = g_malloc (256 * 256 * 4), *p = data;
  int x, y;

  /* Create some image data that is 256x256 where the blue component
     ranges from 0->255 along the x axis and the alpha component
     ranges from 0->255 along the y axis. The red and green components
     are all zero */
  for (y = 0; y < 256; y++)
    for (x = 0; x < 256; x++)
      {
        *(p++) = 0;
        *(p++) = 0;
        *(p++) = x;
        *(p++) = y;
      }

  return data;
}

static void
validate_result (TestState *state)
{
  int i, division_num, x, y;
  CoglHandle sub_texture, test_tex;
  guchar *texture_data, *p;
  gint tex_width, tex_height;

  /* Sub texture of the bottom right corner of the texture */
  g_assert (validate_part (state, 0, 0, DIVISION_WIDTH, DIVISION_HEIGHT,
                           corner_colors +
                           (SOURCE_DIVISIONS_Y - 1) * SOURCE_DIVISIONS_X +
                           SOURCE_DIVISIONS_X - 1));

  /* Sub texture of the top half repeated horizontally */
  for (i = 0; i < 2; i++)
    for (division_num = 0; division_num < SOURCE_DIVISIONS_X; division_num++)
      g_assert (validate_part (state,
                               i * SOURCE_SIZE + division_num * DIVISION_WIDTH,
                               SOURCE_SIZE,
                               DIVISION_WIDTH, DIVISION_HEIGHT,
                               corner_colors + division_num));

  /* Sub texture that repeats the texture (the opposite of a sub-texture?) */
  for (y = 0; y < SOURCE_DIVISIONS_Y * 2; y++)
    for (x = 0; x < SOURCE_DIVISIONS_X * 2; x++)
      {
        guint color_num = (y % SOURCE_DIVISIONS_Y * SOURCE_DIVISIONS_X +
                           x % SOURCE_DIVISIONS_X);
        g_assert (validate_part (state,
                                 x * DIVISION_WIDTH,
                                 y * DIVISION_WIDTH + SOURCE_SIZE * 2,
                                 DIVISION_WIDTH, DIVISION_HEIGHT,
                                 corner_colors + color_num));
      }

  /* Try reading back the texture data */
  sub_texture = cogl_texture_new_from_sub_texture (state->tex,
                                                   0.25f, 0.25f, 0.75f, 0.75f);
  tex_width = cogl_texture_get_width (sub_texture);
  tex_height = cogl_texture_get_height (sub_texture);
  p = texture_data = g_malloc (tex_width * tex_height * 4);
  cogl_texture_get_data (sub_texture, COGL_PIXEL_FORMAT_RGBA_8888,
                         tex_width * 4,
                         texture_data);
  for (y = 0; y < tex_height; y++)
    for (x = 0; x < tex_width; x++)
      {
        int div_x = ((x * SOURCE_SIZE / 2 / tex_width + SOURCE_SIZE / 4) /
                     DIVISION_WIDTH);
        int div_y = ((y * SOURCE_SIZE / 2 / tex_height + SOURCE_SIZE / 4) /
                     DIVISION_HEIGHT);
        const ClutterColor *color = (corner_colors + div_x +
                                     div_y * SOURCE_DIVISIONS_X);
        g_assert (p[0] == color->red);
        g_assert (p[1] == color->green);
        g_assert (p[2] == color->blue);
        p += 4;
      }
  g_free (texture_data);
  cogl_handle_unref (sub_texture);

  /* Try reading back the repeated texture data */
  sub_texture = cogl_texture_new_from_sub_texture (state->tex,
                                                   0.0f, 0.0f, 2.0f, 2.0f);
  tex_width = cogl_texture_get_width (sub_texture);
  tex_height = cogl_texture_get_height (sub_texture);
  p = texture_data = g_malloc (tex_width * tex_height * 4);
  cogl_texture_get_data (sub_texture, COGL_PIXEL_FORMAT_RGBA_8888,
                         tex_width * 4,
                         texture_data);
  for (y = 0; y < tex_height; y++)
    for (x = 0; x < tex_width; x++)
      {
        int div_x = x / DIVISION_WIDTH % SOURCE_DIVISIONS_X;
        int div_y = y / DIVISION_HEIGHT % SOURCE_DIVISIONS_Y;
        const ClutterColor *color = (corner_colors + div_x +
                                     div_y * SOURCE_DIVISIONS_X);
        g_assert (p[0] == color->red);
        g_assert (p[1] == color->green);
        g_assert (p[2] == color->blue);
        p += 4;
      }
  g_free (texture_data);
  cogl_handle_unref (sub_texture);

  /* Create a 256x256 test texture */
  test_tex = create_test_texture ();
  /* Create a sub texture the views the bottom right and top left of
     the texture by wrapping around */
  sub_texture = cogl_texture_new_from_sub_texture (test_tex,
                                                   0.5f, 0.5f, 1.5f, 1.5f);
  /* Update the center of the sub texture */
  texture_data = create_update_data ();
  cogl_texture_set_region (sub_texture, 0, 0, 64, 64, 128, 128, 256, 256,
                           COGL_PIXEL_FORMAT_RGBA_8888_PRE, 256 * 4,
                           texture_data);
  g_free (texture_data);
  cogl_handle_unref (sub_texture);
  /* Get the texture data */
  p = texture_data = g_malloc (256 * 256 * 4);
  cogl_texture_get_data (test_tex, COGL_PIXEL_FORMAT_RGBA_8888,
                         256 * 4, texture_data);

  /* Verify the texture data */
  for (y = 0; y < 256; y++)
    for (x = 0; x < 256; x++)
      {
        /* If we're in the center of the subregion.. */
        if ((x < 64 || x >= 192) && (y < 64 || y >= 192))
          {
            g_assert ((*p++) == 0);
            g_assert ((*p++) == 0);
            g_assert ((*p++) == ((x + 64) & 0xff));
            g_assert ((*p++) == ((y + 64) & 0xff));
          }
        else
          {
            g_assert ((*p++) == x);
            g_assert ((*p++) == y);
            g_assert ((*p++) == 255);
            g_assert ((*p++) == 255);
          }
      }
  g_free (texture_data);
  cogl_handle_unref (test_tex);

  /* Comment this out to see what the test paints */
  clutter_main_quit ();
}

static void
on_paint (ClutterActor *actor, TestState *state)
{
  int frame_num;

  draw_frame (state);

  /* XXX: Experiments have shown that for some buggy drivers, when using
   * glReadPixels there is some kind of race, so we delay our test for a
   * few frames and a few seconds:
   */
  /* Need to increment frame first because clutter_stage_read_pixels
     fires a redraw */
  frame_num = state->frame++;
  if (frame_num == 2)
    validate_result (state);
  else if (frame_num < 2)
    g_usleep (G_USEC_PER_SEC);
}

static gboolean
queue_redraw (gpointer stage)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  return TRUE;
}

void
test_cogl_sub_texture (TestConformSimpleFixture *fixture,
                       gconstpointer data)
{
  TestState state;
  guint idle_source;
  guint paint_handler;

  state.frame = 0;

  state.stage = clutter_stage_get_default ();
  state.tex = create_source ();

  clutter_stage_set_color (CLUTTER_STAGE (state.stage), &stage_color);

  /* We force continuous redrawing of the stage, since we need to skip
   * the first few frames, and we wont be doing anything else that
   * will trigger redrawing. */
  idle_source = g_idle_add (queue_redraw, state.stage);

  paint_handler = g_signal_connect_after (state.stage, "paint",
                                          G_CALLBACK (on_paint), &state);

  clutter_actor_show_all (state.stage);

  clutter_main ();

  g_source_remove (idle_source);
  g_signal_handler_disconnect (state.stage, paint_handler);

  cogl_handle_unref (state.tex);

  /* Remove all of the actors from the stage */
  clutter_container_foreach (CLUTTER_CONTAINER (state.stage),
                             (ClutterCallback) clutter_actor_destroy,
                             NULL);

  if (g_test_verbose ())
    g_print ("OK\n");
}

