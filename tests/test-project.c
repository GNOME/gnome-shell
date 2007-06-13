#include <clutter/clutter.h>
#include <stdio.h>
#include <stdlib.h>

ClutterActor     *stage, *rect, *p[5];
gboolean          init_done = FALSE;

static void
init_handles ()
{
  gint              i;
  ClutterVertex    v[4];
  ClutterFixed     xp, yp, zp;
  ClutterColor blue = { 0, 0, 0xff, 0xff };

  clutter_actor_project_vertices (rect, v);
  for (i = 0; i < 4; ++i)
    {
      p[i] = clutter_rectangle_new_with_color (&blue);
      clutter_actor_set_size (p[i], 5, 5);
      clutter_actor_set_position (p[i], 0, 0);
      clutter_group_add (CLUTTER_GROUP(stage), p[i]);

      clutter_actor_set_position (p[i],
				  CLUTTER_FIXED_INT (v[i].x) -
				  clutter_actor_get_width (p[i])/2,
				  CLUTTER_FIXED_INT (v[i].y) -
				  clutter_actor_get_height (p[i])/2);

      clutter_actor_raise_top (p[i]);

      clutter_actor_show (p[i]);
    }

  xp = CLUTTER_INT_TO_FIXED (clutter_actor_get_width (rect)/2);
  yp = CLUTTER_INT_TO_FIXED (clutter_actor_get_height (rect)/2);
  zp = 0;
  
  clutter_actor_project_point (rect, &xp, &yp, &zp);
  p[4] = clutter_rectangle_new_with_color (&blue);
  clutter_actor_set_size (p[4], 5, 5);
  clutter_actor_set_position (p[4], 0, 0);
  clutter_group_add (CLUTTER_GROUP(stage), p[4]);
  clutter_actor_set_position (p[4],
			      CLUTTER_FIXED_INT (xp) -
			      clutter_actor_get_width (p[4])/2,
			      CLUTTER_FIXED_INT (yp) -
			      clutter_actor_get_height (p[4])/2);

  clutter_actor_raise_top (p[4]);
  
  clutter_actor_show (p[4]);
}

static void
place_handles ()
{
  gint              i;
  ClutterVertex    v[4];
  ClutterFixed     xp, yp, zp;
  ClutterColor blue = { 0, 0, 0xff, 0xff };

  clutter_actor_project_vertices (rect, v);
  for (i = 0; i < 4; ++i)
    {
      clutter_actor_set_position (p[i],
				  CLUTTER_FIXED_INT (v[i].x) -
				  clutter_actor_get_width (p[i])/2,
				  CLUTTER_FIXED_INT (v[i].y) -
				  clutter_actor_get_height (p[i])/2);
    }

  xp = CLUTTER_INT_TO_FIXED (clutter_actor_get_width (rect)/2);
  yp = CLUTTER_INT_TO_FIXED (clutter_actor_get_height (rect)/2);
  zp = 0;
  
  clutter_actor_project_point (rect, &xp, &yp, &zp);
  clutter_actor_set_position (p[4],
			      CLUTTER_FIXED_INT (xp) -
			      clutter_actor_get_width (p[4])/2,
			      CLUTTER_FIXED_INT (yp) -
			      clutter_actor_get_height (p[4])/2);
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


void 
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
	    ClutterVertex    v[4];
	    ClutterActorBox  b;
	    ClutterFixed     vec[4];
	    ClutterFixed     m[16];
	    ClutterFixed     xp, yp, zp;
	    ClutterFixed     xu, yu, zu;

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
	    ClutterFixed xp, yp;
	    
	    i = find_handle_index (dragging);

	    if (i < 0)
	      break;
	    
	    clutter_event_get_coords (event, &x, &y);
	    
	    clutter_actor_allocate_coords (dragging, &box1);
	    clutter_actor_allocate_coords (rect, &box2);

	    xp = CLUTTER_INT_TO_FIXED (x-3) - box1.x1;
	    yp = CLUTTER_INT_TO_FIXED (y-3) - box1.y1;
	    
	    if (i == 4)
	      {
		g_debug ("moving box by %f, %f",
			 CLUTTER_FIXED_TO_FLOAT (xp),
			 CLUTTER_FIXED_TO_FLOAT (yp));
			 
		clutter_actor_move_by (rect,
				       CLUTTER_FIXED_INT(xp),
				       CLUTTER_FIXED_INT(yp));
	      }
	    else
	      {
		g_debug ("adjusting box by %f, %f, handle %d",
			 CLUTTER_FIXED_TO_FLOAT (xp),
			 CLUTTER_FIXED_TO_FLOAT (yp),
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

		clutter_actor_request_coords (rect, &box2);
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
}


int
main (int argc, char *argv[])
{
  ClutterAlpha     *alpha;
  ClutterBehaviour *o_behave;
  ClutterActor     *label;
  
  ClutterColor      stage_color = { 0x0, 0x0, 0x0, 0xff }, 
                    red         = { 0xff, 0, 0, 0xff },
	            white       = { 0xff, 0xff, 0xff, 0xff };

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_actor_set_size (stage, 640, 480);

  rect = clutter_rectangle_new_with_color (&white);
  clutter_actor_set_size (rect, 320, 240);
  clutter_actor_set_position (rect, 180, 120);
  clutter_actor_rotate_y (rect, 60, 0, 0);
  clutter_group_add (CLUTTER_GROUP(stage), rect);

  label = clutter_label_new_with_text ("Mono 8pt",
				       "Drag the blue rectangles");
  clutter_label_set_color (CLUTTER_LABEL (label), &white);
  
  clutter_actor_set_position (label, 10, 10);
  clutter_group_add (CLUTTER_GROUP(stage), label);
  
  clutter_actor_show_all (stage);
  
  g_signal_connect (stage, "event", G_CALLBACK (on_event), NULL);

  init_handles ();
  
  clutter_main();

  return EXIT_SUCCESS;
}
