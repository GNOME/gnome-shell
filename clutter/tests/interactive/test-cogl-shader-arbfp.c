#include <clutter/clutter.h>

#include <errno.h>
#include <stdlib.h>
#include <glib.h>
#include <gmodule.h>

typedef struct
{
  char *name;
  char *source;
} ShaderSource;

static ShaderSource shaders[]=
  {
    /*{"brightness-contrast",
     FRAGMENT_SHADER_VARS
     "uniform float brightness, contrast;"
     FRAGMENT_SHADER_BEGIN
     " color.rgb = (color.rgb - vec3(0.5, 0.5, 0.5)) * contrast + "
          "vec3 (brightness + 0.5, brightness + 0.5, brightness + 0.5);"
     FRAGMENT_SHADER_END
    },*/
    {"brightness-contrast",
     "!!ARBfp1.0\n"
     "PARAM bc = program.local[0];"
     "TEMP color;"
     "TEMP color2;"
     "TEX color.rgba, fragment.texcoord[0], texture[0], 2D;"
     "SUB color.rgb, color, 0.5;"
     "MUL color2, color, bc.w;"
     "ADD color.rgb, color2, bc.z;"
     "MOV result.color, color;"
     "END"
    },

    /*{"box-blur",
     FRAGMENT_SHADER_VARS

     "vec4 get_rgba_rel(sampler2D tex, float dx, float dy)"
     "{"
     "  return texture2D (tex, " TEX_COORD ".st "
     "                         + vec2(dx, dy) * 2.0);"
     "}"

     FRAGMENT_SHADER_BEGIN
     "  float count = 1.0;"
     "  color += get_rgba_rel (tex, -x_step, -y_step); count++;"
     "  color += get_rgba_rel (tex, -x_step,  0.0);    count++;"
     "  color += get_rgba_rel (tex, -x_step,  y_step); count++;"
     "  color += get_rgba_rel (tex,  0.0,    -y_step); count++;"
     "  color += get_rgba_rel (tex,  0.0,     0.0);    count++;"
     "  color += get_rgba_rel (tex,  0.0,     y_step); count++;"
     "  color += get_rgba_rel (tex,  x_step, -y_step); count++;"
     "  color += get_rgba_rel (tex,  x_step,  0.0);    count++;"
     "  color += get_rgba_rel (tex,  x_step,  y_step); count++;"
     "  color = color / count;"
     FRAGMENT_SHADER_END
    },*/
    {"box-blur",
     "!!ARBfp1.0\n"
     "PARAM params = program.local[0];"
     "TEMP accum;"
     "TEMP color;"
     "TEMP coord;"
     "TEMP step;"

     "MUL step, params, 2.0;"

     "SUB coord, fragment.texcoord[0], step;"
     "TEX color.rgba, coord, texture[0], 2D;"
     "MOV accum, color;"

     "MOV coord, fragment.texcoord[0];"
     "SUB coord.x, coord.x, step.x;"
     "TEX color.rgba, coord, texture[0], 2D;"
     "ADD accum, accum, color;"

     "MOV coord, fragment.texcoord[0];"
     "SUB coord.x, coord.x, step.x;"
     "ADD coord.y, coord.y, step.y;"
     "TEX color.rgba, coord, texture[0], 2D;"
     "ADD accum, accum, color;"

     "MOV coord, fragment.texcoord[0];"
     "SUB coord.y, coord.y, step.y;"
     "TEX color.rgba, coord, texture[0], 2D;"
     "ADD accum, accum, color;"

     "MOV coord, fragment.texcoord[0];"
     "TEX color.rgba, coord, texture[0], 2D;"
     "ADD accum, accum, color;"

     "MOV coord, fragment.texcoord[0];"
     "ADD coord.y, coord.y, step.y;"
     "TEX color.rgba, coord, texture[0], 2D;"
     "ADD accum, accum, color;"

     "MOV coord, fragment.texcoord[0];"
     "ADD coord.x, coord.x, step.x;"
     "SUB coord.y, coord.y, step.y;"
     "TEX color.rgba, coord, texture[0], 2D;"
     "ADD accum, accum, color;"

     "MOV coord, fragment.texcoord[0];"
     "ADD coord.x, coord.x, step.x;"
     "TEX color.rgba, coord, texture[0], 2D;"
     "ADD accum, accum, color;"

     "MOV coord, fragment.texcoord[0];"
     "ADD coord.x, coord.x, step.x;"
     "ADD coord.y, coord.y, step.y;"
     "TEX color.rgba, coord, texture[0], 2D;"
     "ADD accum, accum, color;"

     "MUL color, accum, 0.11111111;"
     "MOV result.color, color;"
     "END"
    },

    /*{"invert",
     FRAGMENT_SHADER_VARS
     FRAGMENT_SHADER_BEGIN
     "  color.rgb = vec3(1.0, 1.0, 1.0) - color.rgb;\n"
     FRAGMENT_SHADER_END
    },*/
    {"invert",
     "!!ARBfp1.0\n"
     "TEMP color;"
     "TEX color.rgba, fragment.texcoord[0], texture[0], 2D;"
     "ADD color.rgb, 1.0, -color;"
     "MOV result.color, color;"
     "END"
    },

    /*{"gray",
     FRAGMENT_SHADER_VARS
     FRAGMENT_SHADER_BEGIN
     "  float avg = (color.r + color.g + color.b) / 3.0;"
     "  color.r = avg;"
     "  color.g = avg;"
     "  color.b = avg;"
     FRAGMENT_SHADER_END
    },*/
    {"gray",
     "!!ARBfp1.0\n"
     "TEMP color;"
     "TEMP grey;"
     "TEX color.rgba, fragment.texcoord[0], texture[0], 2D;"
     "ADD grey, color.r, color.g;"
     "ADD grey, grey, color.b;"
     "MUL grey, grey, 0.33333333;"
     "MOV color.rgb, grey;"
     "MOV result.color, color;"
     "END"
    },

/*    {"combined-mirror",
     FRAGMENT_SHADER_VARS
     FRAGMENT_SHADER_BEGIN
     "  vec4 colorB = texture2D (tex, vec2(" TEX_COORD ".ts));"
     "  float avg = (color.r + color.g + color.b) / 3.0;"
     "  color.r = avg;"
     "  color.g = avg;"
     "  color.b = avg;"
     "  color = (color + colorB)/2.0;"
     FRAGMENT_SHADER_END
    },*/
    {"combined-mirror",
     "!!ARBfp1.0\n"
     "TEMP color1;"
     "TEMP color2;"
     "TEMP coord;"
     "MOV coord.x, fragment.texcoord[0].y;"
     "MOV coord.y, fragment.texcoord[0].x;"
     "TEX color1.rgba, fragment.texcoord[0], texture[0], 2D;"
     "TEX color2.rgba, coord, texture[0], 2D;"
     "MUL color1, color1, 0.5;"
     "MUL color2, color2, 0.5;"
     "ADD result.color, color1, color2;"
     "END"
    },

/*    {"edge-detect",
     FRAGMENT_SHADER_VARS
     "float get_avg_rel(sampler2D texB, float dx, float dy)"
     "{"
     "  vec4 colorB = texture2D (texB, " TEX_COORD ".st + vec2(dx, dy));"
     "  return (colorB.r + colorB.g + colorB.b) / 3.0;"
     "}"
     FRAGMENT_SHADER_BEGIN
     "  mat3 sobel_h = mat3( 1.0,  2.0,  1.0,"
     "                       0.0,  0.0,  0.0,"
     "                      -1.0, -2.0, -1.0);"
     "  mat3 sobel_v = mat3( 1.0,  0.0, -1.0,"
     "                       2.0,  0.0, -2.0,"
     "                       1.0,  0.0, -1.0);"
     "  mat3 map = mat3( get_avg_rel(tex, -x_step, -y_step),"
     "                   get_avg_rel(tex, -x_step, 0.0),"
     "                   get_avg_rel(tex, -x_step, y_step),"
     "                   get_avg_rel(tex, 0.0, -y_step),"
     "                   get_avg_rel(tex, 0.0, 0.0),"
     "                   get_avg_rel(tex, 0.0, y_step),"
     "                   get_avg_rel(tex, x_step, -y_step),"
     "                   get_avg_rel(tex, x_step, 0.0),"
     "                   get_avg_rel(tex, x_step, y_step) );"
     "  mat3 gh = sobel_h * map;"
     "  mat3 gv = map * sobel_v;"
     "  float avgh = (gh[0][0] + gh[0][1] + gh[0][2] +"
     "                gh[1][0] + gh[1][1] + gh[1][2] +"
     "                gh[2][0] + gh[2][1] + gh[2][2]) / 18.0 + 0.5;"
     "  float avgv = (gv[0][0] + gv[0][1] + gv[0][2] +"
     "                gv[1][0] + gv[1][1] + gv[1][2] +"
     "                gv[2][0] + gv[2][1] + gv[2][2]) / 18.0 + 0.5;"
     "  float avg = (avgh + avgv) / 2.0;"
     "  color.r = avg * color.r;"
     "  color.g = avg * color.g;"
     "  color.b = avg * color.b;"
     FRAGMENT_SHADER_END
    },*/

    /* Don't really fancy doing this one in assembly :) */
};

