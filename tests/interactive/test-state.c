#include <stdlib.h>
#include <math.h>
#include <gmodule.h>
#include <clutter/clutter.h>

static ClutterState    *state;

static gboolean press_event (ClutterActor *actor,
                             ClutterEvent *event,
                             gpointer      user_data)
{
  clutter_grab_pointer (actor);
  clutter_state_change (state, "end");
  return TRUE;
}

static gboolean release_event (ClutterActor *actor,
                               ClutterEvent *event,
                               gpointer      user_data)
{
  clutter_state_change (state, "start");
  clutter_ungrab_pointer ();
  return TRUE;
}

static void completed (ClutterState *state,
                       gpointer      data)
{
  g_print ("Completed transitioning to state: %s\n",
           clutter_state_get_target_state (state), data);
}

static ClutterActor *new_rect (gint r,
                               gint g,
                               gint b,
                               gint a)
{
  GError *error = NULL;
  ClutterColor *color = clutter_color_new (r, g, b, a);
  ClutterActor *rectangle = clutter_rectangle_new_with_color (color);

  gchar *file = g_build_filename (TESTS_DATADIR, "redhand.png", NULL);
  rectangle = clutter_texture_new_from_file (file, &error);
  if (rectangle == NULL)
    g_error ("image load failed: %s", error->message);
  g_free (file);

  clutter_actor_set_size (rectangle, 128, 128);
  clutter_color_free (color);
  return rectangle;
}

G_MODULE_EXPORT gint
test_state_main (gint    argc,
                 gchar **argv)
{
  ClutterActor *stage;
  ClutterActor *rects[40];
  gint i;
  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  for (i=0; i<2; i++)
    {
      rects[i]=new_rect (255 *(i * 1.0/40), 50, 160, 255);
      clutter_container_add_actor (CLUTTER_CONTAINER (stage), rects[i]);
      clutter_actor_set_anchor_point (rects[i], 64, 64);
      clutter_actor_set_position (rects[i], 320.0, 240.0);
      clutter_actor_set_opacity (rects[i], 0x70);

      clutter_actor_set_reactive (rects[i], TRUE);
      g_signal_connect (rects[i], "button-press-event", G_CALLBACK (press_event), NULL);
      g_signal_connect (rects[i], "button-release-event", G_CALLBACK (release_event), NULL);
    }

  state = clutter_state_new ();
  clutter_state_set (state, NULL, "start",
          rects[0], "depth",   CLUTTER_LINEAR, 0.0,
          rects[0], "x",       CLUTTER_LINEAR, 100.0,
          rects[0], "y",       CLUTTER_LINEAR, 300.0,
          rects[1], "opacity", CLUTTER_LINEAR, 0x20,
          rects[1], "scale-x", CLUTTER_LINEAR, 1.0,
          rects[1], "scale-y", CLUTTER_LINEAR, 1.0,
          NULL);
  clutter_state_set (state, NULL, "end",
          rects[0], "depth",   CLUTTER_LINEAR, 200.0,
          rects[0], "x",       CLUTTER_LINEAR, 320.0,
          rects[0], "y",       CLUTTER_LINEAR, 240.0,
          rects[1], "opacity", CLUTTER_LINEAR, 0xff,
          rects[1], "scale-x", CLUTTER_LINEAR, 2.0,
          rects[1], "scale-y", CLUTTER_LINEAR, 2.0,
          NULL);
  clutter_state_set_duration (state, "start", "end", 5000);
  g_signal_connect (state, "completed", G_CALLBACK (completed), NULL);

  clutter_actor_show (stage);
  clutter_state_change (state, "start");

  clutter_main ();
  g_object_unref (state);

  return EXIT_SUCCESS;
}
