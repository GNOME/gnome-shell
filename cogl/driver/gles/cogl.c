/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-context.h"

typedef struct _CoglGLSymbolTableEntry
{
  const char *name;
  void *ptr;
} CoglGLSymbolTableEntry;

gboolean
cogl_check_extension (const gchar *name, const gchar *ext)
{
  gchar *end;
  gint name_len, n;

  if (name == NULL || ext == NULL)
    return FALSE;

  end = (gchar*)(ext + strlen(ext));

  name_len = strlen(name);

  while (ext < end)
    {
      n = strcspn(ext, " ");

      if ((name_len == n) && (!strncmp(name, ext, n)))
	return TRUE;
      ext += (n + 1);
    }

  return FALSE;
}

gboolean
_cogl_resolve_gl_symbols (CoglGLSymbolTableEntry *symbol_table,
                          const char *suffix)
{
  int i;
  gboolean status = TRUE;
  for (i = 0; symbol_table[i].name; i++)
    {
      char *full_name = g_strdup_printf ("%s%s", symbol_table[i].name, suffix);
      *((CoglFuncPtr *)symbol_table[i].ptr) = cogl_get_proc_address (full_name);
      g_free (full_name);
      if (!*((CoglFuncPtr *)symbol_table[i].ptr))
        {
          status = FALSE;
          break;
        }
    }
  return status;
}


void
_cogl_features_init (void)
{
  CoglFeatureFlags flags = 0;
  int              max_clip_planes = 0;
  GLint            num_stencil_bits = 0;
  const char      *gl_extensions;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  gl_extensions = (const char*) glGetString (GL_EXTENSIONS);

  if (cogl_check_extension ("GL_OES_framebuffer_object", gl_extensions))
    {
      g_assert (0);
      CoglGLSymbolTableEntry symbol_table[] = {
            {"glGenRenderbuffers", &ctx->drv.pf_glGenRenderbuffers},
            {"glDeleteRenderbuffers", &ctx->drv.pf_glDeleteRenderbuffers},
            {"glBindRenderbuffer", &ctx->drv.pf_glBindRenderbuffer},
            {"glRenderbufferStorage", &ctx->drv.pf_glRenderbufferStorage},
            {"glGenFramebuffers", &ctx->drv.pf_glGenFramebuffers},
            {"glBindFramebuffer", &ctx->drv.pf_glBindFramebuffer},
            {"glFramebufferTexture2D", &ctx->drv.pf_glFramebufferTexture2D},
            {"glFramebufferRenderbuffer", &ctx->drv.pf_glFramebufferRenderbuffer},
            {"glCheckFramebufferStatus", &ctx->drv.pf_glCheckFramebufferStatus},
            {"glDeleteFramebuffers", &ctx->drv.pf_glDeleteFramebuffers},
            {"glGenerateMipmap", &ctx->drv.pf_glGenerateMipmap},
            {NULL, NULL}
      };

      if (_cogl_resolve_gl_symbols (symbol_table, "OES"))
        flags |= COGL_FEATURE_OFFSCREEN;
    }

  GE( glGetIntegerv (GL_STENCIL_BITS, &num_stencil_bits) );
  /* We need at least three stencil bits to combine clips */
  if (num_stencil_bits > 2)
    flags |= COGL_FEATURE_STENCIL_BUFFER;

  GE( glGetIntegerv (GL_MAX_CLIP_PLANES, &max_clip_planes) );
  if (max_clip_planes >= 4)
    flags |= COGL_FEATURE_FOUR_CLIP_PLANES;

#ifdef HAVE_COGL_GLES2
  flags |= COGL_FEATURE_SHADERS_GLSL | COGL_FEATURE_OFFSCREEN;
#endif

  flags |= COGL_FEATURE_VBOS;

  /* Cache features */
  ctx->feature_flags = flags;
  ctx->features_cached = TRUE;
}

