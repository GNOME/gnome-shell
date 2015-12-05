/*
 * Pretty cairo flower hack.
 */
#include <clutter/clutter.h>

#ifndef _MSC_VER
#include <unistd.h> 		/* for sleep(), used for screenshots */
#endif
#include <stdlib.h>
#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif
#include <math.h>

#define PETAL_MIN 20
#define PETAL_VAR 40
#define N_FLOWERS 40 /* reduce if you have a small card */

typedef struct Flower
{
  ClutterActor *ctex;
  gint          x,y,rot,v,rv;
}
Flower;

static ClutterActor *stage = NULL;

static gboolean
draw_flower (ClutterCanvas *canvas,
             cairo_t       *cr,
             gint           width,
             gint           height,
             gpointer       user_data)
{
  /* No science here, just a hack from toying */
  gint i, j;

  double colors[] = {
    0.71, 0.81, 0.83,
    1.0,  0.78, 0.57,
    0.64, 0.30, 0.35,
    0.73, 0.40, 0.39,
    0.91, 0.56, 0.64,
    0.70, 0.47, 0.45,
    0.92, 0.75, 0.60,
    0.82, 0.86, 0.85,
    0.51, 0.56, 0.67,
    1.0, 0.79, 0.58,

  };

  gint size;
  gint petal_size;
  gint n_groups;    /* Num groups of petals 1-3 */
  gint n_petals;    /* num of petals 4 - 8  */
  gint pm1, pm2;

  gint idx, last_idx = -1;

  petal_size = GPOINTER_TO_INT (user_data);
  size = petal_size * 8;

  n_groups = rand() % 3 + 1;

  cairo_set_tolerance (cr, 0.1);

  /* Clear */
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint(cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  cairo_translate(cr, size/2, size/2);

  for (i=0; i<n_groups; i++)
    {
      n_petals = rand() % 5 + 4;
      cairo_save (cr);

      cairo_rotate (cr, rand() % 6);

      do {
	idx = (rand() % (sizeof (colors) / sizeof (double) / 3)) * 3;
      } while (idx == last_idx);

      cairo_set_source_rgba (cr, colors[idx], colors[idx+1],
			     colors[idx+2], 0.5);

      last_idx = idx;

      /* some bezier randomness */
      pm1 = rand() % 20;
      pm2 = rand() % 4;

      for (j=1; j<n_petals+1; j++)
	{
	  cairo_save (cr);
	  cairo_rotate (cr, ((2*M_PI)/n_petals)*j);

	  /* Petals are made up beziers */
	  cairo_new_path (cr);
	  cairo_move_to (cr, 0, 0);
	  cairo_rel_curve_to (cr,
			      petal_size, petal_size,
			      (pm2+2)*petal_size, petal_size,
			      (2*petal_size) + pm1, 0);
	  cairo_rel_curve_to (cr,
			      0 + (pm2*petal_size), -petal_size,
			      -petal_size, -petal_size,
			      -((2*petal_size) + pm1), 0);
	  cairo_close_path (cr);
	  cairo_fill (cr);
	  cairo_restore (cr);
	}

      petal_size -= rand() % (size/8);

      cairo_restore (cr);
    }

  /* Finally draw flower center */
  do {
      idx = (rand() % (sizeof (colors) / sizeof (double) / 3)) * 3;
  } while (idx == last_idx);

  if (petal_size < 0)
    petal_size = rand() % 10;

  cairo_set_source_rgba (cr, colors[idx], colors[idx+1], colors[idx+2], 0.5);

  cairo_arc(cr, 0, 0, petal_size, 0, M_PI * 2);
  cairo_fill(cr);

  return TRUE;
}

static ClutterActor *
make_flower_actor (void)
{
  gint petal_size = PETAL_MIN + rand() % PETAL_VAR;
  gint size = petal_size * 8;
  ClutterActor *ctex;
  ClutterContent *canvas;

  canvas = clutter_canvas_new ();
  g_signal_connect (canvas, "draw",
                    G_CALLBACK (draw_flower), GINT_TO_POINTER (petal_size));

  clutter_canvas_set_size (CLUTTER_CANVAS (canvas), size, size);
  ctex = g_object_new (CLUTTER_TYPE_ACTOR,
                       "content", canvas,
                       "width", (gfloat) size,
                       "height", (gfloat) size,
                       NULL);

  g_object_unref (canvas);

  return ctex;
}

static void
tick (ClutterTimeline *timeline,
      gint             msecs,
      gpointer         data)
{
  Flower **flowers = data;
  gint i = 0;

  for (i = 0; i < N_FLOWERS; i++)
    {
      flowers[i]->y   += flowers[i]->v;
      flowers[i]->rot += flowers[i]->rv;

      if (flowers[i]->y > (gint) clutter_actor_get_height (stage))
        flowers[i]->y = -clutter_actor_get_height (flowers[i]->ctex);

      clutter_actor_set_position (flowers[i]->ctex,
				  flowers[i]->x, flowers[i]->y);

      clutter_actor_set_rotation (flowers[i]->ctex,
                                  CLUTTER_Z_AXIS,
                                  flowers[i]->rot,
                                  clutter_actor_get_width (flowers[i]->ctex)/2,
                                  clutter_actor_get_height (flowers[i]->ctex)/2,
                                  0);
    }
}

static void
stop_and_quit (ClutterActor    *actor,
               ClutterTimeline *timeline)
{
  clutter_timeline_stop (timeline);
  clutter_main_quit ();
}

G_MODULE_EXPORT int
test_cairo_flowers_main (int argc, char **argv)
{
  Flower *flowers[N_FLOWERS];
  ClutterTimeline *timeline;
  int i;

  srand (time (NULL));

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  /* Create a timeline to manage animation */
  timeline = clutter_timeline_new (6000);
  clutter_timeline_set_repeat_count (timeline, -1);

  stage = clutter_stage_new ();
  clutter_stage_set_title (CLUTTER_STAGE (stage), "Cairo Flowers");
  g_signal_connect (stage, "destroy", G_CALLBACK (stop_and_quit), timeline);

  clutter_actor_set_background_color (stage, CLUTTER_COLOR_Black);

  for (i=0; i< N_FLOWERS; i++)
    {
      flowers[i]       = g_new0(Flower, 1);
      flowers[i]->ctex = make_flower_actor();
      flowers[i]->x    = rand() % (int) clutter_actor_get_width (stage)
                       - (PETAL_MIN + PETAL_VAR) * 2;
      flowers[i]->y    = rand() % (int) clutter_actor_get_height (stage);
      flowers[i]->rv   = rand() % 5 + 1;
      flowers[i]->v    = rand() % 10 + 2;

      clutter_container_add_actor (CLUTTER_CONTAINER (stage),
                                   flowers[i]->ctex);
      clutter_actor_set_position (flowers[i]->ctex,
				  flowers[i]->x, flowers[i]->y);
    }

  /* fire a callback for frame change */
  g_signal_connect (timeline, "new-frame", G_CALLBACK (tick), flowers);

  clutter_actor_show (stage);

  clutter_timeline_start (timeline);

  g_signal_connect (stage, "key-press-event",
		    G_CALLBACK (clutter_main_quit),
		    NULL);

  clutter_main();

  g_object_unref (timeline);

  return EXIT_SUCCESS;
}

G_MODULE_EXPORT const char *
test_cairo_flowers_describe (void)
{
  return "Drawing pretty flowers with Cairo";
}
