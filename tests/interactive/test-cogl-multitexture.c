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
  ClutterActor    *group;
  CoglHandle       alpha_tex;
  CoglHandle       redhand_tex;
  gfloat          *tex_coords;

  ClutterTimeline *timeline;

  CoglHandle       material0;
  CoglMatrix       tex_matrix0;
  CoglMatrix       rot_matrix0;
  CoglHandle       light_tex0;

  CoglHandle       material1;
  CoglMatrix       tex_matrix1;
  CoglMatrix       rot_matrix1;
  CoglHandle       light_tex1;

} TestMultiLayerMaterialState;


static void
frame_cb (ClutterTimeline  *timeline,
	  gint		   frame_no,
	  gpointer	   data)
{
  TestMultiLayerMaterialState *state = data;

  cogl_matrix_multiply (&state->tex_matrix0,
			&state->tex_matrix0,
			&state->rot_matrix0);
  cogl_material_set_layer_matrix (state->material0, 2, &state->tex_matrix0);

  cogl_matrix_multiply (&state->tex_matrix1,
			&state->tex_matrix1,
			&state->rot_matrix1);
  cogl_material_set_layer_matrix (state->material1, 2, &state->tex_matrix1);
}

static void
material_rectangle_paint (ClutterActor *actor, gpointer data)
{
  TestMultiLayerMaterialState *state = data;

  cogl_push_matrix ();

  cogl_translate (150, 15, 0);

  cogl_set_source (state->material0);
  cogl_rectangle_with_multitexture_coords (0, 0, 200, 213,
                                           state->tex_coords,
                                           12);
  cogl_translate (-300, -30, 0);
  cogl_set_source (state->material1);
  cogl_rectangle_with_multitexture_coords (0, 0, 200, 213,
                                           state->tex_coords,
                                           12);

  cogl_pop_matrix ();
}

static void
animation_completed_cb (ClutterAnimation            *animation,
                        TestMultiLayerMaterialState *state)
{
  static gboolean go_back = FALSE;
  gdouble new_rotation_y;

  if (go_back)
    new_rotation_y = 30;
  else
    new_rotation_y = -30;
  go_back = !go_back;

  clutter_actor_animate_with_timeline (state->group,
                                       CLUTTER_LINEAR,
                                       state->timeline,
                                       "rotation-angle-y", new_rotation_y,
                                       "signal-after::completed",
                                       animation_completed_cb, state,
                                       NULL);


}

G_MODULE_EXPORT int
test_cogl_multitexture_main (int argc, char *argv[])
{
  GError            *error = NULL;
  ClutterActor	    *stage;
  ClutterColor       stage_color = { 0x61, 0x56, 0x56, 0xff };
  TestMultiLayerMaterialState *state = g_new0 (TestMultiLayerMaterialState, 1);
  ClutterGeometry    geom;
  gchar            **files;
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

  files = g_new (gchar*, 4);
  files[0] = g_build_filename (TESTS_DATADIR, "redhand_alpha.png", NULL);
  files[1] = g_build_filename (TESTS_DATADIR, "redhand.png", NULL);
  files[2] = g_build_filename (TESTS_DATADIR, "light0.png", NULL);
  files[3] = NULL;

  state->alpha_tex =
    cogl_texture_new_from_file (files[0],
                                COGL_TEXTURE_NO_SLICING,
				COGL_PIXEL_FORMAT_ANY,
				&error);
  if (!state->alpha_tex)
    g_critical ("Failed to load redhand_alpha.png: %s", error->message);

  state->redhand_tex =
    cogl_texture_new_from_file (files[1],
                                COGL_TEXTURE_NO_SLICING,
				COGL_PIXEL_FORMAT_ANY,
				&error);
  if (!state->redhand_tex)
    g_critical ("Failed to load redhand.png: %s", error->message);

  state->light_tex0 =
    cogl_texture_new_from_file (files[2],
                                COGL_TEXTURE_NO_SLICING,
				COGL_PIXEL_FORMAT_ANY,
				&error);
  if (!state->light_tex0)
    g_critical ("Failed to load light0.png: %s", error->message);

  state->light_tex1 =
    cogl_texture_new_from_file (files[2],
                                COGL_TEXTURE_NO_SLICING,
				COGL_PIXEL_FORMAT_ANY,
				&error);
  if (!state->light_tex1)
    g_critical ("Failed to load light0.png: %s", error->message);

  g_strfreev (files);

  state->material0 = cogl_material_new ();
  cogl_material_set_layer (state->material0, 0, state->alpha_tex);
  cogl_material_set_layer (state->material0, 1, state->redhand_tex);
  cogl_material_set_layer (state->material0, 2, state->light_tex0);

  state->material1 = cogl_material_new ();
  cogl_material_set_layer (state->material1, 0, state->alpha_tex);
  cogl_material_set_layer (state->material1, 1, state->redhand_tex);
  cogl_material_set_layer (state->material1, 2, state->light_tex1);

  state->tex_coords = tex_coords;

  cogl_matrix_init_identity (&state->tex_matrix0);
  cogl_matrix_init_identity (&state->tex_matrix1);
  cogl_matrix_init_identity (&state->rot_matrix0);
  cogl_matrix_init_identity (&state->rot_matrix1);

  cogl_matrix_translate (&state->rot_matrix0, 0.5, 0.5, 0);
  cogl_matrix_rotate (&state->rot_matrix0, 10.0, 0, 0, 1.0);
  cogl_matrix_translate (&state->rot_matrix0, -0.5, -0.5, 0);

  cogl_matrix_translate (&state->rot_matrix1, 0.5, 0.5, 0);
  cogl_matrix_rotate (&state->rot_matrix1, -10.0, 0, 0, 1.0);
  cogl_matrix_translate (&state->rot_matrix1, -0.5, -0.5, 0);

  clutter_actor_set_anchor_point (state->group, 86, 125);
  clutter_container_add_actor (CLUTTER_CONTAINER(stage),
			       state->group);

  state->timeline = clutter_timeline_new (2812);

  g_signal_connect (state->timeline, "new-frame", G_CALLBACK (frame_cb), state);

  clutter_actor_animate_with_timeline (state->group,
                                       CLUTTER_LINEAR,
                                       state->timeline,
                                       "rotation-angle-y", 30.0,
                                       "signal-after::completed",
                                       animation_completed_cb, state,
                                       NULL);

  /* start the timeline and thus the animations */
  clutter_timeline_start (state->timeline);

  clutter_actor_show_all (stage);

  clutter_main();

  cogl_handle_unref (state->material1);
  cogl_handle_unref (state->material0);
  cogl_handle_unref (state->alpha_tex);
  cogl_handle_unref (state->redhand_tex);
  cogl_handle_unref (state->light_tex0);
  cogl_handle_unref (state->light_tex1);
  g_free (state);

  return 0;
}
