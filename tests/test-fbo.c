/*#define TEST_GROUP */

#include <clutter/clutter.h>

#include <errno.h>
#include <stdlib.h>
#include <glib.h>

ClutterActor*
make_source(void)
{
  ClutterActor     *source, *actor;
  GdkPixbuf        *pixbuf;
  GError           *error = NULL;

  ClutterColor      yellow = {0xff, 0xff, 0x00, 0xff};

  pixbuf = gdk_pixbuf_new_from_file ("redhand.png", &error);
  if (!pixbuf)
    g_error("pixbuf load failed: %s", error ? error->message : "Unknown");

  source  = clutter_group_new();
  actor = clutter_texture_new_from_pixbuf (pixbuf);
  clutter_group_add (source, actor);

  actor = clutter_label_new_with_text ("Sans Bold 50px", "Clutter");

  clutter_label_set_color (CLUTTER_LABEL (actor), &yellow);
  clutter_actor_set_y (actor, clutter_actor_get_height(source) + 5);
  clutter_group_add (source, actor);

  return source;
}

ClutterShader*
make_shader(void)
{
  ClutterShader *shader;

  shader = clutter_shader_new ();
  clutter_shader_set_fragment_source (shader, 
   
        "uniform float radius ;"
        "uniform sampler2DRect rectTexture;"
        ""
        "void main()"
        "{"
        "    vec4 color = texture2DRect(rectTexture, gl_TexCoord[0].st);"
        "    float u;"
        "    float v;"
        "    int count = 1;"
        "    for (u=-radius;u<radius;u++)"
        "      for (v=-radius;v<radius;v++)"
        "        {"
        "          color += texture2DRect(rectTexture, vec2(gl_TexCoord[0].s + u * 2.0, gl_TexCoord[0].t +v * 2.0));"
        "          count ++;"
        "        }"
        ""
        "    gl_FragColor = color / float(count);"
        "    gl_FragColor = gl_FragColor * gl_Color;"
        "}",
        -1
   );

  return shader;
}

gint
main (gint   argc,
      gchar *argv[])
{
  ClutterColor      blue   = {0x33, 0x44, 0x55, 0xff};

  ClutterActor     *fbo;
  ClutterActor     *onscreen_source, *offscreen_source, *trans_source;
  ClutterActor     *foo_source;
  ClutterActor     *stage;
  ClutterActor     *clone;
  ClutterShader    *shader;
  gint              padx, pady;

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
  padx = clutter_actor_get_width (onscreen_source) + 10;
  pady = clutter_actor_get_height (onscreen_source) + 10;
  clutter_actor_set_size (stage, padx*4, pady*2);


  /* Second hand from fbo onscreen */
  if ((fbo = clutter_texture_new_from_actor (onscreen_source)) == NULL)
    g_error("onscreen fbo creation failed");

  clutter_actor_set_position (fbo, padx, 0);
  clutter_group_add (stage, fbo);

  /* apply a shader to it */
  shader = make_shader();
  clutter_actor_set_shader (fbo, shader);
  clutter_actor_set_shader_param (fbo, "radius", 2.0);


  /* Third from cloning the fbo texture */
  clone = clutter_clone_texture_new (CLUTTER_TEXTURE(fbo));
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
}
