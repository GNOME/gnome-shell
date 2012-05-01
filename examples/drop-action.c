#include <stdlib.h>
#include <clutter/clutter.h>

#define TARGET_SIZE     200
#define HANDLE_SIZE     128

static ClutterActor *stage   = NULL;
static ClutterActor *target1 = NULL;
static ClutterActor *target2 = NULL;
static ClutterActor *drag    = NULL;

static gboolean drop_successful = FALSE;

static void add_drag_object (ClutterActor *target);

static void
on_drag_end (ClutterDragAction   *action,
             ClutterActor        *actor,
             gfloat               event_x,
             gfloat               event_y,
             ClutterModifierType  modifiers)
{
  ClutterActor *handle = clutter_drag_action_get_drag_handle (action);

  g_print ("Drag ended at: %.0f, %.0f\n",
           event_x, event_y);

  clutter_actor_animate (actor, CLUTTER_LINEAR, 150, "opacity", 255, NULL);

  if (!drop_successful)
    {
      gfloat x_pos, y_pos;

      clutter_actor_animate (clutter_actor_get_parent (actor),
                             CLUTTER_LINEAR, 150,
                             "opacity", 255,
                             NULL);

      clutter_actor_get_transformed_position (actor, &x_pos, &y_pos);
      clutter_actor_animate (handle, CLUTTER_EASE_OUT_BOUNCE, 250,
                             "x", x_pos,
                             "y", y_pos,
                             "opacity", 0,
                             "signal-swapped::completed",
                               G_CALLBACK (clutter_actor_destroy),
                               handle,
                             NULL);
    }
  else
    {
      clutter_actor_animate (handle, CLUTTER_LINEAR, 250,
                             "opacity", 0,
                             "signal-swapped::completed",
                               G_CALLBACK (clutter_actor_destroy),
                               handle,
                             NULL);
    }
}

static void
on_drag_begin (ClutterDragAction   *action,
               ClutterActor        *actor,
               gfloat               event_x,
               gfloat               event_y,
               ClutterModifierType  modifiers)
{
  ClutterActor *handle;
  gfloat x_pos, y_pos;

  clutter_actor_get_position (actor, &x_pos, &y_pos);

  handle = clutter_rectangle_new_with_color (CLUTTER_COLOR_DarkSkyBlue);
  clutter_actor_set_size (handle, 128, 128);
  clutter_actor_set_position (handle, event_x - x_pos, event_y - y_pos);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), handle);

  clutter_drag_action_set_drag_handle (action, handle);

  clutter_actor_animate (actor, CLUTTER_LINEAR, 150, "opacity", 128, NULL);\

  drop_successful = FALSE;
}

static void
add_drag_object (ClutterActor *target)
{
  ClutterActor *parent;

  if (drag == NULL)
    {
      ClutterAction *action;

      drag = clutter_rectangle_new_with_color (CLUTTER_COLOR_LightSkyBlue);
      clutter_actor_set_size (drag, HANDLE_SIZE, HANDLE_SIZE);
      clutter_actor_set_position (drag,
                                  (TARGET_SIZE - HANDLE_SIZE) / 2.0,
                                  (TARGET_SIZE - HANDLE_SIZE) / 2.0);
      clutter_actor_set_reactive (drag, TRUE);

      action = clutter_drag_action_new ();
      g_signal_connect (action, "drag-begin", G_CALLBACK (on_drag_begin), NULL);
      g_signal_connect (action, "drag-end", G_CALLBACK (on_drag_end), NULL);

      clutter_actor_add_action (drag, action);
    }

  parent = clutter_actor_get_parent (drag);

  if (parent == target)
    {
      clutter_actor_animate (target, CLUTTER_LINEAR, 150,
                             "opacity", 255,
                             NULL);
      return;
    }

  g_object_ref (drag);
  if (parent != NULL && parent != stage)
    {
      clutter_container_remove_actor (CLUTTER_CONTAINER (parent), drag);
      clutter_actor_animate (parent, CLUTTER_LINEAR, 150,
                             "opacity", 64,
                             NULL);
    }

  clutter_container_add_actor (CLUTTER_CONTAINER (target), drag);
  clutter_actor_animate (target, CLUTTER_LINEAR, 150,
                         "opacity", 255,
                         NULL);

  g_object_unref (drag);
}

