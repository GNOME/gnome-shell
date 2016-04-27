/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011, 2013 Intel Corporation.
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
  GList *first_snippet, *l;
  CoglSnippet *snippet;
  int snippet_num = 0;
  int n_snippets = 0;

  first_snippet = data->snippets->entries;

  /* First count the number of snippets so we can easily tell when
     we're at the last one */
  for (l = data->snippets->entries; l; l = l->next)
    {
      snippet = l->data;

      if (snippet->hook == data->hook)
        {
          /* Don't bother processing any previous snippets if we reach
             one that has a replacement */
          if (snippet->replace)
            {
              n_snippets = 1;
              first_snippet = l;
            }
          else
            n_snippets++;
        }
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

  for (l = first_snippet; snippet_num < n_snippets; l = l->next)
    {
      snippet = l->data;

      if (snippet->hook == data->hook)
        {
          const char *source;

          if ((source = cogl_snippet_get_declarations (snippet)))
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

          if ((source = cogl_snippet_get_pre (snippet)))
            g_string_append (data->source_buf, source);

          /* Chain on to the next function, or bypass it if there is
             a replace string */
          if ((source = cogl_snippet_get_replace (snippet)))
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

          if ((source = cogl_snippet_get_post (snippet)))
            g_string_append (data->source_buf, source);

          if (data->return_type)
            g_string_append_printf (data->source_buf,
                                    "  return %s;\n",
                                    data->return_variable);

          g_string_append (data->source_buf, "}\n");
          snippet_num++;
        }
    }
}

void
_cogl_pipeline_snippet_generate_declarations (GString *declarations_buf,
                                              CoglSnippetHook hook,
                                              CoglPipelineSnippetList *snippets)
{
  GList *l;

  for (l = snippets->entries; l; l = l->next)
    {
      CoglSnippet *snippet = l->data;

      if (snippet->hook == hook)
        {
          const char *source;

          if ((source = cogl_snippet_get_declarations (snippet)))
            g_string_append (declarations_buf, source);
        }
    }
}

void
_cogl_pipeline_snippet_list_free (CoglPipelineSnippetList *list)
{
  GList *l, *tmp;

  for (l = list->entries; l; l = tmp)
    {
      tmp = l->next;

      cogl_object_unref (l->data);
      g_list_free_1 (l);
    }
}

void
_cogl_pipeline_snippet_list_add (CoglPipelineSnippetList *list,
                                 CoglSnippet *snippet)
{
  list->entries = g_list_append (list->entries, cogl_object_ref (snippet));

  _cogl_snippet_make_immutable (snippet);
}

void
_cogl_pipeline_snippet_list_copy (CoglPipelineSnippetList *dst,
                                  const CoglPipelineSnippetList *src)
{
  GQueue queue = G_QUEUE_INIT;
  const GList *l;

  for (l = src->entries; l; l = l->next)
    g_queue_push_tail (&queue, cogl_object_ref (l->data));

  dst->entries = queue.head;
}

void
_cogl_pipeline_snippet_list_hash (CoglPipelineSnippetList *list,
                                  unsigned int *hash)
{
  GList *l;

  for (l = list->entries; l; l = l->next)
    {
      CoglSnippet *snippet = l->data;

      *hash = _cogl_util_one_at_a_time_hash (*hash,
                                             &snippet,
                                             sizeof (CoglSnippet *));
    }
}

CoglBool
_cogl_pipeline_snippet_list_equal (CoglPipelineSnippetList *list0,
                                   CoglPipelineSnippetList *list1)
{
  GList *l0, *l1;

  for (l0 = list0->entries, l1 = list1->entries;
       l0 && l1;
       l0 = l0->next, l1 = l1->next)
    if (l0->data != l1->data)
      return FALSE;

  return l0 == NULL && l1 == NULL;
}