static CoglHandle redhand;
static CoglMaterial *material;
static unsigned int timeout_id = 0;
static int shader_no = 0;

static void
paint_cb (ClutterActor *actor)
{
  int stage_width = clutter_actor_get_width (actor);
  int stage_height = clutter_actor_get_height (actor);
  int image_width = cogl_texture_get_width (redhand);
  int image_height = cogl_texture_get_height (redhand);

  cogl_set_source (material);
  cogl_rectangle (stage_width/2.0f - image_width/2.0f,
                  stage_height/2.0f - image_height/2.0f,
                  stage_width/2.0f + image_width/2.0f,
                  stage_height/2.0f + image_height/2.0f);
}

static void
set_shader_num (int new_no)
{
  CoglHandle shader;
  CoglHandle program;
  int image_width = cogl_texture_get_width (redhand);
  int image_height = cogl_texture_get_height (redhand);
  float param0[4];
  int uniform_no;

  g_print ("setting shaders[%i] named '%s'\n",
           new_no,
           shaders[new_no].name);

  shader = cogl_create_shader (COGL_SHADER_TYPE_FRAGMENT);
  cogl_shader_source (shader, shaders[new_no].source);
  cogl_shader_compile (shader);

  program = cogl_create_program ();
  cogl_program_attach_shader (program, shader);
  cogl_handle_unref (shader);
  cogl_program_link (program);

  param0[0] = 1.0f/image_width; /* texel x step delta */
  param0[1] = 1.0f/image_height; /* texel y step delta */
  param0[2] = 0.4; /* brightness */
  param0[3] = -1.9; /* contrast */

  uniform_no = cogl_program_get_uniform_location (program, "program.local[0]");
  cogl_program_set_uniform_float (program, uniform_no, 4, 1, param0);

  cogl_material_set_user_program (material, program);
  cogl_handle_unref (program);

  shader_no = new_no;
}

