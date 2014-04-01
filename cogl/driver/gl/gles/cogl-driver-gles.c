/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-context-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-feature-private.h"
#include "cogl-renderer-private.h"
#include "cogl-private.h"
#include "cogl-framebuffer-gl-private.h"
#include "cogl-texture-2d-gl-private.h"
#include "cogl-attribute-gl-private.h"
#include "cogl-clip-stack-gl-private.h"
#include "cogl-buffer-gl-private.h"

#ifndef GL_UNSIGNED_INT_24_8
#define GL_UNSIGNED_INT_24_8 0x84FA
#endif
#ifndef GL_DEPTH_STENCIL
#define GL_DEPTH_STENCIL 0x84F9
#endif
#ifndef GL_RG
#define GL_RG 0x8227
#endif
#ifndef GL_RG8
#define GL_RG8 0x822B
#endif

static CoglBool
_cogl_driver_pixel_format_from_gl_internal (CoglContext *context,
                                            GLenum gl_int_format,
                                            CoglPixelFormat *out_format)
{
  return TRUE;
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

    case COGL_PIXEL_FORMAT_RG_88:
      if (cogl_has_feature (context, COGL_FEATURE_ID_TEXTURE_RG))
        {
          glintformat = GL_RG8;
          glformat = GL_RG;
        }
      else
        {
          /* If red-green textures aren't supported then we'll use RGB
           * as an internal format. Note this should only end up
           * mattering for downloading the data because Cogl will
           * refuse to allocate a texture with RG components if RG
           * textures aren't supported */
          glintformat = GL_RGB;
          glformat = GL_RGB;
          required_format = COGL_PIXEL_FORMAT_RGB_888;
        }
      gltype = GL_UNSIGNED_BYTE;
      break;

    case COGL_PIXEL_FORMAT_BGRA_8888:
    case COGL_PIXEL_FORMAT_BGRA_8888_PRE:
      /* There is an extension to support this format */
      if (_cogl_has_private_feature
          (context,  COGL_PRIVATE_FEATURE_TEXTURE_FORMAT_BGRA8888))
        {
          /* For some reason the extension says you have to specify
             BGRA for the internal format too */
          glintformat = GL_BGRA_EXT;
          glformat = GL_BGRA_EXT;
          gltype = GL_UNSIGNED_BYTE;
          required_format = format;
          break;
        }
      /* flow through */

      /* Just one 24-bit ordering supported */
    case COGL_PIXEL_FORMAT_RGB_888:
    case COGL_PIXEL_FORMAT_BGR_888:
      glintformat = GL_RGB;
      glformat = GL_RGB;
      gltype = GL_UNSIGNED_BYTE;
      required_format = COGL_PIXEL_FORMAT_RGB_888;
      break;

      /* Just one 32-bit ordering supported */
    case COGL_PIXEL_FORMAT_RGBA_8888:
    case COGL_PIXEL_FORMAT_RGBA_8888_PRE:
    case COGL_PIXEL_FORMAT_ARGB_8888:
    case COGL_PIXEL_FORMAT_ARGB_8888_PRE:
    case COGL_PIXEL_FORMAT_ABGR_8888:
    case COGL_PIXEL_FORMAT_ABGR_8888_PRE:
    case COGL_PIXEL_FORMAT_RGBA_1010102:
    case COGL_PIXEL_FORMAT_RGBA_1010102_PRE:
    case COGL_PIXEL_FORMAT_BGRA_1010102:
    case COGL_PIXEL_FORMAT_BGRA_1010102_PRE:
    case COGL_PIXEL_FORMAT_ABGR_2101010:
    case COGL_PIXEL_FORMAT_ABGR_2101010_PRE:
    case COGL_PIXEL_FORMAT_ARGB_2101010:
    case COGL_PIXEL_FORMAT_ARGB_2101010_PRE:
      glintformat = GL_RGBA;
      glformat = GL_RGBA;
      gltype = GL_UNSIGNED_BYTE;
      required_format = COGL_PIXEL_FORMAT_RGBA_8888;
      required_format |= (format & COGL_PREMULT_BIT);
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
      glintformat = GL_DEPTH_COMPONENT;
      glformat = GL_DEPTH_COMPONENT;
      gltype = GL_UNSIGNED_SHORT;
      break;
    case COGL_PIXEL_FORMAT_DEPTH_32:
      glintformat = GL_DEPTH_COMPONENT;
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
  const char *version_string;

  /* Get the OpenGL version number */
  if ((version_string = _cogl_context_get_gl_version (ctx)) == NULL)
    return FALSE;

  if (!g_str_has_prefix (version_string, "OpenGL ES "))
    return FALSE;

  return _cogl_gl_util_parse_gl_version (version_string + 10,
                                         major_out,
                                         minor_out);
}

