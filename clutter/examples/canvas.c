#include <stdlib.h>
#include <math.h>
#include <cairo.h>
#include <clutter/clutter.h>

static gboolean
draw_clock (ClutterCanvas *canvas,
            cairo_t       *cr,
            int            width,
            int            height)
{
  GDateTime *now;
  float hours, minutes, seconds;
  ClutterColor color;

  /* get the current time and compute the angles */
  now = g_date_time_new_now_local ();
  seconds = g_date_time_get_second (now) * G_PI / 30;
  minutes = g_date_time_get_minute (now) * G_PI / 30;
  hours = g_date_time_get_hour (now) * G_PI / 6;

  cairo_save (cr);

  /* clear the contents of the canvas, to avoid painting
   * over the previous frame
   */
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);

  cairo_restore (cr);

  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  /* scale the modelview to the size of the surface */
  cairo_scale (cr, width, height);

  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_width (cr, 0.1);

  /* the black rail that holds the seconds indicator */
  clutter_cairo_set_source_color (cr, CLUTTER_COLOR_Black);
  cairo_translate (cr, 0.5, 0.5);
  cairo_arc (cr, 0, 0, 0.4, 0, G_PI * 2);
  cairo_stroke (cr);

  /* the seconds indicator */
  color = *CLUTTER_COLOR_White;
  color.alpha = 128;
  clutter_cairo_set_source_color (cr, &color);
  cairo_move_to (cr, 0, 0);
  cairo_arc (cr, sinf (seconds) * 0.4, - cosf (seconds) * 0.4, 0.05, 0, G_PI * 2);
  cairo_fill (cr);

  /* the minutes hand */
  color = *CLUTTER_COLOR_DarkChameleon;
  color.alpha = 196;
  clutter_cairo_set_source_color (cr, &color);
  cairo_move_to (cr, 0, 0);
  cairo_line_to (cr, sinf (minutes) * 0.4, -cosf (minutes) * 0.4);
  cairo_stroke (cr);

  /* the hours hand */
  cairo_move_to (cr, 0, 0);
  cairo_line_to (cr, sinf (hours) * 0.2, -cosf (hours) * 0.2);
  cairo_stroke (cr);

  g_date_time_unref (now);

  /* we're done drawing */
  return TRUE;
}

static gboolean
invalidate_clock (gpointer data_)
{
  /* invalidate the contents of the canvas */
  clutter_content_invalidate (data_);

  /* keep the timeout source */
  return G_SOURCE_CONTINUE;
}

static guint idle_resize_id;

static gboolean
idle_resize (gpointer data)
{
  ClutterActor *actor = data;
  float width, height;

  /* match the canvas size to the actor's */
  clutter_actor_get_size (actor, &width, &height);
  clutter_canvas_set_size (CLUTTER_CANVAS (clutter_actor_get_content (actor)),
                           ceilf (width),
                           ceilf (height));

  /* unset the guard */
  idle_resize_id = 0;

  /* remove the timeout */
  return G_SOURCE_REMOVE;
}

static void
on_actor_resize (ClutterActor           *actor,
                 const ClutterActorBox  *allocation,
                 ClutterAllocationFlags  flags,
                 gpointer                user_data)
{
  /* throttle multiple actor allocations to one canvas resize; we use a guard
   * variable to avoid queueing multiple resize operations
   */
  if (idle_resize_id == 0)
    idle_resize_id = clutter_threads_add_timeout (1000, idle_resize, actor);
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage, *actor;
  ClutterContent *canvas;

  /* initialize Clutter */
  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return EXIT_FAILURE;

  /* create a resizable stage */
  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "2D Clock");
  clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);
  clutter_actor_set_background_color (stage, CLUTTER_COLOR_LightSkyBlue);
  clutter_actor_set_size (stage, 300, 300);
  clutter_actor_show (stage);

  /* our 2D canvas, courtesy of Cairo */
  canvas = clutter_canvas_new ();
  clutter_canvas_set_size (CLUTTER_CANVAS (canvas), 300, 300);

  actor = clutter_actor_new ();
  clutter_actor_set_content (actor, canvas);
  clutter_actor_set_content_scaling_filters (actor,
                                             CLUTTER_SCALING_FILTER_TRILINEAR,
                                             CLUTTER_SCALING_FILTER_LINEAR);
  clutter_actor_add_child (stage, actor);

  /* the actor now owns the canvas */
  g_object_unref (canvas);

  /* bind the size of the actor to that of the stage */
  clutter_actor_add_constraint (actor, clutter_bind_constraint_new (stage, CLUTTER_BIND_SIZE, 0));

  /* resize the canvas whenever the actor changes size */
  g_signal_connect (actor, "allocation-changed", G_CALLBACK (on_actor_resize), NULL);

  /* quit on destroy */
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* connect our drawing code */
  g_signal_connect (canvas, "draw", G_CALLBACK (draw_clock), NULL);

  /* invalidate the canvas, so that we can draw before the main loop starts */
  clutter_content_invalidate (canvas);

  /* set up a timer that invalidates the canvas every second */
  clutter_threads_add_timeout (1000, invalidate_clock, canvas);

  clutter_main ();

  return EXIT_SUCCESS;
}
