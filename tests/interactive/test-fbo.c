
/*#define TEST_GROUP */

/* These variables are used instead of the standard GLSL variables on
   GLES 2 */
#ifdef HAVE_COGL_GLES2

#define GLES2_VARS \
  "precision mediump float;\n" \
  "varying vec2 tex_coord;\n" \
  "varying vec4 frag_color;\n"
#define TEX_COORD "tex_coord"
#define COLOR_VAR "frag_color"

#else /* HAVE_COGL_GLES2 */

#define GLES2_VARS ""
#define TEX_COORD "gl_TexCoord[0]"
#define COLOR_VAR "gl_Color"

#endif /* HAVE_COGL_GLES2 */

#include <clutter/clutter.h>

#include <errno.h>
#include <stdlib.h>
#include <glib.h>
#include <gmodule.h>

ClutterActor*
make_source(void)
{
  ClutterActor     *source, *actor;
  GError           *error = NULL;

  ClutterColor      yellow = {0xff, 0xff, 0x00, 0xff};

  source  = clutter_group_new();
  actor = clutter_texture_new_from_file ("redhand.png", &error);
  if (!actor)
    g_error("pixbuf load failed: %s", error ? error->message : "Unknown");

  clutter_group_add (source, actor);

  actor = clutter_text_new_with_text ("Sans Bold 50px", "Clutter");

  clutter_text_set_color (CLUTTER_TEXT (actor), &yellow);
  clutter_actor_set_y (actor, clutter_actor_get_height(source) + 5);
  clutter_group_add (source, actor);

  return source;
}

ClutterShader*
make_shader(void)
{
  ClutterShader *shader;
  GError *error = NULL;

  shader = clutter_shader_new ();
  clutter_shader_set_fragment_source (shader, 
   
	GLES2_VARS
        "uniform float radius ;"
        "uniform sampler2D rectTexture;"
	"uniform float x_step, y_step;"
        ""
        "void main()"
        "{"
        "    vec4 color = texture2D(rectTexture, " TEX_COORD ".st);"
        "    float u;"
        "    float v;"
        "    int count = 1;"
        "    for (u=-radius;u<radius;u++)"
        "      for (v=-radius;v<radius;v++)"
        "        {"
        "          color += texture2D(rectTexture, "
        "                             vec2(" TEX_COORD ".s + u"
        "                                  * 2.0 * x_step,"
        "                                  " TEX_COORD ".t + v"
        "                                  * 2.0 * y_step));"
        "          count ++;"
        "        }"
        ""
        "    gl_FragColor = color / float(count);"
        "    gl_FragColor = gl_FragColor * " COLOR_VAR ";"
        "}",
        -1
   );

  if (!clutter_shader_compile (shader, &error))
    {
      fprintf (stderr, "shader compilation failed:\n%s", error->message);
      g_error_free (error);
    }

  return shader;
}

G_MODULE_EXPORT gint
test_fbo_main (gint argc, gchar *argv[])
{
  ClutterColor      blue   = {0x33, 0x44, 0x55, 0xff};

  ClutterActor     *fbo;
  ClutterActor     *onscreen_source, *offscreen_source, *trans_source;
  ClutterActor     *foo_source;
  ClutterActor     *stage;
  ClutterActor     *clone;
  ClutterShader    *shader;
  gint              padx, pady;
  gint              fbo_width, fbo_height;

  clutter_init (&argc, &argv);

  if (clutter_feature_available (CLUTTER_FEATURE_OFFSCREEN) == FALSE)
    g_error("This test requires CLUTTER_FEATURE_OFFSCREEN");

  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &blue);

  /* Create the first source */
  onscreen_source = make_source();
  clutter_actor_show_all (onscreen_source);
  clutter_group_add (stage, onscreen_source);

  /* Basic sizing for alignment */
  fbo_width = clutter_actor_get_width (onscreen_source);
  fbo_height = clutter_actor_get_height (onscreen_source);
  padx = fbo_width + 10;
  pady = fbo_height + 10;
  clutter_actor_set_size (stage, padx*4, pady*2);

  /* Second hand from fbo onscreen */
  if ((fbo = clutter_texture_new_from_actor (onscreen_source)) == NULL)
    g_error("onscreen fbo creation failed");

  clutter_actor_set_position (fbo, padx, 0);
  clutter_group_add (stage, fbo);

  /* apply a shader to it */
  shader = make_shader();
  clutter_actor_set_shader (fbo, shader);
  clutter_actor_set_shader_param_float (fbo, "radius", 2.0);
  clutter_actor_set_shader_param_float (fbo, "x_step",
				  1.0f / clutter_util_next_p2 (fbo_width));
  clutter_actor_set_shader_param_float (fbo, "y_step",
				  1.0f / clutter_util_next_p2 (fbo_height));

  /* Third from cloning the fbo texture */
  clone = clutter_clone_new (fbo);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), clone);
  clutter_actor_set_position (clone, padx*2, 0);


  /* Forth - an offscreen source */
  offscreen_source = make_source();
  clutter_actor_show_all (offscreen_source); /* need to show() offscreen */
  if ((fbo = clutter_texture_new_from_actor (offscreen_source)) == NULL)
    g_error("offscreen fbo creation failed");

  clutter_actor_set_position (fbo, padx*3, 0);
  clutter_group_add (stage, fbo);


  /* 5th transformed */
  trans_source = make_source();
  clutter_actor_show_all (trans_source); /* need to show() offscreen */

  clutter_actor_set_scale (trans_source, 2.5, 2.5);

  if ((fbo = clutter_texture_new_from_actor (trans_source)) == NULL)
    g_error("transformed fbo creation failed");

  clutter_actor_set_position (fbo, 0, pady);
  clutter_group_add (stage, fbo);


  /* 6th resized bigger, but after fbo creation */
  trans_source = make_source();
  clutter_actor_show_all (trans_source); /* need to show() offscreen */

  if ((fbo = clutter_texture_new_from_actor (trans_source)) == NULL)
    g_error("transformed fbo creation failed");

  /* rotate after */
  clutter_actor_move_anchor_point_from_gravity (trans_source, 
						CLUTTER_GRAVITY_CENTER);
  clutter_actor_set_rotation (trans_source, CLUTTER_Z_AXIS, 90.0, 0, 0, 0);

  clutter_actor_set_position (fbo, padx, pady);
  clutter_group_add (stage, fbo);


  /* non visual breaks */
  foo_source = make_source();
  g_object_ref_sink (foo_source);

  clutter_actor_show_all (foo_source);
  if ((fbo = clutter_texture_new_from_actor (foo_source)) == NULL)
    g_error("foo fbo creation failed");

  g_object_unref (foo_source); 	/* fbo should keep it around */

  clutter_actor_set_position (fbo, padx*3, pady);
  clutter_group_add (stage, fbo);

  /* TODO: 
   *  Check realize/unrealize 
   *  get_pixbuf()
   *  set_rgba on fbo texture.
  */

  clutter_actor_show_all (stage);
  clutter_main ();

  return 0;
}
