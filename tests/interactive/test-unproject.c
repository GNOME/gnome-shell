#include <clutter/clutter.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gmodule.h>

ClutterActor *label;

#define RECT_L 200
#define RECT_T 150
#define RECT_W 320
#define RECT_H 240

static gboolean
on_event (ClutterStage *stage,
	  ClutterEvent *event,
	  gpointer      user_data)
{
  switch (event->type)
    {
    case CLUTTER_BUTTON_PRESS:
      {
	gint x, y;
	ClutterActor * actor;
	ClutterUnit xu2, yu2;

        clutter_event_get_coords (event, &x, &y);

	actor = clutter_stage_get_actor_at_pos (stage, x, y);

	if (clutter_actor_transform_stage_point (actor,
                                                 CLUTTER_UNITS_FROM_DEVICE (x),
						 CLUTTER_UNITS_FROM_DEVICE (y),
						 &xu2, &yu2))
	  {
	    gchar *txt;

	    if (actor != CLUTTER_ACTOR (stage))
	      txt = g_strdup_printf ("Click on rectangle\n"
				     "Screen coords: [%d, %d]\n"
				     "Local coords : [%d, %d]",
				     x, y,
				     CLUTTER_UNITS_TO_DEVICE (xu2),
				     CLUTTER_UNITS_TO_DEVICE (yu2));
	    else
	      txt = g_strdup_printf ("Click on stage\n"
				     "Screen coords: [%d, %d]\n"
				     "Local coords : [%d, %d]",
				     x, y,
				     CLUTTER_UNITS_TO_DEVICE (xu2),
				     CLUTTER_UNITS_TO_DEVICE (yu2));

	    clutter_text_set_text (CLUTTER_TEXT (label), txt);
	    g_free (txt);
	  }
	else
	  clutter_text_set_text (CLUTTER_TEXT (label), "Unprojection failed.");
      }
      break;

    default:
      break;
    }

  return FALSE;
}


G_MODULE_EXPORT int
test_unproject_main (int argc, char *argv[])
{
  gchar *txt;
  ClutterActor *rect, *stage, *label0;
  int i, rotate_x = 0, rotate_y = 60, rotate_z = 0;
  ClutterColor stage_clr = { 0x0,  0x0,  0x0,  0xff },
               white     = { 0xff, 0xff, 0xff, 0xff },
               blue      = { 0x0,  0xff, 0xff, 0xff };

  for (i = 0; i < argc; ++i)
    {
      if (!strncmp (argv[i], "--rotate-x", 10))
	{
	  rotate_x = atoi (argv[i] + 11);
	}
      else if (!strncmp (argv[i], "--rotate-y", 10))
	{
	  rotate_y = atoi (argv[i] + 11);
	}
      else if (!strncmp (argv[i], "--rotate-z", 10))
	{
	  rotate_z = atoi (argv[i] + 11);
	}
      else if (!strncmp (argv[i], "--help", 6))
	{
	  g_print ("%s [--rotage-x=degrees] "
                   "[--rotage-y=degrees] "
                   "[--rotage-z=degrees]\n",
		   argv[0]);

	  return EXIT_FAILURE;
	}
    }

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_clr);
  clutter_actor_set_size (stage, 640, 480);

  rect = clutter_rectangle_new_with_color (&white);
  clutter_actor_set_size (rect, RECT_W, RECT_H);
  clutter_actor_set_position (rect, RECT_L, RECT_T);
  clutter_actor_set_rotation (rect, CLUTTER_X_AXIS, rotate_x, 0, 0, 0);
  clutter_actor_set_rotation (rect, CLUTTER_Y_AXIS, rotate_y, 0, 0, 0);
  clutter_actor_set_rotation (rect, CLUTTER_Z_AXIS, rotate_z, 0, 0, 0);
  clutter_group_add (CLUTTER_GROUP (stage), rect);

  txt = g_strdup_printf ("Rectangle: L %d, R %d, T %d, B %d\n"
			 "Rotation : x %d, y %d, z %d",
			 RECT_L, RECT_L + RECT_W,
			 RECT_T, RECT_T + RECT_H,
			 rotate_x, rotate_y, rotate_z);

  label0 = clutter_text_new_with_text ("Mono 8pt", txt);
  clutter_text_set_color (CLUTTER_TEXT (label0), &white);

  clutter_actor_set_position (label0, 10, 10);
  clutter_group_add (CLUTTER_GROUP (stage), label0);

  g_free (txt);

  label =
    clutter_text_new_with_text ("Mono 8pt", "Click around!");

  clutter_text_set_color (CLUTTER_TEXT (label), &blue);

  clutter_actor_set_position (label, 10, 50);
  clutter_group_add (CLUTTER_GROUP (stage), label);

  clutter_actor_show_all (stage);

  g_signal_connect (stage, "event", G_CALLBACK (on_event), NULL);

  clutter_main();

  return EXIT_SUCCESS;
}
