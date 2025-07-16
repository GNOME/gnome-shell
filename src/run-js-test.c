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

#include <girepository/girepository.h>
#include <gjs/gjs.h>

#include "shell-global.h"
#include "shell-global-private.h"

static int
eval_module (GjsContext  *js_context,
             const char  *filename,
             GError     **error)
{
  g_autoptr (GError) local_error = NULL;
  g_autoptr (GFile) file = NULL;
  g_autofree char *uri = NULL;
  uint8_t code;

  file = g_file_new_for_commandline_arg (filename);
  uri = g_file_get_uri (file);

  if (!gjs_context_register_module (js_context, uri, uri, error))
    return 1;

  if (!gjs_context_eval_module (js_context, uri, &code, &local_error))
    {
      if (!g_error_matches (local_error, GJS_ERROR, GJS_ERROR_SYSTEM_EXIT))
        g_propagate_error (error, g_steal_pointer (&local_error));
    }
  return code;
}

int
main (int argc, char **argv)
{
  GOptionContext *context;
  g_autoptr (GIRepository) repo = NULL;
  g_autoptr (GError) error = NULL;
  ShellGlobal *global;
  GjsContext *js_context;
  const char *filename;
  g_autofree char *title = NULL;;
  uint8_t code;

  context = g_option_context_new (NULL);

  /* pass unknown through to the JS script */
  g_option_context_set_ignore_unknown_options (context, TRUE);

  if (!g_option_context_parse (context, &argc, &argv, &error))
    g_error ("option parsing failed: %s", error->message);

  setlocale (LC_ALL, "");

  _shell_global_init (NULL);
  global = shell_global_get ();
  js_context = _shell_global_get_gjs_context (global);

  repo = gi_repository_dup_default ();

  gi_repository_prepend_search_path (repo, MUTTER_TYPELIB_DIR);
  gi_repository_prepend_search_path (repo, SHELL_TYPELIB_DIR);

  if (argc < 2)
    {
      g_printerr ("Missing filename\n");
      exit (1);
    }

  /* prepare command line arguments */
  gjs_context_set_argv (js_context, argc - 2, (const char**)argv + 2);

  filename = argv[1];
  title = g_filename_display_basename (filename);
  g_set_prgname (title);

  code = eval_module (js_context, filename, &error);
  if (error)
    g_printerr ("%s\n", error->message);

  gjs_context_gc (js_context);
  gjs_context_gc (js_context);

  g_object_unref (js_context);
  exit (code);
}
