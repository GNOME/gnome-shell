#include <stdlib.h>
#include <clutter/clutter.h>

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor red_color = { 0xff, 0x00, 0x00, 0xff };

static gboolean
key_pressed_cb (ClutterActor *actor,
                ClutterEvent *event,
                gpointer      user_data)
{
  ClutterTimeline *timeline = CLUTTER_TIMELINE (user_data);

  if (!clutter_timeline_is_playing (timeline))
    clutter_timeline_start (timeline);

  return TRUE;
}

int
main (int   argc,
      char *argv[])
{
  ClutterActor *stage;
  ClutterActor *actor;
  ClutterTimeline *timeline;
  ClutterAnimator *animator;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 300, 200);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  actor = clutter_rectangle_new_with_color (&red_color);
  clutter_actor_set_size (actor, 100, 100);
  clutter_actor_set_position (actor, 150, 50);

  timeline = clutter_timeline_new (2000);
  clutter_timeline_set_repeat_count (timeline, -1);

  animator = clutter_animator_new ();
  clutter_animator_set_timeline (animator, timeline);

  clutter_animator_set (animator,
                        actor, "x", CLUTTER_LINEAR, 0.0, 150.0,
                        actor, "x", CLUTTER_LINEAR, 0.5, 50.0,
                        actor, "x", CLUTTER_LINEAR, 1.0, 150.0,
                        NULL);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), actor);

  g_signal_connect (stage,
                    "key-press-event",
                    G_CALLBACK (key_pressed_cb),
                    timeline);

  clutter_actor_show (stage);

  clutter_main ();

  g_object_unref (animator);

  return EXIT_SUCCESS;
}
