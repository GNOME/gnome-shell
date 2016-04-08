#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gmodule.h>

#include "test-unit-names.h"

#define MAX_DESC_SIZE   72

static GModule *module = NULL;

static gpointer
get_symbol_with_suffix (const char *unit_name,
                        const char *suffix)
{
  char *main_symbol_name;
  gpointer func;

  main_symbol_name = g_strconcat (unit_name, "_", suffix, NULL);
  main_symbol_name = g_strdelimit (main_symbol_name, "-", '_');

  g_module_symbol (module, main_symbol_name, &func);

  g_free (main_symbol_name);

  return func;
}

static gpointer
get_unit_name_main (const char *unit_name)
{
  return get_symbol_with_suffix (unit_name, "main");
}
static char *
get_unit_name_description (const char *unit_name,
                           gssize      max_len)
{
  const char *description;
  gpointer func;
  char *retval;

  func = get_symbol_with_suffix (unit_name, "describe");
  if (func == NULL)
    description = "No description found";
  else
    {
      const char *(* unit_test_describe) (void);

      unit_test_describe = func;

      description = unit_test_describe ();
    }

  if (max_len > 0 && strlen (description) >= max_len)
    {
      GString *buf = g_string_sized_new (max_len);
      char *newline;

      newline = strchr (description, '\n');
      if (newline != NULL)
        {
          g_string_append_len (buf, description,
                               MIN (newline - description - 1, max_len - 3));
        }
      else
        g_string_append_len (buf, description, max_len - 3);

      g_string_append (buf, "...");

      retval = g_string_free (buf, FALSE);
    }
  else
    retval = g_strdup (description);

  return retval;
}

static gboolean list_all = FALSE;
static gboolean describe = FALSE;
static char **unit_names = NULL;

static GOptionEntry entries[] = {
  {
    "describe", 'd',
    0,
    G_OPTION_ARG_NONE, &describe,
    "Describe the interactive unit test", NULL,
  },
  {
    "list-all", 'l',
    0,
    G_OPTION_ARG_NONE, &list_all,
    "List all available units", NULL,
  },
  {
    G_OPTION_REMAINING, 0,
    0,
    G_OPTION_ARG_STRING_ARRAY, &unit_names,
    "The interactive unit test", "UNIT_NAME"
  },
  { NULL }
};

int
main (int argc, char **argv)
{
  int ret, i, n_unit_names;
  GOptionContext *context;

  context = g_option_context_new (" - Interactive test suite");
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_help_enabled (context, TRUE);
  g_option_context_set_ignore_unknown_options (context, TRUE);
  if (!g_option_context_parse (context, &argc, &argv, NULL))
    {
      g_print ("Usage: test-interactive <unit_test>\n");
      return EXIT_FAILURE;
    }

  g_option_context_free (context);

  module = g_module_open (NULL, 0);
  if (!module)
    g_error ("*** Failed to open self for symbol lookup");

  ret = EXIT_SUCCESS;

  if (list_all)
    {
      g_print ("* Available unit tests:\n");

      for (i = 0; i < G_N_ELEMENTS (test_unit_names); i++)
        {
          char *str;
          gsize len;

          len = MAX_DESC_SIZE - strlen (test_unit_names[i]);
          str = get_unit_name_description (test_unit_names[i], len - 2);

          g_print ("  - %s:%*s%s\n",
                   test_unit_names[i],
                   (int) (len - strlen (str)), " ",
                   str);

          g_free (str);
        }

      ret = EXIT_SUCCESS;
      goto out;
    }

  if (unit_names != NULL)
    n_unit_names = g_strv_length (unit_names);
  else
    {
      g_print ("Usage: test-interactive <unit_test>\n");
      ret = EXIT_FAILURE;
      goto out;
    }

  for (i = 0; i < n_unit_names; i++)
    {
      const char *unit_name = unit_names[i];
      char *unit_test = NULL;
      gboolean found;
      int j;

      unit_test = g_path_get_basename (unit_name);

      found = FALSE;
      for (j = 0; j < G_N_ELEMENTS (test_unit_names); j++)
        {
          if (strcmp (test_unit_names[j], unit_test) == 0)
            {
              found = TRUE;
              break;
            }
        }

      if (!found)
        g_error ("*** Unit '%s' does not exist", unit_test);

      if (describe)
        {
          char *str;

          str = get_unit_name_description (unit_test, -1);

          g_print ("* %s:\n%s\n\n", unit_test, str);

          g_free (str);

          ret = EXIT_SUCCESS;
        }
      else
        {
          int (* unit_test_main) (int argc, char **argv);
          gpointer func;

          func = get_unit_name_main (unit_test);
          if (func == NULL)
            g_error ("*** Unable to find the main entry point for '%s'", unit_test);

          unit_test_main = func;

          ret = unit_test_main (n_unit_names, unit_names);

          g_free (unit_test);

          break;
        }

      g_free (unit_test);
    }

out:
  g_module_close (module);

  return ret;
}