static CoglBool
_cogl_driver_update_features (CoglContext *context,
                              CoglError **error)
{
  unsigned long private_features
    [COGL_FLAGS_N_LONGS_FOR_SIZE (COGL_N_PRIVATE_FEATURES)] = { 0 };
  CoglFeatureFlags flags = 0;
  char **gl_extensions;
  int gl_major, gl_minor;
  int i;

  /* We have to special case getting the pointer to the glGetString
     function because we need to use it to determine what functions we
     can expect */
  context->glGetString =
    (void *) _cogl_renderer_get_proc_address (context->display->renderer,
                                              "glGetString",
                                              TRUE);

  gl_extensions = _cogl_context_get_gl_extensions (context);

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_WINSYS)))
    {
      char *all_extensions = g_strjoinv (" ", gl_extensions);

      COGL_NOTE (WINSYS,
                 "Checking features\n"
                 "  GL_VENDOR: %s\n"
                 "  GL_RENDERER: %s\n"
                 "  GL_VERSION: %s\n"
                 "  GL_EXTENSIONS: %s",
                 context->glGetString (GL_VENDOR),
                 context->glGetString (GL_RENDERER),
                 _cogl_context_get_gl_version (context),
                 all_extensions);

      g_free (all_extensions);
    }

  context->glsl_major = 1;
  context->glsl_minor = 0;
  context->glsl_version_to_use = 100;

  _cogl_gpu_info_init (context, &context->gpu);

  if (!_cogl_get_gl_version (context, &gl_major, &gl_minor))
    {
      gl_major = 1;
      gl_minor = 1;
    }

  _cogl_feature_check_ext_functions (context,
                                     gl_major,
                                     gl_minor,
                                     gl_extensions);

#ifdef HAVE_COGL_GLES
  if (context->driver == COGL_DRIVER_GLES1)
    {
      int max_clip_planes;
      GE( context, glGetIntegerv (GL_MAX_CLIP_PLANES, &max_clip_planes) );
      if (max_clip_planes >= 4)
        COGL_FLAGS_SET (private_features,
                        COGL_PRIVATE_FEATURE_FOUR_CLIP_PLANES, TRUE);
    }
