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
  int status;

  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  context = g_object_new (GJS_TYPE_CONTEXT,
                          "search-path", search_path,
                          NULL);

  if (!gjs_context_define_string_array(context, "ARGV",
                                       argc, (const char**)argv,
                                       &error))
    {
      g_message("Failed to define ARGV: %s", error->message);
      g_error_free (error);
      g_object_unref (context);

      return 1;
    }


  if (!gjs_context_eval (context,
                         "const Main = imports.portalHelper.main; Main.main(ARGV);",
                         -1,
                         "<main>",
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
