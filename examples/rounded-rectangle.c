#include <stdlib.h>
#include <math.h>
#include <cairo.h>
#include <clutter/clutter.h>

static gboolean
draw_content (ClutterCanvas *canvas,
              cairo_t       *cr,
              int            surface_width,
              int            surface_height)
{
  /* rounded rectangle taken from:
   *
   *   http://cairographics.org/samples/rounded_rectangle/
   *
   * we leave 1 pixel around the edges to avoid jagged edges
   * when rotating the actor
   */
  double x             = 1.0,        /* parameters like cairo_rectangle */
         y             = 1.0,
         width         = surface_width - 2.0,
         height        = surface_height - 2.0,
         aspect        = 1.0,     /* aspect ratio */
         corner_radius = height / 20.0;   /* and corner curvature radius */

  double radius = corner_radius / aspect;
  double degrees = M_PI / 180.0;

  cairo_save (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_restore (cr);

  cairo_new_sub_path (cr);
  cairo_arc (cr, x + width - radius, y + radius, radius, -90 * degrees, 0 * degrees);
  cairo_arc (cr, x + width - radius, y + height - radius, radius, 0 * degrees, 90 * degrees);
  cairo_arc (cr, x + radius, y + height - radius, radius, 90 * degrees, 180 * degrees);
  cairo_arc (cr, x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
  cairo_close_path (cr);

  cairo_set_source_rgba (cr, 0.5, 0.5, 1, 0.95);
  cairo_fill (cr);

  /* we're done drawing */
  return TRUE;
}

int
main (int argc, char *argv[])
{
  ClutterActor *stage, *actor;
  ClutterContent *canvas;
  ClutterTransition *transition;

  /* initialize Clutter */
  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return EXIT_FAILURE;

  /* create a stage */
  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Rectangle with rounded corners");
  clutter_stage_set_use_alpha (CLUTTER_STAGE (stage), TRUE);
  clutter_actor_set_background_color (stage, CLUTTER_COLOR_Black);
  clutter_actor_set_size (stage, 500, 500);
  clutter_actor_set_opacity (stage, 64);
  clutter_actor_show (stage);

  /* our 2D canvas, courtesy of Cairo */
  canvas = clutter_canvas_new ();
  clutter_canvas_set_size (CLUTTER_CANVAS (canvas), 300, 300);

  /* the actor that will display the contents of the canvas */
  actor = clutter_actor_new ();
  clutter_actor_set_content (actor, canvas);
  clutter_actor_set_content_gravity (actor, CLUTTER_CONTENT_GRAVITY_CENTER);
  clutter_actor_set_content_scaling_filters (actor,
                                             CLUTTER_SCALING_FILTER_TRILINEAR,
                                             CLUTTER_SCALING_FILTER_LINEAR);
  clutter_actor_set_pivot_point (actor, 0.5f, 0.5f);
  clutter_actor_add_constraint (actor, clutter_align_constraint_new (stage, CLUTTER_ALIGN_BOTH, 0.5));
  clutter_actor_set_request_mode (actor, CLUTTER_REQUEST_CONTENT_SIZE);
  clutter_actor_add_child (stage, actor);

  /* the actor now owns the canvas */
  g_object_unref (canvas);

  /* create the continuous animation of the actor spinning around its center */
  transition = clutter_property_transition_new ("rotation-angle-y");
  clutter_transition_set_from (transition, G_TYPE_DOUBLE, 0.0);
  clutter_transition_set_to (transition, G_TYPE_DOUBLE, 360.0);
  clutter_timeline_set_duration (CLUTTER_TIMELINE (transition), 2000);
  clutter_timeline_set_repeat_count (CLUTTER_TIMELINE (transition), -1);
  clutter_actor_add_transition (actor, "rotateActor", transition);

  /* the actor now owns the transition */
  g_object_unref (transition);

  /* quit on destroy */
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* connect our drawing code */
  g_signal_connect (canvas, "draw", G_CALLBACK (draw_content), NULL);

  /* invalidate the canvas, so that we can draw before the main loop starts */
  clutter_content_invalidate (canvas);

  clutter_main ();

  return EXIT_SUCCESS;
}
