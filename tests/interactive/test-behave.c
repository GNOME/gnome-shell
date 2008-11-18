#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gmodule.h>

#include <clutter/clutter.h>

static gboolean
button_press_cb (ClutterStage       *stage,
                 ClutterButtonEvent *event,
                 gpointer            data)
{
  const gchar *click_type;

  switch (event->click_count)
    {
    case 2:
      click_type = "double";
      break;
    case 3:
      click_type = "triple";
      break;
    default:
      click_type = "single";
      break;
    }

  g_print ("%s button press event\n", click_type);

  return FALSE;
}

static gboolean
scroll_event_cb (ClutterStage       *stage,
                 ClutterScrollEvent *event,
                 gpointer            data)
{
  g_print ("scroll direction: %s\n",
           event->direction == CLUTTER_SCROLL_UP ? "up"
                                                 : "down");

  return FALSE;
}

static void
timeline_completed (ClutterTimeline *timeline)
{
  ClutterTimelineDirection direction;

  direction = clutter_timeline_get_direction (timeline);

  if (direction == CLUTTER_TIMELINE_FORWARD)
    direction = CLUTTER_TIMELINE_BACKWARD;
  else
    direction = CLUTTER_TIMELINE_FORWARD;

  clutter_timeline_set_direction (timeline, direction);
}

typedef enum {
    PATH_POLY,
    PATH_ELLIPSE,
    PATH_BSPLINE
} path_t;

#define MAGIC 0.551784
#define RADIUS 200

G_MODULE_EXPORT int
test_behave_main (int argc, char *argv[])
{
  ClutterTimeline  *timeline;
  ClutterAlpha     *alpha;
  ClutterBehaviour *o_behave, *p_behave;
  ClutterActor     *stage;
  ClutterActor     *group, *rect, *hand;
  ClutterColor      stage_color = { 0xcc, 0xcc, 0xcc, 0xff };
  ClutterColor      rect_bg_color = { 0x33, 0x22, 0x22, 0xff };
  ClutterColor      rect_border_color = { 0, 0, 0, 0 };
  int               i;
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

  clutter_stage_set_color (CLUTTER_STAGE (stage),
		           &stage_color);

  /* Make a hand */
  group = clutter_group_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), group);
  clutter_actor_show (group);
  
  hand = clutter_texture_new_from_file ("redhand.png", NULL);
  if (hand == NULL)
    {
      g_error("pixbuf load failed");
      return 1;
    }
  clutter_actor_set_position (hand, 0, 0);
  clutter_actor_show (hand);

  rect = clutter_rectangle_new ();
  clutter_actor_set_position (rect, 0, 0);
  clutter_actor_set_size (rect,
                          clutter_actor_get_width (hand),
			  clutter_actor_get_height (hand));
  clutter_rectangle_set_color (CLUTTER_RECTANGLE (rect),
                               &rect_bg_color);
  clutter_rectangle_set_border_width (CLUTTER_RECTANGLE (rect), 10);
  clutter_color_parse ("DarkSlateGray", &rect_border_color);
  clutter_rectangle_set_border_color (CLUTTER_RECTANGLE (rect),
                                      &rect_border_color);
  clutter_actor_show (rect);
  
  clutter_container_add (CLUTTER_CONTAINER (group), rect, hand, NULL);
  
  /* Make a timeline */
  timeline = clutter_timeline_new_for_duration (4000); /* num frames, fps */
  clutter_timeline_set_loop (timeline, TRUE);
  g_signal_connect (timeline,
                    "completed", G_CALLBACK (timeline_completed),
                    NULL);

  /* Set an alpha func to power behaviour - ramp is constant rise */
  alpha = clutter_alpha_new_for_mode (CLUTTER_LINEAR);
  clutter_alpha_set_timeline (alpha, timeline);

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
	clutter_behaviour_ellipse_new (alpha, 200, 200, 400, 300,
				       CLUTTER_ROTATE_CW,
				       0.0, 360.0);

      clutter_behaviour_ellipse_set_angle_tilt (CLUTTER_BEHAVIOUR_ELLIPSE (p_behave),
 						CLUTTER_X_AXIS,
 						45.0);
      clutter_behaviour_ellipse_set_angle_tilt (CLUTTER_BEHAVIOUR_ELLIPSE (p_behave),
 						CLUTTER_Z_AXIS,
 						45.0);
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
