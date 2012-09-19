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

#include "cogl-private.h"
#include "cogl-internal.h"
#include "cogl-context-private.h"
#include "cogl-feature-private.h"
#include "cogl-renderer-private.h"
#include "cogl-error-private.h"
#include "cogl-framebuffer-gl-private.h"
#include "cogl-texture-2d-gl-private.h"
#include "cogl-attribute-gl-private.h"

static CoglBool
_cogl_driver_pixel_format_from_gl_internal (CoglContext *context,
                                            GLenum gl_int_format,
                                            CoglPixelFormat *out_format)
{
  /* It doesn't really matter we convert to exact same
     format (some have no cogl match anyway) since format
     is re-matched against cogl when getting or setting
     texture image data.
  */

  switch (gl_int_format)
    {
    case GL_ALPHA: case GL_ALPHA4: case GL_ALPHA8:
    case GL_ALPHA12: case GL_ALPHA16:

      *out_format = COGL_PIXEL_FORMAT_A_8;
      return TRUE;

    case GL_LUMINANCE: case GL_LUMINANCE4: case GL_LUMINANCE8:
    case GL_LUMINANCE12: case GL_LUMINANCE16:

      *out_format = COGL_PIXEL_FORMAT_G_8;
      return TRUE;

    case GL_RGB: case GL_RGB4: case GL_RGB5: case GL_RGB8:
    case GL_RGB10: case GL_RGB12: case GL_RGB16: case GL_R3_G3_B2:

      *out_format = COGL_PIXEL_FORMAT_RGB_888;
      return TRUE;

    case GL_RGBA: case GL_RGBA2: case GL_RGBA4: case GL_RGB5_A1:
    case GL_RGBA8: case GL_RGB10_A2: case GL_RGBA12: case GL_RGBA16:

      *out_format = COGL_PIXEL_FORMAT_RGBA_8888;
      return TRUE;
    }

  return FALSE;
}

