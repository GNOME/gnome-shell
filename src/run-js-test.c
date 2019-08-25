/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Based on gjs/console.c from GJS
 *
 * Copyright (c) 2008  litl, LLC
 * Copyright (c) 2010  Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "config.h"

#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include <girepository.h>
#include <gjs/gjs.h>

#include "shell-global.h"
#include "shell-global-private.h"

static char *command = NULL;

static GOptionEntry entries[] = {
  { "command", 'c', 0, G_OPTION_ARG_STRING, &command, "Program passed in as a string", "COMMAND" },
  { NULL }
};

int
main(int argc, char **argv)
{
  GOptionContext *context;
  GError *error = NULL;
  ShellGlobal *global;
  GjsContext *js_context;
  char *script;
  const char *filename;
  char *title;
  gsize len;
  int code;

  context = g_option_context_new (NULL);

  /* pass unknown through to the JS script */
  g_option_context_set_ignore_unknown_options (context, TRUE);

  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    g_error ("option parsing failed: %s", error->message);

  setlocale (LC_ALL, "");

  _shell_global_init (NULL);
  global = shell_global_get ();
  js_context = _shell_global_get_gjs_context (global);

  /* prepare command line arguments */
  if (!gjs_context_define_string_array (js_context, "ARGV",
                                        argc - 2, (const char**)argv + 2,
                                        &error)) {
    g_printerr ("Failed to defined ARGV: %s", error->message);
    exit (1);
  }

  if (command != NULL) {
    script = command;
    len = strlen (script);
    filename = "<command line>";
  } else if (argc <= 1) {
    script = g_strdup ("const Console = imports.console; Console.interact();");
    len = strlen (script);
    filename = "<stdin>";
  } else /*if (argc >= 2)*/ {
    error = NULL;
    if (!g_file_get_contents (argv[1], &script, &len, &error)) {
      g_printerr ("%s\n", error->message);
      exit (1);
    }
    filename = argv[1];
  }

  title = g_filename_display_basename (filename);
  g_set_prgname (title);
  g_free (title);

  /* evaluate the script */
  error = NULL;
  if (!gjs_context_eval (js_context, script, len,
                         filename, &code, &error)) {
    g_free (script);
    g_printerr ("%s\n", error->message);
    exit (1);
  }

  gjs_context_gc (js_context);
  gjs_context_gc (js_context);

  g_object_unref (js_context);
  g_free (script);
  exit (code);
}
