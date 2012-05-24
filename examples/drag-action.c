#include <stdlib.h>
#include <clutter/clutter.h>

static gboolean
on_enter (ClutterActor *actor,
          ClutterEvent *event)
{
  ClutterTransition *t;

  t = clutter_actor_get_transition (actor, "curl");
  if (t == NULL)
    {
      t = clutter_property_transition_new ("@effects.curl.period");
      clutter_timeline_set_duration (CLUTTER_TIMELINE (t), 250);
      clutter_actor_add_transition (actor, "curl", t);
      g_object_unref (t);
    }

  clutter_transition_set_from (t, G_TYPE_DOUBLE, 0.0);
  clutter_transition_set_to (t, G_TYPE_DOUBLE, 0.25);
  clutter_timeline_rewind (CLUTTER_TIMELINE (t));
  clutter_timeline_start (CLUTTER_TIMELINE (t));

  return CLUTTER_EVENT_STOP;
}

static gboolean
on_leave (ClutterActor *actor,
          ClutterEvent *event)
{
  ClutterTransition *t;

  t = clutter_actor_get_transition (actor, "curl");
  if (t == NULL)
    {
      t = clutter_property_transition_new ("@effects.curl.period");
      clutter_timeline_set_duration (CLUTTER_TIMELINE (t), 250);
      clutter_actor_add_transition (actor, "curl", t);
      g_object_unref (t);
    }

  clutter_transition_set_from (t, G_TYPE_DOUBLE, 0.25);
  clutter_transition_set_to (t, G_TYPE_DOUBLE, 0.0);
  clutter_timeline_rewind (CLUTTER_TIMELINE (t));
  clutter_timeline_start (CLUTTER_TIMELINE (t));

  return CLUTTER_EVENT_STOP;
}

static void
on_drag_begin (ClutterDragAction   *action,
               ClutterActor        *actor,
               gfloat               event_x,
               gfloat               event_y,
               ClutterModifierType  modifiers)
{
  gboolean is_copy = (modifiers & CLUTTER_SHIFT_MASK) ? TRUE : FALSE;
  ClutterActor *drag_handle = NULL;
  ClutterTransition *t;

  if (is_copy)
    {
      ClutterActor *stage = clutter_actor_get_stage (actor);

      drag_handle = clutter_actor_new ();
      clutter_actor_set_size (drag_handle, 48, 48);

      clutter_actor_set_background_color (drag_handle, CLUTTER_COLOR_DarkSkyBlue);

      clutter_actor_add_child (stage, drag_handle);
      clutter_actor_set_position (drag_handle, event_x, event_y);
    }
  else
    drag_handle = actor;

  clutter_drag_action_set_drag_handle (action, drag_handle);

  /* fully desaturate the actor */
  t = clutter_actor_get_transition (actor, "disable");
  if (t == NULL)
    {
      t = clutter_property_transition_new ("@effects.disable.factor");
      clutter_timeline_set_duration (CLUTTER_TIMELINE (t), 250);
      clutter_actor_add_transition (actor, "disable", t);
      g_object_unref (t);
    }

  clutter_transition_set_from (t, G_TYPE_DOUBLE, 0.0);
  clutter_transition_set_to (t, G_TYPE_DOUBLE, 1.0);
  clutter_timeline_rewind (CLUTTER_TIMELINE (t));
  clutter_timeline_start (CLUTTER_TIMELINE (t));
}

