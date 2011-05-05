#include "config.h"

#include <cogl/cogl.h>

#include <glib.h>
#include <locale.h>
#include <stdlib.h>

#include "test-utils.h"

#if 0
void
skip_init (TestUtilsGTestFixture *fixture,
           const void *data)
{
  /* void */
}

static void
skip_test (TestUtilsGTestFixture *fixture,
           const void *data)
{
  /* void */
}

void
skip_fini (TestUtilsGTestFixture *fixture,
           const void *data)
{
  /* void */
}
#endif

static void
run_todo_test (TestUtilsGTestFixture *fixture,
               void *data)
{
#ifdef G_OS_UNIX
  TestUtilsSharedState *state = data;

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      state->todo_func (fixture, data);
      exit (0);
    }

  g_test_trap_assert_failed ();
#endif
}

void
verify_failure (TestUtilsGTestFixture *fixture,
                void *data)
{
  g_assert (FALSE);
}

static TestUtilsSharedState *shared_state = NULL;

/* This is a bit of sugar for adding new conformance tests:
 *
 * - It adds an extern function definition just to save maintaining a header
 *   that lists test entry points.
 * - It sets up callbacks for a fixture, which lets us share initialization
 *   code between tests. (see test-utils.c)
 * - It passes in a shared data pointer that is initialised once in main(),
 *   that gets passed to the fixture setup and test functions. (See the
 *   definition in test-utils.h)
 */
#define ADD_TEST(NAMESPACE, FUNC)            G_STMT_START {             \
  extern void FUNC (TestUtilsGTestFixture *, void *);                   \
  g_test_add ("/conform" NAMESPACE "/" #FUNC,                           \
	      TestUtilsGTestFixture,                                    \
	      shared_state, /* data argument for test */                \
	      test_utils_init,                                          \
	      (void *)(FUNC),                                           \
	      test_utils_fini);    } G_STMT_END

/* this is a macro that conditionally executes a test if CONDITION
 * evaluates to TRUE; otherwise, it will put the test under the
 * "/skip" namespace and execute a dummy function that will always
 * pass.
 */
#define ADD_CONDITIONAL_TEST(CONDITION, NAMESPACE, FUNC)   G_STMT_START {   \
  if (!(CONDITION)) {                                                       \
    g_test_add ("/skipped" NAMESPACE "/" #FUNC,                             \
                TestUtilsGTestFixture,                                      \
                shared_state, /* data argument for test */                  \
                skip_init,                                                  \
                skip_test,                                                  \
                skip_fini);                                                 \
  } else { ADD_TEST (NAMESPACE, FUNC); }     } G_STMT_END

#define ADD_TODO_TEST(NAMESPACE, FUNC)              G_STMT_START {          \
   extern void FUNC (TestUtilsGTestFixture *, void *);                      \
   shared_state->todo_func = FUNC;                                          \
   g_test_add ("/todo" NAMESPACE "/" #FUNC,                                 \
              TestUtilsGTestFixture,                                        \
              shared_state,                                                 \
              test_utils_init,                                              \
              (void *)(run_todo_test),                                      \
              test_utils_fini);    } G_STMT_END

#define UNPORTED_TEST(NAMESPACE, FUNC)

gchar *
clutter_test_get_data_file (const gchar *filename)
{
  return g_build_filename (TESTS_DATADIR, filename, NULL);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=%s");

  /* Initialise the state you need to share with everything.
   */
  shared_state = g_new0 (TestUtilsSharedState, 1);
  shared_state->argc_addr = &argc;
  shared_state->argv_addr = &argv;

  /* This file is run through a sed script during the make step so the
   * lines containing the tests need to be formatted on a single line
   * each. To comment out a test use the SKIP or TODO macros. Using
   * #if 0 would break the script. */

  /* sanity check for the test suite itself */
  ADD_TODO_TEST ("/suite", verify_failure);

  UNPORTED_TEST ("/cogl", test_cogl_object);
  UNPORTED_TEST ("/cogl", test_cogl_fixed);
  UNPORTED_TEST ("/cogl", test_cogl_backface_culling);
  UNPORTED_TEST ("/cogl", test_cogl_materials);
  UNPORTED_TEST ("/cogl", test_cogl_pipeline_user_matrix);
  UNPORTED_TEST ("/cogl", test_cogl_blend_strings);
  UNPORTED_TEST ("/cogl", test_cogl_premult);
  UNPORTED_TEST ("/cogl", test_cogl_readpixels);
  UNPORTED_TEST ("/cogl", test_cogl_path);
  ADD_TEST ("/cogl", test_cogl_depth_test);

  UNPORTED_TEST ("/cogl/texture", test_cogl_npot_texture);
  UNPORTED_TEST ("/cogl/texture", test_cogl_multitexture);
  UNPORTED_TEST ("/cogl/texture", test_cogl_texture_mipmaps);
  UNPORTED_TEST ("/cogl/texture", test_cogl_sub_texture);
  UNPORTED_TEST ("/cogl/texture", test_cogl_pixel_array);
  UNPORTED_TEST ("/cogl/texture", test_cogl_texture_rectangle);
  UNPORTED_TEST ("/cogl/texture", test_cogl_texture_3d);
  UNPORTED_TEST ("/cogl/texture", test_cogl_wrap_modes);
  UNPORTED_TEST ("/cogl/texture", test_cogl_texture_pixmap_x11);
  UNPORTED_TEST ("/cogl/texture", test_cogl_texture_get_set_data);
  UNPORTED_TEST ("/cogl/texture", test_cogl_atlas_migration);

  UNPORTED_TEST ("/cogl/vertex-buffer", test_cogl_vertex_buffer_contiguous);
  UNPORTED_TEST ("/cogl/vertex-buffer", test_cogl_vertex_buffer_interleved);
  UNPORTED_TEST ("/cogl/vertex-buffer", test_cogl_vertex_buffer_mutability);

  UNPORTED_TEST ("/cogl/vertex-array", test_cogl_primitive);

  UNPORTED_TEST ("/cogl/shaders", test_cogl_just_vertex_shader);

  /* left to the end because they aren't currently very orthogonal and tend to
   * break subsequent tests! */
  UNPORTED_TEST ("/cogl", test_cogl_viewport);
  UNPORTED_TEST ("/cogl", test_cogl_offscreen);

  return g_test_run ();
}
