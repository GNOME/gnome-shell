
#define COGL_ENABLE_EXPERIMENTAL_API

#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <string.h>

#include "test-conform-common.h"

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
  ClutterColor color;
  gfloat x, y;
  CoglHandle buffer;
  CoglHandle texture;
} TestTile;

static const ClutterColor
buffer_colors[] =
  {
  };

static const ClutterColor stage_color = { 0x0, 0x0, 0x0, 0xff };

typedef struct _TestState
{
  ClutterActor *stage;
  guint frame;

  TestTile *tiles;

} TestState;

static CoglHandle
create_texture_from_buffer (CoglHandle buffer)
{
  CoglHandle texture;

  texture = cogl_texture_new_from_buffer (buffer,
                                          TILE_SIZE, TILE_SIZE,
                                          COGL_TEXTURE_NO_SLICING,
                                          COGL_PIXEL_FORMAT_RGBA_8888,
                                          COGL_PIXEL_FORMAT_RGBA_8888,
                                          TILE_SIZE * 4,
                                          0);

  g_assert (texture != COGL_INVALID_HANDLE);

  return texture;
}

static void
create_map_tile (TestTile *tile)
{
  CoglHandle buffer;
  guchar *map;
  guint i;

  buffer = cogl_pixel_buffer_new (TILE_SIZE * TILE_SIZE * 4);

  g_assert (cogl_is_pixel_buffer (buffer));
  g_assert (cogl_is_buffer (buffer));

  /* while at it, set/get the hints */
  cogl_buffer_set_usage_hint (buffer, COGL_BUFFER_USAGE_HINT_DRAW);
  g_assert_cmpint (cogl_buffer_get_usage_hint (buffer),
                   ==,
                   COGL_BUFFER_USAGE_HINT_DRAW);
  cogl_buffer_set_update_hint (buffer, COGL_BUFFER_UPDATE_HINT_DYNAMIC);
  g_assert_cmpint (cogl_buffer_get_update_hint (buffer),
            ==,
            COGL_BUFFER_UPDATE_HINT_DYNAMIC);

  map = cogl_buffer_map (buffer, COGL_BUFFER_ACCESS_WRITE);
  g_assert (map);

  for (i = 0; i < TILE_SIZE * TILE_SIZE * 4; i += 4)
      memcpy (map + i, &tile->color, 4);

  cogl_buffer_unmap (buffer);

  tile->buffer = buffer;
  tile->texture = create_texture_from_buffer (tile->buffer);
}

#if 0
static void
create_set_region_tile (TestTile *tile)
{
  CoglHandle buffer;
  ClutterColor bottom_color;
  guint rowstride = 0;
  guchar *data;
  guint i;

  buffer = cogl_pixel_buffer_new_for_size (TILE_SIZE,
                                           TILE_SIZE,
                                           COGL_PIXEL_FORMAT_RGBA_8888,
                                           &rowstride);

  g_assert (cogl_is_pixel_buffer (buffer));
  g_assert (cogl_is_buffer (buffer));

  /* while at it, set/get the hint */
  cogl_buffer_set_hint (buffer, COGL_BUFFER_HINT_STATIC_DRAW);
  g_assert (cogl_buffer_get_hint (buffer) == COGL_BUFFER_HINT_STATIC_DRAW);

  data = g_malloc (TILE_SIZE * TILE_SIZE * 4);
  /* create a buffer with the data we want to copy to the buffer */
  for (i = 0; i < TILE_SIZE * TILE_SIZE * 4; i += 4)
      memcpy (data + i, &tile->color, 4);

  cogl_pixel_buffer_set_region (buffer,
                                data,
                                TILE_SIZE, TILE_SIZE,
                                TILE_SIZE,
                                0, 0);

  bottom_color.red = tile->color.red;
  bottom_color.green = tile->color.blue;
  bottom_color.blue = tile->color.green;
  bottom_color.alpha = tile->color.alpha;
  for (i = 0; i < TILE_SIZE / 2; i++)
    memcpy (data + i, &bottom_color, 4);

  cogl_buffer_set_data (buffer, data, 0, TILE_SIZE * TILE_SIZE * 4 / 2);

  g_free (data);

  tile->buffer = buffer;
  tile->texture = create_texture_from_buffer (tile->buffer);
}
#endif

