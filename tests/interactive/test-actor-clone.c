#include <clutter/clutter.h>

#if defined (_MSC_VER) && !defined (_USE_MATH_DEFINES)
#define _USE_MATH_DEFINES
#endif

#include <math.h>
#include <errno.h>
#include <stdlib.h>
#include <glib.h>
#include <gmodule.h>

#define TRAILS  0
#define NHANDS  6
#define RADIUS  ((CLUTTER_STAGE_WIDTH()+CLUTTER_STAGE_HEIGHT())/NHANDS)

typedef struct SuperOH
{
  ClutterActor   **hand, *bgtex;
  ClutterActor    *group;

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
      gint x, y;

      clutter_event_get_coords (event, &x, &y);

      button_event = (ClutterButtonEvent *) event;
      g_print ("*** button press event (button:%d) ***\n",
	       button_event->button);

      e = clutter_stage_get_actor_at_pos (CLUTTER_STAGE (stage), x, y);

      if (e && (CLUTTER_IS_TEXTURE (e) || CLUTTER_IS_CLONE_TEXTURE (e)))
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
  gint     i;

  /* Rotate everything clockwise about stage center*/

  clutter_actor_set_rotation (CLUTTER_ACTOR (oh->group),
                              CLUTTER_Z_AXIS,
                              frame_num,
			      CLUTTER_STAGE_WIDTH () / 2,
                              CLUTTER_STAGE_HEIGHT () / 2,
			      0);

  for (i = 0; i < n_hands; i++)
    {
      gdouble scale_x, scale_y;

      clutter_actor_get_scale (oh->hand[i], &scale_x, &scale_y);

      /* Rotate each hand around there centers - to get this we need
       * to take into account any scaling.
       *
       * FIXME: scaling causes drift so disabled for now. Need rotation
       * unit based functions to fix.
       */
      clutter_actor_set_rotation (oh->hand[i], CLUTTER_Z_AXIS,
				  - 6.0 * frame_num, 0, 0, 0);
    }
}

G_MODULE_EXPORT int
test_actor_clone_main (int argc, char *argv[])
{
  ClutterTimeline *timeline;
  ClutterAlpha     *alpha;
  ClutterBehaviour *scaler_1, *scaler_2;
  ClutterActor    *stage;
  ClutterColor     stage_color = { 0x61, 0x64, 0x8c, 0xff };
  SuperOH         *oh;
  gint             i;
  GError          *error;
  ClutterActor    *real_hand, *tmp;
  ClutterColor     clr= {0xff, 0xff, 0x00, 0xff};

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

      exit (1);
    }

  stage = clutter_stage_get_default ();
  clutter_actor_set_size (stage, 800, 600);

  clutter_stage_set_title (CLUTTER_STAGE (stage), "Actors Test");
  clutter_stage_set_color (CLUTTER_STAGE (stage),
		           &stage_color);

  oh = g_new(SuperOH, 1);

  /* Create a timeline to manage animation */
  timeline = clutter_timeline_new (360, 60); /* num frames, fps */
  g_object_set (timeline, "loop", TRUE, NULL);   /* have it loop */

  /* fire a callback for frame change */
  g_signal_connect (timeline, "new-frame", G_CALLBACK (frame_cb), oh);

  /* Set up some behaviours to handle scaling  */
  alpha = clutter_alpha_new_with_func (timeline, clutter_sine_func,
                                       NULL, NULL);

  scaler_1 = clutter_behaviour_scale_new (alpha,
					  0.5, 0.5,
					  1.0, 1.0);

  scaler_2 = clutter_behaviour_scale_new (alpha,
					  1.0, 1.0,
					  0.5, 0.5);

  tmp = clutter_texture_new_from_file ("redhand.png", &error);
  if (tmp  == NULL)
    {
      g_error ("image load failed: %s", error->message);
      exit (1);
    }
  clutter_actor_set_size (tmp, 300, 500);
  real_hand = clutter_group_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (real_hand), tmp);
  tmp = clutter_rectangle_new_with_color (&clr);
  clutter_actor_set_size (tmp, 100, 100);
  clutter_container_add_actor (CLUTTER_CONTAINER (real_hand), tmp);
  clutter_actor_set_scale (real_hand, 0.5, 0.5);

  /* Now stick the group we want to clone into another group with a custom
   * opacity to verify that the clones don't traverse this parent when
   * calculating their opacity. */
  tmp = clutter_group_new ();
  clutter_actor_set_opacity (tmp, 0x80);
  clutter_container_add_actor (CLUTTER_CONTAINER (tmp), real_hand);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), tmp);

  /* create a new group to hold multiple actors in a group */
  oh->group = clutter_group_new();

  oh->hand = g_new (ClutterActor*, n_hands);
  for (i = 0; i < n_hands; i++)
    {
      gint x, y, w, h;

      /* Create a texture from file, then clone in to same resources */
      oh->hand[i] = clutter_actor_clone_new (real_hand);
      clutter_actor_set_size (oh->hand[i], 200, 213);

      /* Place around a circle */
      w = clutter_actor_get_width (oh->hand[0]);
      h = clutter_actor_get_height (oh->hand[0]);

      x = CLUTTER_STAGE_WIDTH () / 2
	+ RADIUS
	* cos (i * M_PI / (n_hands / 2))
	- w / 2;

      y = CLUTTER_STAGE_HEIGHT () / 2
	+ RADIUS
	* sin (i * M_PI / (n_hands / 2))
	- h / 2;

      clutter_actor_set_position (oh->hand[i], x, y);

      clutter_actor_move_anchor_point_from_gravity (oh->hand[i],
						   CLUTTER_GRAVITY_CENTER);

      /* Add to our group group */
      clutter_container_add_actor (CLUTTER_CONTAINER (oh->group), oh->hand[i]);

#if 1 /* FIXME: disabled as causes drift? - see comment above */
      if (i % 2)
	clutter_behaviour_apply (scaler_1, oh->hand[i]);
      else
	clutter_behaviour_apply (scaler_2, oh->hand[i]);
#endif
    }

  /* Add the group to the stage */
  clutter_container_add_actor (CLUTTER_CONTAINER (stage),
                               CLUTTER_ACTOR (oh->group));

  /* Show everying ( and map window ) */
  clutter_actor_show (stage);


  g_signal_connect (stage, "button-press-event",
		    G_CALLBACK (input_cb),
		    oh);
  g_signal_connect (stage, "key-release-event",
		    G_CALLBACK (input_cb),
		    oh);

  /* and start it */
  clutter_timeline_start (timeline);

  clutter_main ();

  g_free (oh->hand);
  g_free (oh);

  return 0;
}
