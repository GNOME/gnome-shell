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
_cogl_check_driver_valid (GError **error)
{
  /* The GLES backend doesn't have any particular version requirements */
  return TRUE;
}

/* Define a set of arrays containing the functions required from GL
   for each feature */
#define COGL_FEATURE_BEGIN(name, min_gl_major, min_gl_minor,            \
                           namespaces, extension_names,                 \
                           feature_flags, feature_flags_private)        \
  static const CoglFeatureFunction cogl_feature_ ## name ## _funcs[] = {
#define COGL_FEATURE_FUNCTION(ret, name, args)                          \
  { G_STRINGIFY (name), G_STRUCT_OFFSET (CoglContext, drv.pf_ ## name) },
#define COGL_FEATURE_END()                      \
  { NULL, 0 },                                  \
  };
#include "cogl-feature-functions-gles.h"

/* Define an array of features */
#undef COGL_FEATURE_BEGIN
#define COGL_FEATURE_BEGIN(name, min_gl_major, min_gl_minor,            \
                           namespaces, extension_names,                 \
                           feature_flags, feature_flags_private)        \
  { min_gl_major, min_gl_minor, namespaces,                             \
      extension_names, feature_flags, feature_flags_private,            \
      cogl_feature_ ## name ## _funcs },
#undef COGL_FEATURE_FUNCTION
#define COGL_FEATURE_FUNCTION(ret, name, args)
#undef COGL_FEATURE_END
#define COGL_FEATURE_END()

static const CoglFeatureData cogl_feature_data[] =
  {
#include "cogl-feature-functions-gles.h"
  };

void
_cogl_features_init (void)
{
  CoglFeatureFlags flags = 0;
#ifndef HAVE_COGL_GLES2
  int              max_clip_planes = 0;
#endif
  GLint            num_stencil_bits = 0;
  const char      *gl_extensions;
  int              i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  gl_extensions = (const char*) glGetString (GL_EXTENSIONS);

  for (i = 0; i < G_N_ELEMENTS (cogl_feature_data); i++)
    if (_cogl_feature_check ("GL", cogl_feature_data + i,
                             0, 0,
                             gl_extensions))
        flags |= cogl_feature_data[i].feature_flags;

  GE( glGetIntegerv (GL_STENCIL_BITS, &num_stencil_bits) );
  /* We need at least three stencil bits to combine clips */
  if (num_stencil_bits > 2)
    flags |= COGL_FEATURE_STENCIL_BUFFER;

#ifndef HAVE_COGL_GLES2
  GE( glGetIntegerv (GL_MAX_CLIP_PLANES, &max_clip_planes) );
  if (max_clip_planes >= 4)
    flags |= COGL_FEATURE_FOUR_CLIP_PLANES;
#endif

#ifdef HAVE_COGL_GLES2
  flags |= COGL_FEATURE_SHADERS_GLSL | COGL_FEATURE_OFFSCREEN;
  /* Note GLES 2 core doesn't support mipmaps for npot textures or
   * repeat modes other than CLAMP_TO_EDGE. */
  flags |= COGL_FEATURE_TEXTURE_NPOT_BASIC;
#endif

  flags |= COGL_FEATURE_VBOS;

  /* Both GLES 1.1 and GLES 2.0 support point sprites in core */
  flags |= COGL_FEATURE_POINT_SPRITE;

  /* Cache features */
  ctx->feature_flags = flags;
}

