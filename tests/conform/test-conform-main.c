#include "config.h"

#include <cogl/cogl.h>

#include <glib.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "test-utils.h"

/* A bit of sugar for adding new conformance tests */
#define ADD_TEST(FUNC, REQUIREMENTS, KNOWN_FAIL_REQUIREMENTS)           \
  G_STMT_START {                                                        \
    extern void FUNC (void);                                            \
    if (strcmp (#FUNC, argv[1]) == 0)                                   \
      {                                                                 \
        test_utils_init (REQUIREMENTS, KNOWN_FAIL_REQUIREMENTS);        \
        FUNC ();                                                        \
        test_utils_fini ();                                             \
        exit (0);                                                       \
      }                                                                 \
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
  ADD_TEST (test_pipeline_user_matrix, 0, 0);
  ADD_TEST (test_blend_strings, 0, 0);
  ADD_TEST (test_blend, 0, 0);
  ADD_TEST (test_premult, 0, 0);
  UNPORTED_TEST (test_readpixels);
  ADD_TEST (test_path, 0, 0);
  ADD_TEST (test_depth_test, 0, 0);
  ADD_TEST (test_color_mask, 0, 0);
  ADD_TEST (test_backface_culling, 0, TEST_REQUIREMENT_NPOT);
  ADD_TEST (test_layer_remove, 0, 0);

  ADD_TEST (test_sparse_pipeline, 0, 0);

  ADD_TEST (test_npot_texture, 0, 0);
  UNPORTED_TEST (test_multitexture);
  UNPORTED_TEST (test_texture_mipmaps);
  ADD_TEST (test_sub_texture, 0, 0);
  ADD_TEST (test_pixel_buffer_map, 0, 0);
  ADD_TEST (test_pixel_buffer_set_data, 0, 0);
  ADD_TEST (test_pixel_buffer_sub_region, 0, 0);
  UNPORTED_TEST (test_texture_rectangle);
  ADD_TEST (test_texture_3d, TEST_REQUIREMENT_TEXTURE_3D, 0);
  ADD_TEST (test_wrap_modes, 0, 0);
  UNPORTED_TEST (test_texture_pixmap_x11);
  ADD_TEST (test_texture_get_set_data, 0, 0);
  ADD_TEST (test_atlas_migration, 0, 0);
  ADD_TEST (test_read_texture_formats, 0, 0);
  ADD_TEST (test_write_texture_formats, 0, 0);
  ADD_TEST (test_alpha_textures, 0, 0);
  ADD_TEST (test_wrap_rectangle_textures,
            TEST_REQUIREMENT_TEXTURE_RECTANGLE,
            TEST_KNOWN_FAILURE);

  UNPORTED_TEST (test_vertex_buffer_contiguous);
  UNPORTED_TEST (test_vertex_buffer_interleved);
  UNPORTED_TEST (test_vertex_buffer_mutability);

  ADD_TEST (test_primitive, 0, 0);

  ADD_TEST (test_just_vertex_shader, TEST_REQUIREMENT_GLSL, 0);
  ADD_TEST (test_pipeline_uniforms, TEST_REQUIREMENT_GLSL, 0);
  ADD_TEST (test_snippets, TEST_REQUIREMENT_GLSL, 0);
  ADD_TEST (test_custom_attributes, TEST_REQUIREMENT_GLSL, 0);

  ADD_TEST (test_offscreen, 0, 0);
  ADD_TEST (test_framebuffer_get_bits,
            TEST_REQUIREMENT_OFFSCREEN | TEST_REQUIREMENT_GL,
            0);

  ADD_TEST (test_point_size, 0, 0);
  ADD_TEST (test_point_size_attribute,
            TEST_REQUIREMENT_PER_VERTEX_POINT_SIZE, 0);
  ADD_TEST (test_point_size_attribute_snippet,
            TEST_REQUIREMENT_PER_VERTEX_POINT_SIZE |
            TEST_REQUIREMENT_GLSL, 0);
  ADD_TEST (test_point_sprite,
            TEST_REQUIREMENT_POINT_SPRITE,
            0);
  ADD_TEST (test_point_sprite_orientation,
            TEST_REQUIREMENT_POINT_SPRITE,
            TEST_KNOWN_FAILURE);

  ADD_TEST (test_version, 0, 0);

  ADD_TEST (test_alpha_test, 0, 0);

  ADD_TEST (test_map_buffer_range, TEST_REQUIREMENT_MAP_WRITE, 0);

  ADD_TEST (test_primitive_and_journal, 0, 0);

  ADD_TEST (test_copy_replace_texture, 0, 0);

  ADD_TEST (test_pipeline_cache_unrefs_texture, 0, 0);

  UNPORTED_TEST (test_viewport);

  ADD_TEST (test_gles2_context, TEST_REQUIREMENT_GLES2_CONTEXT, 0);
  ADD_TEST (test_gles2_context_fbo, TEST_REQUIREMENT_GLES2_CONTEXT, 0);
  ADD_TEST (test_gles2_context_copy_tex_image,
            TEST_REQUIREMENT_GLES2_CONTEXT,
            0);

  ADD_TEST (test_euler_quaternion, 0, 0);
  ADD_TEST (test_color_hsl, 0, 0);

  ADD_TEST (test_fence, TEST_REQUIREMENT_FENCE, 0);

  ADD_TEST (test_texture_no_allocate, 0, 0);

  g_printerr ("Unknown test name \"%s\"\n", argv[1]);

  return 1;
}
