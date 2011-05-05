#include <clutter/clutter.h>
#include <cogl/cogl.h>

#include "test-conform-common.h"

#define BLOCK_SIZE 16

/* Number of pixels at the border of a block quadrant to skip when verifying */
#define TEST_INSET 1

static const ClutterColor stage_color = { 0x0, 0x0, 0x0, 0xff };
static const ClutterColor block_color = { 0xff, 0xff, 0xff, 0xff };

typedef struct _TestState
{
  ClutterActor *stage;
  unsigned int frame;
} TestState;

static void
draw_path_at (int x, int y)
{
  cogl_push_matrix ();
  cogl_translate (x * BLOCK_SIZE, y * BLOCK_SIZE, 0.0f);
  cogl_path_fill ();
  cogl_pop_matrix ();
}

static void
verify_block (int block_x, int block_y, int block_mask)
{
  guint8 data[BLOCK_SIZE * BLOCK_SIZE * 4];
  int qx, qy;

  /* Block mask represents which quarters of the block should be
     filled. The bits from 0->3 represent the top left, top right,
     bottom left and bottom right respectively */

  cogl_read_pixels (block_x * BLOCK_SIZE,
                    block_y * BLOCK_SIZE,
                    BLOCK_SIZE, BLOCK_SIZE,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                    data);

  for (qy = 0; qy < 2; qy++)
    for (qx = 0; qx < 2; qx++)
      {
        int bit = qx | (qy << 1);
        const ClutterColor *color =
          ((block_mask & (1 << bit)) ? &block_color : &stage_color);
        int x, y;

        for (x = 0; x < BLOCK_SIZE / 2 - TEST_INSET * 2; x++)
          for (y = 0; y < BLOCK_SIZE / 2 - TEST_INSET * 2; y++)
            {
              const guint8 *p = data + (qx * BLOCK_SIZE / 2 * 4 +
                                        qy * BLOCK_SIZE * 4 * BLOCK_SIZE / 2 +
                                        (x + TEST_INSET) * 4 +
                                        (y + TEST_INSET) * BLOCK_SIZE * 4);
              g_assert_cmpint (p[0], ==, color->red);
              g_assert_cmpint (p[1], ==, color->green);
              g_assert_cmpint (p[2], ==, color->blue);
            }
      }
}

