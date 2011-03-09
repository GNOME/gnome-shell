#include <stdlib.h>
#include <clutter/clutter.h>

static guint VERTICAL = 0;
static guint HORIZONTAL = 1;
static guint BOTH = 2;

static void
swept_cb (ClutterSwipeAction    *action,
          ClutterActor          *actor,
          ClutterSwipeDirection  direction,
          guint                  axis)
{
  gchar *direction_str = "";

  if (axis == HORIZONTAL &&
      (direction == CLUTTER_SWIPE_DIRECTION_UP ||
       direction == CLUTTER_SWIPE_DIRECTION_DOWN))
    return;
  else if (axis == VERTICAL &&
           (direction == CLUTTER_SWIPE_DIRECTION_LEFT ||
            direction == CLUTTER_SWIPE_DIRECTION_RIGHT))
    return;

  if (direction & CLUTTER_SWIPE_DIRECTION_UP)
    direction_str = g_strconcat (direction_str, " up", NULL);

  if (direction & CLUTTER_SWIPE_DIRECTION_DOWN)
    direction_str = g_strconcat (direction_str, " down", NULL);

  if (direction & CLUTTER_SWIPE_DIRECTION_LEFT)
    direction_str = g_strconcat (direction_str, " left", NULL);

  if (direction & CLUTTER_SWIPE_DIRECTION_RIGHT)
    direction_str = g_strconcat (direction_str, " right", NULL);

  g_debug ("swept_cb '%s'%s", clutter_actor_get_name (actor), direction_str);
}

static gboolean
gesture_progress_cb (ClutterSwipeAction    *action,
                     ClutterActor          *actor,
                     gpointer               user_data)
{
  return TRUE;
}

static void
gesture_cancel_cb (ClutterSwipeAction    *action,
                   ClutterActor          *actor,
                   gpointer               user_data)
{
  g_debug ("gesture_cancel_cb '%s'", clutter_actor_get_name (actor));
}

static void
attach_action (ClutterActor *actor, guint axis)
{
  ClutterAction *action;

  action = g_object_new (CLUTTER_TYPE_SWIPE_ACTION, NULL);
  clutter_actor_add_action (actor, action);
  g_signal_connect (action, "swept", G_CALLBACK (swept_cb), (gpointer) axis);
  g_signal_connect (action, "gesture-progress", G_CALLBACK (gesture_progress_cb), NULL);
  g_signal_connect (action, "gesture-cancel", G_CALLBACK (gesture_cancel_cb), NULL);
}

G_MODULE_EXPORT int
test_swipe_action_main (int argc, char *argv[])
{
  ClutterActor *stage, *rect;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_get_default ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Swipe action test");
  clutter_actor_set_size (stage, 640, 480);

  rect = clutter_rectangle_new_with_color (CLUTTER_COLOR_Red);
  clutter_actor_set_name (rect, "Vertical swipes");
  clutter_actor_set_size (rect, 150, 150);
  clutter_actor_set_position (rect, 10, 100);
  clutter_actor_set_reactive (rect, TRUE);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);
  attach_action (rect, VERTICAL);

  rect = clutter_rectangle_new_with_color (CLUTTER_COLOR_Blue);
  clutter_actor_set_name (rect, "Horizontal swipes");
  clutter_actor_set_size (rect, 150, 150);
  clutter_actor_set_position (rect, 170, 100);
  clutter_actor_set_reactive (rect, TRUE);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);
  attach_action (rect, HORIZONTAL);

  rect = clutter_rectangle_new_with_color (CLUTTER_COLOR_Green);
  clutter_actor_set_name (rect, "All swipes");
  clutter_actor_set_size (rect, 150, 150);
  clutter_actor_set_position (rect, 330, 100);
  clutter_actor_set_reactive (rect, TRUE);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);
  attach_action (rect, BOTH);

  clutter_actor_show_all (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
