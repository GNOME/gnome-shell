#include "config.h"

#include <cogl/cogl.h>

#include <glib.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "test-utils.h"

static TestUtilsSharedState *shared_state = NULL;

/* A bit of sugar for adding new conformance tests */
#define ADD_TEST(FUNC, REQUIREMENTS)  G_STMT_START {      \
  extern void FUNC (TestUtilsGTestFixture *, void *);     \
  if (strcmp (#FUNC, argv[1]) == 0)                       \
    {                                                     \
      test_utils_init (shared_state, REQUIREMENTS);       \
      FUNC (NULL, shared_state);                          \
      test_utils_fini (shared_state);                     \
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

  /* Initialise the state you need to share with everything.
   */
  shared_state = g_new0 (TestUtilsSharedState, 1);
  shared_state->argc_addr = &argc;
  shared_state->argv_addr = &argv;

  /* This file is run through a sed script during the make step so the
   * lines containing the tests need to be formatted on a single line
   * each.
   */

  UNPORTED_TEST (test_cogl_object);
  UNPORTED_TEST (test_cogl_fixed);
  UNPORTED_TEST (test_cogl_materials);
  ADD_TEST (test_cogl_pipeline_user_matrix, 0);
  ADD_TEST (test_cogl_blend_strings, 0);
  UNPORTED_TEST (test_cogl_premult);
  UNPORTED_TEST (test_cogl_readpixels);
  ADD_TEST (test_cogl_path, 0);
  ADD_TEST (test_cogl_depth_test, 0);
  ADD_TEST (test_cogl_color_mask, 0);
  ADD_TEST (test_cogl_backface_culling, TEST_REQUIREMENT_NPOT);

  ADD_TEST (test_cogl_sparse_pipeline, 0);

  UNPORTED_TEST (test_cogl_npot_texture);
  UNPORTED_TEST (test_cogl_multitexture);
  UNPORTED_TEST (test_cogl_texture_mipmaps);
  ADD_TEST (test_cogl_sub_texture, TEST_REQUIREMENT_GL);
  UNPORTED_TEST (test_cogl_pixel_array);
  UNPORTED_TEST (test_cogl_texture_rectangle);
  ADD_TEST (test_cogl_texture_3d, 0);
  ADD_TEST (test_cogl_wrap_modes, 0);
  UNPORTED_TEST (test_cogl_texture_pixmap_x11);
  UNPORTED_TEST (test_cogl_texture_get_set_data);
  UNPORTED_TEST (test_cogl_atlas_migration);
  /* This doesn't currently work on GLES because there is no fallback
     conversion to/from alpha-only */
  ADD_TEST (test_cogl_read_alpha_texture, TEST_REQUIREMENT_GL);

  UNPORTED_TEST (test_cogl_vertex_buffer_contiguous);
  UNPORTED_TEST (test_cogl_vertex_buffer_interleved);
  UNPORTED_TEST (test_cogl_vertex_buffer_mutability);

  ADD_TEST (test_cogl_primitive, 0);

  ADD_TEST (test_cogl_just_vertex_shader, 0);
  ADD_TEST (test_cogl_pipeline_uniforms, 0);
  ADD_TEST (test_cogl_snippets, 0);
  ADD_TEST (test_cogl_custom_attributes, 0);

  ADD_TEST (test_cogl_bitmask, 0);

  ADD_TEST (test_cogl_offscreen, 0);

  UNPORTED_TEST (test_cogl_viewport);

  g_printerr ("Unknown test name \"%s\"\n", argv[1]);

  return 1;
}