static void
on_drag_end (ClutterDragAction   *action,
             ClutterActor        *actor,
             gfloat               event_x,
             gfloat               event_y,
             ClutterModifierType  modifiers)
{
  ClutterActor *drag_handle;
  ClutterTransition *t;

  drag_handle = clutter_drag_action_get_drag_handle (action);
  if (actor != drag_handle)
    {
      gfloat real_x, real_y;
      ClutterActor *parent;

      /* if we are dragging a copy we can destroy the copy now
       * and animate the real actor to the drop coordinates,
       * transformed in the parent's coordinate space
       */
      clutter_actor_save_easing_state (drag_handle);
      clutter_actor_set_easing_mode (drag_handle, CLUTTER_LINEAR);
      clutter_actor_set_opacity (drag_handle, 0);
      clutter_actor_restore_easing_state (drag_handle);
      g_signal_connect (drag_handle, "transitions-completed",
                        G_CALLBACK (clutter_actor_destroy),
                        NULL);

      parent = clutter_actor_get_parent (actor);
      clutter_actor_transform_stage_point (parent, event_x, event_y,
                                           &real_x,
                                           &real_y);

      clutter_actor_save_easing_state (actor);
      clutter_actor_set_easing_mode (actor, CLUTTER_EASE_OUT_CUBIC);
      clutter_actor_set_position (actor, real_x, real_y);
      clutter_actor_restore_easing_state (actor);
    }

  t = clutter_actor_get_transition (actor, "disable");
  if (t == NULL)
    {
      t = clutter_property_transition_new ("@effects.disable.factor");
      clutter_timeline_set_duration (CLUTTER_TIMELINE (t), 250);
      clutter_actor_add_transition (actor, "disable", t);
      g_object_unref (t);
    }

  clutter_transition_set_from (t, G_TYPE_DOUBLE, 1.0);
  clutter_transition_set_to (t, G_TYPE_DOUBLE, 0.0);
  clutter_timeline_rewind (CLUTTER_TIMELINE (t));
  clutter_timeline_start (CLUTTER_TIMELINE (t));
}

static ClutterDragAxis
get_drag_axis (const gchar *str)
{
  if (str == NULL || *str == '\0')
    return CLUTTER_DRAG_AXIS_NONE;

  if (*str == 'x' || *str == 'X')
    return CLUTTER_DRAG_X_AXIS;

  if (*str == 'y' || *str == 'Y')
    return CLUTTER_DRAG_Y_AXIS;

  g_warn_if_reached ();

  return CLUTTER_DRAG_AXIS_NONE;
}

static gchar *drag_axis = NULL;
static gint x_drag_threshold = 0;
static gint y_drag_threshold = 0;

static GOptionEntry entries[] = {
  {
    "x-threshold", 'x',
    0,
    G_OPTION_ARG_INT,
    &x_drag_threshold,
    "Set the horizontal drag threshold", "PIXELS"
  },
  {
    "y-threshold", 'y',
    0,
    G_OPTION_ARG_INT,
    &y_drag_threshold,
    "Set the vertical drag threshold", "PIXELS"
  },
  {
    "axis", 'a',
    0,
    G_OPTION_ARG_STRING,
    &drag_axis,
    "Set the drag axis", "AXIS"
  },

  { NULL }
};

int
main (int argc, char *argv[])
{
  ClutterActor *stage, *handle;
  ClutterAction *action;
  GError *error;

  error = NULL;
  if (clutter_init_with_args (&argc, &argv,
                              "test-drag",
                              entries,
                              NULL,
                              &error) != CLUTTER_INIT_SUCCESS)
    {
      g_print ("Unable to run drag-action: %s\n", error->message);
      g_error_free (error);

      return EXIT_FAILURE;
    }

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Drag Test");
  clutter_actor_set_size (stage, 800, 600);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  handle = clutter_actor_new ();
  clutter_actor_set_background_color (handle, CLUTTER_COLOR_SkyBlue);
  clutter_actor_set_size (handle, 128, 128);
  clutter_actor_set_position (handle, (800 - 128) / 2, (600 - 128) / 2);
  clutter_actor_set_reactive (handle, TRUE);
  clutter_actor_add_child (stage, handle);
  g_signal_connect (handle, "enter-event", G_CALLBACK (on_enter), NULL);
  g_signal_connect (handle, "leave-event", G_CALLBACK (on_leave), NULL);

  action = clutter_drag_action_new ();
  clutter_drag_action_set_drag_threshold (CLUTTER_DRAG_ACTION (action),
                                          x_drag_threshold,
                                          y_drag_threshold);
  clutter_drag_action_set_drag_axis (CLUTTER_DRAG_ACTION (action),
                                     get_drag_axis (drag_axis));

  g_signal_connect (action, "drag-begin", G_CALLBACK (on_drag_begin), NULL);
  g_signal_connect (action, "drag-end", G_CALLBACK (on_drag_end), NULL);

  clutter_actor_add_action (handle, action);

  clutter_actor_add_effect_with_name (handle, "disable", clutter_desaturate_effect_new (0.0));
  clutter_actor_add_effect_with_name (handle, "curl", clutter_page_turn_effect_new (0.0, 45.0, 12.0));

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
