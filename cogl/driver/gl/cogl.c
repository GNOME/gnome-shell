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
#include "cogl-feature-private.h"

#ifdef HAVE_CLUTTER_OSX
static gboolean
really_enable_npot (void)
{
  /* OSX backend + ATI Radeon X1600 + NPOT texture + GL_REPEAT seems to crash
   * http://bugzilla.openedhand.com/show_bug.cgi?id=929
   *
   * Temporary workaround until post 0.8 we rejig the features set up a
   * little to allow the backend to overide.
   */
  const char *gl_renderer;
  const char *env_string;

  /* Regardless of hardware, allow user to decide. */
  env_string = g_getenv ("COGL_ENABLE_NPOT");
  if (env_string != NULL)
    return env_string[0] == '1';

  gl_renderer = (char*)glGetString (GL_RENDERER);
  if (strstr (gl_renderer, "ATI Radeon X1600") != NULL)
    return FALSE;

  return TRUE;
}
#endif

static gboolean
_cogl_get_gl_version (int *major_out, int *minor_out)
{
  const char *version_string, *major_end, *minor_end;
  int major = 0, minor = 0;

  /* Get the OpenGL version number */
  if ((version_string = (const char *) glGetString (GL_VERSION)) == NULL)
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
_cogl_check_driver_valid (GError **error)
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

  gl_extensions = (const char*) glGetString (GL_EXTENSIONS);

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

/* Define a set of arrays containing the functions required from GL
   for each feature */
#define COGL_FEATURE_BEGIN(name, min_gl_major, min_gl_minor,            \
                           namespaces, extension_names, feature_flags)  \
  static const CoglFeatureFunction cogl_feature_ ## name ## _funcs[] = {
#define COGL_FEATURE_FUNCTION(ret, name, args)                          \
  { G_STRINGIFY (name), G_STRUCT_OFFSET (CoglContext, drv.pf_ ## name) },
#define COGL_FEATURE_END()                      \
  { NULL, 0 },                                  \
  };
#include "cogl-feature-functions.h"

/* Define an array of features */
#undef COGL_FEATURE_BEGIN
#define COGL_FEATURE_BEGIN(name, min_gl_major, min_gl_minor,            \
                           namespaces, extension_names, feature_flags)  \
  { min_gl_major, min_gl_minor, namespaces,                             \
      extension_names, feature_flags,                                   \
      cogl_feature_ ## name ## _funcs },
#undef COGL_FEATURE_FUNCTION
#define COGL_FEATURE_FUNCTION(ret, name, args)
#undef COGL_FEATURE_END
#define COGL_FEATURE_END()

static const CoglFeatureData cogl_feature_data[] =
  {
#include "cogl-feature-functions.h"
  };

void
_cogl_features_init (void)
{
  CoglFeatureFlags  flags = 0;
  const char       *gl_extensions;
  GLint             max_clip_planes = 0;
  GLint             num_stencil_bits = 0;
  int               gl_major = 0, gl_minor = 0;
  int               i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_get_gl_version (&gl_major, &gl_minor);

  flags = (COGL_FEATURE_TEXTURE_READ_PIXELS
           | COGL_FEATURE_UNSIGNED_INT_INDICES);

  gl_extensions = (const char*) glGetString (GL_EXTENSIONS);

  if (COGL_CHECK_GL_VERSION (gl_major, gl_minor, 2, 0) ||
      _cogl_check_extension ("GL_ARB_texture_non_power_of_two", gl_extensions))
    {
#ifdef HAVE_CLUTTER_OSX
      if (really_enable_npot ())
#endif
        flags |= COGL_FEATURE_TEXTURE_NPOT;
    }

#ifdef GL_YCBCR_MESA
  if (_cogl_check_extension ("GL_MESA_ycbcr_texture", gl_extensions))
    {
      flags |= COGL_FEATURE_TEXTURE_YUV;
    }
#endif

  GE( glGetIntegerv (GL_STENCIL_BITS, &num_stencil_bits) );
  /* We need at least three stencil bits to combine clips */
  if (num_stencil_bits > 2)
    flags |= COGL_FEATURE_STENCIL_BUFFER;

  GE( glGetIntegerv (GL_MAX_CLIP_PLANES, &max_clip_planes) );
  if (max_clip_planes >= 4)
    flags |= COGL_FEATURE_FOUR_CLIP_PLANES;

  for (i = 0; i < G_N_ELEMENTS (cogl_feature_data); i++)
    if (_cogl_feature_check (cogl_feature_data + i,
                             gl_major, gl_minor,
                             gl_extensions))
        flags |= cogl_feature_data[i].feature_flags;

  /* Cache features */
  ctx->feature_flags = flags;
  ctx->features_cached = TRUE;
}