static void
on_target_over (ClutterDropAction *action,
                ClutterActor      *actor,
                gpointer           _data)
{
  gboolean is_over = GPOINTER_TO_UINT (_data);
  guint8 final_opacity = is_over ? 128 : 64;
  ClutterActor *target;

  target = clutter_actor_meta_get_actor (CLUTTER_ACTOR_META (action));

  clutter_actor_animate (target, CLUTTER_LINEAR, 250,
                         "opacity", final_opacity,
                         NULL);
}

static void
on_target_drop (ClutterDropAction *action,
                ClutterActor      *actor,
                gfloat             event_x,
                gfloat             event_y)
{
  gfloat actor_x, actor_y;

  actor_x = actor_y = 0.0f;

  clutter_actor_transform_stage_point (actor, event_x, event_y,
                                       &actor_x,
                                       &actor_y);

  g_print ("Dropped at %.0f, %.0f (screen: %.0f, %.0f)\n",
           actor_x, actor_y,
           event_x, event_y);

  drop_successful = TRUE;
  add_drag_object (actor);
}

int
main (int argc, char *argv[])
{
  ClutterActor *dummy;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return EXIT_FAILURE;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Drop Action");
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  target1 = clutter_box_new (clutter_fixed_layout_new ());
  clutter_box_set_color (CLUTTER_BOX (target1), CLUTTER_COLOR_LightScarletRed);
  clutter_actor_set_size (target1, TARGET_SIZE, TARGET_SIZE);
  clutter_actor_set_opacity (target1, 64);
  clutter_actor_add_constraint (target1, clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.5));
  clutter_actor_set_x (target1, 10);
  clutter_actor_set_reactive (target1, TRUE);

  clutter_actor_add_action_with_name (target1, "drop", clutter_drop_action_new ());
  g_signal_connect (clutter_actor_get_action (target1, "drop"),
                    "over-in",
                    G_CALLBACK (on_target_over),
                    GUINT_TO_POINTER (TRUE));
  g_signal_connect (clutter_actor_get_action (target1, "drop"),
                    "over-out",
                    G_CALLBACK (on_target_over),
                    GUINT_TO_POINTER (FALSE));
  g_signal_connect (clutter_actor_get_action (target1, "drop"),
                    "drop",
                    G_CALLBACK (on_target_drop),
                    NULL);

  dummy = clutter_rectangle_new_with_color (CLUTTER_COLOR_DarkOrange);
  clutter_actor_set_size (dummy,
                          640 - (2 * 10) - (2 * (TARGET_SIZE + 10)),
                          TARGET_SIZE);
  clutter_actor_add_constraint (dummy, clutter_align_constraint_new (stage, CLUTTER_ALIGN_X_AXIS, 0.5));
  clutter_actor_add_constraint (dummy, clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.5));
  clutter_actor_set_reactive (dummy, TRUE);

  target2 = clutter_box_new (clutter_fixed_layout_new ());
  clutter_box_set_color (CLUTTER_BOX (target2), CLUTTER_COLOR_LightChameleon);
  clutter_actor_set_size (target2, TARGET_SIZE, TARGET_SIZE);
  clutter_actor_set_opacity (target2, 64);
  clutter_actor_add_constraint (target2, clutter_align_constraint_new (stage, CLUTTER_ALIGN_Y_AXIS, 0.5));
  clutter_actor_set_x (target2, 640 - TARGET_SIZE - 10);
  clutter_actor_set_reactive (target2, TRUE);

  clutter_actor_add_action_with_name (target2, "drop", clutter_drop_action_new ());
  g_signal_connect (clutter_actor_get_action (target2, "drop"),
                    "over-in",
                    G_CALLBACK (on_target_over),
                    GUINT_TO_POINTER (TRUE));
  g_signal_connect (clutter_actor_get_action (target2, "drop"),
                    "over-out",
                    G_CALLBACK (on_target_over),
                    GUINT_TO_POINTER (FALSE));
  g_signal_connect (clutter_actor_get_action (target2, "drop"),
                    "drop",
                    G_CALLBACK (on_target_drop),
                    NULL);

  clutter_container_add (CLUTTER_CONTAINER (stage), target1, dummy, target2, NULL);

  add_drag_object (target1);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
