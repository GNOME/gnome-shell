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
  uint32_t *pixels, *p;
  char *screen_pixel;
  const char *intended_pixel = "#ffffff";

  /* The textures are setup so that when added together with the
     correct matrices then all of the pixels should be white. We can
     verify this by reading back the entire stage */
  pixels = g_malloc (state->width * state->height * 4);

  cogl_framebuffer_read_pixels (test_fb, 0, 0, state->width, state->height,
                                COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                (uint8_t *)pixels);

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
  uint8_t data0[] = {
    0xff, 0x00, 0x00, /* red -> becomes bottom left */
    0x00, 0xff, 0x00, /* green -> becomes bottom right */
    0x00, 0x00, 0xff, /* blue -> becomes top left */
    0xff, 0x00, 0xff  /* magenta -> becomes top right */
  };
  /* This texture is painted mirrored about the y-axis */
  uint8_t data1[] = {
    0x00, 0xff, 0x00, /* green -> becomes top right */
    0xff, 0xff, 0x00, /* yellow -> becomes top left */
    0xff, 0x00, 0xff, /* magenta -> becomes bottom right */
    0x00, 0xff, 0xff  /* cyan -> becomes bottom left */
  };
  CoglTexture *tex0, *tex1;
  CoglPipeline *pipeline;
  CoglMatrix matrix;
  CoglError *error = NULL;

  cogl_framebuffer_orthographic (test_fb,
                                 0, 0,
                                 state->width,
                                 state->height,
                                 -1,
                                 100);

  cogl_framebuffer_clear4f (test_fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 1);

  cogl_matrix_init_identity (&matrix);
  cogl_framebuffer_set_modelview_matrix (test_fb, &matrix);

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

  pipeline = cogl_pipeline_new (test_ctx);

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

  cogl_framebuffer_draw_rectangle (test_fb,
                                   pipeline,
                                   0, 0,
                                   state->width, state->height);

  cogl_object_unref (tex1);
  cogl_object_unref (tex0);
  cogl_object_unref (pipeline);
}

void
test_pipeline_user_matrix (void)
{
  TestState state;

  state.width = cogl_framebuffer_get_width (test_fb);
  state.height = cogl_framebuffer_get_height (test_fb);

  paint (&state);
  validate_result (&state);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}
