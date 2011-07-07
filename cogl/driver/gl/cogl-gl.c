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

#include "cogl-private.h"
#include "cogl-internal.h"
#include "cogl-context-private.h"
#include "cogl-feature-private.h"

static gboolean
_cogl_get_gl_version (int *major_out, int *minor_out)
{
  const char *version_string, *major_end, *minor_end;
  int major = 0, minor = 0;

  _COGL_GET_CONTEXT (ctx, FALSE);

  /* Get the OpenGL version number */
  if ((version_string = (const char *) ctx->glGetString (GL_VERSION)) == NULL)
    return FALSE;

  /* Extract the major number */
  for (major_end = version_string; *major_end >= '0'
	 && *major_end <= '9'; major_end++)
    major = (major * 10) + *major_end - '0';
  /* If there were no digits or the major number isn't followed by a
     dot then it is invalid */
  if (major_end == version_string || *major_end != '.')
    return FALSE;

  /* Extract the minor number */
  for (minor_end = major_end + 1; *minor_end >= '0'
	 && *minor_end <= '9'; minor_end++)
    minor = (minor * 10) + *minor_end - '0';
  /* If there were no digits or there is an unexpected character then
     it is invalid */
  if (minor_end == major_end + 1
      || (*minor_end && *minor_end != ' ' && *minor_end != '.'))
    return FALSE;

  *major_out = major;
  *minor_out = minor;

  return TRUE;
}

gboolean
_cogl_gl_check_gl_version (CoglContext *ctx,
                           GError **error)
{
  int major, minor;
  const char *gl_extensions;

  if (!_cogl_get_gl_version (&major, &minor))
    {
      g_set_error (error,
                   COGL_DRIVER_ERROR,
                   COGL_DRIVER_ERROR_UNKNOWN_VERSION,
                   "The OpenGL version could not be determined");
      return FALSE;
    }

  /* GL 1.3 supports all of the required functionality in core */
  if (COGL_CHECK_GL_VERSION (major, minor, 1, 3))
    return TRUE;

  gl_extensions = (const char*) ctx->glGetString (GL_EXTENSIONS);

  /* OpenGL 1.2 is only supported if we have the multitexturing
     extension */
  if (!_cogl_check_extension ("GL_ARB_multitexture", gl_extensions))
    {
      g_set_error (error,
                   COGL_DRIVER_ERROR,
                   COGL_DRIVER_ERROR_INVALID_VERSION,
                   "The OpenGL driver is missing "
                   "the GL_ARB_multitexture extension");
      return FALSE;
    }

  /* OpenGL 1.2 is required */
  if (!COGL_CHECK_GL_VERSION (major, minor, 1, 2))
    {
      g_set_error (error,
                   COGL_DRIVER_ERROR,
                   COGL_DRIVER_ERROR_INVALID_VERSION,
                   "The OpenGL version of your driver (%i.%i) "
                   "is not compatible with Cogl",
                   major, minor);
      return FALSE;
    }

  return TRUE;
}

/* Query the GL extensions and lookup the corresponding function
 * pointers. Theoretically the list of extensions can change for
 * different GL contexts so it is the winsys backend's responsiblity
 * to know when to re-query the GL extensions. */
void
_cogl_gl_update_features (CoglContext *context)
{
  CoglPrivateFeatureFlags private_flags = 0;
  CoglFeatureFlags flags = 0;
  const char *gl_extensions;
  int max_clip_planes = 0;
  int num_stencil_bits = 0;
  int gl_major = 0, gl_minor = 0;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

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
             ctx->glGetString (GL_VENDOR),
             ctx->glGetString (GL_RENDERER),
             ctx->glGetString (GL_VERSION),
             ctx->glGetString (GL_EXTENSIONS));

  _cogl_get_gl_version (&gl_major, &gl_minor);

  flags = (COGL_FEATURE_TEXTURE_READ_PIXELS
           | COGL_FEATURE_UNSIGNED_INT_INDICES
           | COGL_FEATURE_DEPTH_RANGE);

  gl_extensions = (const char *)ctx->glGetString (GL_EXTENSIONS);

  _cogl_feature_check_ext_functions (context,
                                     gl_major,
                                     gl_minor,
                                     gl_extensions);

  if (COGL_CHECK_GL_VERSION (gl_major, gl_minor, 2, 0) ||
      _cogl_check_extension ("GL_ARB_texture_non_power_of_two", gl_extensions))
    {
      flags |= COGL_FEATURE_TEXTURE_NPOT
        | COGL_FEATURE_TEXTURE_NPOT_BASIC
        | COGL_FEATURE_TEXTURE_NPOT_MIPMAP
        | COGL_FEATURE_TEXTURE_NPOT_REPEAT;
    }

#ifdef GL_YCBCR_MESA
  if (_cogl_check_extension ("GL_MESA_ycbcr_texture", gl_extensions))
    {
      flags |= COGL_FEATURE_TEXTURE_YUV;
    }
#endif

  GE( ctx, glGetIntegerv (GL_STENCIL_BITS, &num_stencil_bits) );
  /* We need at least three stencil bits to combine clips */
  if (num_stencil_bits > 2)
    flags |= COGL_FEATURE_STENCIL_BUFFER;

  GE( ctx, glGetIntegerv (GL_MAX_CLIP_PLANES, &max_clip_planes) );
  if (max_clip_planes >= 4)
    flags |= COGL_FEATURE_FOUR_CLIP_PLANES;

  if (context->glGenRenderbuffers)
    flags |= COGL_FEATURE_OFFSCREEN;

  if (context->glBlitFramebuffer)
    flags |= COGL_FEATURE_OFFSCREEN_BLIT;

  if (context->glRenderbufferStorageMultisample)
    flags |= COGL_FEATURE_OFFSCREEN_MULTISAMPLE;

  if (COGL_CHECK_GL_VERSION (gl_major, gl_minor, 2, 1) ||
      _cogl_check_extension ("GL_EXT_pixel_buffer_object", gl_extensions))
    flags |= COGL_FEATURE_PBOS;

  if (context->glGenPrograms)
    flags |= COGL_FEATURE_SHADERS_ARBFP;

  if (context->glCreateProgram)
    flags |= COGL_FEATURE_SHADERS_GLSL;

  if (context->glGenBuffers)
    flags |= (COGL_FEATURE_VBOS |
              COGL_FEATURE_MAP_BUFFER_FOR_READ |
              COGL_FEATURE_MAP_BUFFER_FOR_WRITE);

  if (_cogl_check_extension ("GL_ARB_texture_rectangle", gl_extensions))
    flags |= COGL_FEATURE_TEXTURE_RECTANGLE;

  if (context->glTexImage3D)
    flags |= COGL_FEATURE_TEXTURE_3D;

  if (context->glEGLImageTargetTexture2D)
    private_flags |= COGL_PRIVATE_FEATURE_TEXTURE_2D_FROM_EGL_IMAGE;

  /* Cache features */
  context->private_feature_flags |= private_flags;
  context->feature_flags |= flags;
}
