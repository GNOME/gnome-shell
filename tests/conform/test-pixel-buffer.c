#include <cogl/cogl.h>
#include <string.h>

#include "test-utils.h"

#define TILE_SIZE        32.0f

enum
{
  TILE_MAP,
  TILE_SET_DATA,
  NB_TILES,
  TILE_SET_REGION,
};

typedef struct test_tile
{
  uint8_t color[4];
  gfloat x, y;
  CoglBuffer *buffer;
  CoglTexture *texture;
} TestTile;

typedef struct _TestState
{
  TestTile *tiles;
  int width;
  int height;
} TestState;

static CoglTexture *
create_texture_from_bitmap (CoglBitmap *bitmap)
{
  CoglTexture *texture;

  texture = cogl_texture_new_from_bitmap (bitmap,
                                          COGL_TEXTURE_NONE,
                                          COGL_PIXEL_FORMAT_RGBA_8888);

  g_assert (texture != NULL);

  return texture;
}

static void
create_map_tile (CoglContext *context,
                 TestTile *tile)
{
  CoglBitmap *bitmap;
  CoglBuffer *buffer;
  guchar *map;
  unsigned int i;
  unsigned int stride;
  uint8_t *line;

  bitmap = cogl_bitmap_new_with_size (context,
                                      TILE_SIZE,
                                      TILE_SIZE,
                                      COGL_PIXEL_FORMAT_RGBA_8888);
  buffer = COGL_BUFFER (cogl_bitmap_get_buffer (bitmap));
  stride = cogl_bitmap_get_rowstride (bitmap);

  g_assert (cogl_is_pixel_buffer (buffer));
  g_assert (cogl_is_buffer (buffer));

  cogl_buffer_set_update_hint (buffer, COGL_BUFFER_UPDATE_HINT_DYNAMIC);
  g_assert_cmpint (cogl_buffer_get_update_hint (buffer),
            ==,
            COGL_BUFFER_UPDATE_HINT_DYNAMIC);

  map = cogl_buffer_map (buffer,
                         COGL_BUFFER_ACCESS_WRITE,
                         COGL_BUFFER_MAP_HINT_DISCARD);
  g_assert (map);

  line = g_alloca (TILE_SIZE * 4);
  for (i = 0; i < TILE_SIZE * 4; i += 4)
    memcpy (line + i, tile->color, 4);

  for (i = 0; i < TILE_SIZE; i++)
    memcpy (map + stride * i, line, TILE_SIZE * 4);

  cogl_buffer_unmap (buffer);

  tile->buffer = cogl_object_ref (buffer);
  tile->texture = create_texture_from_bitmap (bitmap);

  cogl_object_unref (bitmap);
}

#if 0
static void
create_set_region_tile (CoglContext *context,
                        TestTile *tile)
{
  CoglBitmap *bitmap;
  CoglBuffer *buffer;
  uint8_t bottom_color[4];
  unsigned int rowstride = 0;
  guchar *data;
  unsigned int i;

  bitmap = cogl_bitmap_new_with_size (context,
                                      TILE_SIZE,
                                      TILE_SIZE,
                                      COGL_PIXEL_FORMAT_RGBA_8888);
  buffer = COGL_BUFFER (cogl_bitmap_get_buffer (bitmap));
  rowstride = cogl_bitmap_get_rowstride (bitmap);

  g_assert (cogl_is_pixel_buffer (buffer));
  g_assert (cogl_is_buffer (buffer));

  /* while at it, set/get the hint */
  cogl_buffer_set_update_hint (buffer, COGL_BUFFER_UPDATE_HINT_STATIC);
  g_assert (cogl_buffer_get_update_hint (buffer) ==
            COGL_BUFFER_UPDATE_HINT_STATIC);

  data = g_malloc (TILE_SIZE * TILE_SIZE * 4);
  /* create a buffer with the data we want to copy to the buffer */
  for (i = 0; i < TILE_SIZE * TILE_SIZE * 4; i += 4)
      memcpy (data + i, &tile->color, 4);

  cogl_pixel_array_set_region (buffer,
                                data,
                                TILE_SIZE, TILE_SIZE,
                                TILE_SIZE,
                                0, 0);

  memcpy (bottom_color, tile->color, 4);
  for (i = 0; i < TILE_SIZE / 2; i++)
    memcpy (data + i, bottom_color, 4);

  cogl_buffer_set_data (buffer, 0, data, TILE_SIZE * TILE_SIZE * 4 / 2);

  g_free (data);

  tile->buffer = cogl_object_ref (buffer);
  tile->texture = create_texture_from_bitmap (bitmap);

  cogl_object_unref (bitmap);
}
#endif

