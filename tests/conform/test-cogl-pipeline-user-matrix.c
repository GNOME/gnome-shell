#include <clutter/clutter.h>
#include <string.h>

#include "test-conform-common.h"

static const ClutterColor stage_color = { 0x00, 0x00, 0x00, 0xff };

static void
paint_cb (ClutterActor *stage)
{
  /* This texture is painted mirrored around the x-axis */
  guint8 data0[] = {
    0xff, 0x00, 0x00, /* red -> becomes bottom left */
    0x00, 0xff, 0x00, /* green -> becomes bottom right */
    0x00, 0x00, 0xff, /* blue -> becomes top left */
    0xff, 0x00, 0xff  /* magenta -> becomes top right */
  };
  /* This texture is painted mirrored about the y-axis */
  guint8 data1[] = {
    0x00, 0xff, 0x00, /* green -> becomes top right */
    0xff, 0xff, 0x00, /* yellow -> becomes top left */
    0xff, 0x00, 0xff, /* magenta -> becomes bottom right */
    0x00, 0xff, 0xff  /* cyan -> becomes bottom left */
  };
  CoglHandle tex0, tex1;
  CoglPipeline *pipeline;
  CoglMatrix matrix;
  int width, height;
  guint8 *pixels, *p;

  width = clutter_actor_get_width (stage);
  height = clutter_actor_get_height (stage);

  tex0 = cogl_texture_new_from_data (2, 2,
                                     COGL_TEXTURE_NO_ATLAS,
                                     COGL_PIXEL_FORMAT_RGB_888,
                                     COGL_PIXEL_FORMAT_ANY,
                                     6,
                                     data0);
  tex1 = cogl_texture_new_from_data (2, 2,
                                     COGL_TEXTURE_NO_ATLAS,
                                     COGL_PIXEL_FORMAT_RGB_888,
                                     COGL_PIXEL_FORMAT_ANY,
                                     6,
                                     data1);

  pipeline = cogl_pipeline_new ();

  /* Set the two textures as layers */
  cogl_pipeline_set_layer_texture (pipeline, 0, tex0);
  cogl_pipeline_set_layer_filters (pipeline, 0,
                                   COGL_PIPELINE_FILTER_NEAREST,
                                   COGL_PIPELINE_FILTER_NEAREST);
  cogl_pipeline_set_layer_texture (pipeline, 1, tex1);
  cogl_pipeline_set_layer_filters (pipeline, 1,
                                   COGL_PIPELINE_FILTER_NEAREST,
                                   COGL_PIPELINE_FILTER_NEAREST);

  /* Set a combine mode so that the two textures get added together */
  cogl_pipeline_set_layer_combine (pipeline, 1,
                                   "RGBA=ADD(PREVIOUS, TEXTURE)",
                                   NULL);

  /* Set a matrix on the first layer so that it will mirror about the y-axis */
  cogl_matrix_init_identity (&matrix);
  cogl_matrix_translate (&matrix, 0.0f, 1.0f, 0.0f);
  cogl_matrix_scale (&matrix, 1.0f, -1.0f, 1.0f);
  cogl_pipeline_set_layer_matrix (pipeline, 0, &matrix);

  /* Set a matrix on the second layer so that it will mirror about the x-axis */
  cogl_matrix_init_identity (&matrix);
  cogl_matrix_translate (&matrix, 1.0f, 0.0f, 0.0f);
  cogl_matrix_scale (&matrix, -1.0f, 1.0f, 1.0f);
  cogl_pipeline_set_layer_matrix (pipeline, 1, &matrix);

  cogl_set_source (pipeline);
  cogl_rectangle (0, 0, width, height);

  cogl_handle_unref (tex1);
  cogl_handle_unref (tex0);
  cogl_object_unref (pipeline);

  /* The textures are setup so that when added together with the
     correct matrices then all of the pixels should be white. We can
     verify this by reading back the entire stage */
  pixels = g_malloc (width * height * 4);

  cogl_read_pixels (0, 0, width, height,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                    pixels);

  for (p = pixels + width * height * 4; p > pixels;)
    {
      p -= 4;
      g_assert_cmpint (p[0], ==, 0xff);
      g_assert_cmpint (p[1], ==, 0xff);
      g_assert_cmpint (p[2], ==, 0xff);
    }

  g_free (pixels);

  clutter_main_quit ();
}

static gboolean
queue_redraw (gpointer stage)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  return TRUE;
}

void
test_cogl_pipeline_user_matrix (TestConformSimpleFixture *fixture,
                                gconstpointer data)
{
  ClutterActor *stage;
  guint idle_source;
  guint paint_handler;

  stage = clutter_stage_new ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

  /* We force continuous redrawing of the stage, since we need to skip
   * the first few frames, and we wont be doing anything else that
   * will trigger redrawing. */
  idle_source = g_idle_add (queue_redraw, stage);

  paint_handler = g_signal_connect_after (stage, "paint",
                                          G_CALLBACK (paint_cb),
                                          NULL);

  clutter_actor_show (stage);

  clutter_main ();

  g_source_remove (idle_source);
  g_signal_handler_disconnect (stage, paint_handler);

  clutter_actor_destroy (stage);

  if (g_test_verbose ())
    g_print ("OK\n");
}
