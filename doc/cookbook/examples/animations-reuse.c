#include <stdlib.h>
#include <clutter/clutter.h>

gboolean
foo_button_pressed_cb (ClutterActor *actor,
                       ClutterEvent *event,
                       gpointer      user_data)
{
  ClutterScript *script = CLUTTER_SCRIPT (user_data);

  ClutterAnimator *bounce;
  ClutterActor *rig;
  ClutterTimeline *bounce_timeline;

  clutter_script_get_objects (script,
                              "bounce_timeline", &bounce_timeline,
                              "rig", &rig,
                              "bounce", &bounce,
                              NULL);

  /* don't allow animation to be reused until it is complete */
  if (clutter_timeline_is_playing (bounce_timeline))
    return TRUE;

  clutter_actor_set_position (rig, 0, 0);

  clutter_actor_set_scale (rig, 1.0, 1.0);

  clutter_actor_reparent (actor, rig);

  clutter_animator_start (bounce);

  return TRUE;
}

/* set the actor's position to the rig's position
 * but parented to the background;
 * remove the click handler so the rectangle can't be animated again
 */
static void
_move_to_background_cb (ClutterActor *actor,
                        gpointer      user_data)
{
  ClutterActor *background = CLUTTER_ACTOR (user_data);

  gfloat x, y;
  clutter_actor_get_position (clutter_actor_get_parent (actor), &x, &y);

  gdouble scale_x, scale_y;
  clutter_actor_get_scale (clutter_actor_get_parent (actor), &scale_x, &scale_y);

  clutter_actor_reparent (actor, background);

  clutter_actor_set_position (actor, x, y);
  clutter_actor_set_scale (actor, scale_x, scale_y);

  g_signal_handlers_disconnect_matched (actor,
                                        G_SIGNAL_MATCH_FUNC,
                                        0,
                                        0,
                                        NULL,
                                        foo_button_pressed_cb,
                                        NULL);
}

void
foo_rig_animation_complete_cb (ClutterTimeline *timeline,
                               gpointer         user_data)
{
  ClutterScript *script = CLUTTER_SCRIPT (user_data);

  ClutterContainer *rig;
  ClutterActor *background;

  clutter_script_get_objects (script,
                              "rig", &rig,
                              "background", &background,
                              NULL);

  clutter_container_foreach (rig,
                             CLUTTER_CALLBACK (_move_to_background_cb),
                             background);
}

int
main (int argc, char *argv[])
{
  clutter_init (&argc, &argv);

  gchar *filename = "animations-reuse.json";
  GError *error = NULL;

  ClutterScript *script = clutter_script_new ();
  clutter_script_load_from_file (script, filename, &error);

  if (error != NULL)
    {
      g_critical ("Error loading ClutterScript file %s\n%s", filename, error->message);
      g_error_free (error);
      exit (EXIT_FAILURE);
    }

  clutter_script_connect_signals (script, script);

  ClutterActor *stage;
  clutter_script_get_objects (script,
                              "stage", &stage,
                              NULL);

  clutter_actor_show (stage);

  clutter_main ();

  g_object_unref (script);

  return EXIT_SUCCESS;
}