static void
on_paint (ClutterActor *actor, TestState *state)
{
  CoglHandle path_a, path_b, path_c;

  if (state->frame++ < 2)
    return;

  cogl_set_source_color4ub (255, 255, 255, 255);

  /* Create a path filling just a quarter of a block. It will use two
     rectangles so that we have a sub path in the path */
  cogl_path_new ();
  cogl_path_rectangle (BLOCK_SIZE * 3 / 4, BLOCK_SIZE / 2,
                       BLOCK_SIZE, BLOCK_SIZE);
  cogl_path_rectangle (BLOCK_SIZE / 2, BLOCK_SIZE / 2,
                       BLOCK_SIZE * 3 / 4, BLOCK_SIZE);
  path_a = cogl_handle_ref (cogl_get_path ());
  draw_path_at (0, 0);

  /* Create another path filling the whole block */
  cogl_path_rectangle (0, 0, BLOCK_SIZE, BLOCK_SIZE);
  path_b = cogl_handle_ref (cogl_get_path ());
  draw_path_at (1, 0);

  /* Draw the first path again */
  cogl_set_path (path_a);
  draw_path_at (2, 0);

  /* Draw a copy of path a */
  path_c = cogl_path_copy (path_a);
  cogl_set_path (path_c);
  draw_path_at (3, 0);

  /* Add another rectangle to path a. We'll use line_to's instead of
     cogl_rectangle so that we don't create another sub-path because
     that is more likely to break the copy */
  cogl_set_path (path_a);
  cogl_path_line_to (0, BLOCK_SIZE / 2);
  cogl_path_line_to (0, 0);
  cogl_path_line_to (BLOCK_SIZE / 2, 0);
  cogl_path_line_to (BLOCK_SIZE / 2, BLOCK_SIZE / 2);
  draw_path_at (4, 0);

  /* Draw the copy again. It should not have changed */
  cogl_set_path (path_c);
  draw_path_at (5, 0);

  /* Add another rectangle to path c. It will be added in two halves,
     one as an extension of the previous path and the other as a new
     sub path */
  cogl_set_path (path_c);
  cogl_path_line_to (BLOCK_SIZE / 2, 0);
  cogl_path_line_to (BLOCK_SIZE * 3 / 4, 0);
  cogl_path_line_to (BLOCK_SIZE * 3 / 4, BLOCK_SIZE / 2);
  cogl_path_line_to (BLOCK_SIZE / 2, BLOCK_SIZE / 2);
  cogl_path_rectangle (BLOCK_SIZE * 3 / 4, 0, BLOCK_SIZE, BLOCK_SIZE / 2);
  draw_path_at (6, 0);

  /* Draw the original path again. It should not have changed */
  cogl_set_path (path_a);
  draw_path_at (7, 0);

  cogl_handle_unref (path_a);
  cogl_handle_unref (path_b);
  cogl_handle_unref (path_c);

  /* Draw a self-intersecting path. The part that intersects should be
     inverted */
  cogl_path_rectangle (0, 0, BLOCK_SIZE, BLOCK_SIZE);
  cogl_path_line_to (0, BLOCK_SIZE / 2);
  cogl_path_line_to (BLOCK_SIZE / 2, BLOCK_SIZE / 2);
  cogl_path_line_to (BLOCK_SIZE / 2, 0);
  cogl_path_close ();
  draw_path_at (8, 0);

  /* Draw two sub paths. Where the paths intersect it should be
     inverted */
  cogl_path_rectangle (0, 0, BLOCK_SIZE, BLOCK_SIZE);
  cogl_path_rectangle (BLOCK_SIZE / 2, BLOCK_SIZE / 2, BLOCK_SIZE, BLOCK_SIZE);
  draw_path_at (9, 0);

  /* Draw a clockwise outer path */
  cogl_path_move_to (0, 0);
  cogl_path_line_to (BLOCK_SIZE, 0);
  cogl_path_line_to (BLOCK_SIZE, BLOCK_SIZE);
  cogl_path_line_to (0, BLOCK_SIZE);
  cogl_path_close ();
  /* Add a clockwise sub path in the upper left quadrant */
  cogl_path_move_to (0, 0);
  cogl_path_line_to (BLOCK_SIZE / 2, 0);
  cogl_path_line_to (BLOCK_SIZE / 2, BLOCK_SIZE / 2);
  cogl_path_line_to (0, BLOCK_SIZE / 2);
  cogl_path_close ();
  /* Add a counter-clockwise sub path in the upper right quadrant */
  cogl_path_move_to (BLOCK_SIZE / 2, 0);
  cogl_path_line_to (BLOCK_SIZE / 2, BLOCK_SIZE / 2);
  cogl_path_line_to (BLOCK_SIZE, BLOCK_SIZE / 2);
  cogl_path_line_to (BLOCK_SIZE, 0);
  cogl_path_close ();
  /* Retain the path for the next test */
  path_a = cogl_handle_ref (cogl_get_path ());
  draw_path_at (10, 0);

  /* Draw the same path again with the other fill rule */
  cogl_set_path (path_a);
  cogl_path_set_fill_rule (COGL_PATH_FILL_RULE_NON_ZERO);
  draw_path_at (11, 0);

  cogl_handle_unref (path_a);

  verify_block (0, 0, 0x8 /* bottom right */);
  verify_block (1, 0, 0xf /* all of them */);
  verify_block (2, 0, 0x8 /* bottom right */);
  verify_block (3, 0, 0x8 /* bottom right */);
  verify_block (4, 0, 0x9 /* top left and bottom right */);
  verify_block (5, 0, 0x8 /* bottom right */);
  verify_block (6, 0, 0xa /* bottom right and top right */);
  verify_block (7, 0, 0x9 /* top_left and bottom right */);
  verify_block (8, 0, 0xe /* all but top left */);
  verify_block (9, 0, 0x7 /* all but bottom right */);
  verify_block (10, 0, 0xc /* bottom two */);
  verify_block (11, 0, 0xd /* all but top right */);

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
test_cogl_path (TestUtilsGTestFixture *fixture,
                void *data)
{
  TestState state;
  unsigned int idle_source;
  unsigned int paint_handler;

  state.frame = 0;
  state.stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (state.stage), &stage_color);

  /* We force continuous redrawing of the stage, since we need to skip
   * the first few frames, and we wont be doing anything else that
   * will trigger redrawing. */
  idle_source = g_idle_add (queue_redraw, state.stage);
  paint_handler = g_signal_connect_after (state.stage, "paint",
                                          G_CALLBACK (on_paint), &state);

  clutter_actor_show (state.stage);
  clutter_main ();

  g_signal_handler_disconnect (state.stage, paint_handler);
  g_source_remove (idle_source);

  if (g_test_verbose ())
    g_print ("OK\n");
}

