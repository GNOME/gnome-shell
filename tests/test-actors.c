#include <clutter/clutter.h>
#ifdef CLUTTER_FLAVOUR_GLX
#include <clutter/clutter-glx.h>
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
  ClutterActor **hand, *bgtex;
  ClutterActor *group;
  GdkPixbuf    *bgpixb;

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
  return (CLUTTER_STAGE_WIDTH() + CLUTTER_STAGE_HEIGHT()) / n_hands;
}

void
screensaver_setup (void)
{
#ifdef CLUTTER_FLAVOUR_GLX
  const gchar *preview_xid;
  gboolean foreign_success = FALSE;
  ClutterActor *stage;

  stage = clutter_stage_get_default ();

  preview_xid = g_getenv ("XSCREENSAVER_WINDOW");

  if (preview_xid && *preview_xid)
    {
      char *end;
      Window remote_xwin = (Window) strtoul (preview_xid, &end, 0);

      if ((remote_xwin != None) && (end != NULL) && 
	  ((*end == ' ') || (*end == '\0')) &&
	  ((remote_xwin < G_MAXULONG) || (errno != ERANGE))) 
	{

	  foreign_success =
            clutter_glx_stage_set_foreign (CLUTTER_STAGE (stage), remote_xwin);
        }
    }

  if (!foreign_success)
    clutter_actor_set_size (stage, 800, 600);
#endif
}

/* input handler */
void 
input_cb (ClutterStage *stage, 
	  ClutterEvent *event,
	  gpointer      data)
{
  SuperOH *oh = (SuperOH *)data;

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

      if (e)
	clutter_actor_hide (e);
    }
  else if (event->type == CLUTTER_KEY_RELEASE)
    {
      ClutterKeyEvent *kev = (ClutterKeyEvent *) event;

      g_print ("*** key press event (key:%c) ***\n",
	       clutter_key_event_symbol (kev));
      
      if (clutter_key_event_symbol (kev) == CLUTTER_q)
	clutter_main_quit ();
    }
}


/* Timeline handler */
void
frame_cb (ClutterTimeline *timeline, 
	  gint             frame_num, 
	  gpointer         data)
{
  SuperOH        *oh = (SuperOH *)data;
  ClutterActor *stage = clutter_stage_get_default ();
  gint            i;

#if TRAILS
  oh->bgpixb = clutter_stage_snapshot (CLUTTER_STAGE (stage),
				       0, 0,
				       CLUTTER_STAGE_WIDTH(),
				       CLUTTER_STAGE_HEIGHT());
  clutter_texture_set_pixbuf (CLUTTER_TEXTURE (oh->bgtex), oh->bgpixb);
  g_object_unref (G_OBJECT (oh->bgpixb));
#endif

  /* Rotate everything clockwise about stage center*/
  clutter_actor_rotate_z (CLUTTER_ACTOR (oh->group),
			  frame_num,
			  CLUTTER_STAGE_WIDTH() / 2,
			  CLUTTER_STAGE_HEIGHT() / 2);
  for (i = 0; i < n_hands; i++)
    {
      /* rotate each hand around there centers */
      clutter_actor_rotate_z (oh->hand[i],
			      - 6.0 * frame_num,
			      clutter_actor_get_width (oh->hand[i]) / 2,
			      clutter_actor_get_height (oh->hand[i]) / 2);
    }

  /*
  clutter_actor_rotate_x (CLUTTER_ACTOR(oh->group),
			    75.0,
			    CLUTTER_STAGE_HEIGHT()/2, 0);
  */
}


int
main (int argc, char *argv[])
{
  ClutterTimeline *timeline;
  ClutterActor  *stage;
  ClutterColor     stage_color = { 0x61, 0x64, 0x8c, 0xff };
  GdkPixbuf       *pixbuf;
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

  pixbuf = gdk_pixbuf_new_from_file ("redhand.png", NULL);

  if (!pixbuf)
    g_error("pixbuf load failed");

  /* Set our stage (window) size */
  // clutter_actor_set_size (stage, WINWIDTH, WINHEIGHT);

  /* and its background color */

  screensaver_setup ();

  clutter_stage_set_color (CLUTTER_STAGE (stage),
		           &stage_color);

  oh = g_new(SuperOH, 1);

#if TRAILS
  oh->bgtex = clutter_texture_new();
  clutter_actor_set_size (oh->bgtex, 
			  CLUTTER_STAGE_WIDTH(), CLUTTER_STAGE_HEIGHT());
  clutter_actor_set_opacity (oh->bgtex, 0x99);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), oh->bgtex);
#endif

  /* create a new group to hold multiple actors in a group */
  oh->group = clutter_group_new();

  oh->hand = g_new (ClutterActor*, n_hands);
  for (i = 0; i < n_hands; i++)
    {
      gint x, y, w, h;
      gint radius = get_radius ();

      /* Create a texture from pixbuf, then clone in to same resources */
      if (i == 0)
	oh->hand[i] = clutter_texture_new_from_pixbuf (pixbuf);
      else
	oh->hand[i] = clutter_clone_texture_new (CLUTTER_TEXTURE(oh->hand[0]));

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

      /* Add to our group group */
      clutter_container_add_actor (CLUTTER_CONTAINER (oh->group), oh->hand[i]);
    }

#if 0
  {
    clutter_actor_set_scale (oh->group, .1, 0.1);

    guint w, h;
    clutter_actor_get_abs_size (CLUTTER_ACTOR(oh->hand[0]), &w, &h);
    g_print ("%ix%i\n", w, h);
    g_print ("%ix%i\n", 
	     clutter_actor_get_width(oh->hand[0]), 
	     clutter_actor_get_height(oh->hand[0]));
  }
#endif

  clutter_actor_show_all (oh->group);

  /* Add the group to the stage */
  clutter_container_add_actor (CLUTTER_CONTAINER (stage),
                               CLUTTER_ACTOR (oh->group));

  /* Show everying ( and map window ) */
  clutter_actor_show_all (stage);


  g_signal_connect (stage, "button-press-event",
		    G_CALLBACK (input_cb), 
		    oh);
  g_signal_connect (stage, "key-release-event",
		    G_CALLBACK (input_cb),
		    oh);

  /* Create a timeline to manage animation */
  timeline = clutter_timeline_new (360, 90); /* num frames, fps */
  g_object_set(timeline, "loop", TRUE, 0);   /* have it loop */

  /* fire a callback for frame change */
  g_signal_connect (timeline, "new-frame",
                    G_CALLBACK (frame_cb), oh);

  /* and start it */
  clutter_timeline_start (timeline);

  clutter_main();

  g_free (oh->hand);
  g_free (oh);

  return 0;
}
