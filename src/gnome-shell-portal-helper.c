/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include <gjs/gjs.h>
#include <glib/gi18n.h>

int
main (int argc, char *argv[])
{
  const char *search_path[] = { "resource:///org/gnome/shell", NULL };
  GError *error = NULL;
  GjsContext *context;
  uint8_t status;

  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  context = g_object_new (GJS_TYPE_CONTEXT,
                          "program-name", *argv,
                          "search-path", search_path,
                          NULL);

  gjs_context_set_argv(context, argc - 1, (const char**)argv + 1);

  if (!gjs_context_eval_module_file (context,
                                     "resource:///org/gnome/shell/portalHelper/main.js",
                                     &status,
                                     &error))
    {
      g_message ("Execution of main.js threw exception: %s", error->message);
      g_error_free (error);
      g_object_unref (context);

      return status;
    }

  g_object_unref (context);
  return 0;
}
