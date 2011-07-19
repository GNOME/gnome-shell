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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-context-private.h"
#include "cogl-feature-private.h"

gboolean
_cogl_gles_update_features (CoglContext *context,
                            GError **error)
{
  CoglPrivateFeatureFlags private_flags = 0;
  CoglFeatureFlags flags = 0;
  const char *gl_extensions;
  int num_stencil_bits = 0;

  /* We have to special case getting the pointer to the glGetString
     function because we need to use it to determine what functions we
     can expect */
  context->glGetString =
    (void *) _cogl_get_proc_address (_cogl_context_get_winsys (context),
                                     "glGetString");

  COGL_NOTE (WINSYS,
             "Checking features\n"
             "  GL_VENDOR: %s\n"
             "  GL_RENDERER: %s\n"
             "  GL_VERSION: %s\n"
             "  GL_EXTENSIONS: %s",
             context->glGetString (GL_VENDOR),
             context->glGetString (GL_RENDERER),
             context->glGetString (GL_VERSION),
             context->glGetString (GL_EXTENSIONS));

  gl_extensions = (const char*) context->glGetString (GL_EXTENSIONS);

  _cogl_feature_check_ext_functions (context,
                                     -1 /* GL major version */,
                                     -1 /* GL minor version */,
                                     gl_extensions);

  GE( context, glGetIntegerv (GL_STENCIL_BITS, &num_stencil_bits) );
  /* We need at least three stencil bits to combine clips */
  if (num_stencil_bits > 2)
    flags |= COGL_FEATURE_STENCIL_BUFFER;

#ifdef HAVE_COGL_GLES
  if (context->driver == COGL_DRIVER_GLES1)
    {
      int max_clip_planes;
      GE( context, glGetIntegerv (GL_MAX_CLIP_PLANES, &max_clip_planes) );
      if (max_clip_planes >= 4)
        flags |= COGL_FEATURE_FOUR_CLIP_PLANES;
    }
#endif

  if (context->driver == COGL_DRIVER_GLES2)
    {
      flags |= COGL_FEATURE_SHADERS_GLSL | COGL_FEATURE_OFFSCREEN;
      /* Note GLES 2 core doesn't support mipmaps for npot textures or
       * repeat modes other than CLAMP_TO_EDGE. */
      flags |= COGL_FEATURE_TEXTURE_NPOT_BASIC;
      flags |= COGL_FEATURE_DEPTH_RANGE;
    }

  flags |= COGL_FEATURE_VBOS;

  /* Both GLES 1.1 and GLES 2.0 support point sprites in core */
  flags |= COGL_FEATURE_POINT_SPRITE;

  if (context->glGenRenderbuffers)
    flags |= COGL_FEATURE_OFFSCREEN;

  if (context->glBlitFramebuffer)
    flags |= COGL_FEATURE_OFFSCREEN_BLIT;

  if (_cogl_check_extension ("GL_OES_element_index_uint", gl_extensions))
    flags |= COGL_FEATURE_UNSIGNED_INT_INDICES;

  if (_cogl_check_extension ("GL_OES_texture_npot", gl_extensions) ||
      _cogl_check_extension ("GL_IMG_texture_npot", gl_extensions))
    flags |= (COGL_FEATURE_TEXTURE_NPOT |
              COGL_FEATURE_TEXTURE_NPOT_BASIC |
              COGL_FEATURE_TEXTURE_NPOT_MIPMAP |
              COGL_FEATURE_TEXTURE_NPOT_REPEAT);

  if (context->glTexImage3D)
    flags |= COGL_FEATURE_TEXTURE_3D;

  if (context->glMapBuffer)
    /* The GL_OES_mapbuffer extension doesn't support mapping for
       read */
    flags |= COGL_FEATURE_MAP_BUFFER_FOR_WRITE;

  if (context->glEGLImageTargetTexture2D)
    private_flags |= COGL_PRIVATE_FEATURE_TEXTURE_2D_FROM_EGL_IMAGE;

  /* Cache features */
  context->private_feature_flags |= private_flags;
  context->feature_flags |= flags;

  return TRUE;
}
