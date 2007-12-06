/* #define TEST_GROUP 1 */

#include <clutter/clutter.h>

#include <errno.h>
#include <stdlib.h>
#include <glib.h>

typedef struct
{
  gchar *name;
  gchar *source;
} ShaderSource;

static ShaderSource shaders[]=
  {
    {"brightness-contrast",

      "uniform float brightness;"
      "uniform float contrast;"
      "uniform sampler2DRect pend_s3_tex;"
      ""
      "void main()"
      "{"
      "    vec4 pend_s4_result;"
      "    pend_s4_result = texture2DRect(pend_s3_tex, gl_TexCoord[0].xy);"
      "    pend_s4_result.x = (pend_s4_result.x - 0.5)*contrast + brightness + 0.5;"
      "    pend_s4_result.y = (pend_s4_result.y - 0.5)*contrast + brightness + 0.5;"
      "    pend_s4_result.z = (pend_s4_result.z - 0.5)*contrast + brightness + 0.5;"
      "    gl_FragColor = pend_s4_result;"
      "}",
    },
    {"box-blur",

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
        "          color += texture2DRect(rectTexture, vec2(gl_TexCoord[0].s + u * 2, gl_TexCoord[0].t +v * 2));"
        "          count ++;"
        "        }"
        ""
        "    gl_FragColor = color / count;"
        "}"
    },
    {"brightness-contrast.asm",

      "!!ARBfp1.0\n"
      "PARAM brightness = program.local[0];\n"
      "PARAM contrast = program.local[1];\n"
      "\n"
      "TEMP R0;\n"
      "TEX R0, fragment.texcoord[0], texture[0], RECT;\n"
      "ADD R0.z, R0, -0.5;\n"
      "MUL R0.z, R0, contrast.x;\n"
      "ADD R0.z, R0, brightness.x;\n"
      "ADD R0.y, R0, -0.5;\n"
      "ADD R0.x, R0, -0.5;\n"
      "MUL R0.y, R0, contrast.x;\n"
      "MUL R0.x, R0, contrast.x;\n"
      "ADD R0.y, R0, brightness.x;\n"
      "ADD R0.x, R0, brightness.x;\n"
      "ADD result.color.z, R0, 0.5;\n"
      "ADD result.color.y, R0, 0.5;\n"
      "ADD result.color.x, R0, 0.5;\n"
      "MOV result.color.w, R0;\n"
      "END ",
    },
    {"invert",

      "uniform sampler2DRect tex;\n"
      "void main ()\n"
      "{\n"
      "  vec4 color = texture2DRect (tex, vec2(gl_TexCoord[0].st));\n"
      "  gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0) - color;\n"
      "  gl_FragColor.a = color.a;\n"
      "}"
    },
    {"brightness-contrast",

        "uniform sampler2DRect tex;"
        "uniform float brightness;"
        "uniform float contrast;"
        "void main ()"
        "{"
        "  vec4 color = texture2DRect (tex, vec2(gl_TexCoord[0].st));"
        "  color.r = (color.r - 0.5) * contrast + brightness + 0.5;"
        "  color.g = (color.g - 0.5) * contrast + brightness + 0.5;"
        "  color.b = (color.b - 0.5) * contrast + brightness + 0.5;"
        "  gl_FragColor = color;"
        "}",
    },
    {"gray",
      "uniform sampler2DRect tex;"
      "void main ()"
      "{"
      "  vec4 color = texture2DRect (tex, vec2(gl_TexCoord[0].st));"
      "  float avg = (color.r + color.g + color.b) / 3;"
      "  color.r = avg;"
      "  color.g = avg;"
      "  color.b = avg;"
      "  gl_FragColor = color;"
      "}",
    },
    {"combined-mirror",
      "uniform sampler2DRect tex;"
      "void main ()"
      "{"
      "  vec4 color = texture2DRect (tex, vec2(gl_TexCoord[0].st));"
      "  vec4 colorB = texture2DRect (tex, vec2(gl_TexCoord[0].ts));"
      "  float avg = (color.r + color.g + color.b) / 3;"
      "  color.r = avg;"
      "  color.g = avg;"
      "  color.b = avg;"
      "  color = (color + colorB)/2;"
      "  gl_FragColor = color;"
      "}",
    },
    {NULL, NULL}
};

static gint shader_no = 2;

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
      clutter_shader_load_from_data (shader, CLUTTER_FRAGMENT_SHADER,
                                     shaders[shader_no].source, -1,
                                     &error);
      if (error)
        {
          g_print ("unable to set shaders[%i] named '%s': %s",
                   error->message);
          g_error_free (error);
          g_object_unref (shader);
        }
      else
        {
          clutter_actor_apply_shader (actor, shader);
          clutter_actor_set_shader_param (actor, "radius", 3.0);

          clutter_actor_queue_redraw (actor);
        }
    }

  return FALSE;
}


gint
main (gint   argc,
      gchar *argv[])
{
  ClutterTimeline  *timeline;
  ClutterAlpha     *alpha;
  ClutterActor     *actor;
  ClutterActor     *stage;
  ClutterColor      stage_color = { 0x61, 0x64, 0x8c, 0xff };
  GdkPixbuf        *pixbuf;
  GError           *error;
  ClutterShader    *shader;

  clutter_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  clutter_actor_set_size (stage, 512, 384);

  g_print ("applying shaders[%i] named '%s'\n",
           shader_no,
           shaders[shader_no].name);

  shader = clutter_shader_new ();

  error = NULL;
  clutter_shader_load_from_data (shader, CLUTTER_FRAGMENT_SHADER,
                                 shaders[shader_no].source, -1,
                                 &error);
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

  clutter_actor_set_position (actor, 100, 100);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), actor);

  clutter_actor_apply_shader (actor, shader);

  clutter_actor_set_shader_param (actor, "brightness", 0.4);
  clutter_actor_set_shader_param (actor, "contrast", -1.9);
                                 
  clutter_actor_set_reactive (actor, TRUE);
  g_signal_connect (actor, "button-release-event",
                    G_CALLBACK (button_release_cb), NULL);

  /* Show everying ( and map window ) */
  clutter_actor_show_all (stage);

  /* and start it */
  clutter_timeline_start (timeline);

  clutter_main ();

  return EXIT_SUCCESS;
}
