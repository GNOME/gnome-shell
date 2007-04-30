#include <clutter/clutter.h>

static void
button_press_cb (ClutterStage       *stage,
                 ClutterButtonEvent *event,
                 gpointer            data)
{
  const gchar *click_type;

  switch (event->type)
    {
    case CLUTTER_2BUTTON_PRESS:
      click_type = "double";
      break;
    case CLUTTER_3BUTTON_PRESS:
      click_type = "triple";
      break;
    default:
      click_type = "single";
      break;
    }

  g_print ("%s button press event\n", click_type);
}

static void
scroll_event_cb (ClutterStage       *stage,
                 ClutterScrollEvent *event,
                 gpointer            data)
{
  g_print ("scroll direction: %s\n",
           event->direction == CLUTTER_SCROLL_UP ? "up"
                                                 : "down");
}

typedef enum {
    PATH_POLY,
    PATH_ELLIPSE,
    PATH_BSPLINE
} path_t;

#define MAGIC 0.551784
#define RADIUS 200

int
main (int argc, char *argv[])
{
  ClutterTimeline  *timeline;
  ClutterAlpha     *alpha;
  ClutterBehaviour *o_behave, *p_behave;
  ClutterActor     *stage;
  ClutterActor     *group, *rect, *hand;
  ClutterColor      stage_color = { 0xcc, 0xcc, 0xcc, 0xff };
  ClutterColor      rect_bg_color = { 0x33, 0x22, 0x22, 0xff };
  ClutterColor      rect_border_color = { 0, 0, 0, 0 };
  GdkPixbuf        *pixbuf;
  int               i;
  char             *p;
  path_t            path_type = PATH_POLY;
  
  ClutterKnot       knots_poly[] = {{ 0, 0 }, { 0, 300 }, { 300, 300 },
				    { 300, 0 }, {0, 0 }};
  
  ClutterKnot       origin = { 200, 200 };

  ClutterKnot       knots_bspline[] = {{ -RADIUS, 0 },
				       { -RADIUS, RADIUS*MAGIC },
				       { -RADIUS*MAGIC, RADIUS },
				       { 0, RADIUS },
				       { RADIUS*MAGIC, RADIUS },
				       { RADIUS, RADIUS*MAGIC },
				       { RADIUS, 0 },
				       { RADIUS, -RADIUS*MAGIC },
				       { RADIUS*MAGIC, -RADIUS },
				       { 0, -RADIUS },
				       { -RADIUS*MAGIC, -RADIUS },
				       { -RADIUS, -RADIUS*MAGIC },
				       { -RADIUS, 0}};

  for (i = 0; i < argc; ++i)
    {
      if (!strncmp (argv[i], "--path", 6))
	{
	  if (!strncmp (argv[i] + 7, "poly", 4))
	    path_type  = PATH_POLY;
	  else if (!strncmp (argv[i] + 7, "bspline", 7))
	    path_type  = PATH_BSPLINE;
	  else if (!strncmp (argv[i] + 7, "ellipse", 7))
	    path_type  = PATH_ELLIPSE;
	}
      else if (!strncmp (argv[i], "--help", 6))
	{
	  printf ("behave [--path=poly|ellipse|bspline]\n");
	  exit (0);
	}
    }
  
  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_stage_hide_cursor (CLUTTER_STAGE (stage));

  g_signal_connect (stage, "button-press-event",
                    G_CALLBACK (button_press_cb),
                    NULL);
  g_signal_connect (stage, "scroll-event",
                    G_CALLBACK (scroll_event_cb),
                    NULL);
  g_signal_connect (stage, "key-press-event",
                    G_CALLBACK (clutter_main_quit),
                    NULL);

  pixbuf = gdk_pixbuf_new_from_file ("redhand.png", NULL);

  if (!pixbuf)
    g_error("pixbuf load failed");

  clutter_stage_set_color (CLUTTER_STAGE (stage),
		           &stage_color);

  /* Make a hand */
  group = clutter_group_new ();
  clutter_group_add (CLUTTER_GROUP (stage), group);
  clutter_actor_show (group);
  
  rect = clutter_rectangle_new ();
  clutter_actor_set_position (rect, 0, 0);
  clutter_actor_set_size (rect,
                          gdk_pixbuf_get_width (pixbuf),
                          gdk_pixbuf_get_height (pixbuf));
  clutter_rectangle_set_color (CLUTTER_RECTANGLE (rect),
                               &rect_bg_color);
  clutter_rectangle_set_border_width (CLUTTER_RECTANGLE (rect), 10);
  clutter_color_parse ("DarkSlateGray", &rect_border_color);
  clutter_rectangle_set_border_color (CLUTTER_RECTANGLE (rect),
                                      &rect_border_color);
  clutter_actor_show (rect);
  
  hand = clutter_texture_new_from_pixbuf (pixbuf);
  clutter_actor_set_position (hand, 0, 0);
  clutter_actor_show (hand);

  clutter_group_add_many (CLUTTER_GROUP (group), rect, hand, NULL);
  
  /* Make a timeline */
  timeline = clutter_timeline_new (100, 26); /* num frames, fps */
  g_object_set (timeline, "loop", TRUE, 0);  

  /* Set an alpha func to power behaviour - ramp is constant rise/fall */
  alpha = clutter_alpha_new_full (timeline,
                                  CLUTTER_ALPHA_SINE,
                                  NULL, NULL);

  /* Create a behaviour for that alpha */
  o_behave = clutter_behaviour_opacity_new (alpha, 0X33, 0xff); 

  /* Apply it to our actor */
  clutter_behaviour_apply (o_behave, group);

  /* Make a path behaviour and apply that too */
  switch (path_type)
    {
    case PATH_POLY:
      p_behave = clutter_behaviour_path_new (alpha, knots_poly, 5);
      break;
    case PATH_ELLIPSE:
      p_behave =
	clutter_behaviour_ellipse_new (alpha, &origin, 400, 300,
				       1024, 0);
      break;

    case PATH_BSPLINE:
      origin.x = 0;
      origin.y = RADIUS;
      p_behave =
	clutter_behaviour_bspline_new (alpha, knots_bspline,
				   sizeof (knots_bspline)/sizeof(ClutterKnot));

      clutter_behaviour_bspline_set_origin (
					CLUTTER_BEHAVIOUR_BSPLINE (p_behave),
					&origin);
      break;
    }

  clutter_behaviour_apply (p_behave, group);

  /* start the timeline and thus the animations */
  clutter_timeline_start (timeline);

  clutter_actor_show_all (stage);

  clutter_main();

  g_object_unref (o_behave);
  g_object_unref (p_behave);

  return 0;
}
