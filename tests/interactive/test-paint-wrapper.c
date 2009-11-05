#include <gmodule.h>
#include <clutter/clutter.h>

#if defined (_MSC_VER) && !defined (_USE_MATH_DEFINES)
#define _USE_MATH_DEFINES
#endif

#include <math.h>
#include <errno.h>
#include <stdlib.h>
#include <glib.h>

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

  gboolean *paint_guards;
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

static gboolean
on_button_press_event (ClutterActor *actor,
                       ClutterEvent *event,
                       SuperOH      *oh)
{
  gfloat x, y;

  clutter_event_get_coords (event, &x, &y);

  g_print ("*** button press event (button:%d) at %.2f, %.2f ***\n",
           clutter_event_get_button (event),
           x, y);

  clutter_actor_hide (actor);

  return TRUE;
}

static gboolean
input_cb (ClutterActor *stage,
	  ClutterEvent *event,
	  gpointer      data)
{
  SuperOH *oh = data;

  if (event->type == CLUTTER_KEY_RELEASE)
    {
      g_print ("*** key press event (key:%c) ***\n",
	       clutter_event_get_key_symbol (event));

      if (clutter_event_get_key_symbol (event) == CLUTTER_q)
        {
	  clutter_main_quit ();

          return TRUE;
        }
      else if (clutter_event_get_key_symbol (event) == CLUTTER_r)
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
	  gint             msecs,
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

static void
hand_pre_paint (ClutterActor *actor,
                gpointer      user_data)
{
  SuperOH *oh = user_data;
  gfloat w, h;
  int actor_num;

  for (actor_num = 0; oh->hand[actor_num] != actor; actor_num++)
    ;

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
  SuperOH *oh = user_data;
  gfloat w, h;
  int actor_num;

  for (actor_num = 0; oh->hand[actor_num] != actor; actor_num++)
    ;

  g_assert (oh->paint_guards[actor_num] == TRUE);

  clutter_actor_get_size (actor, &w, &h);

  cogl_set_source_color4ub (0, 255, 0, 128);
  cogl_rectangle (w / 2, h / 2, w, h);

  oh->paint_guards[actor_num] = FALSE;
}

G_MODULE_EXPORT int
test_paint_wrapper_main (int argc, char *argv[])
{
  ClutterAlpha *alpha;
  ClutterActor *stage;
  ClutterColor  stage_color = { 0x61, 0x64, 0x8c, 0xff };
  SuperOH      *oh;
  gint          i;
  GError       *error;
  ClutterActor *real_hand;

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

  clutter_stage_set_title (CLUTTER_STAGE (stage), "Paint Test");
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

  oh = g_new(SuperOH, 1);
  oh->stage = stage;

  /* Create a timeline to manage animation */
  oh->timeline = clutter_timeline_new (6000);
  clutter_timeline_set_loop (oh->timeline, TRUE);

  /* fire a callback for frame change */
  g_signal_connect (oh->timeline, "new-frame", G_CALLBACK (frame_cb), oh);

  /* Set up some behaviours to handle scaling  */
  alpha = clutter_alpha_new_with_func (oh->timeline, my_sine_wave, NULL, NULL);

  oh->scaler_1 = clutter_behaviour_scale_new (alpha, 0.5, 0.5, 1.0, 1.0);
  oh->scaler_2 = clutter_behaviour_scale_new (alpha, 1.0, 1.0, 0.5, 0.5);

  real_hand = clutter_texture_new_from_file (TESTS_DATADIR 
                                             G_DIR_SEPARATOR_S
                                             "redhand.png",
                                             &error);
  if (real_hand == NULL)
    {
      g_error ("image load failed: %s", error->message);
      return EXIT_FAILURE;
    }

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

      if (i == 0)
        oh->hand[i] = real_hand;
      else
        oh->hand[i] = clutter_clone_new (real_hand);

      clutter_actor_set_reactive (oh->hand[i], TRUE);

      clutter_actor_set_size (oh->hand[i], 200, 213);

      /* Place around a circle */
      w = clutter_actor_get_width (oh->hand[i]);
      h = clutter_actor_get_height (oh->hand[i]);

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

      g_signal_connect (oh->hand[i], "button-press-event",
                        G_CALLBACK (on_button_press_event),
                        oh);

      /* paint something before each hand */
      g_signal_connect (oh->hand[i],
                        "paint", G_CALLBACK (hand_pre_paint),
                        oh);

      /* paint something after each hand */
      g_signal_connect_after (oh->hand[i],
                              "paint", G_CALLBACK (hand_post_paint),
                              oh);

      /* Add to our group group */
      clutter_container_add_actor (CLUTTER_CONTAINER (oh->group), oh->hand[i]);

      if (i % 2)
	clutter_behaviour_apply (oh->scaler_1, oh->hand[i]);
      else
	clutter_behaviour_apply (oh->scaler_2, oh->hand[i]);
    }

  oh->paint_guards = g_malloc0 (sizeof (gboolean) * n_hands);

  /* Add the group to the stage */
  clutter_container_add_actor (CLUTTER_CONTAINER (stage),
                               CLUTTER_ACTOR (oh->group));

  /* Show everying ( and map window ) */
  clutter_actor_show (stage);

  g_signal_connect (stage, "key-release-event",
		    G_CALLBACK (input_cb),
		    oh);

  /* and start it */
  clutter_timeline_start (oh->timeline);

  clutter_main ();

  g_object_unref (oh->scaler_1);
  g_object_unref (oh->scaler_2);
  g_object_unref (oh->timeline);
  g_free (oh->paint_guards);
  g_free (oh->hand);
  g_free (oh);

  return 0;
}
