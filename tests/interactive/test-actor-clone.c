#include <clutter/clutter.h>

#include <math.h>
#include <errno.h>
#include <stdlib.h>
#include <glib.h>
#include <gmodule.h>

#define NHANDS  6

typedef struct SuperOH
{
  ClutterActor **hand, *bgtex;
  ClutterActor  *real_hand;
  ClutterActor  *group;
  ClutterActor  *stage;

  gint stage_width;
  gint stage_height;
  gfloat radius;

  ClutterBehaviour *scaler_1;
  ClutterBehaviour *scaler_2;
  ClutterTimeline *timeline;
} SuperOH;

static gint n_hands = NHANDS;

static GOptionEntry super_oh_entries[] = {
  {
    "num-hands", 'n',
    0,
    G_OPTION_ARG_INT, &n_hands,
    "Number of hands", "HANDS"
  },
  { NULL }
};

/* input handler */
static gboolean
input_cb (ClutterActor *stage,
	  ClutterEvent *event,
	  gpointer      data)
{
  SuperOH *oh = data;

  if (event->type == CLUTTER_BUTTON_PRESS)
    {
      ClutterButtonEvent *button_event;
      ClutterActor *e;
      gfloat x, y;

      clutter_event_get_coords (event, &x, &y);

      button_event = (ClutterButtonEvent *) event;
      g_print ("*** button press event (button:%d) at %.2f, %.2f ***\n",
	       button_event->button,
               x, y);

      e = clutter_stage_get_actor_at_pos (CLUTTER_STAGE (stage),
                                          CLUTTER_PICK_ALL,
                                          x, y);

      /* only allow hiding the clones */
      if (e && CLUTTER_IS_CLONE (e))
        {
	  clutter_actor_hide (e);
          return TRUE;
        }
    }
  else if (event->type == CLUTTER_KEY_RELEASE)
    {
      ClutterKeyEvent *kev = (ClutterKeyEvent *) event;

      g_print ("*** key press event (key:%c) ***\n",
	       clutter_key_event_symbol (kev));

      if (clutter_key_event_symbol (kev) == CLUTTER_q)
        {
	  clutter_main_quit ();
          return TRUE;
        }
      else if (clutter_key_event_symbol (kev) == CLUTTER_r)
        {
          gint i;

          for (i = 0; i < n_hands; i++)
            clutter_actor_show (oh->hand[i]);

          clutter_actor_show (oh->real_hand);

          return TRUE;
        }
    }

  return FALSE;
}


/* Timeline handler */
static void
frame_cb (ClutterTimeline *timeline,
	  gint             frame_num,
	  gpointer         data)
{
  SuperOH *oh = data;
  gint i;
  float rotation = clutter_timeline_get_progress (timeline) * 360.0f;

  /* Rotate everything clockwise about stage center*/

  clutter_actor_set_rotation (oh->group,
                              CLUTTER_Z_AXIS,
                              rotation,
			      oh->stage_width / 2,
                              oh->stage_height / 2,
			      0);

  for (i = 0; i < n_hands; i++)
    {
      gdouble scale_x, scale_y;

      clutter_actor_get_scale (oh->hand[i], &scale_x, &scale_y);

      /* Rotate each hand around there centers - to get this we need
       * to take into account any scaling.
       */
      clutter_actor_set_rotation (oh->hand[i],
                                  CLUTTER_Z_AXIS,
                                  -6.0 * rotation,
                                  0, 0, 0);
    }
}

static gdouble
my_sine_wave (ClutterAlpha *alpha,
              gpointer      dummy G_GNUC_UNUSED)
{
  ClutterTimeline *timeline = clutter_alpha_get_timeline (alpha);
  gdouble progress = clutter_timeline_get_progress (timeline);

  return sin (progress * G_PI);
}

