#include <config.h>

#include <dlfcn.h>

#include <test-fixtures/test-unit.h>

int
main (int argc, char **argv)
{
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

  unit_test = dlsym (RTLD_DEFAULT, argv[1]);
  if (!unit_test)
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
