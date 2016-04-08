#include <stdlib.h>
#include <math.h>
#include <gmodule.h>
#include <clutter/clutter.h>
#include "test-common.h"

static gint times = 16;

#define STAGE_WIDTH    800
#define STAGE_HEIGHT   600

#define ACTOR_WIDTH    64
#define ACTOR_HEIGHT   64

#define COLS  (STAGE_WIDTH/ACTOR_WIDTH)
#define ROWS  (STAGE_HEIGHT/ACTOR_HEIGHT)
#define TOTAL (ROWS*COLS)


static void completed (ClutterState *state,
                       gpointer      data)
{
  if (g_str_equal (clutter_state_get_state (state), "right"))
    {
      /* skip straight to left state when reaching right */
      clutter_state_warp_to_state (state, "left");
    }
  else if (g_str_equal (clutter_state_get_state (state), "active"))
      clutter_state_set_state (state, "right");
  else
    {
      clutter_state_set_state (state, "active");
    }
  times --;
  if (times <=0)
    clutter_main_quit ();
}

static ClutterActor *new_rect (gint r,
                               gint g,
                               gint b,
                               gint a)
{
  GError *error = NULL;
  ClutterColor *color = clutter_color_new (r, g, b, a);
  ClutterActor *group = clutter_group_new ();
  ClutterActor *rectangle = clutter_rectangle_new_with_color (color);
  ClutterActor *hand = NULL;

  gchar *file = g_build_filename (TESTS_DATA_DIR, "redhand.png", NULL);

  hand = clutter_texture_new_from_file (file, &error);
  if (rectangle == NULL)
    g_error ("image load failed: %s", error->message);
  g_free (file);
  clutter_actor_set_size (hand, ACTOR_WIDTH,ACTOR_HEIGHT);

  clutter_actor_set_size (rectangle, ACTOR_WIDTH,ACTOR_HEIGHT);
  clutter_color_free (color);
  clutter_container_add (CLUTTER_CONTAINER (group), rectangle, hand, NULL);
  return group;
}

gint
main (gint    argc,
      gchar **argv)
{
  ClutterActor *stage;
  ClutterState *layout_state;
  gint i;

  clutter_perf_fps_init ();
  if (CLUTTER_INIT_SUCCESS != clutter_init (&argc, &argv))
    g_error ("Failed to initialize Clutter");

  stage = clutter_stage_new ();
  layout_state = clutter_state_new ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), CLUTTER_COLOR_Black);
  clutter_stage_set_title (CLUTTER_STAGE (stage), "State Performance");
  clutter_actor_set_size (stage, STAGE_WIDTH, STAGE_HEIGHT);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  for (i=0; i<TOTAL; i++)
    {
      ClutterActor *actor;
      ClutterState *a_state;

      int row = i/COLS;
      int col = i%COLS;

      actor = new_rect (255 * ( 1.0*col/COLS), 50,
                        255 * ( 1.0*row/ROWS), 255);
      clutter_container_add_actor (CLUTTER_CONTAINER (stage), actor);
      clutter_actor_set_position (actor, 320.0, 240.0);
      clutter_actor_set_reactive (actor, TRUE);


      clutter_state_set (layout_state, NULL, "active",
            actor, "delayed::x", CLUTTER_LINEAR,
                                         ACTOR_WIDTH * 1.0 * ((TOTAL-1-i) % COLS), 
                                        ((row*1.0/ROWS))/2, (1.0-(row*1.0/ROWS))/2,
            actor, "delayed::y", CLUTTER_LINEAR,
                                         ACTOR_HEIGHT * 1.0 * ((TOTAL-1-i) / COLS),
                                        ((row*1.0/ROWS))/2, 0.0,
            actor, "rotation-angle-x", CLUTTER_LINEAR, 0.0,
            actor, "rotation-angle-y", CLUTTER_LINEAR, 0.0,
            NULL);

      clutter_state_set (layout_state, NULL, "right",
            actor, "delayed::x", CLUTTER_LINEAR, STAGE_WIDTH * 1.0,
                                    ((row*1.0/ROWS))/2,
                                    (1.0-(row*1.0/ROWS))/2,
            actor, "delayed::y", CLUTTER_LINEAR, STAGE_HEIGHT * 1.0,
                                    ((row*1.0/ROWS))/2,
                                    0.0,
            NULL);

      clutter_state_set (layout_state, NULL, "left",
            actor, "rotation-angle-x", CLUTTER_LINEAR, 45.0,
            actor, "rotation-angle-y", CLUTTER_LINEAR, 5.0,
            actor, "x", CLUTTER_LINEAR, 0-64.0,
            actor, "y", CLUTTER_LINEAR, 0-64.0,
                         NULL);

      a_state = clutter_state_new ();
      g_object_set_data_full (G_OBJECT (actor), "hover-state-machine",
                              a_state, g_object_unref);

      clutter_state_set (a_state, NULL, "normal",
                         actor, "opacity", CLUTTER_LINEAR, 0x77,
                         actor, "rotation-angle-z", CLUTTER_LINEAR, 0.0,
                         NULL);
      clutter_state_set (a_state, NULL, "hover",
                         actor, "opacity", CLUTTER_LINEAR, 0xff,
                         actor, "rotation-angle-z", CLUTTER_LINEAR, 10.0,
                         NULL);
      clutter_actor_set_opacity (actor, 0x77);

      clutter_state_set_duration (a_state, NULL, NULL, 500);
    }

  clutter_state_set_duration (layout_state, NULL, NULL, 1000);
  clutter_state_set_duration (layout_state, "active", "left", 1400);

  g_signal_connect (layout_state, "completed", G_CALLBACK (completed), NULL);

  clutter_actor_show (stage);

  clutter_state_warp_to_state (layout_state, "left");
  clutter_state_set_state (layout_state, "active");

  clutter_perf_fps_start (CLUTTER_STAGE (stage));
  clutter_main ();
  clutter_perf_fps_report ("test-state");
  g_object_unref (layout_state);

  return EXIT_SUCCESS;
}
