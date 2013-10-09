#include <cogl/cogl.h>
#include <string.h>

#include "test-utils.h"

#define SOURCE_SIZE        32
#define SOURCE_DIVISIONS_X 2
#define SOURCE_DIVISIONS_Y 2
#define DIVISION_WIDTH     (SOURCE_SIZE / SOURCE_DIVISIONS_X)
#define DIVISION_HEIGHT    (SOURCE_SIZE / SOURCE_DIVISIONS_Y)

#define TEST_INSET         1

static const uint32_t
corner_colors[SOURCE_DIVISIONS_X * SOURCE_DIVISIONS_Y] =
  {
    0xff0000ff, /* red top left */
    0x00ff00ff, /* green top right */
    0x0000ffff, /* blue bottom left */
    0xff00ffff  /* purple bottom right */
  };

typedef struct _TestState
{
  CoglTexture2D *tex;
} TestState;

static CoglTexture2D *
create_source (TestState *state)
{
  int dx, dy;
  uint8_t *data = g_malloc (SOURCE_SIZE * SOURCE_SIZE * 4);
  CoglTexture2D *tex;

  /* Create a texture with a different coloured rectangle at each
     corner */
  for (dy = 0; dy < SOURCE_DIVISIONS_Y; dy++)
    for (dx = 0; dx < SOURCE_DIVISIONS_X; dx++)
      {
        uint8_t *p = (data + dy * DIVISION_HEIGHT * SOURCE_SIZE * 4 +
                     dx * DIVISION_WIDTH * 4);
        int x, y;

        for (y = 0; y < DIVISION_HEIGHT; y++)
          {
            for (x = 0; x < DIVISION_WIDTH; x++)
              {
                uint32_t color = GUINT32_FROM_BE (corner_colors[dx + dy * SOURCE_DIVISIONS_X]);
                memcpy (p, &color, 4);
                p += 4;
              }

            p += SOURCE_SIZE * 4 - DIVISION_WIDTH * 4;
          }
      }

  tex = cogl_texture_2d_new_from_data (test_ctx,
                                       SOURCE_SIZE, SOURCE_SIZE,
                                       COGL_PIXEL_FORMAT_RGBA_8888,
                                       COGL_PIXEL_FORMAT_ANY,
                                       SOURCE_SIZE * 4,
                                       data,
                                       NULL);
  return tex;
}

static CoglTexture2D *
create_test_texture (TestState *state)
{
  CoglTexture2D *tex;
  uint8_t *data = g_malloc (256 * 256 * 4), *p = data;
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

  tex = cogl_texture_2d_new_from_data (test_ctx,
                                       256, 256,
                                       COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                       COGL_PIXEL_FORMAT_ANY,
                                       256 * 4,
                                       data,
                                       NULL);
  g_free (data);

  return tex;
}

static void
paint (TestState *state)
{
  CoglTexture2D *full_texture;
  CoglSubTexture *sub_texture, *sub_sub_texture;
  CoglPipeline *pipeline = cogl_pipeline_new (test_ctx);

  /* Create a sub texture of the bottom right quarter of the texture */
  sub_texture = cogl_sub_texture_new (test_ctx,
                                      state->tex,
                                      DIVISION_WIDTH,
                                      DIVISION_HEIGHT,
                                      DIVISION_WIDTH,
                                      DIVISION_HEIGHT);

  /* Paint it */
  cogl_pipeline_set_layer_texture (pipeline, 0, sub_texture);
  cogl_object_unref (sub_texture);
  cogl_framebuffer_draw_rectangle (test_fb, pipeline,
                                   0.0f, 0.0f, DIVISION_WIDTH, DIVISION_HEIGHT);


  /* Repeat a sub texture of the top half of the full texture. This is
     documented to be undefined so it doesn't technically have to work
     but it will with the current implementation */
  sub_texture = cogl_sub_texture_new (test_ctx,
                                      state->tex,
                                      0, 0,
                                      SOURCE_SIZE,
                                      DIVISION_HEIGHT);
  cogl_pipeline_set_layer_texture (pipeline, 0, sub_texture);
  cogl_object_unref (sub_texture);
  cogl_framebuffer_draw_textured_rectangle (test_fb, pipeline,
                                            0.0f,
                                            SOURCE_SIZE,
                                            SOURCE_SIZE * 2.0f,
                                            SOURCE_SIZE * 1.5f,
                                            0.0f, 0.0f,
                                            2.0f, 1.0f);

  /* Create a sub texture of a sub texture */
  full_texture = create_test_texture (state);
  sub_texture = cogl_sub_texture_new (test_ctx,
                                      full_texture,
                                      20, 10, 30, 20);
  cogl_object_unref (full_texture);
  sub_sub_texture = cogl_sub_texture_new (test_ctx,
                                          sub_texture,
                                          20, 10, 10, 10);
  cogl_object_unref (sub_texture);
  cogl_pipeline_set_layer_texture (pipeline, 0, sub_sub_texture);
  cogl_object_unref (sub_sub_texture);
  cogl_framebuffer_draw_rectangle (test_fb, pipeline,
                                   0.0f, SOURCE_SIZE * 2.0f,
                                   10.0f, SOURCE_SIZE * 2.0f + 10.0f);

  cogl_object_unref (pipeline);
}

