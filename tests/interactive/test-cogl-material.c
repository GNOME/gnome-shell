#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

#include <clutter/clutter.h>
#include <cogl/cogl.h>

#define TIMELINE_FRAME_COUNT 200

typedef struct _TestMultiLayerMaterialState
{
  ClutterActor	*group;
  CoglHandle	 material;
  CoglHandle	 alpha_tex;
  CoglHandle	 redhand_tex;
  CoglHandle	 light_tex0;
  ClutterFixed	*tex_coords;

  CoglMatrix	 tex_matrix;
  CoglMatrix	 rot_matrix;

} TestMultiLayerMaterialState;


static void
frame_cb (ClutterTimeline  *timeline,
	  gint		   frame_no,
	  gpointer	   data)
{
  TestMultiLayerMaterialState *state = data;

  cogl_matrix_multiply (&state->tex_matrix,
			&state->tex_matrix,
			&state->rot_matrix);
  cogl_material_set_layer_matrix (state->material, 2, &state->tex_matrix);
}

static gboolean
material_rectangle_paint (ClutterActor *actor, gpointer data)
{
  TestMultiLayerMaterialState *state = data;

  cogl_set_source (state->material);
  cogl_material_rectangle (CLUTTER_INT_TO_FIXED(0),
			   CLUTTER_INT_TO_FIXED(0),
			   CLUTTER_INT_TO_FIXED(TIMELINE_FRAME_COUNT),
			   CLUTTER_INT_TO_FIXED(TIMELINE_FRAME_COUNT),
			   state->tex_coords);
}

G_MODULE_EXPORT int
test_cogl_material_main (int argc, char *argv[])
{
  ClutterTimeline   *timeline;
  ClutterAlpha	    *alpha;
  ClutterBehaviour  *r_behave;
  ClutterActor	    *stage;
  ClutterColor       stage_color = { 0x61, 0x56, 0x56, 0xff };
  TestMultiLayerMaterialState *state = g_new0 (TestMultiLayerMaterialState, 1);
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
		    G_CALLBACK(material_rectangle_paint), state);

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

  state->material = cogl_material_new ();
  cogl_material_set_layer (state->material, 0, state->alpha_tex);
  cogl_material_set_layer (state->material, 1, state->redhand_tex);
  cogl_material_set_layer (state->material, 2, state->light_tex0);

  state->tex_coords = tex_coords;

  cogl_matrix_init_identity (&state->tex_matrix);
  cogl_matrix_init_identity (&state->rot_matrix);

  cogl_matrix_translate (&state->rot_matrix, 0.5, 0.5, 0);
  cogl_matrix_rotate (&state->rot_matrix, 10.0, 0, 0, 1.0);
  cogl_matrix_translate (&state->rot_matrix, -0.5, -0.5, 0);

  clutter_actor_set_anchor_point (state->group, 86, 125);
  clutter_container_add_actor (CLUTTER_CONTAINER(stage),
			       state->group);

  timeline = clutter_timeline_new (TIMELINE_FRAME_COUNT, 26 /* fps */);
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

  cogl_material_unref (state->material);
  cogl_texture_unref (state->alpha_tex);
  cogl_texture_unref (state->redhand_tex);
  cogl_texture_unref (state->light_tex0);
  g_free (state);

  g_object_unref (r_behave);

  return 0;
}
