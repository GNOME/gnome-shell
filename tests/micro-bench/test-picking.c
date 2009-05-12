
#include <math.h>
#include <stdlib.h>
#include <clutter/clutter.h>

#define N_ACTORS 100
#define N_EVENTS 5

static gboolean
motion_event_cb (ClutterActor *actor, ClutterEvent *event, gpointer user_data)
{
  return FALSE;
}

static void
do_events (ClutterActor *stage)
{
  glong i;
  static gdouble angle = 0;
  ClutterEvent event;

  for (i = 0; i < N_EVENTS; i++)
    {
      angle += (2.0 * M_PI) / (gdouble)N_ACTORS;
      while (angle > M_PI * 2.0)
        angle -= M_PI * 2.0;

      event.type = CLUTTER_MOTION;
      event.any.stage = CLUTTER_STAGE (stage);
      event.any.time = CLUTTER_CURRENT_TIME;
      event.motion.flags = 0;
      event.motion.source = NULL;
      event.motion.x = (gint)(256.0 + 206.0 * cos (angle));
      event.motion.y = (gint)(256.0 + 206.0 * sin (angle));
      event.motion.modifier_state = 0;
      event.motion.axes = NULL;
      event.motion.device = NULL;

      clutter_event_put (&event);
    }
}

static gboolean
fps_cb (gpointer data)
{
  ClutterActor *stage = CLUTTER_ACTOR (data);

  static GTimer *timer = NULL;
  static gint fps = 0;

  if (!timer)
    {
      timer = g_timer_new ();
      g_timer_start (timer);
    }

  if (g_timer_elapsed (timer, NULL) >= 1)
    {
      printf ("fps: %d\n", fps);
      g_timer_start (timer);
      fps = 0;
    }

  clutter_actor_paint (stage);
  do_events (stage);
  ++fps;

  return TRUE;
}

int
main (int argc, char **argv)
{
  glong i;
  gdouble angle;
  const ClutterColor black = { 0x00, 0x00, 0x00, 0xff };
  ClutterColor color = { 0x00, 0x00, 0x00, 0xff };

  ClutterActor *stage, *rect;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_actor_set_size (stage, 512, 512);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &black);

  printf ("Picking performance test with "
          "%d actors and %d events per frame\n",
          N_ACTORS,
          N_EVENTS);

  for (i = N_ACTORS-1; i >= 0; i--)
    {
      angle = ((2.0 * M_PI) / (gdouble)N_ACTORS) * i;

      color.red = (1.0 - ABS ((MAX (0, MIN (N_ACTORS/2.0 + 0, i))) /
                  (gdouble)(N_ACTORS/4.0) - 1.0)) * 255.0;
      color.green = (1.0 - ABS ((MAX (0, MIN (N_ACTORS/2.0 + 0,
                    fmod (i + (N_ACTORS/3.0)*2, N_ACTORS)))) /
                    (gdouble)(N_ACTORS/4) - 1.0)) * 255.0;
      color.blue = (1.0 - ABS ((MAX (0, MIN (N_ACTORS/2.0 + 0,
                   fmod ((i + (N_ACTORS/3.0)), N_ACTORS)))) /
                   (gdouble)(N_ACTORS/4.0) - 1.0)) * 255.0;

      rect = clutter_rectangle_new_with_color (&color);
      clutter_actor_set_size (rect, 100, 100);
      clutter_actor_set_anchor_point_from_gravity (rect,
                                                   CLUTTER_GRAVITY_CENTER);
      clutter_actor_set_position (rect,
                                  256 + 206 * cos (angle),
                                  256 + 206 * sin (angle));
      clutter_actor_set_reactive (rect, TRUE);
      g_signal_connect (rect, "motion-event",
                        G_CALLBACK (motion_event_cb), NULL);

      clutter_container_add_actor (CLUTTER_CONTAINER (stage), rect);
    }

  clutter_actor_show (stage);

  g_idle_add (fps_cb, (gpointer)stage);

  clutter_main ();

  return 0;
}


