#include <cogl/cogl.h>

/* These will be redefined in config.h */
#undef COGL_ENABLE_EXPERIMENTAL_2_0_API
#undef COGL_ENABLE_EXPERIMENTAL_API

#include "test-utils.h"
#include "config.h"

/* So we can use _COGL_STATIC_ASSERT we include the internal
 * cogl-util.h header. Since internal headers explicitly guard against
 * applications including them directly instead of including
 * <cogl/cogl.h> we define __COGL_H_INSIDE__ here to subvert those
 * guards in this case... */
#define __COGL_H_INSIDE__
#include <cogl/cogl-util.h>
#undef __COGL_H_INSIDE__

_COGL_STATIC_ASSERT (COGL_VERSION_ENCODE (COGL_VERSION_MAJOR,
                                          COGL_VERSION_MINOR,
                                          COGL_VERSION_MICRO) ==
                     COGL_VERSION,
                     "The pre-encoded Cogl version does not match the version "
                     "encoding macro");

_COGL_STATIC_ASSERT (COGL_VERSION_GET_MAJOR (COGL_VERSION_ENCODE (100,
                                                                  200,
                                                                  300)) ==
                     100,
                     "Getting the major component out of a encoded version "
                     "does not work");
_COGL_STATIC_ASSERT (COGL_VERSION_GET_MINOR (COGL_VERSION_ENCODE (100,
                                                                  200,
                                                                  300)) ==
                     200,
                     "Getting the minor component out of a encoded version "
                     "does not work");
_COGL_STATIC_ASSERT (COGL_VERSION_GET_MICRO (COGL_VERSION_ENCODE (100,
                                                                  200,
                                                                  300)) ==
                     300,
                     "Getting the micro component out of a encoded version "
                     "does not work");

_COGL_STATIC_ASSERT (COGL_VERSION_CHECK (COGL_VERSION_MAJOR,
                                         COGL_VERSION_MINOR,
                                         COGL_VERSION_MICRO),
                     "Checking the Cogl version against the current version "
                     "does not pass");
_COGL_STATIC_ASSERT (!COGL_VERSION_CHECK (COGL_VERSION_MAJOR,
                                          COGL_VERSION_MINOR,
                                          COGL_VERSION_MICRO + 1),
                     "Checking the Cogl version against a later micro version "
                     "should not pass");
_COGL_STATIC_ASSERT (!COGL_VERSION_CHECK (COGL_VERSION_MAJOR,
                                          COGL_VERSION_MINOR + 1,
                                          COGL_VERSION_MICRO),
                     "Checking the Cogl version against a later minor version "
                     "should not pass");
_COGL_STATIC_ASSERT (!COGL_VERSION_CHECK (COGL_VERSION_MAJOR + 1,
                                          COGL_VERSION_MINOR,
                                          COGL_VERSION_MICRO),
                     "Checking the Cogl version against a later major version "
                     "should not pass");

_COGL_STATIC_ASSERT (COGL_VERSION_CHECK (COGL_VERSION_MAJOR - 1,
                                         COGL_VERSION_MINOR,
                                         COGL_VERSION_MICRO),
                     "Checking the Cogl version against a older major version "
                     "should pass");

void
test_version (void)
{
  const char *version = g_strdup_printf ("version = %i.%i.%i",
                                         COGL_VERSION_MAJOR,
                                         COGL_VERSION_MINOR,
                                         COGL_VERSION_MICRO);

  g_assert_cmpstr (version, ==, "version = " COGL_VERSION_STRING);

  if (cogl_test_verbose ())
    g_print ("OK\n");
}