G_MODULE_EXPORT int
test_actor_clone_main (int argc, char *argv[])
{
  ClutterAlpha     *alpha;
  ClutterActor    *stage;
  ClutterColor     stage_color = { 0x61, 0x64, 0x8c, 0xff };
  SuperOH         *oh;
  gint             i;
  GError          *error;
  ClutterActor    *real_hand, *tmp;
  ClutterColor     clr = { 0xff, 0xff, 0x00, 0xff };

  error = NULL;

  clutter_init_with_args (&argc, &argv,
                          NULL,
                          super_oh_entries,
                          NULL,
                          &error);
  if (error)
    {
      g_warning ("Unable to initialise Clutter:\n%s",
                 error->message);
      g_error_free (error);

      return EXIT_FAILURE;
    }

  stage = clutter_stage_get_default ();
  clutter_actor_set_size (stage, 800, 600);

  clutter_stage_set_title (CLUTTER_STAGE (stage), "Clone Test");
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

  oh = g_new (SuperOH, 1);

  /* Create a timeline to manage animation */
  oh->timeline = clutter_timeline_new (6000);
  clutter_timeline_set_loop (oh->timeline, TRUE);

  /* fire a callback for frame change */
  g_signal_connect (oh->timeline, "new-frame", G_CALLBACK (frame_cb), oh);

  /* Set up some behaviours to handle scaling  */
  alpha = clutter_alpha_new_with_func (oh->timeline, my_sine_wave, NULL, NULL);

  oh->scaler_1 = clutter_behaviour_scale_new (alpha, 0.5, 0.5, 1.0, 1.0);
  oh->scaler_2 = clutter_behaviour_scale_new (alpha, 1.0, 1.0, 0.5, 0.5);

  tmp = clutter_texture_new_from_file ("redhand.png", &error);
  if (tmp == NULL)
    {
      g_error ("image load failed: %s", error->message);
      return EXIT_FAILURE;
    }

  clutter_actor_set_size (tmp, 300, 500);

  real_hand = clutter_group_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (real_hand), tmp);
  tmp = clutter_rectangle_new_with_color (&clr);
  clutter_actor_set_size (tmp, 100, 100);
  clutter_container_add_actor (CLUTTER_CONTAINER (real_hand), tmp);
  clutter_actor_set_scale (real_hand, 0.5, 0.5);
  oh->real_hand = real_hand;

  /* Now stick the group we want to clone into another group with a custom
   * opacity to verify that the clones don't traverse this parent when
   * calculating their opacity. */
  tmp = clutter_group_new ();
  clutter_actor_set_opacity (tmp, 0x80);
  clutter_container_add_actor (CLUTTER_CONTAINER (tmp), real_hand);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), tmp);

  /* now hide the group so that we can verify that hidden source actors
   * still get painted by the Clone
   */
  clutter_actor_hide (real_hand);

  /* create a new group to hold multiple actors in a group */
  oh->group = clutter_group_new();

  oh->hand = g_new (ClutterActor*, n_hands);

  oh->stage_width = clutter_actor_get_width (stage);
  oh->stage_height = clutter_actor_get_height (stage);
  oh->radius = (oh->stage_width + oh->stage_height)
             / n_hands;

  for (i = 0; i < n_hands; i++)
    {
      gint x, y, w, h;

      /* Create a texture from file, then clone in to same resources */
      oh->hand[i] = clutter_clone_new (real_hand);
      clutter_actor_set_size (oh->hand[i], 200, 213);

      /* Place around a circle */
      w = clutter_actor_get_width (oh->hand[0]);
      h = clutter_actor_get_height (oh->hand[0]);

      x = oh->stage_width / 2
	+ oh->radius
	* cos (i * G_PI / (n_hands / 2))
	- w / 2;

      y = oh->stage_height / 2
	+ oh->radius
	* sin (i * G_PI / (n_hands / 2))
	- h / 2;

      clutter_actor_set_position (oh->hand[i], x, y);

      clutter_actor_move_anchor_point_from_gravity (oh->hand[i],
						   CLUTTER_GRAVITY_CENTER);

      /* Add to our group group */
      clutter_container_add_actor (CLUTTER_CONTAINER (oh->group), oh->hand[i]);

      if (i % 2)
	clutter_behaviour_apply (oh->scaler_1, oh->hand[i]);
      else
	clutter_behaviour_apply (oh->scaler_2, oh->hand[i]);
    }

  /* Add the group to the stage */
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), oh->group);

  /* Show everying */
  clutter_actor_show (stage);

  g_signal_connect (stage, "button-press-event",
		    G_CALLBACK (input_cb),
		    oh);
  g_signal_connect (stage, "key-release-event",
		    G_CALLBACK (input_cb),
		    oh);

  /* and start it */
  clutter_timeline_start (oh->timeline);

  clutter_main ();

  /* clean up */
  g_object_unref (oh->scaler_1);
  g_object_unref (oh->scaler_2);
  g_object_unref (oh->timeline);
  g_free (oh->hand);
  g_free (oh);

  return EXIT_SUCCESS;
}
