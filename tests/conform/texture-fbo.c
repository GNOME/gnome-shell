#include <clutter/clutter.h>
#include <cogl/cogl.h>

#include "test-conform-common.h"

#define SOURCE_SIZE        32
#define SOURCE_DIVISIONS_X 2
#define SOURCE_DIVISIONS_Y 2
#define DIVISION_WIDTH     (SOURCE_SIZE / SOURCE_DIVISIONS_X)
#define DIVISION_HEIGHT    (SOURCE_SIZE / SOURCE_DIVISIONS_Y)

static const ClutterColor
corner_colors[SOURCE_DIVISIONS_X * SOURCE_DIVISIONS_Y] =
  {
    { 0xff, 0x00, 0x00, 0xff }, /* red top left */
    { 0x00, 0xff, 0x00, 0xff }, /* green top right */
    { 0x00, 0x00, 0xff, 0xff }, /* blue bottom left */
    { 0xff, 0x00, 0xff, 0xff }  /* purple bottom right */
  };

static const ClutterColor stage_color = { 0x0, 0x0, 0x0, 0xff };

typedef struct _TestState
{
  ClutterActor *stage;
  guint frame;
} TestState;

static ClutterActor *
create_source (void)
{
  int x, y;
  ClutterActor *group = clutter_group_new ();

  /* Create a group with a different coloured rectangle at each
     corner */
  for (y = 0; y < SOURCE_DIVISIONS_Y; y++)
    for (x = 0; x < SOURCE_DIVISIONS_X; x++)
      {
        ClutterActor *rect = clutter_rectangle_new ();
        clutter_actor_set_size (rect, DIVISION_WIDTH, DIVISION_HEIGHT);
        clutter_actor_set_position (rect,
                                    DIVISION_WIDTH * x,
                                    DIVISION_HEIGHT * y);
        clutter_rectangle_set_color (CLUTTER_RECTANGLE (rect),
                                     corner_colors +
                                     (y * SOURCE_DIVISIONS_X + x));
        clutter_container_add (CLUTTER_CONTAINER (group), rect, NULL);
      }

  return group;
}

static void
pre_paint_clip_cb (void)
{
  /* Generate a clip path that clips out the top left division */
  cogl_path_move_to (DIVISION_WIDTH, 0);
  cogl_path_line_to (SOURCE_SIZE, 0);
  cogl_path_line_to (SOURCE_SIZE, SOURCE_SIZE);
  cogl_path_line_to (0, SOURCE_SIZE);
  cogl_path_line_to (0, DIVISION_HEIGHT);
  cogl_path_line_to (DIVISION_WIDTH, DIVISION_HEIGHT);
  cogl_path_close ();
  cogl_clip_push_from_path ();
}

static void
post_paint_clip_cb (void)
{
  cogl_clip_pop ();
}

static gboolean
validate_part (TestState *state,
               int xpos, int ypos,
               int clip_flags)
{
  int x, y;
  gboolean pass = TRUE;

  /* Check whether the center of each division is the right color */
  for (y = 0; y < SOURCE_DIVISIONS_Y; y++)
    for (x = 0; x < SOURCE_DIVISIONS_X; x++)
      {
        guchar *pixels;
        const ClutterColor *correct_color;

        /* Read the center pixels of this division */
        pixels = clutter_stage_read_pixels (CLUTTER_STAGE (state->stage),
                                            x * DIVISION_WIDTH +
                                            DIVISION_WIDTH / 2 + xpos,
                                            y * DIVISION_HEIGHT +
                                            DIVISION_HEIGHT / 2 + ypos,
                                            1, 1);

        /* If this division is clipped then it should be the stage
           color */
        if ((clip_flags & (1 << ((y * SOURCE_DIVISIONS_X) + x))))
          correct_color = &stage_color;
        else
          /* Otherwise it should be the color for this division */
          correct_color = corner_colors + (y * SOURCE_DIVISIONS_X) + x;

        if (pixels == NULL ||
            pixels[0] != correct_color->red ||
            pixels[1] != correct_color->green ||
            pixels[2] != correct_color->blue)
          pass = FALSE;

        g_free (pixels);
      }

  return pass;
}