static CoglPixelFormat
_cogl_driver_pixel_format_to_gl (CoglContext *context,
                                 CoglPixelFormat  format,
                                 GLenum *out_glintformat,
                                 GLenum *out_glformat,
                                 GLenum *out_gltype)
{
  CoglPixelFormat required_format;
  GLenum glintformat;
  GLenum glformat = 0;
  GLenum gltype;

  required_format = format;

  /* Find GL equivalents */
  switch (format)
    {
    case COGL_PIXEL_FORMAT_A_8:
      glintformat = GL_ALPHA;
      glformat = GL_ALPHA;
      gltype = GL_UNSIGNED_BYTE;
      break;
    case COGL_PIXEL_FORMAT_G_8:
      glintformat = GL_LUMINANCE;
      glformat = GL_LUMINANCE;
      gltype = GL_UNSIGNED_BYTE;
      break;

    case COGL_PIXEL_FORMAT_RGB_888:
      glintformat = GL_RGB;
      glformat = GL_RGB;
      gltype = GL_UNSIGNED_BYTE;
      break;
    case COGL_PIXEL_FORMAT_BGR_888:
      glintformat = GL_RGB;
      glformat = GL_BGR;
      gltype = GL_UNSIGNED_BYTE;
      break;
    case COGL_PIXEL_FORMAT_RGBA_8888:
    case COGL_PIXEL_FORMAT_RGBA_8888_PRE:
      glintformat = GL_RGBA;
      glformat = GL_RGBA;
      gltype = GL_UNSIGNED_BYTE;
      break;
    case COGL_PIXEL_FORMAT_BGRA_8888:
    case COGL_PIXEL_FORMAT_BGRA_8888_PRE:
      glintformat = GL_RGBA;
      glformat = GL_BGRA;
      gltype = GL_UNSIGNED_BYTE;
      break;

      /* The following two types of channel ordering
       * have no GL equivalent unless defined using
       * system word byte ordering */
    case COGL_PIXEL_FORMAT_ARGB_8888:
    case COGL_PIXEL_FORMAT_ARGB_8888_PRE:
      glintformat = GL_RGBA;
      glformat = GL_BGRA;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      gltype = GL_UNSIGNED_INT_8_8_8_8;
#else
      gltype = GL_UNSIGNED_INT_8_8_8_8_REV;
#endif
      break;

    case COGL_PIXEL_FORMAT_ABGR_8888:
    case COGL_PIXEL_FORMAT_ABGR_8888_PRE:
      glintformat = GL_RGBA;
      glformat = GL_RGBA;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      gltype = GL_UNSIGNED_INT_8_8_8_8;
#else
      gltype = GL_UNSIGNED_INT_8_8_8_8_REV;
#endif
      break;

    case COGL_PIXEL_FORMAT_RGBA_1010102:
    case COGL_PIXEL_FORMAT_RGBA_1010102_PRE:
      glintformat = GL_RGBA;
      glformat = GL_RGBA;
      gltype = GL_UNSIGNED_INT_10_10_10_2;
      break;

    case COGL_PIXEL_FORMAT_BGRA_1010102:
    case COGL_PIXEL_FORMAT_BGRA_1010102_PRE:
      glintformat = GL_RGBA;
      glformat = GL_BGRA;
      gltype = GL_UNSIGNED_INT_10_10_10_2;
      break;

    case COGL_PIXEL_FORMAT_ABGR_2101010:
    case COGL_PIXEL_FORMAT_ABGR_2101010_PRE:
      glintformat = GL_RGBA;
      glformat = GL_RGBA;
      gltype = GL_UNSIGNED_INT_2_10_10_10_REV;
      break;

    case COGL_PIXEL_FORMAT_ARGB_2101010:
    case COGL_PIXEL_FORMAT_ARGB_2101010_PRE:
      glintformat = GL_RGBA;
      glformat = GL_BGRA;
      gltype = GL_UNSIGNED_INT_2_10_10_10_REV;
      break;

      /* The following three types of channel ordering
       * are always defined using system word byte
       * ordering (even according to GLES spec) */
    case COGL_PIXEL_FORMAT_RGB_565:
      glintformat = GL_RGB;
      glformat = GL_RGB;
      gltype = GL_UNSIGNED_SHORT_5_6_5;
      break;
    case COGL_PIXEL_FORMAT_RGBA_4444:
    case COGL_PIXEL_FORMAT_RGBA_4444_PRE:
      glintformat = GL_RGBA;
      glformat = GL_RGBA;
      gltype = GL_UNSIGNED_SHORT_4_4_4_4;
      break;
    case COGL_PIXEL_FORMAT_RGBA_5551:
    case COGL_PIXEL_FORMAT_RGBA_5551_PRE:
      glintformat = GL_RGBA;
      glformat = GL_RGBA;
      gltype = GL_UNSIGNED_SHORT_5_5_5_1;
      break;

    case COGL_PIXEL_FORMAT_DEPTH_16:
      glintformat = GL_DEPTH_COMPONENT16;
      glformat = GL_DEPTH_COMPONENT;
      gltype = GL_UNSIGNED_SHORT;
      break;
    case COGL_PIXEL_FORMAT_DEPTH_32:
      glintformat = GL_DEPTH_COMPONENT32;
      glformat = GL_DEPTH_COMPONENT;
      gltype = GL_UNSIGNED_INT;
      break;

    case COGL_PIXEL_FORMAT_DEPTH_24_STENCIL_8:
      glintformat = GL_DEPTH_STENCIL;
      glformat = GL_DEPTH_STENCIL;
      gltype = GL_UNSIGNED_INT_24_8;
      break;

    case COGL_PIXEL_FORMAT_ANY:
    case COGL_PIXEL_FORMAT_YUV:
      g_assert_not_reached ();
      break;
    }

  /* All of the pixel formats are handled above so if this hits then
     we've been given an invalid pixel format */
  g_assert (glformat != 0);

  if (out_glintformat != NULL)
    *out_glintformat = glintformat;
  if (out_glformat != NULL)
    *out_glformat = glformat;
  if (out_gltype != NULL)
    *out_gltype = gltype;

  return required_format;
}

