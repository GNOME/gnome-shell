
#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <string.h>

#include "test-conform-common.h"

CoglUserDataKey private_key0;
CoglUserDataKey private_key1;
CoglUserDataKey private_key2;

static int user_data0;
static int user_data1;
static int user_data2;

static int destroy0_count = 0;
static int destroy1_count = 0;
static int destroy2_count = 0;

static void
destroy0_cb (void *user_data)
{
  g_assert (user_data == &user_data0);
  destroy0_count++;
}

static void
destroy1_cb (void *user_data)
{
  g_assert (user_data == &user_data1);
  destroy1_count++;
}

static void
destroy2_cb (void *user_data)
{
  g_assert (user_data == &user_data2);
  destroy2_count++;
}

void
test_object (TestUtilsGTestFixture *fixture,
                  void *data)
{
  CoglPath *path;

  /* Assuming that COGL_OBJECT_N_PRE_ALLOCATED_USER_DATA_ENTRIES == 2
   * test associating 2 pointers to private data with an object */
  cogl_path_new ();
  path = cogl_get_path ();

  cogl_object_set_user_data (COGL_OBJECT (path),
                             &private_key0,
                             &user_data0,
                             destroy0_cb);

  cogl_object_set_user_data (COGL_OBJECT (path),
                             &private_key1,
                             &user_data1,
                             destroy1_cb);

  cogl_object_set_user_data (COGL_OBJECT (path),
                             &private_key2,
                             &user_data2,
                             destroy2_cb);

  cogl_object_set_user_data (COGL_OBJECT (path),
                             &private_key1,
                             NULL,
                             destroy1_cb);

  cogl_object_set_user_data (COGL_OBJECT (path),
                             &private_key1,
                             &user_data1,
                             destroy1_cb);

  cogl_object_unref (path);

  g_assert_cmpint (destroy0_count, ==, 1);
  g_assert_cmpint (destroy1_count, ==, 2);
  g_assert_cmpint (destroy2_count, ==, 1);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}

