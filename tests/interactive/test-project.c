#include <clutter/clutter.h>
#include <stdio.h>
#include <stdlib.h>
#include <gmodule.h>

static ClutterActor *main_stage, *rect, *p[5];

static void
init_handles ()
{
  gint              i;
  ClutterVertex    v[4];
  ClutterVertex    v1, v2;
  ClutterColor blue = { 0, 0, 0xff, 0xff };

  clutter_actor_get_abs_allocation_vertices (rect, v);
  for (i = 0; i < 4; ++i)
    {
      p[i] = clutter_rectangle_new_with_color (&blue);
      clutter_actor_set_size (p[i], 5, 5);
      clutter_actor_set_position (p[i], 0, 0);
      clutter_group_add (CLUTTER_GROUP (main_stage), p[i]);

      clutter_actor_set_positionu (p[i],
				   v[i].x -
				   clutter_actor_get_widthu (p[i])/2,
				   v[i].y -
				   clutter_actor_get_heightu (p[i])/2);

      clutter_actor_raise_top (p[i]);

      clutter_actor_show (p[i]);
    }

  v1.x = clutter_actor_get_widthu (rect) / 2;
  v1.y = clutter_actor_get_heightu (rect) / 2;
  v1.z = 0;
  
  clutter_actor_apply_transform_to_point (rect, &v1, &v2);
  p[4] = clutter_rectangle_new_with_color (&blue);
  clutter_actor_set_size (p[4], 5, 5);
  clutter_actor_set_position (p[4], 0, 0);
  clutter_group_add (CLUTTER_GROUP (main_stage), p[4]);
  clutter_actor_set_positionu (p[4],
			       v2.x -
			       clutter_actor_get_widthu (p[4])/2,
			       v2.y -
			       clutter_actor_get_heightu (p[4])/2);

  clutter_actor_raise_top (p[4]);
  
  clutter_actor_show (p[4]);
}

static void
place_handles ()
{
  gint              i;
  ClutterVertex    v[4];
  ClutterVertex    v1, v2;

  clutter_actor_get_abs_allocation_vertices (rect, v);
  for (i = 0; i < 4; ++i)
    {
      clutter_actor_set_positionu (p[i],
				   v[i].x -
				   clutter_actor_get_widthu (p[i])/2,
				   v[i].y -
				   clutter_actor_get_heightu (p[i])/2);
    }

  v1.x = clutter_actor_get_widthu (rect)/2;
  v1.y = clutter_actor_get_heightu (rect)/2;
  v1.z = 0;
  
  clutter_actor_apply_transform_to_point (rect, &v1, &v2);
  clutter_actor_set_positionu (p[4],
			       v2.x - clutter_actor_get_widthu (p[4])/2,
			       v2.y - clutter_actor_get_heightu (p[4])/2);
}

#define M(m,row,col)  (m)[col*4+row]

static gint
find_handle_index (ClutterActor * a)
{
    gint i;
    for (i = 0; i < sizeof(p)/sizeof(p[0]); ++i)
	if (p[i] == a)
	    return i;

    return -1;
}

static gboolean
on_event (ClutterStage *stage,
	  ClutterEvent *event,
	  gpointer      user_data)
{
  static ClutterActor * dragging = NULL;
  
  switch (event->type)
    {
    case CLUTTER_BUTTON_PRESS:
      {
	gint x, y;
	ClutterActor * actor;

        clutter_event_get_coords (event, &x, &y);

	actor = clutter_stage_get_actor_at_pos (stage, x, y);
	
        if (actor != CLUTTER_ACTOR (stage))
	  {
	    if (actor != rect)
	      dragging = actor;
	  }
      }
      break;

    case CLUTTER_MOTION:
      {
	if (dragging)
	  {
	    gint x, y;
	    gint i;
	    ClutterActorBox box1, box2;
	    ClutterUnit xp, yp;
	    
	    i = find_handle_index (dragging);

	    if (i < 0)
	      break;
	    
	    clutter_event_get_coords (event, &x, &y);
	    
	    clutter_actor_get_allocation_box (dragging, &box1);
	    clutter_actor_get_allocation_box (rect, &box2);

	    xp = CLUTTER_UNITS_FROM_DEVICE (x - 3) - box1.x1;
	    yp = CLUTTER_UNITS_FROM_DEVICE (y - 3) - box1.y1;
	    
	    if (i == 4)
	      {
		g_debug ("moving box by %f, %f",
			 CLUTTER_UNITS_TO_FLOAT (xp),
			 CLUTTER_UNITS_TO_FLOAT (yp));
			 
		clutter_actor_move_byu (rect, xp, yp);
	      }
	    else
	      {
		g_debug ("adjusting box by %f, %f, handle %d",
			 CLUTTER_UNITS_TO_FLOAT (xp),
			 CLUTTER_UNITS_TO_FLOAT (yp),
			 i);

		switch (i)
		  {
		  case 0:
		    box2.x1 += xp;
		    box2.y1 += yp;
		    break;
		  case 1:
		    box2.x2 += xp;
		    box2.y1 += yp;
		    break;
		  case 2:
		    box2.x1 += xp;
		    box2.y2 += yp;
		    break;
		  case 3:
		    box2.x2 += xp;
		    box2.y2 += yp;
		    break;
		  }

                /* FIXME this is just plain wrong, to allocate directly
                 * like this
                 */
		clutter_actor_allocate (rect, &box2, TRUE);
	      }
	    
 	    place_handles ();
	  }
      }
      break;
      
    case CLUTTER_BUTTON_RELEASE:
      {
	dragging = NULL;
      }
      break;
      
    default:
      break;
    }

  return FALSE;
}


G_MODULE_EXPORT int
test_project_main (int argc, char *argv[])
{
  ClutterActor *label;
  
  ClutterColor      stage_color = { 0x0, 0x0, 0x0, 0xff }, 
	            white       = { 0xff, 0xff, 0xff, 0xff };

  clutter_init (&argc, &argv);

  main_stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (main_stage), &stage_color);
  clutter_actor_set_size (main_stage, 640, 480);

  rect = clutter_rectangle_new_with_color (&white);
  clutter_actor_set_size (rect, 320, 240);
  clutter_actor_set_position (rect, 180, 120);
  clutter_actor_set_rotation (rect, CLUTTER_Y_AXIS, 60, 0, 0, 0);
  clutter_group_add (CLUTTER_GROUP (main_stage), rect);

  label = clutter_text_new_with_text ("Mono 8pt", "Drag the blue rectangles");
  clutter_text_set_color (CLUTTER_TEXT (label), &white);
  
  clutter_actor_set_position (label, 10, 10);
  clutter_group_add (CLUTTER_GROUP (main_stage), label);
  
  clutter_actor_show_all (main_stage);
  
  g_signal_connect (main_stage, "event", G_CALLBACK (on_event), NULL);

  init_handles ();
  
  clutter_main();

  return EXIT_SUCCESS;
}
