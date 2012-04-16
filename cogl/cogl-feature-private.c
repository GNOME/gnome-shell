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

#include "cogl-context-private.h"

#include "cogl-feature-private.h"
#include "cogl-renderer-private.h"

CoglBool
_cogl_feature_check (CoglRenderer *renderer,
                     const char *driver_prefix,
                     const CoglFeatureData *data,
                     int gl_major,
                     int gl_minor,
                     CoglDriver driver,
                     const char *extensions_string,
                     void *function_table)

{
  const char *suffix = NULL;
  int func_num;

  /* First check whether the functions should be directly provided by
     GL */
  if ((driver == COGL_DRIVER_GL &&
       COGL_CHECK_GL_VERSION (gl_major, gl_minor,
                              data->min_gl_major, data->min_gl_minor)) ||
      (driver == COGL_DRIVER_GLES1 &&
       (data->gles_availability & COGL_EXT_IN_GLES)) ||
      (driver == COGL_DRIVER_GLES2 &&
       (data->gles_availability & COGL_EXT_IN_GLES2)))
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
              g_string_assign (full_extension_name, driver_prefix);
              g_string_append_c (full_extension_name, '_');
              g_string_append_len (full_extension_name,
                                   namespace, namespace_len);
              g_string_append_c (full_extension_name, '_');
              g_string_append (full_extension_name, extension);
              if (_cogl_check_extension (full_extension_name->str,
                                         extensions_string))
                break;
            }

          g_string_free (full_extension_name, TRUE);

          /* If we found an extension with this namespace then use it
             as the suffix */
          if (*extension)
            {
              suffix = namespace_suffix;
              break;
            }
        }
    }

  /* If we couldn't find anything that provides the functions then
     give up */
  if (suffix == NULL)
    goto error;

  /* Try to get all of the entry points */
  for (func_num = 0; data->functions[func_num].name; func_num++)
    {
      void *func;
      char *full_function_name;

      full_function_name = g_strconcat (data->functions[func_num].name,
                                        suffix, NULL);
      func = _cogl_renderer_get_proc_address (renderer, full_function_name);
      g_free (full_function_name);

      if (func == NULL)
        goto error;

      /* Set the function pointer in the context */
      *(void **) ((uint8_t *) function_table +
                  data->functions[func_num].pointer_offset) = func;
    }

  return TRUE;

  /* If the extension isn't found or one of the functions wasn't found
   * then set all of the functions pointers to NULL so Cogl can safely
   * do feature testing by just looking at the function pointers */
error:
  for (func_num = 0; data->functions[func_num].name; func_num++)
    *(void **) ((uint8_t *) function_table +
                data->functions[func_num].pointer_offset) = NULL;

  return FALSE;
}

/* Define a set of arrays containing the functions required from GL
   for each feature */
#define COGL_EXT_BEGIN(name,                                            \
                       min_gl_major, min_gl_minor,                      \
                       gles_availability,                               \
                       namespaces, extension_names)                     \
  static const CoglFeatureFunction cogl_ext_ ## name ## _funcs[] = {
#define COGL_EXT_FUNCTION(ret, name, args)                          \
  { G_STRINGIFY (name), G_STRUCT_OFFSET (CoglContext, name) },
#define COGL_EXT_END()                      \
  { NULL, 0 },                                  \
  };
#include "gl-prototypes/cogl-all-functions.h"

/* Define an array of features */
#undef COGL_EXT_BEGIN
#define COGL_EXT_BEGIN(name,                                            \
                       min_gl_major, min_gl_minor,                      \
                       gles_availability,                               \
                       namespaces, extension_names)                     \
  { min_gl_major, min_gl_minor, gles_availability, namespaces,          \
      extension_names, 0, 0, 0,                                         \
    cogl_ext_ ## name ## _funcs },
#undef COGL_EXT_FUNCTION
#define COGL_EXT_FUNCTION(ret, name, args)
#undef COGL_EXT_END
#define COGL_EXT_END()

static const CoglFeatureData
cogl_feature_ext_functions_data[] =
  {
#include "gl-prototypes/cogl-all-functions.h"
  };

void
_cogl_feature_check_ext_functions (CoglContext *context,
                                   int gl_major,
                                   int gl_minor,
                                   const char *gl_extensions)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (cogl_feature_ext_functions_data); i++)
    _cogl_feature_check (context->display->renderer,
                         "GL", cogl_feature_ext_functions_data + i,
                         gl_major, gl_minor, context->driver,
                         gl_extensions,
                         context);
}