static void
validate_result (TestState *state)
{
  int ypos = 0;

  if (g_test_verbose ())
    g_print ("Testing onscreen clone...\n");
  g_assert (validate_part (state, SOURCE_SIZE, ypos * SOURCE_SIZE, 0));
  ypos++;

#if 0 /* this doesn't work */
  if (g_test_verbose ())
    g_print ("Testing offscreen clone...\n");
  g_assert (validate_part (state, SOURCE_SIZE, ypos * SOURCE_SIZE, 0));
#endif
  ypos++;

  if (g_test_verbose ())
    g_print ("Testing onscreen clone with rectangular clip...\n");
  g_assert (validate_part (state, SOURCE_SIZE, ypos * SOURCE_SIZE, ~1));
  ypos++;

  if (g_test_verbose ())
    g_print ("Testing onscreen clone with path clip...\n");
  g_assert (validate_part (state, SOURCE_SIZE, ypos * SOURCE_SIZE, 1));
  ypos++;

  /* Comment this out if you want visual feedback of what this test
   * paints.
   */
  clutter_main_quit ();
}

static void
on_paint (ClutterActor *actor, TestState *state)
{
  int frame_num;

  /* XXX: validate_result calls clutter_stage_read_pixels which will result in
   * another paint run so to avoid infinite recursion we only aim to validate
   * the first frame. */
  frame_num = state->frame++;
  if (frame_num == 1)
    validate_result (state);
}

static gboolean
queue_redraw (gpointer stage)
{
  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage));

  return G_SOURCE_CONTINUE;
}

void
texture_fbo (TestConformSimpleFixture *fixture,
             gconstpointer data)
{
  TestState state;
  ClutterActor *actor;
  int ypos = 0;

  if (!cogl_features_available (COGL_FEATURE_OFFSCREEN))
    return;

  state.frame = 0;

  state.stage = clutter_stage_new ();

  clutter_stage_set_color (CLUTTER_STAGE (state.stage), &stage_color);

  /* Onscreen source with clone next to it */
  actor = create_source ();
  clutter_container_add (CLUTTER_CONTAINER (state.stage), actor, NULL);
  clutter_actor_set_position (actor, 0, ypos * SOURCE_SIZE);
  actor = clutter_texture_new_from_actor (actor);
  clutter_actor_set_position (actor, SOURCE_SIZE, ypos * SOURCE_SIZE);
  clutter_container_add (CLUTTER_CONTAINER (state.stage), actor, NULL);
  ypos++;

  /* Offscreen source with clone */
#if 0 /* this doesn't work */
  actor = create_source ();
  actor = clutter_texture_new_from_actor (actor);
  clutter_actor_set_position (actor, SOURCE_SIZE, ypos * SOURCE_SIZE);
  clutter_container_add (CLUTTER_CONTAINER (state.stage), actor, NULL);
#endif
  ypos++;

  /* Source clipped to the top left division */
  actor = create_source ();
  clutter_container_add (CLUTTER_CONTAINER (state.stage), actor, NULL);
  clutter_actor_set_position (actor, 0, ypos * SOURCE_SIZE);
  clutter_actor_set_clip (actor, 0, 0, DIVISION_WIDTH, DIVISION_HEIGHT);
  actor = clutter_texture_new_from_actor (actor);
  clutter_actor_set_position (actor, SOURCE_SIZE, ypos * SOURCE_SIZE);
  clutter_container_add (CLUTTER_CONTAINER (state.stage), actor, NULL);
  ypos++;

  /* Source clipped to everything but top left division using a
     path */
  actor = create_source ();
  clutter_container_add (CLUTTER_CONTAINER (state.stage), actor, NULL);
  clutter_actor_set_position (actor, 0, ypos * SOURCE_SIZE);
  g_signal_connect (actor, "paint",
                    G_CALLBACK (pre_paint_clip_cb), NULL);
  g_signal_connect_after (actor, "paint",
                          G_CALLBACK (post_paint_clip_cb), NULL);
  actor = clutter_texture_new_from_actor (actor);
  clutter_actor_set_position (actor, SOURCE_SIZE, ypos * SOURCE_SIZE);
  clutter_container_add (CLUTTER_CONTAINER (state.stage), actor, NULL);
  ypos++;

  /* We force continuous redrawing of the stage, since we need to skip
   * the first few frames, and we wont be doing anything else that
   * will trigger redrawing. */
  clutter_threads_add_idle (queue_redraw, state.stage);
  g_signal_connect_after (state.stage, "paint", G_CALLBACK (on_paint), &state);

  clutter_actor_show_all (state.stage);

  clutter_main ();

  clutter_actor_destroy (state.stage);

  if (g_test_verbose ())
    g_print ("OK\n");
}
