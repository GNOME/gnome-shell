#include <stdlib.h>
#include <math.h>
#include <gmodule.h>
#include <clutter/clutter.h>

static ClutterState    *state;
static ClutterAnimator *animator;

static gboolean press_event (ClutterActor *actor,
                             ClutterEvent *event,
                             gpointer      user_data)
{
  clutter_grab_pointer (actor);
  clutter_state_set_state (state, "end");
  return TRUE;
}

static gboolean release_event (ClutterActor *actor,
                               ClutterEvent *event,
                               gpointer      user_data)
{
  clutter_state_set_state (state, "start");
  clutter_ungrab_pointer ();
  return TRUE;
}


static ClutterActor *new_rect (gint r,
                               gint g,
                               gint b,
                               gint a)
{
  GError *error = NULL;
  ClutterColor *color = clutter_color_new (r, g, b, a);
  ClutterActor *rectangle;

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
test_state_animator_main (gint    argc,
                          gchar **argv)
{
  ClutterActor *stage;
  ClutterActor *rects[40];
  gint i;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "State and Animator");
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  for (i = 0; i < 2; i++)
    {
      rects[i] = new_rect (255 * (i * 1.0 / 40), 50, 160, 255);
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

  animator = clutter_animator_new ();
  clutter_animator_set (animator,
     rects[0], "depth",   -1,                   0.0, 0.0,
     rects[0], "depth",   CLUTTER_LINEAR,       1.0, 275.0,
     rects[0], "x",       -1,                   0.0, 0.0,
     rects[0], "x",       CLUTTER_LINEAR,       0.5,  200.0,
     rects[0], "x",       CLUTTER_LINEAR,       1.0,  320.0,

     rects[0], "y",       -1,                   0.0,  0.0,
     rects[0], "y",       CLUTTER_LINEAR,       0.3,  100.0,
     rects[0], "y",       CLUTTER_LINEAR,       1.0,  240.0,

     rects[1], "opacity", -1,                   0.0,  0x20,
     rects[1], "opacity", CLUTTER_LINEAR,       1.0,  0xff,
     rects[1], "scale-x", -1,                   0.0,  1.0,
     rects[1], "scale-x", CLUTTER_LINEAR,       0.5,  2.0,
     rects[1], "scale-x", CLUTTER_LINEAR,       1.0,  2.0,
     rects[1], "scale-y", -1,                   0.0,  1.0,
     rects[1], "scale-y", CLUTTER_LINEAR,       0.5,  2.0,
     rects[1], "scale-y", CLUTTER_LINEAR,       1.0,  2.0,
     NULL);

  clutter_animator_property_set_ease_in (animator, G_OBJECT (rects[0]), "depth", TRUE);
  clutter_animator_property_set_ease_in (animator, G_OBJECT (rects[0]), "x", TRUE);
  clutter_animator_property_set_ease_in (animator, G_OBJECT (rects[0]), "y", TRUE);
  clutter_animator_property_set_ease_in (animator, G_OBJECT (rects[1]), "opacity", TRUE);
  clutter_animator_property_set_ease_in (animator, G_OBJECT (rects[1]), "scale-x", TRUE);
  clutter_animator_property_set_ease_in (animator, G_OBJECT (rects[1]), "scale-y", TRUE);

  clutter_animator_property_set_interpolation (animator, G_OBJECT (rects[0]), "x",
                                               CLUTTER_INTERPOLATION_CUBIC);
  clutter_animator_property_set_interpolation (animator, G_OBJECT (rects[0]), "y",
                                               CLUTTER_INTERPOLATION_CUBIC);

  clutter_state_set_animator (state, "start", "end", animator);
  g_object_unref (animator);

  clutter_actor_show (stage);
  clutter_state_set_state (state, "start");

  clutter_main ();
  g_object_unref (state);

  return EXIT_SUCCESS;
}

G_MODULE_EXPORT const char *
test_state_animator_describe (void)
{
  return "Animate using the State and Animator classes.";
}
