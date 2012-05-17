#include "config.h"

#include <cogl/cogl.h>

#include <glib.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "test-utils.h"

/* A bit of sugar for adding new conformance tests */
#define ADD_TEST(FUNC, REQUIREMENTS)  G_STMT_START {      \
  extern void FUNC (void);                                \
  if (strcmp (#FUNC, argv[1]) == 0)                       \
    {                                                     \
      test_utils_init (REQUIREMENTS);                     \
      FUNC ();                                            \
      test_utils_fini ();                                 \
      exit (0);                                           \
    }                                                     \
} G_STMT_END

#define UNPORTED_TEST(FUNC)

int
main (int argc, char **argv)
{
  int i;

  if (argc != 2)
    {
      g_printerr ("usage %s UNIT_TEST\n", argv[0]);
      exit (1);
    }

  /* Just for convenience in case people try passing the wrapper
   * filenames for the UNIT_TEST argument we normalize '-' characters
   * to '_' characters... */
  for (i = 0; argv[1][i]; i++)
    {
      if (argv[1][i] == '-')
        argv[1][i] = '_';
    }

  /* This file is run through a sed script during the make step so the
   * lines containing the tests need to be formatted on a single line
   * each.
   */

  UNPORTED_TEST (test_object);
  UNPORTED_TEST (test_fixed);
  UNPORTED_TEST (test_materials);
  ADD_TEST (test_pipeline_user_matrix, 0);
  ADD_TEST (test_blend_strings, 0);
  ADD_TEST (test_premult, 0);
  UNPORTED_TEST (test_readpixels);
  ADD_TEST (test_path, 0);
  ADD_TEST (test_depth_test, 0);
  ADD_TEST (test_color_mask, 0);
  ADD_TEST (test_backface_culling, TEST_REQUIREMENT_NPOT);

  ADD_TEST (test_sparse_pipeline, 0);

  UNPORTED_TEST (test_npot_texture);
  UNPORTED_TEST (test_multitexture);
  UNPORTED_TEST (test_texture_mipmaps);
  ADD_TEST (test_sub_texture, 0);
  ADD_TEST (test_pixel_buffer, 0);
  UNPORTED_TEST (test_texture_rectangle);
  ADD_TEST (test_texture_3d, 0);
  ADD_TEST (test_wrap_modes, 0);
  UNPORTED_TEST (test_texture_pixmap_x11);
  UNPORTED_TEST (test_texture_get_set_data);
  ADD_TEST (test_atlas_migration, 0);
  ADD_TEST (test_read_texture_formats, 0);
  ADD_TEST (test_write_texture_formats, 0);

  UNPORTED_TEST (test_vertex_buffer_contiguous);
  UNPORTED_TEST (test_vertex_buffer_interleved);
  UNPORTED_TEST (test_vertex_buffer_mutability);

  ADD_TEST (test_primitive, 0);

  ADD_TEST (test_just_vertex_shader, 0);
  ADD_TEST (test_pipeline_uniforms, 0);
  ADD_TEST (test_snippets, 0);
  ADD_TEST (test_custom_attributes, 0);

  ADD_TEST (test_bitmask, 0);

  ADD_TEST (test_offscreen, 0);

  ADD_TEST (test_point_size, 0);
  ADD_TEST (test_point_sprite,
            TEST_KNOWN_FAILURE | TEST_REQUIREMENT_POINT_SPRITE);

  ADD_TEST (test_version, 0);

  UNPORTED_TEST (test_viewport);

  ADD_TEST (test_gles2_context, TEST_REQUIREMENT_GLES2_CONTEXT);

  ADD_TEST (test_euler_quaternion, 0);

  g_printerr ("Unknown test name \"%s\"\n", argv[1]);

  return 1;
}
