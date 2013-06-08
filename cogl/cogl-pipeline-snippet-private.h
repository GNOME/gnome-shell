/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011, 2013 Intel Corporation.
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

#ifndef __COGL_PIPELINE_SNIPPET_PRIVATE_H
#define __COGL_PIPELINE_SNIPPET_PRIVATE_H

#include <glib.h>

#include "cogl-snippet.h"

typedef struct
{
  GList *entries;
} CoglPipelineSnippetList;

/* Arguments to pass to _cogl_pipeline_snippet_generate_code() */
typedef struct
{
  CoglPipelineSnippetList *snippets;

  /* Only snippets at this hook point will be used */
  CoglSnippetHook hook;

  /* The final function to chain on to after all of the snippets code
     has been run */
  const char *chain_function;

  /* The name of the final generated function */
  const char *final_name;

  /* A prefix to insert before each generate function name */
  const char *function_prefix;

  /* The return type of all of the functions, or NULL to use void */
  const char *return_type;

  /* A variable to return from the functions. The snippets are
     expected to modify this variable. Ignored if return_type is
     NULL */
  const char *return_variable;

  /* If this is TRUE then it won't allocate a separate variable for
     the return value. Instead it is expected that the snippet will
     modify one of the argument variables directly and that will be
     returned */
  CoglBool return_variable_is_argument;

  /* The argument names or NULL if there are none */
  const char *arguments;

  /* The argument types or NULL */
  const char *argument_declarations;

  /* The string to generate the source into */
  GString *source_buf;
} CoglPipelineSnippetData;

void
_cogl_pipeline_snippet_generate_code (const CoglPipelineSnippetData *data);

void
_cogl_pipeline_snippet_generate_declarations (GString *declarations_buf,
                                              CoglSnippetHook hook,
                                              CoglPipelineSnippetList *list);

void
_cogl_pipeline_snippet_list_free (CoglPipelineSnippetList *list);

void
_cogl_pipeline_snippet_list_add (CoglPipelineSnippetList *list,
                                 CoglSnippet *snippet);

void
_cogl_pipeline_snippet_list_copy (CoglPipelineSnippetList *dst,
                                  const CoglPipelineSnippetList *src);

void
_cogl_pipeline_snippet_list_hash (CoglPipelineSnippetList *list,
                                  unsigned int *hash);

CoglBool
_cogl_pipeline_snippet_list_equal (CoglPipelineSnippetList *list0,
                                   CoglPipelineSnippetList *list1);

#endif /* __COGL_PIPELINE_SNIPPET_PRIVATE_H */

