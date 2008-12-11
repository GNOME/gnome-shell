#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

#include <clutter/clutter.h>
#include <cogl/cogl.h>

#define TIMELINE_FRAME_COUNT 200

typedef struct _MultiTextureState
{
  ClutterActor	*group;
  CoglHandle	 multi_tex;
  CoglHandle	 alpha_tex;
  CoglHandle	 redhand_tex;
  CoglHandle	 light_tex0;
  CoglHandle	 light_tex1;
  ClutterFixed	*tex_coords;

  /* For handling light switching */
  guint		 last_light_change;
  gboolean	 light_on;

  /* For handling texture coord sliding */
  guint		 last_frame_no;
  gint		 light_x_dir;
  gint		 light_y_dir;
  gint		 light_x_pos;
  gint		 light_y_pos;

} MultiTextureState;


static void
frame_cb (ClutterTimeline  *timeline,
	  gint		   frame_no,
	  gpointer	   data)
{
  MultiTextureState *state = data;
  ClutterFixed	    *tex_coords = &state->tex_coords[8];
  int prev_frame_delta;
  int light_duration;

  light_duration = frame_no - state->last_light_change;
  if (light_duration < 0)
    light_duration += TIMELINE_FRAME_COUNT;

  if (light_duration > 10)
    {
      if (state->light_on)
	{
	  cogl_multi_texture_layer_set_texture (state->multi_tex,
						2,
						state->light_tex1);
	  state->light_on = FALSE;
	}
      else
	{
	  cogl_multi_texture_layer_set_texture (state->multi_tex,
						2,
						state->light_tex0);
	  state->light_on = TRUE;
	}
      state->last_light_change = frame_no;
    }

  /* slide the texture coordinates */

  /* This is worked out as if the texture has a virtual resolution
   * of TIMELINE_FRAME_COUNT x TIMELINE_FRAME_COUNT.
   *
   * We are always showing an apature of the texture that is
   * (TIMELINE_FRAME_COUNT/2) x (TIMELINE_FRAME_COUNT/2)
   *
   * To remain within the texture we don't let the tx1, ty1 positions
   * go past ((TIMELINE_FRAME_COUNT/2), (TIMELINE_FRAME_COUNT/2))
   */
  prev_frame_delta = frame_no - state->last_frame_no;
  if (prev_frame_delta < 0)
    prev_frame_delta += TIMELINE_FRAME_COUNT;

  state->light_x_pos += prev_frame_delta * state->light_x_dir;
  if (state->light_x_pos > TIMELINE_FRAME_COUNT/2)
    {
      state->light_x_pos = TIMELINE_FRAME_COUNT/2;
      state->light_x_dir = -state->light_x_dir;
    }
  else if (state->light_x_pos < 0)
    {
      state->light_x_pos = 0;
      state->light_x_dir = 1;
    }

  state->light_y_pos += prev_frame_delta * state->light_y_dir;
  if (state->light_y_pos > TIMELINE_FRAME_COUNT/2)
    {
      state->light_y_pos = TIMELINE_FRAME_COUNT/2;
      state->light_y_dir = -state->light_y_dir;
    }
  else if (state->light_y_pos < 0)
    {
      state->light_y_pos = 0;
      state->light_y_dir = 1;
    }

#define CI_F CLUTTER_INT_TO_FIXED
  tex_coords[0] = (CI_F(1)/TIMELINE_FRAME_COUNT) * state->light_x_pos;
  tex_coords[1] = (CI_F(1)/TIMELINE_FRAME_COUNT) * state->light_y_pos;
  tex_coords[2] = tex_coords[0] + CI_F(1)/2;
  tex_coords[3] = tex_coords[1] + CI_F(1)/2;
#undef CI_F

  state->last_frame_no = frame_no;
}

static gboolean
multi_texture_paint (ClutterActor *actor, gpointer data)
{
  MultiTextureState *state = data;

  cogl_multi_texture_rectangle (state->multi_tex,
			        CLUTTER_INT_TO_FIXED(0),
			        CLUTTER_INT_TO_FIXED(0),
			        CLUTTER_INT_TO_FIXED(TIMELINE_FRAME_COUNT),
			        CLUTTER_INT_TO_FIXED(TIMELINE_FRAME_COUNT),
			        state->tex_coords);
}

