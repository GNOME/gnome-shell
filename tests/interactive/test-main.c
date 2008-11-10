#include <glib.h>
#include <gmodule.h>


int
main (int argc, char **argv)
{
  GModule *module;
  char *unit_test;
  char *main_symbol_name;
  gpointer func;
  int (*unit_test_main) (int argc, char **argv);
  int ret;

  if (argc != 2)
    g_error ("Usage: %s unit_test", argv[0]);
  
  module = g_module_open (NULL, 0);
  if (!module)
    g_error ("Failed to open self for symbol lookup");

  unit_test = g_path_get_basename (argv[1]);

  main_symbol_name = g_strdup_printf ("%s_main", unit_test);
  main_symbol_name = g_strdelimit (main_symbol_name, "-", '_');

  if (!g_module_symbol (module, main_symbol_name, &func))
    g_error ("Failed to look up main symbol for the test: %s", unit_test);

  unit_test_main = func;
  ret = unit_test_main (argc - 1, argv + 1);
  
  g_free (unit_test);
  g_free (main_symbol_name);
  g_module_close (module);

  return ret;
}