static void
create_set_data_tile (TestTile *tile)
{
  CoglHandle buffer;
  guint rowstride = 0;
  gboolean res;
  guchar *data;
  guint i;

  buffer = cogl_pixel_buffer_new_for_size (TILE_SIZE,
                                           TILE_SIZE,
                                           COGL_PIXEL_FORMAT_RGBA_8888,
                                           &rowstride);

  g_assert (cogl_is_pixel_buffer (buffer));
  g_assert (cogl_is_buffer (buffer));
  g_assert_cmpint (cogl_buffer_get_size (buffer), ==, rowstride * TILE_SIZE);

  /* while at it, set/get the hint */
  cogl_buffer_set_usage_hint (buffer, COGL_BUFFER_USAGE_HINT_DRAW);
  g_assert_cmpint (cogl_buffer_get_usage_hint (buffer),
                   ==,
                   COGL_BUFFER_USAGE_HINT_DRAW);

  /* create a buffer with the data we want to copy to the buffer */
  data = g_malloc (TILE_SIZE * TILE_SIZE * 4);
  for (i = 0; i < TILE_SIZE * TILE_SIZE * 4; i += 4)
      memcpy (data + i, &tile->color, 4);

  res = cogl_buffer_set_data (buffer, 0, data, TILE_SIZE * TILE_SIZE * 4);
  g_assert (res);

  g_free (data);

  tile->buffer = buffer;
  tile->texture = create_texture_from_buffer (tile->buffer);
}

static void
draw_frame (TestState *state)
{
  guint i;

  /* Paint the textures */
  for (i = 0; i < NB_TILES; i++)
    {
      cogl_set_source_texture (state->tiles[i].texture);
      cogl_rectangle (state->tiles[i].x,
                      state->tiles[i].y,
                      state->tiles[i].x + TILE_SIZE,
                      state->tiles[i].y + TILE_SIZE);
    }

}

static gboolean
validate_tile (TestState *state,
               TestTile  *tile)
{
  int x, y;
  guchar *pixels, *p;

  p = pixels = clutter_stage_read_pixels (CLUTTER_STAGE (state->stage),
                                          tile->x,
                                          tile->y,
                                          TILE_SIZE,
                                          TILE_SIZE);

  /* Check whether the center of each division is the right color */
  for (y = 0; y < TILE_SIZE; y++)
    for (x = 0; x < TILE_SIZE; x++)
      {
        if (p[0] != tile->color.red ||
            p[1] != tile->color.green ||
            p[2] != tile->color.blue ||
            p[3] != tile->color.alpha)
          {
            return FALSE;
          }

        p += 4;
      }

  return TRUE;
}

static void
validate_result (TestState *state)
{
  guint i;

  for (i = 0; i < NB_TILES; i++)
    g_assert (validate_tile (state, &state->tiles[i]));

  /* comment this if you want to see what's being drawn */
#if 1
  clutter_main_quit ();
#endif
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
test_cogl_pixel_buffer (TestConformSimpleFixture *fixture,
			gconstpointer             data)
{
  TestState state;
  guint idle_source;
  guint paint_handler, i;
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

  state.frame = 0;

  state.stage = clutter_stage_get_default ();

  create_map_tile (&tiles[TILE_MAP]);
#if 0
  create_set_region_tile (&tiles[TILE_SET_REGION]);
#endif
  create_set_data_tile (&tiles[TILE_SET_DATA]);

  state.tiles = tiles;

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

  for (i = 0; i < NB_TILES; i++)
    {
      cogl_handle_unref (state.tiles[i].buffer);
      cogl_handle_unref (state.tiles[i].texture);
    }

  /* Remove all of the actors from the stage */
  clutter_container_foreach (CLUTTER_CONTAINER (state.stage),
                             (ClutterCallback) clutter_actor_destroy,
                             NULL);

  if (g_test_verbose ())
    g_print ("OK\n");
}

