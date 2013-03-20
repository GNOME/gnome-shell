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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-types.h"
#include "cogl-pipeline-snippet-private.h"
#include "cogl-snippet-private.h"
#include "cogl-util.h"

/* Helper functions that are used by both GLSL pipeline backends */

void
_cogl_pipeline_snippet_generate_code (const CoglPipelineSnippetData *data)
{
  CoglPipelineSnippet *first_snippet, *snippet;
  int snippet_num = 0;
  int n_snippets = 0;

  first_snippet = COGL_LIST_FIRST (data->snippets);

  /* First count the number of snippets so we can easily tell when
     we're at the last one */
  COGL_LIST_FOREACH (snippet, data->snippets, list_node)
    if (snippet->snippet->hook == data->hook)
      {
        /* Don't bother processing any previous snippets if we reach
           one that has a replacement */
        if (snippet->snippet->replace)
          {
            n_snippets = 1;
            first_snippet = snippet;
          }
        else
          n_snippets++;
      }

  /* If there weren't any snippets then generate a stub function with
     the final name */
  if (n_snippets == 0)
    {
      if (data->return_type)
        g_string_append_printf (data->source_buf,
                                "\n"
                                "%s\n"
                                "%s (%s)\n"
                                "{\n"
                                "  return %s (%s);\n"
                                "}\n",
                                data->return_type,
                                data->final_name,
                                data->argument_declarations ?
                                data->argument_declarations : "",
                                data->chain_function,
                                data->arguments ? data->arguments : "");
      else
        g_string_append_printf (data->source_buf,
                                "\n"
                                "void\n"
                                "%s (%s)\n"
                                "{\n"
                                "  %s (%s);\n"
                                "}\n",
                                data->final_name,
                                data->argument_declarations ?
                                data->argument_declarations : "",
                                data->chain_function,
                                data->arguments ? data->arguments : "");

      return;
    }

  for (snippet = first_snippet, snippet_num = 0;
       snippet_num < n_snippets;
       snippet = COGL_LIST_NEXT (snippet, list_node))
    if (snippet->snippet->hook == data->hook)
      {
        const char *source;

        if ((source = cogl_snippet_get_declarations (snippet->snippet)))
          g_string_append (data->source_buf, source);

        g_string_append_printf (data->source_buf,
                                "\n"
                                "%s\n",
                                data->return_type ?
                                data->return_type :
                                "void");

        if (snippet_num + 1 < n_snippets)
          g_string_append_printf (data->source_buf,
                                  "%s_%i",
                                  data->function_prefix,
                                  snippet_num);
        else
          g_string_append (data->source_buf, data->final_name);

        g_string_append (data->source_buf, " (");

        if (data->argument_declarations)
          g_string_append (data->source_buf, data->argument_declarations);

        g_string_append (data->source_buf,
                         ")\n"
                         "{\n");

        if (data->return_type && !data->return_variable_is_argument)
          g_string_append_printf (data->source_buf,
                                  "  %s %s;\n"
                                  "\n",
                                  data->return_type,
                                  data->return_variable);

        if ((source = cogl_snippet_get_pre (snippet->snippet)))
          g_string_append (data->source_buf, source);

        /* Chain on to the next function, or bypass it if there is
           a replace string */
        if ((source = cogl_snippet_get_replace (snippet->snippet)))
          g_string_append (data->source_buf, source);
        else
          {
            g_string_append (data->source_buf, "  ");

            if (data->return_type)
              g_string_append_printf (data->source_buf,
                                      "%s = ",
                                      data->return_variable);

            if (snippet_num > 0)
              g_string_append_printf (data->source_buf,
                                      "%s_%i",
                                      data->function_prefix,
                                      snippet_num - 1);
            else
              g_string_append (data->source_buf, data->chain_function);

            g_string_append (data->source_buf, " (");

            if (data->arguments)
              g_string_append (data->source_buf, data->arguments);

            g_string_append (data->source_buf, ");\n");
          }

        if ((source = cogl_snippet_get_post (snippet->snippet)))
          g_string_append (data->source_buf, source);

        if (data->return_type)
          g_string_append_printf (data->source_buf,
                                  "  return %s;\n",
                                  data->return_variable);

        g_string_append (data->source_buf, "}\n");
        snippet_num++;
      }
}

