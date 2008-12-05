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

  const char       *knots_poly = ("M 0, 0   L 0, 300 L 300, 300 "
                                  "L 300, 0 L 0, 0");

  /* A spiral created with inkscake */
  const char       *knots_bspline =
    "M 34.285713,35.219326 "
    "C 44.026891,43.384723 28.084874,52.378758 20.714286,51.409804 "
    "C 0.7404474,48.783999 -4.6171866,23.967448 1.904757,8.0764719 "
    "C 13.570984,-20.348756 49.798303,-26.746504 74.999994,-13.352108 "
    "C 111.98449,6.3047056 119.56591,55.259271 99.047626,89.505034 "
    "C 71.699974,135.14925 9.6251774,143.91924 -33.571422,116.17172 "
    "C -87.929934,81.254291 -97.88804,5.8941057 -62.857155,-46.209236 "
    "C -20.430061,-109.31336 68.300385,-120.45954 129.2857,-78.114021 "
    "C 201.15479,-28.21129 213.48932,73.938876 163.80954,143.79074 "
    "C 106.45226,224.43749 -9.1490153,237.96076 -87.85713,180.93363 "
    "C -177.29029,116.13577 -192.00272,-12.937817 -127.61907,-100.49494 "
    "C -55.390344,-198.72081 87.170553,-214.62275 183.57141,-142.87593 "
    "C 290.59464,-63.223369 307.68641,92.835839 228.57145,198.07645";

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
      {
        ClutterPath *path = clutter_path_new ();
        clutter_path_set_description (path, knots_poly);
        p_behave = clutter_behaviour_path_new (alpha, path);
      }
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
      {
        ClutterPath *path = clutter_path_new ();
        clutter_path_set_description (path, knots_bspline);
        p_behave = clutter_behaviour_path_new (alpha, path);
      }
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
