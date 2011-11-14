#include <stdlib.h>
#include <clutter/clutter.h>

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor red_color = { 0xff, 0x00, 0x00, 0xff };
static const ClutterColor blue_color = { 0x00, 0x00, 0xff, 0xff };

void
clicked_cb (ClutterClickAction *action,
            ClutterActor       *actor,
            gpointer            user_data)
{
  g_debug ("Button %d clicked", clutter_click_action_get_button (action));
}

int
main (int   argc,
      char *argv[])
{
  ClutterActor *stage;
  ClutterAction *action1;
  ClutterAction *action2;
  ClutterActor *actor1;
  ClutterActor *actor2;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 400, 400);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  actor1 = clutter_rectangle_new_with_color (&red_color);
  clutter_actor_set_size (actor1, 100, 100);
  clutter_actor_set_reactive (actor1, TRUE);
  clutter_actor_set_position (actor1, 50, 150);

  actor2 = clutter_rectangle_new_with_color (&blue_color);
  clutter_actor_set_size (actor2, 100, 100);
  clutter_actor_set_position (actor2, 250, 150);
  clutter_actor_set_reactive (actor2, TRUE);

  action1 = clutter_click_action_new ();
  clutter_actor_add_action (actor1, action1);

  action2 = clutter_click_action_new ();
  clutter_actor_add_action (actor2, action2);

  clutter_container_add (CLUTTER_CONTAINER (stage), actor1, actor2, NULL);

  g_signal_connect (action1,
                    "clicked",
                    G_CALLBACK (clicked_cb),
                    NULL);

  g_signal_connect (action2,
                    "clicked",
                    G_CALLBACK (clicked_cb),
                    NULL);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