static CoglBool
_cogl_get_gl_version (CoglContext *ctx,
                      int *major_out,
                      int *minor_out)
{
  const char *version_string, *major_end, *minor_end;
  int major = 0, minor = 0;

  /* Get the OpenGL version number */
  if ((version_string = _cogl_context_get_gl_version (ctx)) == NULL)
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

static CoglBool
check_gl_version (CoglContext *ctx,
                  CoglError **error)
{
  int major, minor;
  const char *gl_extensions;

  if (!_cogl_get_gl_version (ctx, &major, &minor))
    {
      _cogl_set_error (error,
                   COGL_DRIVER_ERROR,
                   COGL_DRIVER_ERROR_UNKNOWN_VERSION,
                   "The OpenGL version could not be determined");
      return FALSE;
    }

  /* GL 1.3 supports all of the required functionality in core */
  if (COGL_CHECK_GL_VERSION (major, minor, 1, 3))
    return TRUE;

  gl_extensions = _cogl_context_get_gl_extensions (ctx);

  /* OpenGL 1.2 is only supported if we have the multitexturing
     extension */
  if (!_cogl_check_extension ("GL_ARB_multitexture", gl_extensions))
    {
      _cogl_set_error (error,
                   COGL_DRIVER_ERROR,
                   COGL_DRIVER_ERROR_INVALID_VERSION,
                   "The OpenGL driver is missing "
                   "the GL_ARB_multitexture extension");
      return FALSE;
    }

  /* OpenGL 1.2 is required */
  if (!COGL_CHECK_GL_VERSION (major, minor, 1, 2))
    {
      _cogl_set_error (error,
                   COGL_DRIVER_ERROR,
                   COGL_DRIVER_ERROR_INVALID_VERSION,
                   "The OpenGL version of your driver (%i.%i) "
                   "is not compatible with Cogl",
                   major, minor);
      return FALSE;
    }

  return TRUE;
}

static CoglBool
_cogl_driver_update_features (CoglContext *ctx,
                              CoglError **error)
{
  CoglPrivateFeatureFlags private_flags = 0;
  CoglFeatureFlags flags = 0;
  const char *gl_extensions;
  int max_clip_planes = 0;
  int num_stencil_bits = 0;
  int gl_major = 0, gl_minor = 0;

  /* We have to special case getting the pointer to the glGetString
     function because we need to use it to determine what functions we
     can expect */
  ctx->glGetString =
    (void *) _cogl_renderer_get_proc_address (ctx->display->renderer,
                                              "glGetString",
                                              TRUE);

  if (!check_gl_version (ctx, error))
    return FALSE;

  COGL_NOTE (WINSYS,
             "Checking features\n"
             "  GL_VENDOR: %s\n"
             "  GL_RENDERER: %s\n"
             "  GL_VERSION: %s\n"
             "  GL_EXTENSIONS: %s",
             ctx->glGetString (GL_VENDOR),
             ctx->glGetString (GL_RENDERER),
             _cogl_context_get_gl_version (ctx),
             _cogl_context_get_gl_extensions (ctx));

  _cogl_get_gl_version (ctx, &gl_major, &gl_minor);

  _cogl_gpu_info_init (ctx, &ctx->gpu);

  flags = (COGL_FEATURE_TEXTURE_READ_PIXELS
           | COGL_FEATURE_UNSIGNED_INT_INDICES
           | COGL_FEATURE_DEPTH_RANGE);
  COGL_FLAGS_SET (ctx->features,
                  COGL_FEATURE_ID_UNSIGNED_INT_INDICES, TRUE);
  COGL_FLAGS_SET (ctx->features, COGL_FEATURE_ID_DEPTH_RANGE, TRUE);

  if (COGL_CHECK_GL_VERSION (gl_major, gl_minor, 1, 4))
    COGL_FLAGS_SET (ctx->features, COGL_FEATURE_ID_MIRRORED_REPEAT, TRUE);

  gl_extensions = _cogl_context_get_gl_extensions (ctx);

  _cogl_feature_check_ext_functions (ctx,
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
      COGL_FLAGS_SET (ctx->features, COGL_FEATURE_ID_TEXTURE_NPOT, TRUE);
      COGL_FLAGS_SET (ctx->features,
                      COGL_FEATURE_ID_TEXTURE_NPOT_BASIC, TRUE);
      COGL_FLAGS_SET (ctx->features,
                      COGL_FEATURE_ID_TEXTURE_NPOT_MIPMAP, TRUE);
      COGL_FLAGS_SET (ctx->features,
                      COGL_FEATURE_ID_TEXTURE_NPOT_REPEAT, TRUE);
    }

  if (_cogl_check_extension ("GL_MESA_pack_invert", gl_extensions))
    private_flags |= COGL_PRIVATE_FEATURE_MESA_PACK_INVERT;

  GE( ctx, glGetIntegerv (GL_STENCIL_BITS, &num_stencil_bits) );
  /* We need at least three stencil bits to combine clips */
  if (num_stencil_bits > 2)
    private_flags |= COGL_PRIVATE_FEATURE_STENCIL_BUFFER;

  GE( ctx, glGetIntegerv (GL_MAX_CLIP_PLANES, &max_clip_planes) );
  if (max_clip_planes >= 4)
    private_flags |= COGL_PRIVATE_FEATURE_FOUR_CLIP_PLANES;

  if (ctx->glGenRenderbuffers)
    {
      flags |= COGL_FEATURE_OFFSCREEN;
      COGL_FLAGS_SET (ctx->features, COGL_FEATURE_ID_OFFSCREEN, TRUE);
    }

  if (ctx->glBlitFramebuffer)
    private_flags |= COGL_PRIVATE_FEATURE_OFFSCREEN_BLIT;

  if (ctx->glRenderbufferStorageMultisampleIMG)
    {
      flags |= COGL_FEATURE_OFFSCREEN_MULTISAMPLE;
      COGL_FLAGS_SET (ctx->features,
                      COGL_FEATURE_ID_OFFSCREEN_MULTISAMPLE, TRUE);
    }

  if (COGL_CHECK_GL_VERSION (gl_major, gl_minor, 3, 0) ||
      _cogl_check_extension ("GL_ARB_depth_texture", gl_extensions))
    {
      flags |= COGL_FEATURE_DEPTH_TEXTURE;
      COGL_FLAGS_SET (ctx->features, COGL_FEATURE_ID_DEPTH_TEXTURE, TRUE);
    }

  if (COGL_CHECK_GL_VERSION (gl_major, gl_minor, 2, 1) ||
      _cogl_check_extension ("GL_EXT_pixel_buffer_object", gl_extensions))
    private_flags |= COGL_PRIVATE_FEATURE_PBOS;

  if (COGL_CHECK_GL_VERSION (gl_major, gl_minor, 2, 0) ||
      _cogl_check_extension ("GL_ARB_point_sprite", gl_extensions))
    {
      flags |= COGL_FEATURE_POINT_SPRITE;
      COGL_FLAGS_SET (ctx->features, COGL_FEATURE_ID_POINT_SPRITE, TRUE);
    }

  if (ctx->glGenPrograms)
    {
      flags |= COGL_FEATURE_SHADERS_ARBFP;
      COGL_FLAGS_SET (ctx->features, COGL_FEATURE_ID_ARBFP, TRUE);
    }

  if (ctx->glCreateProgram)
    {
      flags |= COGL_FEATURE_SHADERS_GLSL;
      COGL_FLAGS_SET (ctx->features, COGL_FEATURE_ID_GLSL, TRUE);
    }
  /* If all of the old GLSL extensions are available then we can fake
   * the GL 2.0 GLSL support by diverting to the old function names */
  else if (ctx->glCreateProgramObject && /* GL_ARB_shader_objects */
           ctx->glVertexAttribPointer && /* GL_ARB_vertex_shader */
           _cogl_check_extension ("GL_ARB_fragment_shader", gl_extensions))
    {
      ctx->glCreateShader = ctx->glCreateShaderObject;
      ctx->glCreateProgram = ctx->glCreateProgramObject;
      ctx->glDeleteShader = ctx->glDeleteObject;
      ctx->glDeleteProgram = ctx->glDeleteObject;
      ctx->glAttachShader = ctx->glAttachObject;
      ctx->glUseProgram = ctx->glUseProgramObject;
      ctx->glGetProgramInfoLog = ctx->glGetInfoLog;
      ctx->glGetShaderInfoLog = ctx->glGetInfoLog;
      ctx->glGetShaderiv = ctx->glGetObjectParameteriv;
      ctx->glGetProgramiv = ctx->glGetObjectParameteriv;
      ctx->glDetachShader = ctx->glDetachObject;
      ctx->glGetAttachedShaders = ctx->glGetAttachedObjects;
      /* FIXME: there doesn't seem to be an equivalent for glIsShader
       * and glIsProgram. This doesn't matter for now because Cogl
       * doesn't use these but if we add support for simulating a
       * GLES2 context on top of regular GL then we'll need to do
       * something here */

      flags |= COGL_FEATURE_SHADERS_GLSL;
      COGL_FLAGS_SET (ctx->features, COGL_FEATURE_ID_GLSL, TRUE);
    }

  if (ctx->glGenBuffers)
    {
      private_flags |= COGL_PRIVATE_FEATURE_VBOS;
      flags |= (COGL_FEATURE_MAP_BUFFER_FOR_READ |
                COGL_FEATURE_MAP_BUFFER_FOR_WRITE);
      COGL_FLAGS_SET (ctx->features,
                         COGL_FEATURE_ID_MAP_BUFFER_FOR_READ, TRUE);
      COGL_FLAGS_SET (ctx->features,
                      COGL_FEATURE_ID_MAP_BUFFER_FOR_WRITE, TRUE);
    }

  if (_cogl_check_extension ("GL_ARB_texture_rectangle", gl_extensions))
    {
      flags |= COGL_FEATURE_TEXTURE_RECTANGLE;
      COGL_FLAGS_SET (ctx->features,
                      COGL_FEATURE_ID_TEXTURE_RECTANGLE, TRUE);
    }

  if (ctx->glTexImage3D)
    {
      flags |= COGL_FEATURE_TEXTURE_3D;
      COGL_FLAGS_SET (ctx->features, COGL_FEATURE_ID_TEXTURE_3D, TRUE);
    }

  if (ctx->glEGLImageTargetTexture2D)
    private_flags |= COGL_PRIVATE_FEATURE_TEXTURE_2D_FROM_EGL_IMAGE;

  if (_cogl_check_extension ("GL_EXT_packed_depth_stencil", gl_extensions))
    private_flags |= COGL_PRIVATE_FEATURE_EXT_PACKED_DEPTH_STENCIL;

  if (ctx->glGenSamplers)
    private_flags |= COGL_PRIVATE_FEATURE_SAMPLER_OBJECTS;

  /* Cache features */
  ctx->private_feature_flags |= private_flags;
  ctx->feature_flags |= flags;

  return TRUE;
}

const CoglDriverVtable
_cogl_driver_gl =
  {
    _cogl_driver_pixel_format_from_gl_internal,
    _cogl_driver_pixel_format_to_gl,
    _cogl_driver_update_features,
    _cogl_offscreen_gl_allocate,
    _cogl_offscreen_gl_free,
    _cogl_framebuffer_gl_flush_state,
    _cogl_framebuffer_gl_clear,
    _cogl_framebuffer_gl_query_bits,
    _cogl_framebuffer_gl_finish,
    _cogl_framebuffer_gl_discard_buffers,
    _cogl_framebuffer_gl_draw_attributes,
    _cogl_framebuffer_gl_draw_indexed_attributes,
    _cogl_texture_2d_gl_free,
    _cogl_texture_2d_gl_can_create,
    _cogl_texture_2d_gl_init,
    _cogl_texture_2d_gl_new_with_size,
    _cogl_texture_2d_gl_new_from_bitmap,
#if defined (COGL_HAS_EGL_SUPPORT) && defined (EGL_KHR_image_base)
    _cogl_egl_texture_2d_gl_new_from_image,
#endif
    _cogl_texture_2d_gl_copy_from_framebuffer,
    _cogl_texture_2d_gl_get_gl_handle,
    _cogl_texture_2d_gl_generate_mipmap,
    _cogl_texture_2d_gl_copy_from_bitmap,
    _cogl_texture_2d_gl_get_data,
    _cogl_gl_flush_attributes_state,
  };
