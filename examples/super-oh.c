#include <clutter/clutter.h>
#include <math.h>

#define TRAILS 0
#define NHANDS  6
#define WINWIDTH   800
#define WINHEIGHT  800
#define RADIUS     250

typedef struct SuperOH
{
  ClutterElement *hand[NHANDS], *bgtex;
  ClutterGroup   *group;
  GdkPixbuf      *bgpixb;

} SuperOH; 

/* input handler */
void 
input_cb (ClutterStage *stage, 
	  ClutterEvent *event,
	  gpointer      data)
{
  SuperOH *oh = (SuperOH *)data;

  if (event->type == CLUTTER_BUTTON_PRESS)
    {
      ClutterElement *e;

      e = clutter_stage_pick (stage, 
			      clutter_button_event_x(event),
			      clutter_button_event_y(event));

      if (e)
	clutter_element_hide(e);
    }
}


/* Timeline handler */
void
frame_cb (ClutterTimeline *timeline, 
	  gint             frame_num, 
	  gpointer         data)
{
  SuperOH *oh = (SuperOH *)data;
  gint     i;

#if TRAILS
  oh->bgpixb = clutter_stage_snapshot (CLUTTER_STAGE(clutter_stage()),
				       0, 0, WINWIDTH, WINHEIGHT);
  clutter_texture_set_pixbuf(CLUTTER_TEXTURE(oh->bgtex), oh->bgpixb);
  g_object_unref(G_OBJECT(oh->bgpixb));
#endif

  /* Rotate everything clockwise about stage center*/
  clutter_element_rotate_z (CLUTTER_ELEMENT(oh->group),
			    frame_num,
			    WINWIDTH/2,
			    WINHEIGHT/2);
  for (i = 0; i < NHANDS; i++)
    {
      /* rotate each hand around there centers */
      clutter_element_rotate_z (oh->hand[i],
				- 6.0 * frame_num,
				clutter_element_get_width (oh->hand[i])/2,
				clutter_element_get_height (oh->hand[i])/2);
    }

  /*
  clutter_element_rotate_x (CLUTTER_ELEMENT(oh->group),
			    75.0,
			    WINHEIGHT/2, 0);
  */
}

int
main (int argc, char *argv[])
{
  ClutterTimeline *timeline;
  GdkPixbuf       *pixbuf;
  SuperOH         *oh;
  gint             i;

  clutter_init (&argc, &argv);

  pixbuf = gdk_pixbuf_new_from_file ("redhand.png", NULL);

  if (!pixbuf)
    g_error("pixbuf load failed");

  /* Set our stage (window) size */
  clutter_element_set_size (CLUTTER_ELEMENT(clutter_stage()),
			    WINWIDTH, WINHEIGHT);

  /* and its background color */
  clutter_stage_set_color (CLUTTER_STAGE(clutter_stage()), 0x61648cff);

  oh = g_new(SuperOH, 1);

#if TRAILS
  oh->bgtex = clutter_texture_new();
  clutter_element_set_size (oh->bgtex, WINWIDTH, WINHEIGHT);
  clutter_element_set_opacity (oh->bgtex, 0x99);
  clutter_group_add (clutter_stage(), oh->bgtex);
#endif

  /* create a new group to hold multiple elements in a group */
  oh->group = clutter_group_new();
  
  for (i = 0; i < NHANDS; i++)
    {
      gint x, y, w, h;

      /* Create a texture from pixbuf, then clone in to same resources */
      if (i == 0)
       oh->hand[i] = clutter_texture_new_from_pixbuf (pixbuf);
     else
       oh->hand[i] = clutter_clone_texture_new (CLUTTER_TEXTURE(oh->hand[0]));

      /* Place around a circle */
      w = clutter_element_get_width (oh->hand[0]);
      h = clutter_element_get_height (oh->hand[0]);

      x = WINWIDTH/2  + RADIUS * cos (i * M_PI / (NHANDS/2)) - w/2;
      y = WINHEIGHT/2 + RADIUS * sin (i * M_PI / (NHANDS/2)) - h/2;

      clutter_element_set_position (oh->hand[i], x, y);

      /* Add to our group group */
      clutter_group_add (oh->group, oh->hand[i]);
    }

  /* Add the group to the stage */
  clutter_group_add (clutter_stage(), CLUTTER_ELEMENT(oh->group));

  /* Show everying ( and map window ) */
  clutter_group_show_all (oh->group);
  clutter_group_show_all (clutter_stage());

  g_signal_connect (clutter_stage(), "input-event",
		    G_CALLBACK (input_cb), 
		    oh);

  /* Create a timeline to manage animation */
  timeline = clutter_timeline_new (360, 60); /* num frames, fps */
  g_object_set(timeline, "loop", TRUE, 0);   /* have it loop */

  /* fire a callback for frame change */
  g_signal_connect(timeline, "new-frame",  G_CALLBACK (frame_cb), oh);

  /* and start it */
  clutter_timeline_start (timeline);

  clutter_main();

  return 0;
}
