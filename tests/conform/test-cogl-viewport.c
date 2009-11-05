
#include <clutter/clutter.h>
#include <cogl/cogl.h>

#include "test-conform-common.h"

#define RED 0
#define GREEN 1
#define BLUE 2
#define ALPHA 3

#define DRAW_BUFFER_WIDTH  640
#define DRAW_BUFFER_HEIGHT 480

static const ClutterColor stage_color = { 0x0, 0x0, 0x0, 0xff };

static void
assert_region_color (int x,
                     int y,
                     int width,
                     int height,
                     guint8 red,
                     guint8 green,
                     guint8 blue,
                     guint8 alpha)
{
  guint8 *data = g_malloc0 (width * height * 4);
  cogl_read_pixels (x, y, width, height,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888,
                    data);
  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      {
        guint8 *pixel = &data[y*width*4 + x*4];
#if 1
        g_assert (pixel[RED] == red &&
                  pixel[GREEN] == green &&
                  pixel[BLUE] == blue &&
                  pixel[ALPHA] == alpha);
#endif
      }
  g_free (data);
}

static void
assert_rectangle_color_and_black_border (int x,
                                         int y,
                                         int width,
                                         int height,
                                         guint8 red,
                                         guint8 green,
                                         guint8 blue)
{
  /* check the rectangle itself... */
  assert_region_color (x, y, width, height, red, green, blue, 0xff);
  /* black to left of the rectangle */
  assert_region_color (x-10, y-10, 10, height+20, 0x00, 0x00, 0x00, 0xff);
  /* black to right of the rectangle */
  assert_region_color (x+width, y-10, 10, height+20, 0x00, 0x00, 0x00, 0xff);
  /* black above the rectangle */
  assert_region_color (x-10, y-10, width+20, 10, 0x00, 0x00, 0x00, 0xff);
  /* and black below the rectangle */
  assert_region_color (x-10, y+height, width+20, 10, 0x00, 0x00, 0x00, 0xff);
}


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
  CoglColor black;
  float x0;
  float y0;
  float width;
  float height;

  /* for clearing the offscreen draw buffer to black... */
  cogl_color_set_from_4ub (&black, 0x00, 0x00, 0x00, 0xff);

  cogl_get_viewport (saved_viewport);
  cogl_get_projection_matrix (&saved_projection);
  cogl_push_matrix ();

  cogl_matrix_init_identity (&projection);
  cogl_matrix_init_identity (&modelview);

  cogl_set_projection_matrix (&projection);
  cogl_set_modelview_matrix (&modelview);

  /* - Create a 100x200 viewport (i.e. smaller than the onscreen draw buffer)
   *   and position it a (20, 10) inside the draw buffer.
   * - Fill the whole viewport with a purple rectangle
   * - Verify that the draw buffer is black with a 100x200 purple rectangle at
   *   (20, 10)
   */
  cogl_set_viewport (20, /* x */
                     10, /* y */
                     100, /* width */
                     200); /* height */
  /* clear everything... */
  cogl_clear (&black, COGL_BUFFER_BIT_COLOR);
  /* fill the viewport with purple.. */
  cogl_set_source_color4ub (0xff, 0x00, 0xff, 0xff);
  cogl_rectangle (-1, 1, 1, -1);
  assert_rectangle_color_and_black_border (20, 10, 100, 200,
                                           0xff, 0x00, 0xff);


  /* - Create a viewport twice the size of the onscreen draw buffer with
   *   a negative offset positioning it at (-20, -10) relative to the
   *   buffer itself.
   * - Draw a 100x200 green rectangle at (40, 20) within the viewport (which
   *   is (20, 10) within the draw buffer)
   * - Verify that the draw buffer is black with a 100x200 green rectangle at
   *   (20, 10)
   */
  cogl_set_viewport (-20, /* x */
                     -10, /* y */
                     DRAW_BUFFER_WIDTH * 2, /* width */
                     DRAW_BUFFER_HEIGHT * 2); /* height */
  /* clear everything... */
  cogl_clear (&black, COGL_BUFFER_BIT_COLOR);
  /* draw a 100x200 green rectangle offset into the viewport such that its
   * top left corner should be found at (20, 10) in the offscreen buffer */
  /* (offset 40 pixels right from the left of the viewport) */
  x0 = -1.0f + (1.0f / DRAW_BUFFER_WIDTH) * 40.f;
  /* (offset 20 pixels down from the top of the viewport) */
  y0 = 1.0f - (1.0f / DRAW_BUFFER_HEIGHT) * 20.0f;
  width = (1.0f / DRAW_BUFFER_WIDTH) * 100;
  height = (1.0f / DRAW_BUFFER_HEIGHT) * 200;
  cogl_set_source_color4ub (0x00, 0xff, 0x00, 0xff);
  cogl_rectangle (x0, y0, x0 + width, y0 - height);
  assert_rectangle_color_and_black_border (20, 10, 100, 200,
                                           0x00, 0xff, 0x00);


  /* - Create a 200x400 viewport and position it a (20, 10) inside the draw
   *   buffer.
   * - Push a 100x200 window space clip rectangle at (20, 10)
   * - Fill the whole viewport with a blue rectangle
   * - Verify that the draw buffer is black with a 100x200 blue rectangle at
   *   (20, 10)
   */
  cogl_set_viewport (20, /* x */
                     10, /* y */
                     200, /* width */
                     400); /* height */
  /* clear everything... */
  cogl_clear (&black, COGL_BUFFER_BIT_COLOR);
  cogl_clip_push_window_rectangle (20, 10, 100, 200);
  /* fill the viewport with blue.. */
  cogl_set_source_color4ub (0x00, 0x00, 0xff, 0xff);
  cogl_rectangle (-1, 1, 1, -1);
  cogl_clip_pop ();
  assert_rectangle_color_and_black_border (20, 10, 100, 200,
                                           0x00, 0x00, 0xff);


  /* - Create a 200x400 viewport and position it a (20, 10) inside the draw
   *   buffer.
   * - Push a 100x200 model space clip rectangle at (20, 10) in the viewport
   *   (i.e. (40, 20) inside the draw buffer)
   * - Fill the whole viewport with a green rectangle
   * - Verify that the draw buffer is black with a 100x200 green rectangle at
   *   (40, 20)
   */
  cogl_set_viewport (20, /* x */
                     10, /* y */
                     200, /* width */
                     400); /* height */
  /* clear everything... */
  cogl_clear (&black, COGL_BUFFER_BIT_COLOR);
  /* figure out where to position our clip rectangle in model space
   * coordinates... */
  /* (offset 40 pixels right from the left of the viewport) */
  x0 = -1.0f + (2.0f / 200) * 20.f;
  /* (offset 20 pixels down from the top of the viewport) */
  y0 = 1.0f - (2.0f / 400) * 10.0f;
  width = (2.0f / 200) * 100;
  height = (2.0f / 400) * 200;
  /* add the clip rectangle... */
  cogl_push_matrix ();
  cogl_translate (x0 + (width/2.0), y0 - (height/2.0), 0);
  /* XXX: Rotate just enough to stop Cogl from converting our model space
   * rectangle into a window space rectangle.. */
  cogl_rotate (0.1, 0, 0, 1);
  cogl_clip_push_rectangle (-(width/2.0), -(height/2.0),
                            width/2.0, height/2.0);
  cogl_pop_matrix ();
  /* fill the viewport with green.. */
  cogl_set_source_color4ub (0x00, 0xff, 0x00, 0xff);
  cogl_rectangle (-1, 1, 1, -1);
  cogl_clip_pop ();
  assert_rectangle_color_and_black_border (40, 20, 100, 200,
                                           0x00, 0xff, 0x00);


  /* Set the viewport to something specific so we can verify that it gets
   * restored after we are done testing with an offscreen draw buffer... */
  cogl_set_viewport (20, 10, 100, 200);

  /*
   * Next test offscreen drawing...
   */
  cogl_push_draw_buffer ();

  data = g_malloc (DRAW_BUFFER_WIDTH * 4 * DRAW_BUFFER_HEIGHT);
  tex = cogl_texture_new_from_data (DRAW_BUFFER_WIDTH, DRAW_BUFFER_HEIGHT,
                                    COGL_TEXTURE_NO_SLICING,
                                    COGL_PIXEL_FORMAT_RGBA_8888, /* data fmt */
                                    COGL_PIXEL_FORMAT_ANY, /* internal fmt */
                                    DRAW_BUFFER_WIDTH * 4, /* rowstride */
                                    data);
  g_free (data);
  offscreen = cogl_offscreen_new_to_texture (tex);

  cogl_set_draw_buffer (0 /* unused */, offscreen);


  /* - Create a 100x200 viewport (i.e. smaller than the offscreen draw buffer)
   *   and position it a (20, 10) inside the draw buffer.
   * - Fill the whole viewport with a blue rectangle
   * - Verify that the draw buffer is black with a 100x200 blue rectangle at
   *   (20, 10)
   */
  cogl_set_viewport (20, /* x */
                     10, /* y */
                     100, /* width */
                     200); /* height */
  /* clear everything... */
  cogl_clear (&black, COGL_BUFFER_BIT_COLOR);
  /* fill the viewport with blue.. */
  cogl_set_source_color4ub (0x00, 0x00, 0xff, 0xff);
  cogl_rectangle (-1, 1, 1, -1);
  assert_rectangle_color_and_black_border (20, 10, 100, 200,
                                           0x00, 0x00, 0xff);


  /* - Create a viewport twice the size of the offscreen draw buffer with
   *   a negative offset positioning it at (-20, -10) relative to the
   *   buffer itself.
   * - Draw a 100x200 red rectangle at (40, 20) within the viewport (which
   *   is (20, 10) within the draw buffer)
   * - Verify that the draw buffer is black with a 100x200 red rectangle at
   *   (20, 10)
   */
  cogl_set_viewport (-20, /* x */
                     -10, /* y */
                     DRAW_BUFFER_WIDTH * 2, /* width */
                     DRAW_BUFFER_HEIGHT * 2); /* height */
  /* clear everything... */
  cogl_clear (&black, COGL_BUFFER_BIT_COLOR);
  /* draw a 100x200 red rectangle offset into the viewport such that its
   * top left corner should be found at (20, 10) in the offscreen buffer */
  /* (offset 40 pixels right from the left of the viewport) */
  x0 = -1.0f + (1.0f / DRAW_BUFFER_WIDTH) * 40.f;
  /* (offset 20 pixels down from the top of the viewport) */
  y0 = 1.0f - (1.0f / DRAW_BUFFER_HEIGHT) * 20.0f;
  width = (1.0f / DRAW_BUFFER_WIDTH) * 100;
  height = (1.0f / DRAW_BUFFER_HEIGHT) * 200;
  cogl_set_source_color4ub (0xff, 0x00, 0x00, 0xff);
  cogl_rectangle (x0, y0, x0 + width, y0 - height);
  assert_rectangle_color_and_black_border (20, 10, 100, 200,
                                           0xff, 0x00, 0x00);


  /* - Create a 200x400 viewport and position it a (20, 10) inside the draw
   *   buffer.
   * - Push a 100x200 window space clip rectangle at (20, 10)
   * - Fill the whole viewport with a blue rectangle
   * - Verify that the draw buffer is black with a 100x200 blue rectangle at
   *   (20, 10)
   */
  cogl_set_viewport (20, /* x */
                     10, /* y */
                     200, /* width */
                     400); /* height */
  /* clear everything... */
  cogl_clear (&black, COGL_BUFFER_BIT_COLOR);
  cogl_clip_push_window_rectangle (20, 10, 100, 200);
  /* fill the viewport with blue.. */
  cogl_set_source_color4ub (0x00, 0x00, 0xff, 0xff);
  cogl_rectangle (-1, 1, 1, -1);
  cogl_clip_pop ();
  assert_rectangle_color_and_black_border (20, 10, 100, 200,
                                           0x00, 0x00, 0xff);


  /* - Create a 200x400 viewport and position it a (20, 10) inside the draw
   *   buffer.
   * - Push a 100x200 model space clip rectangle at (20, 10) in the viewport
   *   (i.e. (40, 20) inside the draw buffer)
   * - Fill the whole viewport with a green rectangle
   * - Verify that the draw buffer is black with a 100x200 green rectangle at
   *   (40, 20)
   */
  cogl_set_viewport (20, /* x */
                     10, /* y */
                     200, /* width */
                     400); /* height */
  /* clear everything... */
  cogl_clear (&black, COGL_BUFFER_BIT_COLOR);
  /* figure out where to position our clip rectangle in model space
   * coordinates... */
  /* (offset 40 pixels right from the left of the viewport) */
  x0 = -1.0f + (2.0f / 200) * 20.f;
  /* (offset 20 pixels down from the top of the viewport) */
  y0 = 1.0f - (2.0f / 400) * 10.0f;
  width = (2.0f / 200) * 100;
  height = (2.0f / 400) * 200;
  /* add the clip rectangle... */
  cogl_push_matrix ();
  cogl_translate (x0 + (width/2.0), y0 - (height/2.0), 0);
  /* XXX: Rotate just enough to stop Cogl from converting our model space
   * rectangle into a window space rectangle.. */
  cogl_rotate (0.1, 0, 0, 1);
  cogl_clip_push_rectangle (-(width/2.0), -(height/2.0),
                            width/2, height/2);
  cogl_pop_matrix ();
  /* fill the viewport with green.. */
  cogl_set_source_color4ub (0x00, 0xff, 0x00, 0xff);
  cogl_rectangle (-1, 1, 1, -1);
  cogl_clip_pop ();
  assert_rectangle_color_and_black_border (40, 20, 100, 200,
                                           0x00, 0xff, 0x00);


  /* Set the viewport to something obscure to verify that it gets
   * replace when we switch back to the onscreen draw buffer... */
  cogl_set_viewport (0, 0, 10, 10);

  cogl_pop_draw_buffer ();
  cogl_handle_unref (offscreen);

  /*
   * Verify that the previous onscreen draw buffer's viewport was restored
   * by drawing a white rectangle across the whole viewport. This should
   * draw a 100x200 rectangle at (20,10) relative to the onscreen draw
   * buffer...
   */
  cogl_clear (&black, COGL_BUFFER_BIT_COLOR);
  cogl_set_source_color4ub (0xff, 0xff, 0xff, 0xff);
  cogl_rectangle (-1, 1, 1, -1);
  assert_rectangle_color_and_black_border (20, 10, 100, 200,
                                           0xff, 0xff, 0xff);


  /* Uncomment to display the last contents of the offscreen draw buffer */
#if 1
  cogl_matrix_init_identity (&projection);
  cogl_matrix_init_identity (&modelview);
  cogl_set_viewport (0, 0, DRAW_BUFFER_WIDTH, DRAW_BUFFER_HEIGHT);
  cogl_set_projection_matrix (&projection);
  cogl_set_modelview_matrix (&modelview);
  cogl_set_source_texture (tex);
  cogl_rectangle (-1, 1, 1, -1);
#endif

  cogl_handle_unref (tex);

  /* Finally restore the stage's original state... */
  cogl_pop_matrix ();
  cogl_set_projection_matrix (&saved_projection);
  cogl_set_viewport (saved_viewport[0], saved_viewport[1],
                     saved_viewport[2], saved_viewport[3]);


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
test_cogl_viewport (TestConformSimpleFixture *fixture,
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

