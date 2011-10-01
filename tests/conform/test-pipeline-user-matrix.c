#include <cogl/cogl.h>

#include <string.h>

#include "test-utils.h"

typedef struct _TestState
{
  int width;
  int height;
} TestState;

static void
validate_result (TestState *state)
{
  guint32 *pixels, *p;
  char *screen_pixel;
  const char *intended_pixel = "#ffffff";

  /* The textures are setup so that when added together with the
     correct matrices then all of the pixels should be white. We can
     verify this by reading back the entire stage */
  pixels = g_malloc (state->width * state->height * 4);

  cogl_read_pixels (0, 0, state->width, state->height,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                    (guint8 *)pixels);

  for (p = pixels; p < pixels + state->width * state->height; p++)
    {
      screen_pixel = g_strdup_printf ("#%06x", GUINT32_FROM_BE (*p) >> 8);
      g_assert_cmpstr (screen_pixel, ==, intended_pixel);
      g_free (screen_pixel);
    }
}

static void
paint (TestState *state)
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
  CoglColor bg;
  CoglHandle tex0, tex1;
  CoglPipeline *pipeline;
  CoglMatrix matrix;
  GError *error = NULL;

  cogl_ortho (0, state->width, /* left, right */
              state->height, 0, /* bottom, top */
              -1, 100 /* z near, far */);

  cogl_color_init_from_4ub (&bg, 0, 0, 0, 255);
  cogl_clear (&bg, COGL_BUFFER_BIT_COLOR);

  cogl_matrix_init_identity (&matrix);
  cogl_set_modelview_matrix (&matrix);

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
  if (!cogl_pipeline_set_layer_combine (pipeline, 1,
					"RGBA=ADD(PREVIOUS, TEXTURE)",
					&error))
    {
      g_warning ("Error setting blend string: %s", error->message);
      g_assert_not_reached ();
    }

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
  cogl_rectangle (0, 0, state->width, state->height);

  cogl_handle_unref (tex1);
  cogl_handle_unref (tex0);
  cogl_object_unref (pipeline);
}

void
test_cogl_pipeline_user_matrix (TestUtilsGTestFixture *fixture,
                                void *data)
{
  TestUtilsSharedState *shared_state = data;
  TestState state;

  state.width = cogl_framebuffer_get_width (shared_state->fb);
  state.height = cogl_framebuffer_get_height (shared_state->fb);

  paint (&state);
  validate_result (&state);

  if (g_test_verbose ())
    g_print ("OK\n");
}