static void
create_set_data_tile (CoglContext *context,
                      TestTile *tile)
{
  CoglBitmap *bitmap;
  CoglBuffer *buffer;
  unsigned int rowstride = 0;
  CoglBool res;
  guchar *data;
  unsigned int i;

  bitmap = cogl_bitmap_new_with_size (context,
                                      TILE_SIZE,
                                      TILE_SIZE,
                                      COGL_PIXEL_FORMAT_RGBA_8888);
  buffer = COGL_BUFFER (cogl_bitmap_get_buffer (bitmap));
  rowstride = cogl_bitmap_get_rowstride (bitmap);

  g_assert (cogl_is_pixel_buffer (buffer));
  g_assert (cogl_is_buffer (buffer));
  g_assert_cmpint (cogl_buffer_get_size (buffer), ==, rowstride * TILE_SIZE);

  /* create a buffer with the data we want to copy to the buffer */
  data = g_malloc (TILE_SIZE * TILE_SIZE * 4);
  for (i = 0; i < TILE_SIZE * TILE_SIZE * 4; i += 4)
      memcpy (data + i, tile->color, 4);

  /* FIXME: this doesn't consider the rowstride */
  res = cogl_buffer_set_data (buffer, 0, data, TILE_SIZE * TILE_SIZE * 4);
  g_assert (res);

  g_free (data);

  tile->buffer = cogl_object_ref (buffer);
  tile->texture = create_texture_from_bitmap (bitmap);

  cogl_object_unref (bitmap);
}

static void
draw_frame (TestState *state)
{
  unsigned int i;

  /* Paint the textures */
  for (i = 0; i < NB_TILES; i++)
    {
      CoglPipeline *pipeline = cogl_pipeline_new (ctx);
      cogl_pipeline_set_layer_texture (pipeline, 0, state->tiles[i].texture);
      cogl_framebuffer_draw_rectangle (fb,
                                       pipeline,
                                       state->tiles[i].x,
                                       state->tiles[i].y,
                                       state->tiles[i].x + TILE_SIZE,
                                       state->tiles[i].y + TILE_SIZE);
      cogl_object_unref (pipeline);
    }

}

static void
validate_tile (TestState *state,
               TestTile  *tile)
{
  test_utils_check_region (fb,
                           tile->x, tile->y,
                           TILE_SIZE, TILE_SIZE,
                           (tile->color[0] << 24) |
                           (tile->color[1] << 16) |
                           (tile->color[2] << 8) |
                           0xff);
}

static void
validate_result (TestState *state)
{
  unsigned int i;

  for (i = 0; i < NB_TILES; i++)
    validate_tile (state, &state->tiles[i]);
}

void
test_pixel_buffer (void)
{
  TestState state;
  int i;
  static TestTile tiles[NB_TILES] =
    {
        /*         color             x  y buffer tex */

        /* MAP */
        { { 0xff, 0x00, 0x00, 0xff }, 0.0f, 0.0f, NULL, NULL },
#if 0
        /* SET_REGION */
        { { 0x7e, 0x7e, 0xff, 0x7e }, 0.0f, TILE_SIZE, NULL, NULL },
#endif
        /* SET_DATA */
        { { 0x7e, 0xff, 0x7e, 0xff }, 0.0f, TILE_SIZE, NULL, NULL }
    };

  state.width = cogl_framebuffer_get_width (fb);
  state.height = cogl_framebuffer_get_height (fb);
  cogl_framebuffer_orthographic (fb,
                                 0, 0,
                                 state.width,
                                 state.height,
                                 -1,
                                 100);

  create_map_tile (ctx, &tiles[TILE_MAP]);
#if 0
  create_set_region_tile (shared_state->ctx, &tiles[TILE_SET_REGION]);
#endif
  create_set_data_tile (ctx, &tiles[TILE_SET_DATA]);

  state.tiles = tiles;

  draw_frame (&state);
  validate_result (&state);

  for (i = 0; i < NB_TILES; i++)
    {
      cogl_object_unref (state.tiles[i].buffer);
      cogl_object_unref (state.tiles[i].texture);
    }

  if (cogl_test_verbose ())
    g_print ("OK\n");
}

