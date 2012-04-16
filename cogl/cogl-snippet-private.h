/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
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

