/* Simple rectangle drawing using button and pointer events;
 * click, drag and release a mouse button to draw a rectangle
 */
#include <stdlib.h>
#include <clutter/clutter.h>

static const ClutterColor stage_color = { 0x33, 0x33, 0x55, 0xff };
static const ClutterColor lasso_color = { 0xaa, 0xaa, 0xaa, 0x33 };

typedef struct
{
  ClutterActor *actor;
  gfloat        x;
  gfloat        y;
} Lasso;

static guint
random_color_component ()
{
  return (guint) (155 + (100.0 * rand () / (RAND_MAX + 1.0)));
}

static gboolean
button_pressed_cb (ClutterActor *actor,
                   ClutterEvent *event,
                   gpointer      user_data)
{
  Lasso *lasso = (Lasso *) user_data;

  /* start drawing the lasso actor */
  lasso->actor = clutter_rectangle_new_with_color (&lasso_color);

  /* store lasso's start coordinates */
  clutter_event_get_coords (event, &(lasso->x), &(lasso->y));

  clutter_container_add_actor (CLUTTER_CONTAINER (actor), lasso->actor);

  return TRUE;
}

static gboolean
button_released_cb (ClutterActor *stage,
                    ClutterEvent *event,
                    gpointer      user_data)
{
  Lasso *lasso = (Lasso *) user_data;
  ClutterActor *rectangle;
  ClutterColor *random_color;
  gfloat x;
  gfloat y;
  gfloat width;
  gfloat height;

  if (lasso->actor == NULL)
    return TRUE;

  /* create a new rectangle */
  random_color = clutter_color_new (random_color_component (),
                                    random_color_component (),
                                    random_color_component (),
                                    random_color_component ());
  rectangle = clutter_rectangle_new_with_color (random_color);

  /* set the rectangle to the same size and shape as the lasso */
  clutter_actor_get_position (lasso->actor, &x, &y);
  clutter_actor_get_size (lasso->actor, &width, &height);

  clutter_actor_set_position (rectangle, x, y);
  clutter_actor_set_size (rectangle, width, height);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), rectangle);

  /* clear up the lasso actor */
  clutter_actor_destroy (lasso->actor);
  lasso->actor = NULL;

  clutter_actor_queue_redraw (stage);

  return TRUE;

}

static gboolean
pointer_motion_cb (ClutterActor *stage,
                   ClutterEvent *event,
                   gpointer      user_data)
{
  gfloat pointer_x;
  gfloat pointer_y;
  gfloat new_x;
  gfloat new_y;
  gfloat width;
  gfloat height;

  Lasso *lasso = (Lasso *) user_data;

  if (lasso->actor == NULL)
    return TRUE;

  /* redraw the lasso actor */
  clutter_event_get_coords (event, &pointer_x, &pointer_y);

  new_x = MIN (pointer_x, lasso->x);
  new_y = MIN (pointer_y, lasso->y);
  width = MAX (pointer_x, lasso->x) - new_x;
  height = MAX (pointer_y, lasso->y) - new_y;

  clutter_actor_set_position (lasso->actor, new_x, new_y);
  clutter_actor_set_size (lasso->actor, width, height);

  return TRUE;
}

int
main (int   argc,
      char *argv[])
{
  Lasso *lasso = g_new0 (Lasso, 1);

  ClutterActor *stage;

  /* seed random number generator */
  srand ((unsigned int) time (NULL));

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 320, 240);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  g_signal_connect (stage,
                    "button-press-event",
                    G_CALLBACK (button_pressed_cb),
                    lasso);

  g_signal_connect (stage,
                    "button-release-event",
                    G_CALLBACK (button_released_cb),
                    lasso);

  g_signal_connect (stage,
                    "motion-event",
                    G_CALLBACK (pointer_motion_cb),
                    lasso);

  clutter_actor_show (stage);

  clutter_main ();

  g_free (lasso);

  return EXIT_SUCCESS;
}
