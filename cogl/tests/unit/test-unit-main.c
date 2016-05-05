#include "cogl-config.h"

#include <gmodule.h>

#include <test-fixtures/test-unit.h>
#include <stdlib.h>

int
main (int argc, char **argv)
{
  GModule *main_module;
  const CoglUnitTest *unit_test;
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

  main_module = g_module_open (NULL, /* use main module */
                               0 /* flags */);

  if (!g_module_symbol (main_module, argv[1], (void **) &unit_test))
    {
      g_printerr ("Unknown test name \"%s\"\n", argv[1]);
      return 1;
    }

  test_utils_init (unit_test->requirement_flags,
                   unit_test->known_failure_flags);
  unit_test->run ();
  test_utils_fini ();

  return 0;
}