static void
validate_part (int xpos, int ypos,
               int width, int height,
               uint32_t color)
{
  test_utils_check_region (test_fb,
                           xpos + TEST_INSET,
                           ypos + TEST_INSET,
                           width - TEST_INSET - 2,
                           height - TEST_INSET - 2,
                           color);
}

static uint8_t *
create_update_data (void)
{
  uint8_t *data = g_malloc (256 * 256 * 4), *p = data;
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
  CoglTexture2D *test_tex;
  CoglSubTexture *sub_texture;
  uint8_t *texture_data, *p;
  int tex_width, tex_height;

  /* Sub texture of the bottom right corner of the texture */
  validate_part (0, 0, DIVISION_WIDTH, DIVISION_HEIGHT,
                 corner_colors[
                 (SOURCE_DIVISIONS_Y - 1) * SOURCE_DIVISIONS_X +
                 SOURCE_DIVISIONS_X - 1]);

  /* Sub texture of the top half repeated horizontally */
  for (i = 0; i < 2; i++)
    for (division_num = 0; division_num < SOURCE_DIVISIONS_X; division_num++)
      validate_part (i * SOURCE_SIZE + division_num * DIVISION_WIDTH,
                     SOURCE_SIZE,
                     DIVISION_WIDTH, DIVISION_HEIGHT,
                     corner_colors[division_num]);

  /* Sub sub texture */
  p = texture_data = g_malloc (10 * 10 * 4);
  cogl_flush ();
  cogl_framebuffer_read_pixels (test_fb,
                                0, SOURCE_SIZE * 2, 10, 10,
                                COGL_PIXEL_FORMAT_RGBA_8888,
                                p);
  for (y = 0; y < 10; y++)
    for (x = 0; x < 10; x++)
      {
        g_assert (*(p++) == x + 40);
        g_assert (*(p++) == y + 20);
        p += 2;
      }
  g_free (texture_data);

  /* Try reading back the texture data */
  sub_texture = cogl_sub_texture_new (test_ctx,
                                      state->tex,
                                      SOURCE_SIZE / 4,
                                      SOURCE_SIZE / 4,
                                      SOURCE_SIZE / 2,
                                      SOURCE_SIZE / 2);
  tex_width = cogl_texture_get_width (sub_texture);
  tex_height = cogl_texture_get_height (sub_texture);
  p = texture_data = g_malloc (tex_width * tex_height * 4);
  cogl_texture_get_data (sub_texture,
                         COGL_PIXEL_FORMAT_RGBA_8888,
                         tex_width * 4,
                         texture_data);
  for (y = 0; y < tex_height; y++)
    for (x = 0; x < tex_width; x++)
      {
        int div_x = ((x * SOURCE_SIZE / 2 / tex_width + SOURCE_SIZE / 4) /
                     DIVISION_WIDTH);
        int div_y = ((y * SOURCE_SIZE / 2 / tex_height + SOURCE_SIZE / 4) /
                     DIVISION_HEIGHT);
        uint32_t reference = corner_colors[div_x + div_y * SOURCE_DIVISIONS_X] >> 8;
        uint32_t color = GUINT32_FROM_BE (*((uint32_t *)p)) >> 8;
        g_assert (color == reference);
        p += 4;
      }
  g_free (texture_data);
  cogl_object_unref (sub_texture);

  /* Create a 256x256 test texture */
  test_tex = create_test_texture (state);
  /* Create a sub texture the views the center half of the texture */
  sub_texture = cogl_sub_texture_new (test_ctx,
                                      test_tex,
                                      64, 64, 128, 128);
  /* Update the center half of the sub texture */
  texture_data = create_update_data ();
  cogl_texture_set_region (sub_texture,
                           0, 0, 32, 32, 64, 64, 256, 256,
                           COGL_PIXEL_FORMAT_RGBA_8888_PRE, 256 * 4,
                           texture_data);
  g_free (texture_data);
  cogl_object_unref (sub_texture);
  /* Get the texture data */
  p = texture_data = g_malloc (256 * 256 * 4);
  cogl_texture_get_data (test_tex,
                         COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                         256 * 4, texture_data);

  /* Verify the texture data */
  for (y = 0; y < 256; y++)
    for (x = 0; x < 256; x++)
      {
        /* If we're in the center quarter */
        if (x >= 96 && x < 160 && y >= 96 && y < 160)
          {
            g_assert ((*p++) == 0);
            g_assert ((*p++) == 0);
            g_assert ((*p++) == x - 96);
            g_assert ((*p++) == y - 96);
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
  cogl_object_unref (test_tex);
}

void
test_sub_texture (void)
{
  TestState state;

  state.tex = create_source (&state);

  cogl_framebuffer_orthographic (test_fb,
                                 0, 0,
                                 cogl_framebuffer_get_width (test_fb),
                                 cogl_framebuffer_get_height (test_fb),
                                 -1,
                                 100);

  paint (&state);
  validate_result (&state);

  cogl_object_unref (state.tex);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}

