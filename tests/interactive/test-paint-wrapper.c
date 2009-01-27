#include <gmodule.h>
#include <clutter/clutter.h>

#if defined (_MSC_VER) && !defined (_USE_MATH_DEFINES)
#define _USE_MATH_DEFINES
#endif

#include <math.h>
#include <errno.h>
#include <stdlib.h>
#include <glib.h>

#define TRAILS  0
#define NHANDS  6
#define RADIUS  ((CLUTTER_STAGE_WIDTH()+CLUTTER_STAGE_HEIGHT())/NHANDS)

typedef struct SuperOH
{
  ClutterActor   **hand, *bgtex;
  ClutterActor    *group;

  gboolean        *paint_guards;

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

static gint
get_radius (void)
{
  return (CLUTTER_STAGE_HEIGHT() + CLUTTER_STAGE_HEIGHT()) / n_hands ;
}

/* input handler */
static gboolean
input_cb (ClutterStage *stage,
	  ClutterEvent *event,
	  gpointer      data)
{
  if (event->type == CLUTTER_BUTTON_PRESS)
    {
      ClutterButtonEvent *button_event;
      ClutterActor *e;
      gint x, y;

      clutter_event_get_coords (event, &x, &y);

      button_event = (ClutterButtonEvent *) event;
      g_print ("*** button press event (button:%d) ***\n",
	       button_event->button);

      e = clutter_stage_get_actor_at_pos (stage, x, y);

      if (e && (CLUTTER_IS_TEXTURE (e) || CLUTTER_IS_CLONE (e)))
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
    }

  return FALSE;
}


/* Timeline handler */
static void
frame_cb (ClutterTimeline *timeline,
	  gint             frame_num,
	  gpointer         data)
{
  SuperOH        *oh = (SuperOH *)data;
  gint            i;

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

static void
hand_pre_paint (ClutterActor *actor,
                gpointer      user_data)
{
  SuperOH *oh = (SuperOH *) user_data;
  guint w, h;
  int actor_num;

  for (actor_num = 0; oh->hand[actor_num] != actor; actor_num++);

  g_assert (oh->paint_guards[actor_num] == FALSE);

  clutter_actor_get_size (actor, &w, &h);

  cogl_set_source_color4ub (255, 0, 0, 128);
  cogl_rectangle (0, 0, w / 2, h / 2);

  oh->paint_guards[actor_num] = TRUE;
}

static void
hand_post_paint (ClutterActor *actor,
                 gpointer      user_data)
{
  SuperOH *oh = (SuperOH *) user_data;
  guint w, h;
  int actor_num;

  for (actor_num = 0; oh->hand[actor_num] != actor; actor_num++);

  g_assert (oh->paint_guards[actor_num] == TRUE);

  clutter_actor_get_size (actor, &w, &h);

  cogl_set_source_color4ub (0, 255, 0, 128);
  cogl_rectangle (w / 2, h / 2, w / 2, h / 2);

  oh->paint_guards[actor_num] = FALSE;
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
test_paint_wrapper_main (int argc, char *argv[])
{
  ClutterTimeline *timeline;
  ClutterAlpha     *alpha;
  ClutterBehaviour *scaler_1, *scaler_2;
  ClutterActor    *stage;
  ClutterColor     stage_color = { 0x61, 0x64, 0x8c, 0xff };
  SuperOH         *oh;
  gint             i;
  GError          *error;

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
  alpha = clutter_alpha_new_with_func (timeline, my_sine_wave, NULL, NULL);

  scaler_1 = clutter_behaviour_scale_new (alpha,
					  0.5, 0.5,
					  1.0, 1.0);

  scaler_2 = clutter_behaviour_scale_new (alpha,
					  1.0, 1.0,
					  0.5, 0.5);

  /* create a new group to hold multiple actors in a group */
  oh->group = clutter_group_new();

  oh->hand = g_new (ClutterActor*, n_hands);
  for (i = 0; i < n_hands; i++)
    {
      gint x, y, w, h;
      gint radius = get_radius ();

      /* Create a texture from file, then clone in to same resources */
      if (i == 0)
	{
	  if ((oh->hand[i] = clutter_texture_new_from_file ("redhand.png",
							    &error)) == NULL)
	    {
	      g_error ("image load failed: %s", error->message);
	      exit (1);
	    }
	}
      else
	oh->hand[i] = clutter_clone_new (oh->hand[0]);

      /* paint something before each hand */
      g_signal_connect (oh->hand[i],
                        "paint", G_CALLBACK (hand_pre_paint),
                        oh);

      /* paint something after each hand */
      g_signal_connect_after (oh->hand[i],
                              "paint", G_CALLBACK (hand_post_paint),
                              oh);

      /* Place around a circle */
      w = clutter_actor_get_width (oh->hand[0]);
      h = clutter_actor_get_height (oh->hand[0]);

      x = CLUTTER_STAGE_WIDTH () / 2
	+ radius
	* cos (i * M_PI / (n_hands / 2))
	- w / 2;

      y = CLUTTER_STAGE_HEIGHT () / 2
	+ radius
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

  oh->paint_guards = g_malloc0 (sizeof (gboolean) * n_hands);

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
  g_free (oh->paint_guards);
  g_free (oh);

  return 0;
}