void
_cogl_pipeline_snippet_generate_declarations (GString *declarations_buf,
                                              CoglSnippetHook hook,
                                              CoglPipelineSnippetList *snippets)
{
  CoglPipelineSnippet *snippet;

  COGL_LIST_FOREACH (snippet, snippets, list_node)
    if (snippet->snippet->hook == hook)
      {
        const char *source;

        if ((source = cogl_snippet_get_declarations (snippet->snippet)))
          g_string_append (declarations_buf, source);
      }
}

static void
_cogl_pipeline_snippet_free (CoglPipelineSnippet *pipeline_snippet)
{
  cogl_object_unref (pipeline_snippet->snippet);
  g_slice_free (CoglPipelineSnippet, pipeline_snippet);
}

void
_cogl_pipeline_snippet_list_free (CoglPipelineSnippetList *list)
{
  CoglPipelineSnippet *pipeline_snippet, *tmp;

  COGL_LIST_FOREACH_SAFE (pipeline_snippet, list, list_node, tmp)
    _cogl_pipeline_snippet_free (pipeline_snippet);
}

void
_cogl_pipeline_snippet_list_add (CoglPipelineSnippetList *list,
                                 CoglSnippet *snippet)
{
  CoglPipelineSnippet *pipeline_snippet = g_slice_new (CoglPipelineSnippet);

  pipeline_snippet->snippet = cogl_object_ref (snippet);

  _cogl_snippet_make_immutable (pipeline_snippet->snippet);

  if (COGL_LIST_EMPTY (list))
    COGL_LIST_INSERT_HEAD (list, pipeline_snippet, list_node);
  else
    {
      CoglPipelineSnippet *tail;

      for (tail = COGL_LIST_FIRST (list);
           COGL_LIST_NEXT (tail, list_node);
           tail = COGL_LIST_NEXT (tail, list_node));

      COGL_LIST_INSERT_AFTER (tail, pipeline_snippet, list_node);
    }
}

void
_cogl_pipeline_snippet_list_copy (CoglPipelineSnippetList *dst,
                                  const CoglPipelineSnippetList *src)
{
  CoglPipelineSnippet *tail = NULL;
  const CoglPipelineSnippet *l;

  COGL_LIST_INIT (dst);

  COGL_LIST_FOREACH (l, src, list_node)
    {
      CoglPipelineSnippet *copy = g_slice_dup (CoglPipelineSnippet, l);

      cogl_object_ref (copy->snippet);

      if (tail)
        COGL_LIST_INSERT_AFTER (tail, copy, list_node);
      else
        COGL_LIST_INSERT_HEAD (dst, copy, list_node);

      tail = copy;
    }
}

void
_cogl_pipeline_snippet_list_hash (CoglPipelineSnippetList *list,
                                  unsigned int *hash)
{
  CoglPipelineSnippet *l;

  COGL_LIST_FOREACH (l, list, list_node)
    {
      *hash = _cogl_util_one_at_a_time_hash (*hash,
                                             &l->snippet,
                                             sizeof (CoglSnippet *));
    }
}

CoglBool
_cogl_pipeline_snippet_list_equal (CoglPipelineSnippetList *list0,
                                   CoglPipelineSnippetList *list1)
{
  CoglPipelineSnippet *l0, *l1;

  for (l0 = COGL_LIST_FIRST (list0), l1 = COGL_LIST_FIRST (list1);
       l0 && l1;
       l0 = COGL_LIST_NEXT (l0, list_node), l1 = COGL_LIST_NEXT (l1, list_node))
    if (l0->snippet != l1->snippet)
      return FALSE;

  return l0 == NULL && l1 == NULL;
}
