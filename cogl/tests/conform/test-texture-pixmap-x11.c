#include <clutter/clutter.h>

#include "test-conform-common.h"

static const ClutterColor stage_color = { 0x00, 0x00, 0x00, 0xff };

#ifdef COGL_HAS_XLIB

#include <clutter/x11/clutter-x11.h>
#include <cogl/cogl-texture-pixmap-x11.h>

#define PIXMAP_WIDTH 512
#define PIXMAP_HEIGHT 256
#define GRID_SQUARE_SIZE 16

/* Coordinates of a square that we'll update */
#define PIXMAP_CHANGE_X 1
#define PIXMAP_CHANGE_Y 1

typedef struct _TestState
{
  ClutterActor *stage;
  CoglHandle tfp;
  Pixmap pixmap;
  unsigned int frame_count;
  Display *display;
} TestState;

static Pixmap
create_pixmap (TestState *state)
{
  Pixmap pixmap;
  XGCValues gc_values = { 0, };
  GC black_gc, white_gc;
  int screen = DefaultScreen (state->display);
  int x, y;

  pixmap = XCreatePixmap (state->display,
                          DefaultRootWindow (state->display),
                          PIXMAP_WIDTH, PIXMAP_HEIGHT,
                          DefaultDepth (state->display, screen));

  gc_values.foreground = BlackPixel (state->display, screen);
  black_gc = XCreateGC (state->display, pixmap, GCForeground, &gc_values);
  gc_values.foreground = WhitePixel (state->display, screen);
  white_gc = XCreateGC (state->display, pixmap, GCForeground, &gc_values);

  /* Draw a grid of alternative black and white rectangles to the
     pixmap */
  for (y = 0; y < PIXMAP_HEIGHT / GRID_SQUARE_SIZE; y++)
    for (x = 0; x < PIXMAP_WIDTH / GRID_SQUARE_SIZE; x++)
      XFillRectangle (state->display, pixmap,
                  ((x ^ y) & 1) ? black_gc : white_gc,
                  x * GRID_SQUARE_SIZE,
                  y * GRID_SQUARE_SIZE,
                  GRID_SQUARE_SIZE,
                  GRID_SQUARE_SIZE);

  XFreeGC (state->display, black_gc);
  XFreeGC (state->display, white_gc);

  return pixmap;
}

static void
update_pixmap (TestState *state)
{
  XGCValues gc_values = { 0, };
  GC black_gc;
  int screen = DefaultScreen (state->display);

  gc_values.foreground = BlackPixel (state->display, screen);
  black_gc = XCreateGC (state->display, state->pixmap,
                        GCForeground, &gc_values);

  /* Fill in one the rectangles with black */
  XFillRectangle (state->display, state->pixmap,
                  black_gc,
                  PIXMAP_CHANGE_X * GRID_SQUARE_SIZE,
                  PIXMAP_CHANGE_Y * GRID_SQUARE_SIZE,
                  GRID_SQUARE_SIZE, GRID_SQUARE_SIZE);

  XFreeGC (state->display, black_gc);
}

static CoglBool
check_paint (TestState *state, int x, int y, int scale)
{
  uint8_t *data, *p, update_value = 0;

  p = data = g_malloc (PIXMAP_WIDTH * PIXMAP_HEIGHT * 4);

  cogl_read_pixels (x, y, PIXMAP_WIDTH / scale, PIXMAP_HEIGHT / scale,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                    data);

  for (y = 0; y < PIXMAP_HEIGHT / scale; y++)
    for (x = 0; x < PIXMAP_WIDTH / scale; x++)
      {
        int grid_x = x * scale / GRID_SQUARE_SIZE;
        int grid_y = y * scale / GRID_SQUARE_SIZE;

        /* If this is the updatable square then we'll let it be either
           color but we'll return which one it was */
        if (grid_x == PIXMAP_CHANGE_X && grid_y == PIXMAP_CHANGE_Y)
          {
            if (x % (GRID_SQUARE_SIZE / scale) == 0 &&
                y % (GRID_SQUARE_SIZE / scale) == 0)
              update_value = *p;
            else
              g_assert_cmpint (p[0], ==, update_value);

            g_assert (p[1] == update_value);
            g_assert (p[2] == update_value);
            p += 4;
          }
        else
          {
            uint8_t value = ((grid_x ^ grid_y) & 1) ? 0x00 : 0xff;
            g_assert_cmpint (*(p++), ==, value);
            g_assert_cmpint (*(p++), ==, value);
            g_assert_cmpint (*(p++), ==, value);
            p++;
          }
      }

  g_free (data);

  return update_value == 0x00;
}

