/*#define TEST_GROUP */

#include <clutter/clutter.h>

#include <errno.h>
#include <stdlib.h>
#include <glib.h>


gint
main (gint   argc,
      gchar *argv[])
{
  ClutterColor      color={0x33, 0x44, 0x55, 0xff};
  ClutterActor     *fbo;
  ClutterActor     *actor;
  ClutterActor     *actor2;
  ClutterActor     *group;
  ClutterShader    *shader;
  ClutterActor     *stage;
  ClutterActor     *rectangle;
  ClutterActor     *clone;
  GdkPixbuf        *pixbuf;
  GError           *error = NULL;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &color);

  pixbuf = gdk_pixbuf_new_from_file ("redhand.png", &error);
  if (!pixbuf)
    g_error("pixbuf load failed: %s", error ? error->message : "Unknown");

  /* actor = clutter_texture_new_from_pixbuf (pixbuf);*/

  group = clutter_group_new ();
  {
    ClutterColor nothing = {0, 0,0,0};
    rectangle = clutter_rectangle_new_with_color (&nothing);
    clutter_actor_set_size (rectangle, 800, 270);
  }
  
  actor2 = clutter_texture_new_from_pixbuf (pixbuf);
  clutter_container_add_actor (CLUTTER_CONTAINER (group), actor2);
  {
    ClutterColor yellow = {0xff, 0xff, 0x00, 0xff};
    actor = clutter_label_new_with_text ("Sans 50px", "Hello hadyness");
    clutter_label_set_color (CLUTTER_LABEL (actor), &yellow);
  }

  clutter_container_add_actor (CLUTTER_CONTAINER (group), actor);
  clutter_container_add_actor (CLUTTER_CONTAINER (group), rectangle);
  clutter_actor_set_position (actor, 0, 15);

  clutter_actor_show_all (group);

  fbo = clutter_texture_new_from_actor (group);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), fbo);
  clutter_actor_set_position (fbo, 20, 120);
  clutter_actor_set_position (actor2, 130, 20);

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

        "}",
        -1
   );

  clone = clutter_clone_texture_new (fbo);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), clone);
  clutter_actor_set_position (clone, 40, 300);

 
  if(1)clutter_actor_apply_shader (clone, shader);
  if(1)clutter_actor_set_shader_param (clone, "radius", 2.0);
  

  clutter_actor_show_all (stage);
  clutter_main ();
}
