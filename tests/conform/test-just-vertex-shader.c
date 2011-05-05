#include <clutter/clutter.h>
#include <string.h>

#include "test-conform-common.h"

static const ClutterColor stage_color = { 0x00, 0x00, 0xff, 0xff };

static void
draw_frame (void)
{
  CoglHandle material = cogl_material_new ();
  CoglColor color;
  GError *error = NULL;
  CoglHandle shader, program;

  /* Set the primary vertex color as red */
  cogl_color_set_from_4ub (&color, 0xff, 0x00, 0x00, 0xff);
  cogl_material_set_color (material, &color);

  /* Override the vertex color in the texture environment with a
     constant green color */
  cogl_color_set_from_4ub (&color, 0x00, 0xff, 0x00, 0xff);
  cogl_material_set_layer_combine_constant (material, 0, &color);
  if (!cogl_material_set_layer_combine (material, 0,
                                        "RGBA=REPLACE(CONSTANT)",
                                        &error))
    {
      g_warning ("Error setting blend constant: %s", error->message);
      g_assert_not_reached ();
    }

  /* Set up a dummy vertex shader that does nothing but the usual
     fixed function transform */
  shader = cogl_create_shader (COGL_SHADER_TYPE_VERTEX);
  cogl_shader_source (shader,
                      "void\n"
                      "main ()\n"
                      "{\n"
                      "  cogl_position_out = "
                      "cogl_modelview_projection_matrix * "
                      "cogl_position_in;\n"
                      "  cogl_color_out = cogl_color_in;\n"
                      "}\n");
  cogl_shader_compile (shader);
  if (!cogl_shader_is_compiled (shader))
    {
      char *log = cogl_shader_get_info_log (shader);
      g_warning ("Shader compilation failed:\n%s", log);
      g_free (log);
      g_assert_not_reached ();
    }

  program = cogl_create_program ();
  cogl_program_attach_shader (program, shader);
  cogl_program_link (program);

  cogl_handle_unref (shader);

  /* Draw something using the material */
  cogl_set_source (material);
  cogl_rectangle (0, 0, 50, 50);

  /* Draw it again using the program. It should look exactly the same */
  cogl_program_use (program);
  cogl_rectangle (50, 0, 100, 50);
  cogl_program_use (COGL_INVALID_HANDLE);

  cogl_handle_unref (material);
  cogl_handle_unref (program);
}

static void
validate_pixel (int x, int y)
{
  guint8 pixels[4];

  cogl_read_pixels (x, y, 1, 1, COGL_READ_PIXELS_COLOR_BUFFER,
                    COGL_PIXEL_FORMAT_RGBA_8888_PRE, pixels);

  /* The final color should be green. If it's blue then the layer
     state is being ignored. If it's green then the stage is showing
     through */
  g_assert_cmpint (pixels[0], ==, 0x00);
  g_assert_cmpint (pixels[1], ==, 0xff);
  g_assert_cmpint (pixels[2], ==, 0x00);
}

static void
validate_result (void)
{
  /* Non-shader version */
  validate_pixel (25, 25);
  /* Shader version */
  validate_pixel (75, 25);
}

static void
on_paint (void)
{
  draw_frame ();

  validate_result ();

  /* Comment this out to see what the test paints */
  clutter_main_quit ();
}

void
test_cogl_just_vertex_shader (TestUtilsGTestFixture *fixture,
                              void *data)
{
  ClutterActor *stage;
  unsigned int paint_handler;

  stage = clutter_stage_get_default ();

  /* If shaders aren't supported then we can't run the test */
  if (cogl_features_available (COGL_FEATURE_SHADERS_GLSL))
    {
      clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

      paint_handler = g_signal_connect_after (stage, "paint",
                                              G_CALLBACK (on_paint), NULL);

      clutter_actor_show (stage);

      clutter_main ();

      g_signal_handler_disconnect (stage, paint_handler);

      if (g_test_verbose ())
        g_print ("OK\n");
    }
  else if (g_test_verbose ())
    g_print ("Skipping\n");
}

