#include <config.h>
#include <glib.h>
#include <gmodule.h>
#include <stdlib.h>
#include <clutter/clutter.h>
#include <cogl/cogl.h>

typedef void (*PaintFunc) (void);

static void
test_paint_line ()
{
  cogl_path_line (-50, -25, 50, 25);
}

static void
test_paint_rect ()
{
  cogl_path_rectangle (-50, -25, 50, 25);
}

static void
test_paint_rndrect()
{
  cogl_path_round_rectangle (-50, -25, 50, 25, 10, 5);
}

static void
test_paint_polyl ()
{
  gfloat poly_coords[] = {
    -50, -50,
    +50, -30,
    +30, +30,
    -30, +40
  };

  cogl_path_polyline (poly_coords, 4);
}

static void
test_paint_polyg ()
{
  gfloat poly_coords[] = {
    -50, -50,
    +50, -30,
    +30, +30,
    -30, +40
  };

  cogl_path_polygon (poly_coords, 4);
}

static void
test_paint_elp ()
{
  cogl_path_ellipse (0, 0, 60, 40);
}

static void
test_paint_curve ()
{
  cogl_path_move_to (-50, +50);

  cogl_path_curve_to (+100, -50,
                      -100, -50,
                      +50,  +50);
}

static PaintFunc paint_func []=
{
  test_paint_line,
  test_paint_rect,
  test_paint_rndrect,
  test_paint_polyl,
  test_paint_polyg,
  test_paint_elp,
  test_paint_curve
};

static void
paint_cb (ClutterActor *self, ClutterTimeline *tl)
{
  gint paint_index = (clutter_timeline_get_current_frame (tl)
                      % G_N_ELEMENTS (paint_func));

  cogl_push_matrix ();

  paint_func[paint_index] ();

  cogl_translate (100, 100, 0);
  cogl_set_source_color4ub (0, 160, 0, 255);
  cogl_path_stroke_preserve ();

  cogl_translate (150, 0, 0);
  cogl_set_source_color4ub (200, 0, 0, 255);
  cogl_path_fill ();

  cogl_pop_matrix();
}

G_MODULE_EXPORT int
test_cogl_primitives_main (int argc, char *argv[])
{
  ClutterActor *stage;
  ClutterActor *coglbox;
  ClutterTimeline *tl;

  clutter_init(&argc, &argv);

  /* One frame for each paint function at one frame per second */
  tl = clutter_timeline_new (G_N_ELEMENTS (paint_func), 1);
  clutter_timeline_set_loop (tl, TRUE);
  clutter_timeline_start (tl);

  stage = clutter_stage_get_default ();
  clutter_actor_set_size (stage, 400, 400);
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Cogl Test");

  coglbox = clutter_group_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), coglbox);
  g_signal_connect (coglbox, "paint", G_CALLBACK (paint_cb), tl);
  /* Redraw every frame of the timeline */
  g_signal_connect_swapped (tl, "new-frame",
                            G_CALLBACK (clutter_actor_queue_redraw), coglbox);

  clutter_actor_set_rotation (coglbox, CLUTTER_Y_AXIS, -30, 200, 0, 0);
  clutter_actor_set_position (coglbox, 0, 100);

  clutter_actor_show (stage);

  clutter_main ();

  g_object_unref (tl);

  return 0;
}
