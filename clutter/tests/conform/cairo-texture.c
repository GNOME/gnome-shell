#include <clutter/clutter.h>
#include <cogl/cogl.h>

#include "test-conform-common.h"

#define BLOCK_SIZE 16

/* Number of pixels at the border of a block to skip when verifying */
#define TEST_INSET 1

static const ClutterColor stage_color = { 0x0, 0x0, 0x0, 0xff };

typedef enum
{
  /* The first frame is drawn using clutter_cairo_texture_create. The
     second frame is an update of the first frame using
     clutter_cairo_texture_create_region. The states are stored like
     this because the cairo drawing is done on idle and the validation
     is done during paint and we need to synchronize the two */
  TEST_BEFORE_DRAW_FIRST_FRAME,
  TEST_BEFORE_VALIDATE_FIRST_FRAME,
  TEST_BEFORE_DRAW_SECOND_FRAME,
  TEST_BEFORE_VALIDATE_SECOND_FRAME,
  TEST_DONE
} TestProgress;

typedef struct _TestState
{
  ClutterActor *stage;
  ClutterActor *ct;
  guint frame;
  TestProgress progress;
} TestState;

static void
validate_part (int block_x, int block_y, const ClutterColor *color)
{
  guint8 data[BLOCK_SIZE * BLOCK_SIZE * 4];
  int x, y;

  cogl_read_pixels (block_x * BLOCK_SIZE,
                    block_y * BLOCK_SIZE,
                    BLOCK_SIZE, BLOCK_SIZE,
                    COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                    data);

  for (x = 0; x < BLOCK_SIZE - TEST_INSET * 2; x++)
    for (y = 0; y < BLOCK_SIZE - TEST_INSET * 2; y++)
      {
        const guint8 *p = data + ((x + TEST_INSET) * 4 +
                                  (y + TEST_INSET) * BLOCK_SIZE * 4);

        g_assert_cmpint (p[0], ==, color->red);
        g_assert_cmpint (p[1], ==, color->green);
        g_assert_cmpint (p[2], ==, color->blue);
      }
}

static void
paint_cb (ClutterActor *actor, TestState *state)
{
  static const ClutterColor red = { 0xff, 0x00, 0x00, 0xff };
  static const ClutterColor green = { 0x00, 0xff, 0x00, 0xff };
  static const ClutterColor blue = { 0x00, 0x00, 0xff, 0xff };

  if (state->frame++ < 2)
    return;

  switch (state->progress)
    {
    case TEST_BEFORE_DRAW_FIRST_FRAME:
    case TEST_BEFORE_DRAW_SECOND_FRAME:
    case TEST_DONE:
      /* Handled by the idle callback */
      break;

    case TEST_BEFORE_VALIDATE_FIRST_FRAME:
      /* In the first frame there is a red rectangle next to a green
         rectangle */
      validate_part (0, 0, &red);
      validate_part (1, 0, &green);

      state->progress = TEST_BEFORE_DRAW_SECOND_FRAME;
      break;

    case TEST_BEFORE_VALIDATE_SECOND_FRAME:
      /* The second frame is the same except the green rectangle is
         replaced with a blue one */
      validate_part (0, 0, &red);
      validate_part (1, 0, &blue);

      state->progress = TEST_DONE;
      break;
    }
}

static gboolean
idle_cb (gpointer data)
{
  TestState *state = data;
  cairo_t *cr;

  if (state->frame < 2)
    clutter_actor_queue_redraw (CLUTTER_ACTOR (state->stage));
  else
    switch (state->progress)
      {
      case TEST_BEFORE_DRAW_FIRST_FRAME:
        /* Draw two different colour rectangles */
        cr = clutter_cairo_texture_create (CLUTTER_CAIRO_TEXTURE (state->ct));

        cairo_save (cr);
        cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

        cairo_save (cr);
        cairo_rectangle (cr, 0, 0, BLOCK_SIZE, BLOCK_SIZE);
        cairo_clip (cr);
        cairo_set_source_rgb (cr, 1.0, 0.0, 0.0);
        cairo_paint (cr);
        cairo_restore (cr);

        cairo_rectangle (cr, BLOCK_SIZE, 0, BLOCK_SIZE, BLOCK_SIZE);
        cairo_clip (cr);
        cairo_set_source_rgb (cr, 0.0, 1.0, 0.0);
        cairo_paint (cr);

        cairo_restore (cr);

        cairo_destroy (cr);

        state->progress = TEST_BEFORE_VALIDATE_FIRST_FRAME;

        break;

      case TEST_BEFORE_DRAW_SECOND_FRAME:
        /* Replace the second rectangle with a blue one */
        cr = clutter_cairo_texture_create (CLUTTER_CAIRO_TEXTURE (state->ct));

        cairo_rectangle (cr, BLOCK_SIZE, 0, BLOCK_SIZE, BLOCK_SIZE);
        cairo_set_source_rgb (cr, 0.0, 0.0, 1.0);
        cairo_fill (cr);

        cairo_destroy (cr);

        state->progress = TEST_BEFORE_VALIDATE_SECOND_FRAME;

        break;

      case TEST_BEFORE_VALIDATE_FIRST_FRAME:
      case TEST_BEFORE_VALIDATE_SECOND_FRAME:
        /* Handled by the paint callback */
        break;

      case TEST_DONE:
        clutter_main_quit ();
        break;
      }

  return G_SOURCE_CONTINUE;
}

void
texture_cairo (TestConformSimpleFixture *fixture,
               gconstpointer data)
{
  TestState state;
  unsigned int idle_source;
  unsigned int paint_handler;

  state.frame = 0;
  state.stage = clutter_stage_new ();
  state.progress = TEST_BEFORE_DRAW_FIRST_FRAME;

  state.ct = clutter_cairo_texture_new (BLOCK_SIZE * 2, BLOCK_SIZE);
  clutter_container_add_actor (CLUTTER_CONTAINER (state.stage), state.ct);

  clutter_stage_set_color (CLUTTER_STAGE (state.stage), &stage_color);

  /* We force continuous redrawing of the stage, since we need to skip
   * the first few frames, and we wont be doing anything else that
   * will trigger redrawing. */
  idle_source = clutter_threads_add_idle (idle_cb, &state);
  paint_handler = g_signal_connect_after (state.stage, "paint",
                                          G_CALLBACK (paint_cb), &state);

  clutter_actor_show (state.stage);
  clutter_main ();

  g_signal_handler_disconnect (state.stage, paint_handler);
  g_source_remove (idle_source);

  if (g_test_verbose ())
    g_print ("OK\n");

  clutter_actor_destroy (state.stage);
}

