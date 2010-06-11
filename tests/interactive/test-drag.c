#include <stdlib.h>
#include <gmodule.h>
#include <clutter/clutter.h>

static gboolean
on_enter (ClutterActor *actor,
          ClutterEvent *event)
{
  clutter_actor_animate (actor, CLUTTER_LINEAR, 150,
                         "@effects.curl.period", 0.25,
                         NULL);

  return FALSE;
}

static gboolean
on_leave (ClutterActor *actor,
          ClutterEvent *event)
{
  clutter_actor_animate (actor, CLUTTER_LINEAR, 150,
                         "@effects.curl.period", 0.0,
                         NULL);

  return FALSE;
}

static void
on_drag_begin (ClutterDragAction   *action,
               ClutterActor        *actor,
               gfloat               event_x,
               gfloat               event_y,
               gint                 button,
               ClutterModifierType  modifiers)
{
  gboolean is_copy = (modifiers & CLUTTER_SHIFT_MASK) ? TRUE : FALSE;
  ClutterActor *drag_handle = NULL;

  if (is_copy)
    {
      ClutterActor *stage = clutter_actor_get_stage (actor);
      ClutterColor handle_color;

      drag_handle = clutter_rectangle_new ();
      clutter_actor_set_size (drag_handle, 48, 48);

      clutter_color_from_string (&handle_color, "#204a87aa");
      clutter_rectangle_set_color (CLUTTER_RECTANGLE (drag_handle), &handle_color);

      clutter_container_add_actor (CLUTTER_CONTAINER (stage), drag_handle);
      clutter_actor_set_position (drag_handle, event_x, event_y);
    }
  else
    drag_handle = actor;

  clutter_drag_action_set_drag_handle (action, drag_handle);

  /* fully desaturate the actor */
  clutter_actor_animate (actor, CLUTTER_LINEAR, 150,
                         "@effects.disable.factor", 1.0,
                         NULL);
}

static void
on_drag_end (ClutterDragAction   *action,
             ClutterActor        *actor,
             gfloat               event_x,
             gfloat               event_y,
             gint                 button,
             ClutterModifierType  modifiers)
{
  ClutterActor *drag_handle;

  drag_handle = clutter_drag_action_get_drag_handle (action);
  if (actor != drag_handle)
    {
      gfloat real_x, real_y;
      ClutterActor *parent;

      /* if we are dragging a copy we can destroy the copy now
       * and animate the real actor to the drop coordinates,
       * transformed in the parent's coordinate space
       */
      clutter_actor_animate (drag_handle, CLUTTER_LINEAR, 150,
                             "opacity", 0,
                             "signal-swapped-after::completed",
                               G_CALLBACK (clutter_actor_destroy),
                               drag_handle,
                             NULL);

      parent = clutter_actor_get_parent (actor);
      clutter_actor_transform_stage_point (parent, event_x, event_y,
                                           &real_x,
                                           &real_y);

      clutter_actor_animate (actor, CLUTTER_EASE_OUT_CUBIC, 150,
                             "@effects.disable.factor", 0.0,
                             "x", real_x,
                             "y", real_y,
                             NULL);
    }
  else
    clutter_actor_animate (actor, CLUTTER_LINEAR, 150,
                           "@effects.disable.factor", 0.0,
                           NULL);
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
static gint drag_threshold = 0;

static GOptionEntry entries[] = {
  {
    "threshold", 't',
    0,
    G_OPTION_ARG_INT,
    &drag_threshold,
    "Set the drag threshold", "PIXELS"
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

G_MODULE_EXPORT int
test_drag_main (int argc, char *argv[])
{
  ClutterActor *stage, *handle;
  ClutterAction *action;
  ClutterColor handle_color;
  GError *error;

  error = NULL;
  clutter_init_with_args (&argc, &argv,
                          "test-drag",
                          entries,
                          NULL,
                          &error);
  if (error != NULL)
    {
      g_print ("Unable to run test-drag: %s\n", error->message);
      g_error_free (error);

      return EXIT_FAILURE;
    }

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Drag Test");
  clutter_actor_set_size (stage, 800, 600);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  clutter_color_from_string (&handle_color, "#729fcfff");

  handle = clutter_rectangle_new ();
  clutter_rectangle_set_color (CLUTTER_RECTANGLE (handle), &handle_color);
  clutter_actor_set_size (handle, 128, 128);
  clutter_actor_set_position (handle, (800 - 128) / 2, (600 - 128) / 2);
  clutter_actor_set_reactive (handle, TRUE);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), handle);
  g_signal_connect (handle, "enter-event", G_CALLBACK (on_enter), NULL);
  g_signal_connect (handle, "leave-event", G_CALLBACK (on_leave), NULL);

  action = clutter_drag_action_new ();
  clutter_drag_action_set_drag_threshold (CLUTTER_DRAG_ACTION (action),
                                          drag_threshold);
  clutter_drag_action_set_drag_axis (CLUTTER_DRAG_ACTION (action),
                                     get_drag_axis (drag_axis));

  g_signal_connect (action, "drag-begin", G_CALLBACK (on_drag_begin), NULL);
  g_signal_connect (action, "drag-end", G_CALLBACK (on_drag_end), NULL);

  clutter_actor_add_action (handle, action);

  clutter_actor_add_effect_with_name (handle, "disable", clutter_desaturate_effect_new (0.0));
  clutter_actor_add_effect_with_name (handle, "curl", clutter_page_turn_effect_new (0.0, 135.0, 12.0));

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}
