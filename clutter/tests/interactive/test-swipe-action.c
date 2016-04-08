#include <stdlib.h>
#include <clutter/clutter.h>

enum {
  VERTICAL      = 0,
  HORIZONTAL    = 1,
  BOTH          = 2
};

static void
swept_cb (ClutterSwipeAction    *action,
          ClutterActor          *actor,
          ClutterSwipeDirection  direction,
          gpointer               data_)
{
  guint axis = GPOINTER_TO_UINT (data_);
  gchar *direction_str = g_strdup ("");

  if (axis == HORIZONTAL &&
      ((direction & CLUTTER_SWIPE_DIRECTION_UP) != 0 ||
       (direction & CLUTTER_SWIPE_DIRECTION_DOWN) != 0))
    {
      g_print ("discarding non-horizontal swipe on '%s'\n",
               clutter_actor_get_name (actor));
      return;
    }

  if (axis == VERTICAL &&
      ((direction & CLUTTER_SWIPE_DIRECTION_LEFT) != 0 ||
       (direction & CLUTTER_SWIPE_DIRECTION_RIGHT) != 0))
    {
      g_print ("discarding non-vertical swipe on '%s'\n",
               clutter_actor_get_name (actor));
      return;
    }

  if (direction & CLUTTER_SWIPE_DIRECTION_UP)
    {
      char *old_str = direction_str;

      direction_str = g_strconcat (direction_str, " up", NULL);
      g_free (old_str);
    }

  if (direction & CLUTTER_SWIPE_DIRECTION_DOWN)
    {
      char *old_str = direction_str;

      direction_str = g_strconcat (direction_str, " down", NULL);
      g_free (old_str);
    }

  if (direction & CLUTTER_SWIPE_DIRECTION_LEFT)
    {
      char *old_str = direction_str;

      direction_str = g_strconcat (direction_str, " left", NULL);
      g_free (old_str);
    }

  if (direction & CLUTTER_SWIPE_DIRECTION_RIGHT)
    {
      char *old_str = direction_str;

      direction_str = g_strconcat (direction_str, " right", NULL);
      g_free (old_str);
    }

  g_print ("swept: '%s': %s\n", clutter_actor_get_name (actor), direction_str);

  g_free (direction_str);
}

static void
gesture_cancel_cb (ClutterSwipeAction    *action,
                   ClutterActor          *actor,
                   gpointer               user_data)
{
  g_debug ("gesture cancelled: '%s'", clutter_actor_get_name (actor));
}

static void
attach_action (ClutterActor *actor, guint axis)
{
  ClutterAction *action;

  action = g_object_new (CLUTTER_TYPE_SWIPE_ACTION, NULL);
  clutter_actor_add_action (actor, action);
  g_signal_connect (action, "swept", G_CALLBACK (swept_cb), GUINT_TO_POINTER (axis));
  g_signal_connect (action, "gesture-cancel", G_CALLBACK (gesture_cancel_cb), NULL);
}

G_MODULE_EXPORT int
test_swipe_action_main (int argc, char *argv[])
{
  ClutterActor *stage, *rect;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Swipe action");
  clutter_actor_set_size (stage, 640, 480);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

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

  {
    ClutterLayoutManager *layout = clutter_box_layout_new ();
    ClutterActor *box, *label;
    float offset;

    clutter_box_layout_set_vertical (CLUTTER_BOX_LAYOUT (layout), TRUE);
    clutter_box_layout_set_spacing (CLUTTER_BOX_LAYOUT (layout), 6);

    box = clutter_box_new (layout);

    label = clutter_text_new ();
    clutter_text_set_markup (CLUTTER_TEXT (label),
                             "<b>Red</b>: vertical swipes only");
    clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (layout),
                             label,
                             TRUE,
                             TRUE, TRUE,
                             CLUTTER_BOX_ALIGNMENT_START,
                             CLUTTER_BOX_ALIGNMENT_CENTER);

    label = clutter_text_new ();
    clutter_text_set_markup (CLUTTER_TEXT (label),
                             "<b>Blue</b>: horizontal swipes only");
    clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (layout),
                             label,
                             TRUE,
                             TRUE, TRUE,
                             CLUTTER_BOX_ALIGNMENT_START,
                             CLUTTER_BOX_ALIGNMENT_CENTER);

    label = clutter_text_new ();
    clutter_text_set_markup (CLUTTER_TEXT (label),
                             "<b>Green</b>: both");
    clutter_box_layout_pack (CLUTTER_BOX_LAYOUT (layout),
                             label,
                             TRUE,
                             TRUE, TRUE,
                             CLUTTER_BOX_ALIGNMENT_START,
                             CLUTTER_BOX_ALIGNMENT_CENTER);

    offset = clutter_actor_get_height (stage)
           - clutter_actor_get_height (box)
           - 12.0;

    clutter_container_add_actor (CLUTTER_CONTAINER (stage), box);
    clutter_actor_add_constraint (box, clutter_bind_constraint_new (stage,
                                                                    CLUTTER_BIND_X,
                                                                    12.0));
    clutter_actor_add_constraint (box, clutter_bind_constraint_new (stage,
                                                                    CLUTTER_BIND_Y,
                                                                    offset));
  }

  clutter_actor_show_all (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}

G_MODULE_EXPORT const char *
test_swipe_action_describe (void)
{
  return "Swipe gesture recognizer.";
}