static gboolean
button_release_cb (ClutterActor *actor,
                   ClutterEvent *event,
                   void *data)
{
  int new_no;

  /* Stop the automatic cycling if the user want to manually control
   * which shader to display */
  if (timeout_id)
    {
      g_source_remove (timeout_id);
      timeout_id = 0;
    }

  if (event->button.button == 1)
    {
      new_no = shader_no - 1;
      if (new_no < 0)
        new_no = G_N_ELEMENTS (shaders) - 1;
    }
  else
    {
      new_no = shader_no + 1;
      if (new_no >= G_N_ELEMENTS (shaders))
        new_no = 0;
    }

  set_shader_num (new_no);

  return CLUTTER_EVENT_STOP;
}

static gboolean
key_release_cb (ClutterActor *actor,
                ClutterEvent *event,
                gpointer user_data)
{
  guint keysym = clutter_event_get_key_symbol (event);
  ClutterModifierType mods = clutter_event_get_state (event);

  if (keysym == CLUTTER_KEY_q ||
      ((mods & CLUTTER_SHIFT_MASK) && keysym == CLUTTER_KEY_q))
    clutter_main_quit ();

  return CLUTTER_EVENT_STOP;
}

static gboolean
timeout_cb (gpointer user_data)
{
  shader_no++;
  if (shader_no > (G_N_ELEMENTS (shaders) - 1))
    shader_no = 0;

  set_shader_num (shader_no);

  return G_SOURCE_CONTINUE;
}

static gboolean
idle_cb (gpointer data)
{
  clutter_actor_queue_redraw (data);

  return G_SOURCE_CONTINUE;
}

static gboolean
destroy_window_cb (ClutterStage *stage,
                   ClutterEvent *event,
                   void *user_data)
{
  clutter_main_quit ();
  return TRUE;
}

G_MODULE_EXPORT int
test_cogl_shader_arbfp_main (int argc, char *argv[])
{
  ClutterActor *stage;
  char *file;
  GError *error;
  ClutterColor stage_color = { 0x61, 0x64, 0x8c, 0xff };

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();

  clutter_stage_set_title (CLUTTER_STAGE (stage), "Assembly Shader Test");
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

  file = g_build_filename (TESTS_DATADIR, "redhand.png", NULL);
  error = NULL;
  redhand = cogl_texture_new_from_file (file, 0, COGL_PIXEL_FORMAT_ANY,
                                        &error);
  if (redhand == COGL_INVALID_HANDLE)
    g_error ("image load failed: %s", error->message);

  material = cogl_material_new ();
  cogl_material_set_layer (material, 0, redhand);

  set_shader_num (0);
  g_signal_connect_after (stage, "paint", G_CALLBACK (paint_cb), NULL);

  clutter_actor_set_reactive (stage, TRUE);
  g_signal_connect (stage, "button-release-event",
                    G_CALLBACK (button_release_cb), NULL);
  g_signal_connect (stage, "key-release-event",
                    G_CALLBACK (key_release_cb), NULL);

  g_signal_connect (stage, "delete-event",
                    G_CALLBACK (destroy_window_cb), NULL);

  timeout_id = clutter_threads_add_timeout (1000, timeout_cb, NULL);

  clutter_threads_add_idle (idle_cb, stage);

  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}