#endif

  if (context->driver == COGL_DRIVER_GLES2)
    {
      flags |= COGL_FEATURE_SHADERS_GLSL | COGL_FEATURE_OFFSCREEN;
      /* Note GLES 2 core doesn't support mipmaps for npot textures or
       * repeat modes other than CLAMP_TO_EDGE. */
      flags |= COGL_FEATURE_TEXTURE_NPOT_BASIC;
      flags |= COGL_FEATURE_DEPTH_RANGE;
      COGL_FLAGS_SET (context->features, COGL_FEATURE_ID_GLSL, TRUE);
      COGL_FLAGS_SET (context->features, COGL_FEATURE_ID_OFFSCREEN, TRUE);
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_TEXTURE_NPOT_BASIC, TRUE);
      COGL_FLAGS_SET (context->features, COGL_FEATURE_ID_DEPTH_RANGE, TRUE);
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_MIRRORED_REPEAT, TRUE);
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_PER_VERTEX_POINT_SIZE, TRUE);

      COGL_FLAGS_SET (private_features,
                      COGL_PRIVATE_FEATURE_BLEND_CONSTANT, TRUE);
    }
  else if (context->driver == COGL_DRIVER_GLES1)
    {
      COGL_FLAGS_SET (private_features, COGL_PRIVATE_FEATURE_GL_FIXED, TRUE);
      COGL_FLAGS_SET (private_features, COGL_PRIVATE_FEATURE_ALPHA_TEST, TRUE);
      COGL_FLAGS_SET (private_features,
                      COGL_PRIVATE_FEATURE_BUILTIN_POINT_SIZE_UNIFORM, TRUE);
    }

  COGL_FLAGS_SET (private_features, COGL_PRIVATE_FEATURE_VBOS, TRUE);
  COGL_FLAGS_SET (private_features, COGL_PRIVATE_FEATURE_ANY_GL, TRUE);
  COGL_FLAGS_SET (private_features, COGL_PRIVATE_FEATURE_ALPHA_TEXTURES, TRUE);

  /* Both GLES 1.1 and GLES 2.0 support point sprites in core */
  flags |= COGL_FEATURE_POINT_SPRITE;
  COGL_FLAGS_SET (context->features, COGL_FEATURE_ID_POINT_SPRITE, TRUE);

  if (context->glGenRenderbuffers)
    {
      flags |= COGL_FEATURE_OFFSCREEN;
      COGL_FLAGS_SET (context->features, COGL_FEATURE_ID_OFFSCREEN, TRUE);
    }

  if (context->glBlitFramebuffer)
    COGL_FLAGS_SET (private_features,
                    COGL_PRIVATE_FEATURE_OFFSCREEN_BLIT, TRUE);

  if (_cogl_check_extension ("GL_OES_element_index_uint", gl_extensions))
    {
      flags |= COGL_FEATURE_UNSIGNED_INT_INDICES;
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_UNSIGNED_INT_INDICES, TRUE);
    }

  if (_cogl_check_extension ("GL_OES_depth_texture", gl_extensions))
    {
      flags |= COGL_FEATURE_DEPTH_TEXTURE;
      COGL_FLAGS_SET (context->features, COGL_FEATURE_ID_DEPTH_TEXTURE, TRUE);
    }

  if (_cogl_check_extension ("GL_OES_texture_npot", gl_extensions))
    {
      flags |= (COGL_FEATURE_TEXTURE_NPOT |
                COGL_FEATURE_TEXTURE_NPOT_BASIC |
                COGL_FEATURE_TEXTURE_NPOT_MIPMAP |
                COGL_FEATURE_TEXTURE_NPOT_REPEAT);
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_TEXTURE_NPOT, TRUE);
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_TEXTURE_NPOT_BASIC, TRUE);
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_TEXTURE_NPOT_MIPMAP, TRUE);
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_TEXTURE_NPOT_REPEAT, TRUE);
    }
  else if (_cogl_check_extension ("GL_IMG_texture_npot", gl_extensions))
    {
      flags |= (COGL_FEATURE_TEXTURE_NPOT_BASIC |
                COGL_FEATURE_TEXTURE_NPOT_MIPMAP);
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_TEXTURE_NPOT_BASIC, TRUE);
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_TEXTURE_NPOT_MIPMAP, TRUE);
    }

  if (context->glTexImage3D)
    {
      flags |= COGL_FEATURE_TEXTURE_3D;
      COGL_FLAGS_SET (context->features, COGL_FEATURE_ID_TEXTURE_3D, TRUE);
    }

  if (context->glMapBuffer)
    {
      /* The GL_OES_mapbuffer extension doesn't support mapping for
         read */
      flags |= COGL_FEATURE_MAP_BUFFER_FOR_WRITE;
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_MAP_BUFFER_FOR_WRITE, TRUE);
    }

  if (context->glEGLImageTargetTexture2D)
    COGL_FLAGS_SET (private_features,
                    COGL_PRIVATE_FEATURE_TEXTURE_2D_FROM_EGL_IMAGE, TRUE);

  if (_cogl_check_extension ("GL_OES_packed_depth_stencil", gl_extensions))
    COGL_FLAGS_SET (private_features,
                    COGL_PRIVATE_FEATURE_OES_PACKED_DEPTH_STENCIL, TRUE);

  if (_cogl_check_extension ("GL_EXT_texture_format_BGRA8888", gl_extensions))
    COGL_FLAGS_SET (private_features,
                    COGL_PRIVATE_FEATURE_TEXTURE_FORMAT_BGRA8888, TRUE);

  if (_cogl_check_extension ("GL_EXT_unpack_subimage", gl_extensions))
    COGL_FLAGS_SET (private_features,
                    COGL_PRIVATE_FEATURE_UNPACK_SUBIMAGE, TRUE);

  /* A nameless vendor implemented the extension, but got the case wrong
   * per the spec. */
  if (_cogl_check_extension ("GL_OES_EGL_sync", gl_extensions) ||
      _cogl_check_extension ("GL_OES_egl_sync", gl_extensions))
    COGL_FLAGS_SET (private_features, COGL_PRIVATE_FEATURE_OES_EGL_SYNC, TRUE);

  if (_cogl_check_extension ("GL_EXT_texture_rg", gl_extensions))
    COGL_FLAGS_SET (context->features,
                    COGL_FEATURE_ID_TEXTURE_RG,
                    TRUE);

  /* Cache features */
  for (i = 0; i < G_N_ELEMENTS (private_features); i++)
    context->private_features[i] |= private_features[i];
  context->feature_flags |= flags;

  g_strfreev (gl_extensions);

  return TRUE;
}

const CoglDriverVtable
_cogl_driver_gles =
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
    _cogl_framebuffer_gl_read_pixels_into_bitmap,
    _cogl_texture_2d_gl_free,
    _cogl_texture_2d_gl_can_create,
    _cogl_texture_2d_gl_init,
    _cogl_texture_2d_gl_allocate,
    _cogl_texture_2d_gl_copy_from_framebuffer,
    _cogl_texture_2d_gl_get_gl_handle,
    _cogl_texture_2d_gl_generate_mipmap,
    _cogl_texture_2d_gl_copy_from_bitmap,
    NULL, /* texture_2d_get_data */
    _cogl_gl_flush_attributes_state,
    _cogl_clip_stack_gl_flush,
    _cogl_buffer_gl_create,
    _cogl_buffer_gl_destroy,
    _cogl_buffer_gl_map_range,
    _cogl_buffer_gl_unmap,
    _cogl_buffer_gl_set_data,
  };
