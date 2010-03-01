/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009 Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl.h"
#include "cogl-context.h"

#include "cogl-feature-private.h"

gboolean
_cogl_feature_check (const CoglFeatureData *data,
                     unsigned int gl_major,
                     unsigned int gl_minor,
                     const char *extensions_string)

{
  const char *suffix = NULL;
  int func_num;

  _COGL_GET_CONTEXT (ctx, FALSE);

  /* First check whether the functions should be directly provided by
     GL */
  if (COGL_CHECK_GL_VERSION (gl_major, gl_minor,
                             data->min_gl_major, data->min_gl_minor))
    suffix = "";
  else
    {
      /* Otherwise try all of the extensions */
      const char *namespace, *namespace_suffix;
      unsigned int namespace_len;

      for (namespace = data->namespaces;
           *namespace;
           namespace += strlen (namespace) + 1)
        {
          const char *extension;
          GString *full_extension_name = g_string_new ("");

          /* If the namespace part contains a ':' then the suffix for
             the function names is different from the name space */
          if ((namespace_suffix = strchr (namespace, ':')))
            {
              namespace_len = namespace_suffix - namespace;
              namespace_suffix++;
            }
          else
            {
              namespace_len = strlen (namespace);
              namespace_suffix = namespace;
            }

          for (extension = data->extension_names;
               *extension;
               extension += strlen (extension) + 1)
            {
              g_string_set_size (full_extension_name, 0);
              g_string_append (full_extension_name, "GL_");
              g_string_append_len (full_extension_name,
                                   namespace, namespace_len);
              g_string_append_c (full_extension_name, '_');
              g_string_append (full_extension_name, extension);
              if (!_cogl_check_extension (full_extension_name->str,
                                          extensions_string))
                break;
            }

          g_string_free (full_extension_name, TRUE);

          /* If we found all of the extensions with this namespace
             then use it as the suffix */
          if (*extension == '\0')
            {
              suffix = namespace_suffix;
              break;
            }
        }
    }

  /* If we couldn't find anything that provides the functions then
     give up */
  if (suffix == NULL)
    return FALSE;

  /* Try to get all of the entry points */
  for (func_num = 0; data->functions[func_num].name; func_num++)
    {
      void *func;
      char *full_function_name;

      full_function_name = g_strconcat (data->functions[func_num].name,
                                        suffix, NULL);
      func = cogl_get_proc_address (full_function_name);
      g_free (full_function_name);

      if (func == NULL)
        break;

      /* Set the function pointer in the context */
      *(void **) ((guint8 *) ctx +
                  data->functions[func_num].pointer_offset) = func;
    }

  /* If one of the functions wasn't found then we should set all of
     the function pointers back to NULL so that the rest of Cogl can
     safely do feature testing by just looking at the function
     pointers */
  if (data->functions[func_num].name)
    {
      while (func_num-- > 0)
        *(void **) ((guint8 *) ctx +
                    data->functions[func_num].pointer_offset) = NULL;
      return FALSE;
    }
  else
    return TRUE;
}
