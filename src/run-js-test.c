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

int
main(int argc, char **argv)
{
  GOptionContext *context;
  GError *error = NULL;
  ShellGlobal *global;
  GjsContext *js_context;
  const char *filename;
  char *title;
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

  /* prepare command line arguments */
  if (!gjs_context_define_string_array (js_context, "ARGV",
                                        argc - 2, (const char**)argv + 2,
                                        &error)) {
    g_printerr ("Failed to defined ARGV: %s", error->message);
    exit (1);
  }

  if (argc < 2) {
    g_printerr ("Missing filename");
    exit(1);
  }

  filename = argv[1];
  title = g_filename_display_basename (filename);
  g_set_prgname (title);
  g_free (title);

  error = NULL;

  /* evaluate the script */
  bool success = gjs_context_eval_module_file(js_context, filename, &code, &error);
  if (!success) {
    g_printerr ("%s\n", error->message);
    exit (1);
  }

  gjs_context_gc (js_context);
  gjs_context_gc (js_context);

  g_object_unref (js_context);
  exit (code);
}
