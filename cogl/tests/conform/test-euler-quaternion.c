#include <cogl/cogl.h>
#include <math.h>
#include <string.h>

#include "test-utils.h"

/* Macros are used here instead of functions so that the
 * g_assert_cmpfloat will give a more interesting message when it
 * fails */

#define COMPARE_FLOATS(a, b)                    \
  do {                                          \
    if (fabsf ((a) - (b)) >= 0.0001f)           \
      g_assert_cmpfloat ((a), ==, (b));         \
  } while (0)

#define COMPARE_MATRICES(a, b) \
  do {                                          \
    COMPARE_FLOATS ((a)->xx, (b)->xx);          \
    COMPARE_FLOATS ((a)->yx, (b)->yx);          \
    COMPARE_FLOATS ((a)->zx, (b)->zx);          \
    COMPARE_FLOATS ((a)->wx, (b)->wx);          \
    COMPARE_FLOATS ((a)->xy, (b)->xy);          \
    COMPARE_FLOATS ((a)->yy, (b)->yy);          \
    COMPARE_FLOATS ((a)->zy, (b)->zy);          \
    COMPARE_FLOATS ((a)->wy, (b)->wy);          \
    COMPARE_FLOATS ((a)->xz, (b)->xz);          \
    COMPARE_FLOATS ((a)->yz, (b)->yz);          \
    COMPARE_FLOATS ((a)->zz, (b)->zz);          \
    COMPARE_FLOATS ((a)->wz, (b)->wz);          \
    COMPARE_FLOATS ((a)->xw, (b)->xw);          \
    COMPARE_FLOATS ((a)->yw, (b)->yw);          \
    COMPARE_FLOATS ((a)->zw, (b)->zw);          \
    COMPARE_FLOATS ((a)->ww, (b)->ww);          \
  } while (0)

void
test_euler_quaternion (void)
{
  CoglEuler euler;
  CoglQuaternion quaternion;
  CoglMatrix matrix_a, matrix_b;

  /* Try doing the rotation with three separate rotations */
  cogl_matrix_init_identity (&matrix_a);
  cogl_matrix_rotate (&matrix_a, -30.0f, 0.0f, 1.0f, 0.0f);
  cogl_matrix_rotate (&matrix_a, 40.0f, 1.0f, 0.0f, 0.0f);
  cogl_matrix_rotate (&matrix_a, 50.0f, 0.0f, 0.0f, 1.0f);

  /* And try the same rotation with a euler */
  cogl_euler_init (&euler, -30, 40, 50);
  cogl_matrix_init_from_euler (&matrix_b, &euler);

  /* Verify that the matrices are approximately the same */
  COMPARE_MATRICES (&matrix_a, &matrix_b);

  /* Try converting the euler to a matrix via a quaternion */
  cogl_quaternion_init_from_euler (&quaternion, &euler);
  memset (&matrix_b, 0, sizeof (matrix_b));
  cogl_matrix_init_from_quaternion (&matrix_b, &quaternion);
  COMPARE_MATRICES (&matrix_a, &matrix_b);

  /* Try applying the rotation from a euler to a framebuffer */
  cogl_framebuffer_identity_matrix (test_fb);
  cogl_framebuffer_rotate_euler (test_fb, &euler);
  memset (&matrix_b, 0, sizeof (matrix_b));
  cogl_framebuffer_get_modelview_matrix (test_fb, &matrix_b);
  COMPARE_MATRICES (&matrix_a, &matrix_b);

  /* And again with a quaternion */
  cogl_framebuffer_identity_matrix (test_fb);
  cogl_framebuffer_rotate_quaternion (test_fb, &quaternion);
  memset (&matrix_b, 0, sizeof (matrix_b));
  cogl_framebuffer_get_modelview_matrix (test_fb, &matrix_b);
  COMPARE_MATRICES (&matrix_a, &matrix_b);

  /* FIXME: This needs a lot more tests! */

  if (cogl_test_verbose ())
    g_print ("OK\n");
}
