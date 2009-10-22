
#include <clutter/clutter.h>
#include <cogl/cogl.h>

#include "test-conform-common.h"

#define RED 0
#define GREEN 1
#define BLUE 2

#define DRAW_BUFFER_WIDTH  640
#define DRAW_BUFFER_HEIGHT 480

static const ClutterColor stage_color = { 0x0, 0x0, 0x0, 0xff };


static void
on_paint (ClutterActor *actor, void *state)
{
  float saved_viewport[4];
  CoglMatrix saved_projection;
  CoglMatrix projection;
  CoglMatrix modelview;
  guchar *data;
  CoglHandle tex;
  CoglHandle offscreen;
  guint32 *pixels;

  /* Save the Clutter viewport/matrices and load identity matrices */

  cogl_get_viewport (saved_viewport);
  cogl_get_projection_matrix (&saved_projection);
  cogl_push_matrix ();

  cogl_matrix_init_identity (&projection);
  cogl_matrix_init_identity (&modelview);

  cogl_set_projection_matrix (&projection);
  cogl_set_modelview_matrix (&modelview);

  /* All offscreen rendering is done upside down so the first thing we
   * verify is reading back grid of colors from a CoglOffscreen draw buffer
   */

  data = g_malloc (DRAW_BUFFER_WIDTH * 4 * DRAW_BUFFER_HEIGHT);
  tex = cogl_texture_new_from_data (DRAW_BUFFER_WIDTH, DRAW_BUFFER_HEIGHT,
                                    COGL_TEXTURE_NO_SLICING,
                                    COGL_PIXEL_FORMAT_RGBA_8888, /* data fmt */
                                    COGL_PIXEL_FORMAT_ANY, /* internal fmt */
                                    DRAW_BUFFER_WIDTH * 4, /* rowstride */
                                    data);
  g_free (data);
  offscreen = cogl_offscreen_new_to_texture (tex);

  cogl_push_draw_buffer ();
  cogl_set_draw_buffer (0 /* unused */, offscreen);

  /* red, top left */
  cogl_set_source_color4ub (0xff, 0x00, 0x00, 0xff);
  cogl_rectangle (-1, 1, 0, 0);
  /* green, top right */
  cogl_set_source_color4ub (0x00, 0xff, 0x00, 0xff);
  cogl_rectangle (0, 1, 1, 0);
  /* blue, bottom left */
  cogl_set_source_color4ub (0x00, 0x00, 0xff, 0xff);
  cogl_rectangle (-1, 0, 0, -1);
  /* white, bottom right */
  cogl_set_source_color4ub (0xff, 0xff, 0xff, 0xff);
  cogl_rectangle (0, 0, 1, -1);

  pixels = g_malloc0 (DRAW_BUFFER_WIDTH * 4 * DRAW_BUFFER_HEIGHT);
  cogl_read_pixels (0, 0, DRAW_BUFFER_WIDTH, DRAW_BUFFER_HEIGHT,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888,
                    (guchar *)pixels);

  g_assert (pixels[0] == 0xff0000ff);
  g_assert (pixels[DRAW_BUFFER_WIDTH - 1] == 0xff00ff00);
  g_assert (pixels[(DRAW_BUFFER_HEIGHT - 1) * DRAW_BUFFER_WIDTH] == 0xffff0000);
  g_assert (pixels[(DRAW_BUFFER_HEIGHT - 1) * DRAW_BUFFER_WIDTH +
                   DRAW_BUFFER_WIDTH - 1] == 0xffffffff);
  g_free (pixels);

  cogl_pop_draw_buffer ();
  cogl_handle_unref (offscreen);

  /* Now verify reading back from an onscreen draw buffer...
   */

  cogl_set_source_texture (tex);
  cogl_rectangle (-1, 1, 1, -1);

  pixels = g_malloc0 (DRAW_BUFFER_WIDTH * 4 * DRAW_BUFFER_HEIGHT);
  cogl_read_pixels (0, 0, DRAW_BUFFER_WIDTH, DRAW_BUFFER_HEIGHT,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888,
                    (guchar *)pixels);

  g_assert (pixels[0] == 0xff0000ff);
  g_assert (pixels[DRAW_BUFFER_WIDTH - 1] == 0xff00ff00);
  g_assert (pixels[(DRAW_BUFFER_HEIGHT - 1) * DRAW_BUFFER_WIDTH] == 0xffff0000);
  g_assert (pixels[(DRAW_BUFFER_HEIGHT - 1) * DRAW_BUFFER_WIDTH +
                   DRAW_BUFFER_WIDTH - 1] == 0xffffffff);
  g_free (pixels);

  cogl_handle_unref (tex);


  /* Comment this out if you want visual feedback of what this test
   * paints.
   */
  clutter_main_quit ();
}

static gboolean
queue_redraw (gpointer stage)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  return TRUE;
}

void
test_cogl_readpixels (TestConformSimpleFixture *fixture,
                      gconstpointer data)
{
  guint idle_source;
  ClutterActor *stage;

  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_actor_set_size (stage, DRAW_BUFFER_WIDTH, DRAW_BUFFER_HEIGHT);

  /* We force continuous redrawing of the stage, since we need to skip
   * the first few frames, and we wont be doing anything else that
   * will trigger redrawing. */
  idle_source = g_idle_add (queue_redraw, stage);
  g_signal_connect_after (stage, "paint", G_CALLBACK (on_paint), NULL);

  clutter_actor_show (stage);
  clutter_main ();

  g_source_remove (idle_source);

  /* Remove all of the actors from the stage */
  clutter_container_foreach (CLUTTER_CONTAINER (stage),
                             (ClutterCallback) clutter_actor_destroy,
                             NULL);

  if (g_test_verbose ())
    g_print ("OK\n");
}

