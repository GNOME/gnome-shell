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
raise_top (gpointer ignored)
{
  clutter_actor_raise_top (raise_actor[raise_no]);
  raise_no = !raise_no;
  return TRUE;
}

static ClutterActor *
clone_box (ClutterTexture *original)
{
  guint width, height;
  ClutterActor *group;
  ClutterActor *clone;

  clutter_actor_get_size (CLUTTER_ACTOR (original), &width, &height);

  group = clutter_group_new ();
  clone = clutter_clone_texture_new (original);
  clutter_container_add_actor (CLUTTER_CONTAINER (group), clone);
  clutter_actor_set_depth (clone, width/2);

  clone = clutter_clone_texture_new (original);
  clutter_container_add_actor (CLUTTER_CONTAINER (group), clone);
  clutter_actor_set_rotation (clone, CLUTTER_Y_AXIS, 180, width/2, 0, 0);
  clutter_actor_set_depth (clone, -width/2);

  clone = clutter_clone_texture_new (original);
  clutter_container_add_actor (CLUTTER_CONTAINER (group), clone);
  clutter_actor_set_rotation (clone, CLUTTER_Y_AXIS, 90, 0, 0, 0);
  clutter_actor_set_depth (clone, width/2);
  clutter_actor_set_position (clone, 0, 0);

  clone = clutter_clone_texture_new (original);
  clutter_container_add_actor (CLUTTER_CONTAINER (group), clone);
  clutter_actor_set_rotation (clone, CLUTTER_Y_AXIS, 90, 0, 0, 0);
  clutter_actor_set_depth (clone, width/2);
  clutter_actor_set_position (clone, width, 0);

  clone = clutter_clone_texture_new (original);
  clutter_container_add_actor (CLUTTER_CONTAINER (group), clone);
  clutter_actor_set_rotation (clone, CLUTTER_X_AXIS, 90, 0, 0, 0);
  clutter_actor_set_depth (clone, -width/2);
  clutter_actor_set_position (clone, 0, height);

  clone = clutter_clone_texture_new (original);
  clutter_container_add_actor (CLUTTER_CONTAINER (group), clone);
  clutter_actor_set_rotation (clone, CLUTTER_X_AXIS, 90, 0, 0, 0);
  clutter_actor_set_depth (clone, -width/2);
  clutter_actor_set_position (clone, 0, 0);

  clutter_actor_show_all (group);
  return group;
}

static ClutterActor *
janus_group (const gchar *front_text,
             const gchar *back_text)
{
  ClutterColor  slide_color = {0x00, 0x00, 0x00, 0xff};
  ClutterColor  red = {0xff, 0x00, 0x00, 0xff};
  ClutterColor  green = {0x00, 0xff, 0x00, 0xff};
  ClutterActor *group, *rectangle, *front, *back;
  guint width, height;
  guint width2, height2;

  group = clutter_group_new ();
  rectangle = clutter_rectangle_new_with_color (&slide_color);
  front = clutter_label_new_with_text ("Sans 50px", front_text);
  back = clutter_label_new_with_text ("Sans 50px", back_text);
  clutter_label_set_color (CLUTTER_LABEL (front), &red);
  clutter_label_set_color (CLUTTER_LABEL (back), &green);

  clutter_actor_get_size (front, &width, &height);
  clutter_actor_get_size (back, &width2, &height2);

  if (width2 > width)
    width = width2;
  if (height2 > height)
    height = height2;

  clutter_actor_set_size (rectangle, width, height);
  clutter_actor_set_rotation (back, CLUTTER_Y_AXIS, 180, width/2, 0, 0);

  clutter_container_add (CLUTTER_CONTAINER (group),
                         back, rectangle, front, NULL);

  clutter_actor_show_all (group);
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
  ClutterColor      stage_color = { 0xcc, 0xcc, 0xcc, 0xff };
  ClutterColor      rect_color  = { 0, 0, 0, 0x88 };
  GError           *error;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_stage_set_use_fog (CLUTTER_STAGE (stage), TRUE);
  clutter_stage_set_fog (CLUTTER_STAGE (stage), 1.0, 10, -50);

  g_signal_connect (stage,
                    "button-press-event", G_CALLBACK (clutter_main_quit),
                    NULL);

  group = clutter_group_new ();
  clutter_stage_add (stage, group);
  clutter_actor_show (group);

  label = clutter_label_new_with_text ("Mono 26", "Clutter");
  clutter_actor_set_position (label, 120, 200);
  clutter_actor_show (label);

  error = NULL;
  hand = clutter_texture_new_from_file ("redhand.png", &error);
  if (error)
    g_error ("Unable to load redhand.png: %s", error->message);
  clutter_actor_set_position (hand, 240, 100);
  clutter_actor_show (hand);

  rect = clutter_rectangle_new_with_color (&rect_color);
  clutter_actor_set_position (rect, 340, 100);
  clutter_actor_set_size (rect, 200, 200);
  clutter_actor_show (rect);

  clutter_container_add (CLUTTER_CONTAINER (group), hand, rect, NULL);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), label);

  /* 3 seconds, at 60 fps */
  timeline = clutter_timeline_new (180, 60);
  g_signal_connect (timeline,
                    "completed", G_CALLBACK (timeline_completed),
                    NULL);

  d_behave = clutter_behaviour_depth_new (clutter_alpha_new_full (timeline,
                                                                  clutter_ramp_inc_func,
                                                                  NULL, NULL),
                                          -100, 100);
  clutter_behaviour_apply (d_behave, label);

  /* add two faced actor */
  janus = janus_group ("GREEN", "RED");
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), janus);
  clutter_actor_set_position (janus, 300, 350);

  r_behave = clutter_behaviour_rotate_new (clutter_alpha_new_full (timeline,
                                                                  clutter_ramp_inc_func,
                                                                  NULL, NULL),
                                          CLUTTER_Y_AXIS,
                                          CLUTTER_ROTATE_CW,
                                          0, 360);
  clutter_behaviour_apply (r_behave, janus);


  /* add hand box */
  box = clone_box (CLUTTER_TEXTURE (hand));
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), box);
  clutter_actor_set_position (box, 200, 250);
  clutter_actor_set_scale (box, 0.5, 0.5);
  clutter_actor_set_rotation (box, CLUTTER_X_AXIS, 45, 0, 0, 0);
  clutter_actor_set_opacity (box, 0x44);

  r_behave = clutter_behaviour_rotate_new (clutter_alpha_new_full (timeline,
                                                                  clutter_ramp_inc_func,
                                                                  NULL, NULL),
                                          CLUTTER_Y_AXIS,
                                          CLUTTER_ROTATE_CW,
                                          0, 360);
  clutter_behaviour_apply (r_behave, box);


  clutter_actor_show (stage);

  clutter_timeline_start (timeline);

  raise_actor[0] = rect;
  raise_actor[1] = hand;
  g_timeout_add (2000, raise_top, NULL);

  clutter_main ();

  g_object_unref (d_behave);
  g_object_unref (timeline);

  return EXIT_SUCCESS;
}
