#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

/* each time the timeline animating the label completes, swap the direction */
static void
timeline_completed (ClutterTimeline *timeline,
                    gpointer         user_data)
{
  clutter_timeline_set_direction (timeline,
                                  !clutter_timeline_get_direction (timeline));
  clutter_timeline_start (timeline);
}

static ClutterActor *raise_actor[2];
static gboolean raise_no = 0;

static gboolean
raise_top (gpointer ignored G_GNUC_UNUSED)
{
  ClutterActor *parent = clutter_actor_get_parent (raise_actor[raise_no]);

  clutter_actor_set_child_above_sibling (parent, raise_actor[raise_no], NULL);
  raise_no = !raise_no;

  return G_SOURCE_CONTINUE;
}

static ClutterActor *
clone_box (ClutterActor *original)
{
  gfloat width, height;
  ClutterActor *group;
  ClutterActor *clone;

  clutter_actor_get_size (original, &width, &height);

  group = clutter_actor_new ();
  clone = clutter_clone_new (original);
  clutter_actor_add_child (group, clone);
  clutter_actor_set_depth (clone, width / 2);

  clone = clutter_clone_new (original);
  clutter_actor_add_child (group, clone);
  clutter_actor_set_rotation (clone, CLUTTER_Y_AXIS, 180, width / 2, 0, 0);
  clutter_actor_set_depth (clone, -width / 2);

  clone = clutter_clone_new (original);
  clutter_actor_add_child (group, clone);
  clutter_actor_set_rotation (clone, CLUTTER_Y_AXIS, 90, 0, 0, 0);
  clutter_actor_set_depth (clone, width / 2);
  clutter_actor_set_position (clone, 0, 0);

  clone = clutter_clone_new (original);
  clutter_actor_add_child (group, clone);
  clutter_actor_set_rotation (clone, CLUTTER_Y_AXIS, 90, 0, 0, 0);
  clutter_actor_set_depth (clone, width / 2);
  clutter_actor_set_position (clone, width, 0);

  clone = clutter_clone_new (original);
  clutter_actor_add_child (group, clone);
  clutter_actor_set_rotation (clone, CLUTTER_X_AXIS, 90, 0, 0, 0);
  clutter_actor_set_depth (clone, -width / 2);
  clutter_actor_set_position (clone, 0, height);

  clone = clutter_clone_new (original);
  clutter_actor_add_child (group, clone);
  clutter_actor_set_rotation (clone, CLUTTER_X_AXIS, 90, 0, 0, 0);
  clutter_actor_set_depth (clone, -width / 2);
  clutter_actor_set_position (clone, 0, 0);

  return group;
}

static ClutterActor *
janus_group (const gchar *front_text,
             const gchar *back_text)
{
  ClutterActor *group, *rectangle, *front, *back;
  gfloat width, height;
  gfloat width2, height2;

  group = clutter_actor_new ();
  rectangle = clutter_actor_new ();
  clutter_actor_set_background_color (rectangle, CLUTTER_COLOR_White);
  front = clutter_text_new_with_text ("Sans 50px", front_text);
  back = clutter_text_new_with_text ("Sans 50px", back_text);
  clutter_text_set_color (CLUTTER_TEXT (front), CLUTTER_COLOR_Red);
  clutter_text_set_color (CLUTTER_TEXT (back), CLUTTER_COLOR_Green);

  clutter_actor_get_size (front, &width, &height);
  clutter_actor_get_size (back, &width2, &height2);

  if (width2 > width)
    width = width2;

  if (height2 > height)
    height = height2;

  clutter_actor_set_size (rectangle, width, height);
  clutter_actor_set_rotation (back, CLUTTER_Y_AXIS, 180, width / 2, 0, 0);

  clutter_actor_add_child (group, back);
  clutter_actor_add_child (group, rectangle);
  clutter_actor_add_child (group, front);

  return group;
}

G_MODULE_EXPORT gint
test_depth_main (int argc, char *argv[])
{
  ClutterTimeline  *timeline;
  ClutterBehaviour *d_behave;
  ClutterBehaviour *r_behave;
  ClutterActor     *stage;
  ClutterActor     *group, *hand, *label, *rect, *janus, *box;
  GError           *error;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return EXIT_FAILURE;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Depth Test");
  clutter_actor_set_background_color (stage, CLUTTER_COLOR_Aluminium2);
  g_signal_connect (stage,
                    "destroy", G_CALLBACK (clutter_main_quit),
                    NULL);
  g_signal_connect (stage,
                    "button-press-event", G_CALLBACK (clutter_main_quit),
                    NULL);

  group = clutter_actor_new ();
  clutter_actor_add_child (stage, group);

  label = clutter_text_new_with_text ("Mono 26", "Clutter");
  clutter_actor_set_position (label, 120, 200);
  clutter_actor_add_child (stage, label);

  error = NULL;
  hand = clutter_texture_new_from_file (TESTS_DATADIR
                                        G_DIR_SEPARATOR_S
                                        "redhand.png",
                                        &error);
  if (error)
    g_error ("Unable to load redhand.png: %s", error->message);
  clutter_actor_set_position (hand, 240, 100);

  rect = clutter_rectangle_new_with_color (CLUTTER_COLOR_Black);
  clutter_actor_set_position (rect, 340, 100);
  clutter_actor_set_size (rect, 200, 200);
  clutter_actor_set_opacity (rect, 128);

  clutter_actor_add_child (group, hand);
  clutter_actor_add_child (group, rect);

  timeline = clutter_timeline_new (3000);
  g_signal_connect (timeline,
                    "completed", G_CALLBACK (timeline_completed),
                    NULL);

  d_behave =
    clutter_behaviour_depth_new (clutter_alpha_new_full (timeline,
                                                         CLUTTER_LINEAR),
                                 -100, 100);
  clutter_behaviour_apply (d_behave, label);

  /* add two faced actor */
  janus = janus_group ("GREEN", "RED");
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), janus);
  clutter_actor_set_position (janus, 300, 350);

  r_behave =
    clutter_behaviour_rotate_new (clutter_alpha_new_full (timeline,
                                                          CLUTTER_LINEAR),
                                  CLUTTER_Y_AXIS,
                                  CLUTTER_ROTATE_CW,
                                  0, 360);
  clutter_behaviour_apply (r_behave, janus);

  /* add hand box */
  box = clone_box (hand);
  clutter_actor_add_child (stage, box);
  clutter_actor_set_position (box, 200, 250);
  clutter_actor_set_scale (box, 0.5, 0.5);
  clutter_actor_set_rotation (box, CLUTTER_X_AXIS, 45, 0, 0, 0);
  clutter_actor_set_opacity (box, 0x44);

  r_behave =
    clutter_behaviour_rotate_new (clutter_alpha_new_full (timeline,
                                                          CLUTTER_LINEAR),
                                  CLUTTER_Y_AXIS,
                                  CLUTTER_ROTATE_CW,
                                  0, 360);
  clutter_behaviour_apply (r_behave, box);

  clutter_actor_show (stage);

  clutter_timeline_start (timeline);

  raise_actor[0] = rect;
  raise_actor[1] = hand;
  clutter_threads_add_timeout (2000, raise_top, NULL);

  clutter_main ();

  g_object_unref (d_behave);
  g_object_unref (timeline);

  return EXIT_SUCCESS;
}
