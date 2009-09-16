#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

#include <clutter/clutter.h>
#include <cogl/cogl.h>

typedef struct _TestMultiLayerMaterialState
{
  ClutterActor	*group;
  CoglHandle	 material;
  CoglHandle	 alpha_tex;
  CoglHandle	 redhand_tex;
  CoglHandle	 light_tex0;
  gfloat        *tex_coords;

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

static void
material_rectangle_paint (ClutterActor *actor, gpointer data)
{
  TestMultiLayerMaterialState *state = data;

  cogl_set_source (state->material);
  cogl_rectangle_with_multitexture_coords (0, 0, 200, 200,
                                           state->tex_coords,
                                           12);
}

G_MODULE_EXPORT int
test_cogl_multitexture_main (int argc, char *argv[])
{
  GError            *error = NULL;
  ClutterTimeline   *timeline;
  ClutterBehaviour  *r_behave;
  ClutterActor	    *stage;
  ClutterColor       stage_color = { 0x61, 0x56, 0x56, 0xff };
  TestMultiLayerMaterialState *state = g_new0 (TestMultiLayerMaterialState, 1);
  ClutterGeometry    geom;
  gfloat             tex_coords[] =
    {
    /* tx1  ty1  tx2  ty2 */
         0,   0,   1,   1,
         0,   0,   1,   1,
         0,   0,   1,   1
    };

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_actor_get_geometry (stage, &geom);

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

  /* We create a non-descript actor that we know doesn't have a
   * default paint handler, so that we can easily control
   * painting in a paint signal handler, without having to
   * sub-class anything etc. */
  state->group = clutter_group_new ();
  clutter_actor_set_position (state->group, geom.width/2, geom.height/2);
  g_signal_connect (state->group, "paint",
		    G_CALLBACK(material_rectangle_paint), state);

  state->alpha_tex =
    cogl_texture_new_from_file ("redhand_alpha.png",
                                COGL_TEXTURE_NO_SLICING,
				COGL_PIXEL_FORMAT_ANY,
				&error);
  if (!state->alpha_tex)
    g_critical ("Failed to load redhand_alpha.png: %s", error->message);

  state->redhand_tex =
    cogl_texture_new_from_file ("redhand.png",
                                COGL_TEXTURE_NO_SLICING,
				COGL_PIXEL_FORMAT_ANY,
				&error);
  if (!state->redhand_tex)
    g_critical ("Failed to load redhand.png: %s", error->message);

  state->light_tex0 =
    cogl_texture_new_from_file ("light0.png",
                                COGL_TEXTURE_NO_SLICING,
				COGL_PIXEL_FORMAT_ANY,
				&error);
  if (!state->light_tex0)
    g_critical ("Failed to load light0.png: %s", error->message);

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

  timeline = clutter_timeline_new (7692);
  clutter_timeline_set_loop (timeline, TRUE);

  g_signal_connect (timeline, "new-frame", G_CALLBACK (frame_cb), state);

  r_behave =
    clutter_behaviour_rotate_new (clutter_alpha_new_full (timeline,
                                                          CLUTTER_LINEAR),
				  CLUTTER_Y_AXIS,
				  CLUTTER_ROTATE_CW,
				  0.0, 360.0);

  /* Apply it to our actor */
  clutter_behaviour_apply (r_behave, state->group);

  /* start the timeline and thus the animations */
  clutter_timeline_start (timeline);

  clutter_actor_show_all (stage);

  clutter_main();

  cogl_handle_unref (state->material);
  cogl_handle_unref (state->alpha_tex);
  cogl_handle_unref (state->redhand_tex);
  cogl_handle_unref (state->light_tex0);
  g_free (state);

  g_object_unref (r_behave);

  return 0;
}
