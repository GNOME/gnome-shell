/*#define TEST_GROUP */

#include <clutter/clutter.h>

#include <errno.h>
#include <stdlib.h>
#include <glib.h>

/* Dynamic branching appeared in "Shader Model 3.0" that low-end IGPs
 * don't support.
 */
#define GPU_SUPPORTS_DYNAMIC_BRANCHING 0

typedef struct
{
  gchar *name;
  gchar *source;
} ShaderSource;

/* a couple of boilerplate defines that are common amongst all the
 * sample shaders
 */

/* FRAGMENT_SHADER_BEGIN: generate boilerplate with a local vec4 color already initialized,
 * from a sampler2DRect in a variable tex.
 */
#define FRAGMENT_SHADER_BEGIN                  \
     "uniform sampler2DRect tex;"  \
      "void main (){"              \
      "  vec4 color = texture2DRect (tex, vec2(gl_TexCoord[0].st));"

/* FRAGMENT_SHADER_END: apply the changed color to the output buffer correctly blended
 * with the gl specified color (makes the opacity of actors work correctly).
 */
#define FRAGMENT_SHADER_END                    \
      "  gl_FragColor = color;"    \
      "  gl_FragColor = gl_FragColor * gl_Color;" \
      "}"

static ShaderSource shaders[]=
  {
    {"brightness-contrast",
     "uniform float brightness, contrast;"
     FRAGMENT_SHADER_BEGIN
     " color.rgb = (color.rgb - vec3(0.5, 0.5, 0.5)) * contrast + vec3 (brightness + 0.5, brightness + 0.5, brightness + 0.5);"
     FRAGMENT_SHADER_END
    },

    {"box-blur",
#if GPU_SUPPORTS_DYNAMIC_BRANCHING
     "uniform float radius;"
     FRAGMENT_SHADER_BEGIN
     "float u, v;"
     "int count = 1;"
     "for (u=-radius;u<radius;u++)"
     "  for (v=-radius;v<radius;v++)"
     "    {"
     "      color += texture2DRect(tex, "
     "          vec2(gl_TexCoord[0].s + u * 2.0, gl_TexCoord[0].t +v * 2.0));"
     "      count ++;"
     "    }"
     "color = color / float(count);"
     FRAGMENT_SHADER_END
#else
     "vec4 get_rgba_rel(sampler2DRect tex, float dx, float dy)"
     "{"
     "  return texture2DRect (tex, gl_TexCoord[0].st + vec2(dx,dy) * 2.0);"
     "}"

     FRAGMENT_SHADER_BEGIN
     "  float count = 1.0;"
     "  color += get_rgba_rel (tex, -1.0, -1.0); count++;"
     "  color += get_rgba_rel (tex, -1.0,  0.0); count++;"
     "  color += get_rgba_rel (tex, -1.0,  1.0); count++;"
     "  color += get_rgba_rel (tex,  0.0, -1.0); count++;"
     "  color += get_rgba_rel (tex,  0.0,  0.0); count++;"
     "  color += get_rgba_rel (tex,  0.0,  1.0); count++;"
     "  color += get_rgba_rel (tex,  1.0, -1.0); count++;"
     "  color += get_rgba_rel (tex,  1.0,  0.0); count++;"
     "  color += get_rgba_rel (tex,  1.0,  1.0); count++;"
     "  color = color / count;"
     FRAGMENT_SHADER_END
#endif
    },

    {"invert",
     FRAGMENT_SHADER_BEGIN
     "  color.rgb = vec3(1.0, 1.0, 1.0) - color.rgb;\n"
     FRAGMENT_SHADER_END
    },

    {"brightness-contrast",
     "uniform float brightness;"
     "uniform float contrast;"
     FRAGMENT_SHADER_BEGIN
     "  color.r = (color.r - 0.5) * contrast + brightness + 0.5;"
     "  color.g = (color.g - 0.5) * contrast + brightness + 0.5;"
     "  color.b = (color.b - 0.5) * contrast + brightness + 0.5;"
     FRAGMENT_SHADER_END
    },

    {"gray",
     FRAGMENT_SHADER_BEGIN
     "  float avg = (color.r + color.g + color.b) / 3.0;"
     "  color.r = avg;"
     "  color.g = avg;"
     "  color.b = avg;"
     FRAGMENT_SHADER_END
    },

    {"combined-mirror",
     FRAGMENT_SHADER_BEGIN 
     "  vec4 colorB = texture2DRect (tex, vec2(gl_TexCoord[0].ts));"
     "  float avg = (color.r + color.g + color.b) / 3.0;"
     "  color.r = avg;"
     "  color.g = avg;"
     "  color.b = avg;"
     "  color = (color + colorB)/2.0;"
     FRAGMENT_SHADER_END
    },
    /* Terminating NULL sentinel */
    {NULL, NULL}
};

static gint shader_no = 0;

