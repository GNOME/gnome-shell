/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifndef __COGL_SNIPPET_PRIVATE_H
#define __COGL_SNIPPET_PRIVATE_H

#include <glib.h>

#include "cogl-snippet.h"
#include "cogl-object-private.h"

/* These values are also used in the enum for CoglSnippetHook. They
   are copied here because we don't really want these names to be part
   of the public API */
#define COGL_SNIPPET_HOOK_BAND_SIZE 2048
#define COGL_SNIPPET_FIRST_PIPELINE_HOOK 0
#define COGL_SNIPPET_FIRST_PIPELINE_VERTEX_HOOK \
  COGL_SNIPPET_FIRST_PIPELINE_HOOK
#define COGL_SNIPPET_FIRST_PIPELINE_FRAGMENT_HOOK \
  (COGL_SNIPPET_FIRST_PIPELINE_VERTEX_HOOK + COGL_SNIPPET_HOOK_BAND_SIZE)
#define COGL_SNIPPET_FIRST_LAYER_HOOK (COGL_SNIPPET_HOOK_BAND_SIZE * 2)
#define COGL_SNIPPET_FIRST_LAYER_VERTEX_HOOK COGL_SNIPPET_FIRST_LAYER_HOOK
#define COGL_SNIPPET_FIRST_LAYER_FRAGMENT_HOOK \
  (COGL_SNIPPET_FIRST_LAYER_VERTEX_HOOK + COGL_SNIPPET_HOOK_BAND_SIZE)

struct _CoglSnippet
{
  CoglObject _parent;

  CoglSnippetHook hook;

  /* This is set to TRUE the first time the snippet is attached to the
     pipeline. After that any attempts to modify the snippet will be
     ignored. */
  CoglBool immutable;

  char *declarations;
  char *pre;
  char *replace;
  char *post;
};

void
_cogl_snippet_make_immutable (CoglSnippet *snippet);

#endif /* __COGL_SNIPPET_PRIVATE_H */

