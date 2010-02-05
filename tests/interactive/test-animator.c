#include <stdlib.h>
#include <math.h>
#include <gmodule.h>
#include <clutter/clutter.h>

static ClutterAnimator *animator;

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

static gboolean nuke_one (gpointer actor)
{
  clutter_actor_destroy (actor);
  return FALSE;
}

#define COUNT 4

static void reverse_timeline (ClutterTimeline *timeline,
                              gpointer         data)
{
  ClutterTimelineDirection direction = clutter_timeline_get_direction (timeline);
  if (direction == CLUTTER_TIMELINE_FORWARD)
    clutter_timeline_set_direction (timeline, CLUTTER_TIMELINE_BACKWARD);
  else
    clutter_timeline_set_direction (timeline, CLUTTER_TIMELINE_FORWARD);
  clutter_timeline_start (timeline);
}


G_MODULE_EXPORT gint
test_animator_main (gint    argc,
                    gchar **argv)
{
  ClutterActor *stage;
  ClutterActor *rects[COUNT];
  gint i;
  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  for (i=0; i<COUNT; i++)
    {
      rects[i]=new_rect (255 *(i * 1.0/COUNT), 50, 160, 255);
      clutter_container_add_actor (CLUTTER_CONTAINER (stage), rects[i]);
      clutter_actor_set_anchor_point (rects[i], 64, 64);
      clutter_actor_set_position (rects[i], 320.0, 240.0);
      clutter_actor_set_opacity (rects[i], 0x70);
    }

  g_timeout_add (10000, nuke_one, rects[2]);

  animator = clutter_animator_new ();

  /* Note: when both animations are active for the same actor at the same
   * time there is a race, such races should be handled by avoiding
   * controlling the same properties from multiple animations. This is
   * an intentional design flaw of this test for testing the corner case.
   */

  clutter_animator_set (animator,
         rects[0], "x",       1,                    0.0,  180.0,
         rects[0], "x",       CLUTTER_LINEAR,       0.25, 450.0,
         rects[0], "x",       CLUTTER_LINEAR,       0.5,  450.0,
         rects[0], "x",       CLUTTER_LINEAR,       0.75, 180.0,
         rects[0], "x",       CLUTTER_LINEAR,       1.0,  180.0,

         rects[0], "y",       -1,                   0.0,   100.0,
         rects[0], "y",       CLUTTER_LINEAR,       0.25,  100.0,
         rects[0], "y",       CLUTTER_LINEAR,       0.5,   380.0,
         rects[0], "y",       CLUTTER_LINEAR,       0.75,  380.0,
         rects[0], "y",       CLUTTER_LINEAR,       1.0,   100.0,

         rects[3], "x",       0,                    0.0,  180.0,
         rects[3], "x",       CLUTTER_LINEAR,       0.25, 180.0,
         rects[3], "x",       CLUTTER_LINEAR,       0.5,  450.0,
         rects[3], "x",       CLUTTER_LINEAR,       0.75, 450.0,
         rects[3], "x",       CLUTTER_LINEAR,       1.0,  180.0,

         rects[3], "y",       0,                    0.0,  100.0,
         rects[3], "y",       CLUTTER_LINEAR,       0.25, 380.0,
         rects[3], "y",       CLUTTER_LINEAR,       0.5,  380.0,
         rects[3], "y",       CLUTTER_LINEAR,       0.75, 100.0,
         rects[3], "y",       CLUTTER_LINEAR,       1.0,  100.0,


         rects[2], "rotation-angle-y",       0,                    0.0,  0.0,
         rects[2], "rotation-angle-y",       CLUTTER_LINEAR,       1.0,  360.0,

         rects[1], "scale-x", 0,                    0.0,  1.0,
         rects[1], "scale-x", CLUTTER_LINEAR,       1.0,  2.0,
         rects[1], "scale-y", 0,                    0.0,  1.0,
         rects[1], "scale-y", CLUTTER_LINEAR,       1.0,  2.0,
         NULL);


  clutter_actor_set_scale (rects[0], 1.4, 1.4);
  clutter_animator_property_set_ease_in (animator, G_OBJECT (rects[0]), "x",
                                         TRUE);
  clutter_animator_property_set_ease_in (animator, G_OBJECT (rects[0]), "y",
                                         TRUE);
  clutter_animator_property_set_interpolation (animator, G_OBJECT (rects[0]),
                                               "x", CLUTTER_INTERPOLATION_CUBIC);
  clutter_animator_property_set_interpolation (animator, G_OBJECT (rects[0]),
                                               "y", CLUTTER_INTERPOLATION_CUBIC);

  clutter_actor_show (stage);

  clutter_animator_set_duration (animator, 5000);

  g_signal_connect (clutter_animator_run (animator),
                    "completed", G_CALLBACK (reverse_timeline), NULL);
  clutter_main ();
  g_object_unref (animator);

  return EXIT_SUCCESS;
}