static gboolean
button_release_cb (ClutterActor    *actor,
                   ClutterEvent    *event,
                   gpointer         data)
{
  gint new_no;

  if (event->button.button == 1)
    {
      new_no = shader_no - 1;
    }
  else
    {
      new_no = shader_no + 1;
    }

  if (new_no >= 0 && shaders[new_no].name)
    {
      ClutterShader *shader;
      GError *error;
      shader_no = new_no;
      
      g_print ("setting shaders[%i] named '%s'\n",
               shader_no,
               shaders[shader_no].name);

      shader = clutter_shader_new ();
      
      error = NULL;
      g_object_set (G_OBJECT (shader), "fragment-source", shaders[shader_no].source, NULL);

      /* try to bind the shader, provoking an error we catch if there is issues
       * with the shader sources we've provided. At a later stage it should be possible to
       * iterate through a set of alternate shader sources (glsl -> asm -> cg?) and the one
       * that succesfully compiles is used.
       */
      clutter_shader_bind (shader, &error);

      if (error)
        {
          g_print ("unable to set shaders[%i] named '%s': %s",
                   shader_no, shaders[shader_no].name,
                   error->message);
          g_error_free (error);
          clutter_actor_set_shader (actor, NULL);
        }
      else
        {
          clutter_actor_set_shader (actor, NULL);
          clutter_actor_set_shader (actor, shader);
          clutter_actor_set_shader_param (actor, "radius", 3.0);
          clutter_actor_set_shader_param (actor, "brightness", 0.4);
          clutter_actor_set_shader_param (actor, "contrast", -1.9);
        }
    }
  return FALSE;
}


gint
main (gint   argc,
      gchar *argv[])
{
  ClutterTimeline  *timeline;
  ClutterActor     *actor;
  ClutterActor     *stage;
  ClutterColor      stage_color = { 0x61, 0x64, 0x8c, 0xff };
  ClutterShader    *shader;
  GdkPixbuf        *pixbuf;
  GError           *error;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_actor_set_size (stage, 512, 384);

  g_print ("applying shaders[%i] named '%s'\n",
           shader_no,
           shaders[shader_no].name);

  shader = clutter_shader_new ();

  error = NULL;
  clutter_shader_set_fragment_source (shader, shaders[shader_no].source, -1);
  clutter_shader_bind (shader, &error);
  if (error)
    {
      g_print ("unable to load shaders[%d] named '%s': %s\n",
               shader_no,
               shaders[shader_no].name,
               error->message);
      g_error_free (error);

      return EXIT_FAILURE;
    }

  pixbuf = gdk_pixbuf_new_from_file ("redhand.png", &error);
  if (!pixbuf)
    g_error("pixbuf load failed: %s", error ? error->message : "Unknown");

  clutter_stage_set_title (CLUTTER_STAGE (stage), "Shader Test");
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

  /* Create a timeline to manage animation */
  timeline = clutter_timeline_new (360, 60); /* num frames, fps */
  g_object_set (timeline, "loop", TRUE, NULL);   /* have it loop */

#ifndef TEST_GROUP
  actor = clutter_texture_new_from_pixbuf (pixbuf);
#else
  actor = clutter_group_new ();
    {
      ClutterActor *child1, *child2, *child3, *child4;
      ClutterColor  color = { 0xff, 0x22, 0x66, 0x99 };

      child1 = clutter_texture_new_from_pixbuf (pixbuf);
      child2 = clutter_texture_new_from_pixbuf (pixbuf);
      child3 = clutter_rectangle_new ();
      child4 = clutter_label_new_with_text ("Sans 20px", "Shady stuff");

      clutter_rectangle_set_color (child3, &color);
      clutter_actor_set_size (child3, 50, 50);
      clutter_actor_set_position (child1, 0, 0);
      clutter_actor_set_position (child2, 50, 100);
      clutter_actor_set_position (child3, 30, -30);
      clutter_actor_set_position (child4, -50, 20);

      clutter_group_add (CLUTTER_GROUP (actor), child1);
      clutter_group_add (CLUTTER_GROUP (actor), child2);
      clutter_group_add (CLUTTER_GROUP (actor), child3);
      clutter_group_add (CLUTTER_GROUP (actor), child4);

      clutter_actor_show_all (actor);
    }
#endif

  clutter_actor_set_shader (actor, shader);
  clutter_actor_set_position (actor, 100, 100);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), actor);


  clutter_actor_set_shader_param (actor, "brightness", 0.4);
  clutter_actor_set_shader_param (actor, "contrast", -1.9);
                                 
  clutter_actor_set_reactive (actor, TRUE);
  g_signal_connect (actor, "button-release-event",
                    G_CALLBACK (button_release_cb), NULL);

  /*clutter_actor_set_opacity (actor, 0x77);*/

  /* Show everying ( and map window ) */
  clutter_actor_show_all (stage);

  /* and start it */
  clutter_timeline_start (timeline);

  clutter_main ();

  return EXIT_SUCCESS;
}