/* We skip these frames first */
#define FRAME_COUNT_BASE 5
/* First paint the tfp with no mipmaps */
#define FRAME_COUNT_NORMAL 6
/* Then use mipmaps */
#define FRAME_COUNT_MIPMAP 7
/* After this frame will start waiting for the pixmap to change */
#define FRAME_COUNT_UPDATED 8

static void
on_paint (ClutterActor *actor, TestState *state)
{
  CoglHandle material;

  material = cogl_material_new ();
  cogl_material_set_layer (material, 0, state->tfp);
  if (state->frame_count == FRAME_COUNT_MIPMAP)
    {
      const CoglMaterialFilter min_filter =
        COGL_MATERIAL_FILTER_NEAREST_MIPMAP_NEAREST;
      cogl_material_set_layer_filters (material, 0,
                                       min_filter,
                                       COGL_MATERIAL_FILTER_NEAREST);
    }
  else
    cogl_material_set_layer_filters (material, 0,
                                     COGL_MATERIAL_FILTER_NEAREST,
                                     COGL_MATERIAL_FILTER_NEAREST);
  cogl_set_source (material);

  cogl_rectangle (0, 0, PIXMAP_WIDTH, PIXMAP_HEIGHT);

  cogl_rectangle (0, PIXMAP_HEIGHT,
                  PIXMAP_WIDTH / 4, PIXMAP_HEIGHT * 5 / 4);

  if (state->frame_count >= 5)
    {
      CoglBool big_updated, small_updated;

      big_updated = check_paint (state, 0, 0, 1);
      small_updated = check_paint (state, 0, PIXMAP_HEIGHT, 4);

      g_assert (big_updated == small_updated);

      if (state->frame_count < FRAME_COUNT_UPDATED)
        g_assert (big_updated == FALSE);
      else if (state->frame_count == FRAME_COUNT_UPDATED)
        /* Change the pixmap and keep drawing until it updates */
        update_pixmap (state);
      else if (big_updated)
        /* If we successfully got the update then the test is over */
        clutter_main_quit ();
    }

  state->frame_count++;
}

static CoglBool
queue_redraw (void *stage)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  return TRUE;
}

#endif /* COGL_HAS_XLIB */

void
test_texture_pixmap_x11 (TestUtilsGTestFixture *fixture,
                              void *data)
{
#ifdef COGL_HAS_XLIB

  TestState state;
  unsigned int idle_handler;
  unsigned int paint_handler;

  state.frame_count = 0;
  state.stage = clutter_stage_get_default ();

  state.display = clutter_x11_get_default_display ();

  state.pixmap = create_pixmap (&state);
  state.tfp = cogl_texture_pixmap_x11_new (state.pixmap, TRUE);

  clutter_stage_set_color (CLUTTER_STAGE (state.stage), &stage_color);

  paint_handler = g_signal_connect_after (state.stage, "paint",
                                          G_CALLBACK (on_paint), &state);

  idle_handler = g_idle_add (queue_redraw, state.stage);

  clutter_actor_show_all (state.stage);

  clutter_main ();

  g_signal_handler_disconnect (state.stage, paint_handler);

  g_source_remove (idle_handler);

  XFreePixmap (state.display, state.pixmap);

  if (cogl_test_verbose ())
    g_print ("OK\n");

#else /* COGL_HAS_XLIB */

  if (cogl_test_verbose ())
   g_print ("Skipping\n");

#endif /* COGL_HAS_XLIB */
}

