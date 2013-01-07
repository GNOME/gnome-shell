/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
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
 *   Robert Bragg <robert@linux.intel.com>
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-context-private.h"
#include "cogl-glsl-shader-private.h"
#include "cogl-glsl-shader-boilerplate.h"
#include "cogl-internal.h"

#include <string.h>

#include <glib.h>

void
_cogl_glsl_shader_set_source_with_boilerplate (CoglContext *ctx,
                                               GLuint shader_gl_handle,
                                               GLenum shader_gl_type,
                                               int n_tex_coord_attribs,
                                               GLsizei count_in,
                                               const char **strings_in,
                                               const GLint *lengths_in)
{
  const char *vertex_boilerplate;
  const char *fragment_boilerplate;

  const char **strings = g_alloca (sizeof (char *) * (count_in + 3));
  GLint *lengths = g_alloca (sizeof (GLint) * (count_in + 3));
  int count = 0;
  char *tex_coord_declarations = NULL;

  if (ctx->driver == COGL_DRIVER_GLES2)
    {
      vertex_boilerplate = _COGL_VERTEX_SHADER_BOILERPLATE_GLES2;
      fragment_boilerplate = _COGL_FRAGMENT_SHADER_BOILERPLATE_GLES2;
    }
  else
    {
      vertex_boilerplate = _COGL_VERTEX_SHADER_BOILERPLATE_GL;
      fragment_boilerplate = _COGL_FRAGMENT_SHADER_BOILERPLATE_GL;
    }

  if (ctx->driver == COGL_DRIVER_GLES2 &&
      cogl_has_feature (ctx, COGL_FEATURE_ID_TEXTURE_3D))
    {
      static const char texture_3d_extension[] =
        "#extension GL_OES_texture_3D : enable\n";
      strings[count] = texture_3d_extension;
      lengths[count++] = sizeof (texture_3d_extension) - 1;
    }

  if (shader_gl_type == GL_VERTEX_SHADER)
    {
      strings[count] = vertex_boilerplate;
      lengths[count++] = strlen (vertex_boilerplate);
    }
  else if (shader_gl_type == GL_FRAGMENT_SHADER)
    {
      strings[count] = fragment_boilerplate;
      lengths[count++] = strlen (fragment_boilerplate);
    }

  if (ctx->driver == COGL_DRIVER_GLES2 &&
      n_tex_coord_attribs)
    {
      GString *declarations = g_string_new (NULL);

      g_string_append_printf (declarations,
                              "varying vec4 _cogl_tex_coord[%d];\n",
                              n_tex_coord_attribs);

      if (shader_gl_type == GL_VERTEX_SHADER)
        {
          int i;

          g_string_append_printf (declarations,
                                  "uniform mat4 cogl_texture_matrix[%d];\n",
                                  n_tex_coord_attribs);

          for (i = 0; i < n_tex_coord_attribs; i++)
            g_string_append_printf (declarations,
                                    "attribute vec4 cogl_tex_coord%d_in;\n",
                                    i);
        }

      tex_coord_declarations = g_string_free (declarations, FALSE);
      strings[count] = tex_coord_declarations;
      lengths[count++] = -1; /* null terminated */
    }

  memcpy (strings + count, strings_in, sizeof (char *) * count_in);
  if (lengths_in)
    memcpy (lengths + count, lengths_in, sizeof (GLint) * count_in);
  else
    {
      int i;

      for (i = 0; i < count_in; i++)
        lengths[count + i] = -1; /* null terminated */
    }
  count += count_in;

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_SHOW_SOURCE)))
    {
      GString *buf = g_string_new (NULL);
      int i;

      g_string_append_printf (buf,
                              "%s shader:\n",
                              shader_gl_type == GL_VERTEX_SHADER ?
                              "vertex" : "fragment");
      for (i = 0; i < count; i++)
        if (lengths[i] != -1)
          g_string_append_len (buf, strings[i], lengths[i]);
        else
          g_string_append (buf, strings[i]);

      g_message ("%s", buf->str);

      g_string_free (buf, TRUE);
    }

  GE( ctx, glShaderSource (shader_gl_handle, count,
                           (const char **) strings, lengths) );

  g_free (tex_coord_declarations);
}