G_MODULE_EXPORT int
test_cogl_multi_texture_main (int argc, char *argv[])
{
  ClutterTimeline  *timeline;
  ClutterAlpha     *alpha;
  ClutterBehaviour *r_behave;
  ClutterActor	    *stage;
  ClutterColor       stage_color = { 0x15, 0x93, 0x15, 0xff };
  MultiTextureState *state = g_new0 (MultiTextureState, 1);
  CoglHandle	     layer;
  ClutterGeometry    geom;
  ClutterFixed	     tex_coords[] =
  {
    /* tx1 ty1  tx2			  ty2 */
       0,  0,	CLUTTER_INT_TO_FIXED (1), CLUTTER_INT_TO_FIXED (1),
       0,  0,	CLUTTER_INT_TO_FIXED (1), CLUTTER_INT_TO_FIXED (1),
       0,  0,	CLUTTER_INT_TO_FIXED (1), CLUTTER_INT_TO_FIXED (1)
  };


  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_actor_get_geometry (stage, &geom);

  clutter_stage_set_color (CLUTTER_STAGE (stage),
		           &stage_color);

  /* We create a non-descript actor that we know doesn't have a
   * default paint handler, so that we can easily control
   * painting in a paint signal handler, without having to
   * sub-class anything etc. */
  state->group = clutter_group_new ();
  clutter_actor_set_position (state->group, geom.width/2, geom.height/2);
  g_signal_connect (state->group, "paint",
		    G_CALLBACK(multi_texture_paint), state);

  state->alpha_tex =
    cogl_texture_new_from_file ("./redhand_alpha.png",
				-1, /* disable slicing */
				TRUE,
				COGL_PIXEL_FORMAT_ANY,
				NULL);
  state->redhand_tex =
    cogl_texture_new_from_file ("./redhand.png",
				-1, /* disable slicing */
				TRUE,
				COGL_PIXEL_FORMAT_ANY,
				NULL);
  state->light_tex0 =
    cogl_texture_new_from_file ("./light0.png",
				-1, /* disable slicing */
				TRUE,
				COGL_PIXEL_FORMAT_ANY,
				NULL);
  state->light_tex1 =
    cogl_texture_new_from_file ("./light1.png",
				-1, /* disable slicing */
				TRUE,
				COGL_PIXEL_FORMAT_ANY,
				NULL);

  state->multi_tex = cogl_multi_texture_new ();

  cogl_multi_texture_layer_set_texture (state->multi_tex, 0,
				        state->alpha_tex);
  cogl_multi_texture_layer_set_texture (state->multi_tex, 1,
					state->redhand_tex);
  cogl_multi_texture_layer_set_texture (state->multi_tex, 2,
					state->light_tex0);

  state->tex_coords = tex_coords;

  /* pick a random starting direction */
  state->light_x_dir = rand() % 5;
  state->light_y_dir = rand() % 5;

  clutter_actor_set_anchor_point (state->group, 86, 125);
  clutter_container_add_actor (CLUTTER_CONTAINER(stage),
			       state->group);


  timeline = clutter_timeline_new (TIMELINE_FRAME_COUNT, 26); /* num frames, fps */
  g_object_set (timeline, "loop", TRUE, NULL);

  g_signal_connect (timeline, "new-frame", G_CALLBACK (frame_cb), state);

  /* Set an alpha func to power behaviour - ramp is constant rise/fall */
  alpha = clutter_alpha_new_for_mode (CLUTTER_LINEAR);
  clutter_alpha_set_timeline (alpha, timeline);

  /* Create a behaviour for that alpha */
  r_behave = clutter_behaviour_rotate_new (alpha,
					   CLUTTER_Y_AXIS,
					   CLUTTER_ROTATE_CW,
					   0.0, 360.0);

  /* Apply it to our actor */
  clutter_behaviour_apply (r_behave, state->group);

  /* start the timeline and thus the animations */
  clutter_timeline_start (timeline);

  clutter_actor_show_all (stage);

  clutter_main();

  cogl_multi_texture_unref (state->multi_tex);
  cogl_texture_unref (state->alpha_tex);
  cogl_texture_unref (state->redhand_tex);
  cogl_texture_unref (state->light_tex0);
  cogl_texture_unref (state->light_tex1);
  g_free (state);

  g_object_unref (r_behave);

  return 0;
}
