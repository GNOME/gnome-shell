/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
 * Copyright (C) 2010 Red Hat, Inc.
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
 *
 * Authors:
 *  Matthew Allum  <mallum@openedhand.com>
 *  Neil Roberts   <neil@linux.intel.com>
 *  Robert Bragg   <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-util.h"
#include "cogl-bitmap.h"
#include "cogl-bitmap-private.h"
#include "cogl-buffer-private.h"
#include "cogl-pixel-buffer-private.h"
#include "cogl-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-driver.h"
#include "cogl-texture-2d-sliced-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-texture-rectangle-private.h"
#include "cogl-sub-texture-private.h"
#include "cogl-atlas-texture-private.h"
#include "cogl-pipeline.h"
#include "cogl-context-private.h"
#include "cogl-handle.h"
#include "cogl-object-private.h"
#include "cogl-primitives.h"
#include "cogl-framebuffer-private.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

GQuark
cogl_texture_error_quark (void)
{
  return g_quark_from_static_string ("cogl-texture-error-quark");
}

/* XXX:
 * The CoglHandle macros don't support any form of inheritance, so for
 * now we implement the CoglHandle support for the CoglTexture
 * abstract class manually.
 */

void
_cogl_texture_register_texture_type (GQuark type)
{
  _COGL_GET_CONTEXT (ctxt, NO_RETVAL);

  ctxt->texture_types = g_slist_prepend (ctxt->texture_types,
                                         GINT_TO_POINTER (type));
}

gboolean
cogl_is_texture (CoglHandle handle)
{
  CoglHandleObject *obj = (CoglHandleObject *)handle;
  GSList *l;

  _COGL_GET_CONTEXT (ctxt, FALSE);

  if (handle == COGL_INVALID_HANDLE)
    return FALSE;

  for (l = ctxt->texture_types; l; l = l->next)
    if (GPOINTER_TO_INT (l->data) == obj->klass->type)
      return TRUE;

  return FALSE;
}

CoglHandle
cogl_texture_ref (CoglHandle handle)
{
  if (!cogl_is_texture (handle))
    return COGL_INVALID_HANDLE;

  _COGL_HANDLE_DEBUG_REF (CoglTexture, handle);

  cogl_handle_ref (handle);

  return handle;
}

void
cogl_texture_unref (CoglHandle handle)
{
  if (!cogl_is_texture (handle))
    {
      g_warning (G_STRINGIFY (cogl_texture_unref)
                 ": Ignoring unref of Cogl handle "
                 "due to type mismatch");
      return;
    }

  _COGL_HANDLE_DEBUG_UNREF (CoglTexture, handle);

  cogl_handle_unref (handle);
}

void
_cogl_texture_init (CoglTexture *texture,
                    const CoglTextureVtable *vtable)
{
  texture->vtable = vtable;
  texture->framebuffers = NULL;
}

void
_cogl_texture_free (CoglTexture *texture)
{
  g_free (texture);
}

static gboolean
_cogl_texture_needs_premult_conversion (CoglPixelFormat src_format,
                                        CoglPixelFormat dst_format)
{
  return ((src_format & dst_format & COGL_A_BIT) &&
          src_format != COGL_PIXEL_FORMAT_A_8 &&
          dst_format != COGL_PIXEL_FORMAT_A_8 &&
          (src_format & COGL_PREMULT_BIT) !=
          (dst_format & COGL_PREMULT_BIT));
}

CoglPixelFormat
_cogl_texture_determine_internal_format (CoglPixelFormat src_format,
                                         CoglPixelFormat dst_format)
{
  /* If the application hasn't specified a specific format then we'll
   * pick the most appropriate. By default Cogl will use a
   * premultiplied internal format. Later we will add control over
   * this. */
  if (dst_format == COGL_PIXEL_FORMAT_ANY)
    {
      if ((src_format & COGL_A_BIT) &&
          src_format != COGL_PIXEL_FORMAT_A_8)
        return src_format | COGL_PREMULT_BIT;
      else
        return src_format;
    }
  else
    return dst_format;
}

CoglBitmap *
_cogl_texture_prepare_for_upload (CoglBitmap      *src_bmp,
                                  CoglPixelFormat  dst_format,
                                  CoglPixelFormat *dst_format_out,
                                  GLenum          *out_glintformat,
                                  GLenum          *out_glformat,
                                  GLenum          *out_gltype)
{
  CoglPixelFormat src_format = _cogl_bitmap_get_format (src_bmp);
  CoglBitmap *dst_bmp;

  _COGL_GET_CONTEXT (ctx, NULL);

  dst_format = _cogl_texture_determine_internal_format (src_format,
                                                        dst_format);

  /* OpenGL supports specifying a different format for the internal
     format when uploading texture data. We should use this to convert
     formats because it is likely to be faster and support more types
     than the Cogl bitmap code. However under GLES the internal format
     must be the same as the bitmap format and it only supports a
     limited number of formats so we must convert using the Cogl
     bitmap code instead */

  if (ctx->driver == COGL_DRIVER_GL)
    {
      /* If the source format does not have the same premult flag as the
         dst format then we need to copy and convert it */
      if (_cogl_texture_needs_premult_conversion (src_format,
                                                  dst_format))
        {
          dst_bmp = _cogl_bitmap_copy (src_bmp);

          if (!_cogl_bitmap_convert_premult_status (dst_bmp,
                                                    src_format ^
                                                    COGL_PREMULT_BIT))
            {
              cogl_object_unref (dst_bmp);
              return NULL;
            }
        }
      else
        dst_bmp = cogl_object_ref (src_bmp);

      /* Use the source format from the src bitmap type and the internal
         format from the dst format type so that GL can do the
         conversion */
      ctx->texture_driver->pixel_format_to_gl (src_format,
                                               NULL, /* internal format */
                                               out_glformat,
                                               out_gltype);
      ctx->texture_driver->pixel_format_to_gl (dst_format,
                                               out_glintformat,
                                               NULL,
                                               NULL);

    }
  else
    {
      CoglPixelFormat closest_format;

      closest_format = ctx->texture_driver->pixel_format_to_gl (dst_format,
                                                                out_glintformat,
                                                                out_glformat,
                                                                out_gltype);

      if (closest_format != src_format)
        dst_bmp = _cogl_bitmap_convert_format_and_premult (src_bmp,
                                                           closest_format);
      else
        dst_bmp = cogl_object_ref (src_bmp);
    }

  if (dst_format_out)
    *dst_format_out = dst_format;

  return dst_bmp;
}

void
_cogl_texture_prep_gl_alignment_for_pixels_upload (int pixels_rowstride)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!(pixels_rowstride & 0x7))
    GE( ctx, glPixelStorei (GL_UNPACK_ALIGNMENT, 8) );
  else if (!(pixels_rowstride & 0x3))
    GE( ctx, glPixelStorei (GL_UNPACK_ALIGNMENT, 4) );
  else if (!(pixels_rowstride & 0x1))
    GE( ctx, glPixelStorei (GL_UNPACK_ALIGNMENT, 2) );
  else
    GE( ctx, glPixelStorei (GL_UNPACK_ALIGNMENT, 1) );
}

void
_cogl_texture_prep_gl_alignment_for_pixels_download (int pixels_rowstride)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!(pixels_rowstride & 0x7))
    GE( ctx, glPixelStorei (GL_PACK_ALIGNMENT, 8) );
  else if (!(pixels_rowstride & 0x3))
    GE( ctx, glPixelStorei (GL_PACK_ALIGNMENT, 4) );
  else if (!(pixels_rowstride & 0x1))
    GE( ctx, glPixelStorei (GL_PACK_ALIGNMENT, 2) );
  else
    GE( ctx, glPixelStorei (GL_PACK_ALIGNMENT, 1) );
}

/* FIXME: wrap modes should be set on pipelines not textures */
void
_cogl_texture_set_wrap_mode_parameters (CoglHandle handle,
                                        GLenum wrap_mode_s,
                                        GLenum wrap_mode_t,
                                        GLenum wrap_mode_p)
{
  CoglTexture *tex = COGL_TEXTURE (handle);

  tex->vtable->set_wrap_mode_parameters (tex,
                                         wrap_mode_s,
                                         wrap_mode_t,
                                         wrap_mode_p);
}

/* This is like CoglSpanIter except it deals with floats and it
   effectively assumes there is only one span from 0.0 to 1.0 */
typedef struct _CoglTextureIter
{
  float pos, end, next_pos;
  gboolean flipped;
  float t_1, t_2;
} CoglTextureIter;

static void
_cogl_texture_iter_update (CoglTextureIter *iter)
{
  float t_2;
  float frac_part;

  frac_part = modff (iter->pos, &iter->next_pos);

  /* modff rounds the int part towards zero so we need to add one if
     we're meant to be heading away from zero */
  if (iter->pos >= 0.0f || frac_part == 0.0f)
    iter->next_pos += 1.0f;

  if (iter->next_pos > iter->end)
    t_2 = iter->end;
  else
    t_2 = iter->next_pos;

  if (iter->flipped)
    {
      iter->t_1 = t_2;
      iter->t_2 = iter->pos;
    }
  else
    {
      iter->t_1 = iter->pos;
      iter->t_2 = t_2;
    }
}

static void
_cogl_texture_iter_begin (CoglTextureIter *iter,
                          float t_1, float t_2)
{
  if (t_1 <= t_2)
    {
      iter->pos = t_1;
      iter->end = t_2;
      iter->flipped = FALSE;
    }
  else
    {
      iter->pos = t_2;
      iter->end = t_1;
      iter->flipped = TRUE;
    }

  _cogl_texture_iter_update (iter);
}

static void
_cogl_texture_iter_next (CoglTextureIter *iter)
{
  iter->pos = iter->next_pos;
  _cogl_texture_iter_update (iter);
}

static gboolean
_cogl_texture_iter_end (CoglTextureIter *iter)
{
  return iter->pos >= iter->end;
}

/* This invokes the callback with enough quads to cover the manually
   repeated range specified by the virtual texture coordinates without
   emitting coordinates outside the range [0,1] */
void
_cogl_texture_iterate_manual_repeats (CoglTextureManualRepeatCallback callback,
                                      float tx_1, float ty_1,
                                      float tx_2, float ty_2,
                                      void *user_data)
{
  CoglTextureIter x_iter, y_iter;

  for (_cogl_texture_iter_begin (&y_iter, ty_1, ty_2);
       !_cogl_texture_iter_end (&y_iter);
       _cogl_texture_iter_next (&y_iter))
    for (_cogl_texture_iter_begin (&x_iter, tx_1, tx_2);
         !_cogl_texture_iter_end (&x_iter);
         _cogl_texture_iter_next (&x_iter))
      {
        float coords[4] = { x_iter.t_1, y_iter.t_1, x_iter.t_2, y_iter.t_2 };
        callback (coords, user_data);
      }
}

CoglHandle
cogl_texture_new_with_size (unsigned int     width,
			    unsigned int     height,
                            CoglTextureFlags flags,
			    CoglPixelFormat  internal_format)
{
  CoglHandle tex;

  _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);

  /* First try creating a fast-path non-sliced texture */
  tex = cogl_texture_2d_new_with_size (ctx,
                                       width, height, internal_format,
                                       NULL);

  /* If it fails resort to sliced textures */
  if (tex == COGL_INVALID_HANDLE)
    tex = _cogl_texture_2d_sliced_new_with_size (width,
                                                 height,
                                                 flags,
                                                 internal_format);

  return tex;
}

CoglHandle
cogl_texture_new_from_data (unsigned int      width,
			    unsigned int      height,
                            CoglTextureFlags  flags,
			    CoglPixelFormat   format,
			    CoglPixelFormat   internal_format,
			    unsigned int      rowstride,
			    const guint8     *data)
{
  CoglBitmap *bmp;
  CoglHandle tex;

  if (format == COGL_PIXEL_FORMAT_ANY)
    return COGL_INVALID_HANDLE;

  if (data == NULL)
    return COGL_INVALID_HANDLE;

  /* Rowstride from width if not given */
  if (rowstride == 0)
    rowstride = width * _cogl_get_format_bpp (format);

  /* Wrap the data into a bitmap */
  bmp = _cogl_bitmap_new_from_data ((guint8 *) data,
                                    format,
                                    width,
                                    height,
                                    rowstride,
                                    NULL, NULL);

  tex = cogl_texture_new_from_bitmap (bmp, flags, internal_format);

  cogl_object_unref (bmp);

  return tex;
}

CoglHandle
cogl_texture_new_from_bitmap (CoglHandle       bmp_handle,
                              CoglTextureFlags flags,
                              CoglPixelFormat  internal_format)
{
  CoglHandle tex;

  /* First try putting the texture in the atlas */
  if ((tex = _cogl_atlas_texture_new_from_bitmap (bmp_handle,
                                                  flags,
                                                  internal_format)))
    return tex;

  /* If that doesn't work try a fast path 2D texture */
  if ((tex = _cogl_texture_2d_new_from_bitmap (bmp_handle,
                                               flags,
                                               internal_format,
                                               NULL)))
    return tex;

  /* Otherwise create a sliced texture */
  return _cogl_texture_2d_sliced_new_from_bitmap (bmp_handle,
                                                  flags,
                                                  internal_format);
}

CoglHandle
cogl_texture_new_from_file (const char        *filename,
                            CoglTextureFlags   flags,
                            CoglPixelFormat    internal_format,
                            GError           **error)
{
  CoglBitmap *bmp;
  CoglHandle handle = COGL_INVALID_HANDLE;
  CoglPixelFormat src_format;

  g_return_val_if_fail (error == NULL || *error == NULL, COGL_INVALID_HANDLE);

  bmp = cogl_bitmap_new_from_file (filename, error);
  if (bmp == NULL)
    return COGL_INVALID_HANDLE;

  src_format = _cogl_bitmap_get_format (bmp);

  /* We know that the bitmap data is solely owned by this function so
     we can do the premult conversion in place. This avoids having to
     copy the bitmap which will otherwise happen in
     _cogl_texture_prepare_for_upload */
  internal_format =
    _cogl_texture_determine_internal_format (src_format, internal_format);
  if (!_cogl_texture_needs_premult_conversion (src_format, internal_format) ||
      _cogl_bitmap_convert_premult_status (bmp, src_format ^ COGL_PREMULT_BIT))
    handle = cogl_texture_new_from_bitmap (bmp, flags, internal_format);

  cogl_object_unref (bmp);

  return handle;
}

CoglHandle
cogl_texture_new_from_foreign (GLuint           gl_handle,
			       GLenum           gl_target,
			       GLuint           width,
			       GLuint           height,
			       GLuint           x_pot_waste,
			       GLuint           y_pot_waste,
			       CoglPixelFormat  format)
{
#if HAVE_COGL_GL
  if (gl_target == GL_TEXTURE_RECTANGLE_ARB)
    {
      if (x_pot_waste != 0 || y_pot_waste != 0)
        {
          /* It shouldn't be necessary to have waste in this case since
           * the texture isn't limited to power of two sizes. */
          g_warning ("You can't create a foreign GL_TEXTURE_RECTANGLE cogl "
                     "texture with waste\n");
          return COGL_INVALID_HANDLE;
        }

      return _cogl_texture_rectangle_new_from_foreign (gl_handle,
                                                       width,
                                                       height,
                                                       format);
    }
#endif

  if (x_pot_waste != 0 || y_pot_waste != 0)
    return _cogl_texture_2d_sliced_new_from_foreign (gl_handle,
                                                     gl_target,
                                                     width,
                                                     height,
                                                     x_pot_waste,
                                                     y_pot_waste,
                                                     format);
  else
    {
      _COGL_GET_CONTEXT (ctx, COGL_INVALID_HANDLE);
      return cogl_texture_2d_new_from_foreign (ctx,
                                               gl_handle,
                                               width,
                                               height,
                                               format,
                                               NULL);
    }
}

gboolean
_cogl_texture_is_foreign (CoglHandle handle)
{
  CoglTexture *tex;

  g_return_val_if_fail (cogl_is_texture (handle), FALSE);

  tex = COGL_TEXTURE (handle);

  if (tex->vtable->is_foreign)
    return tex->vtable->is_foreign (tex);
  else
    return FALSE;
}

CoglHandle
cogl_texture_new_from_sub_texture (CoglHandle full_texture,
                                   int        sub_x,
                                   int        sub_y,
                                   int        sub_width,
                                   int        sub_height)
{
  return _cogl_sub_texture_new (full_texture, sub_x, sub_y,
                                sub_width, sub_height);
}

CoglHandle
cogl_texture_new_from_buffer_EXP (CoglHandle          buffer,
                                  unsigned int        width,
                                  unsigned int        height,
                                  CoglTextureFlags    flags,
                                  CoglPixelFormat     format,
                                  CoglPixelFormat     internal_format,
                                  unsigned int        rowstride,
                                  const unsigned int  offset)
{
  CoglHandle texture;
  CoglBuffer *cogl_buffer;
  CoglPixelBuffer *pixel_buffer;
  CoglBitmap *bmp;

  g_return_val_if_fail (cogl_is_buffer (buffer), COGL_INVALID_HANDLE);

  if (format == COGL_PIXEL_FORMAT_ANY)
    return COGL_INVALID_HANDLE;

  cogl_buffer = COGL_BUFFER (buffer);
  pixel_buffer = COGL_PIXEL_BUFFER (buffer);

  /* Rowstride from CoglBuffer or even width * bpp if not given */
  if (rowstride == 0)
    rowstride = pixel_buffer->stride;
  if (rowstride == 0)
    rowstride = width * _cogl_get_format_bpp (format);

  /* use the CoglBuffer height and width as last resort */
  if (width == 0)
    width = pixel_buffer->width;
  if (height == 0)
    height = pixel_buffer->height;
  if (width == 0 || height == 0)
    {
      /* no width or height specified, neither at creation time (because the
       * array was created by cogl_pixel_buffer_new()) nor when calling this
       * function */
      return COGL_INVALID_HANDLE;
    }

  /* Wrap the buffer into a bitmap */
  bmp = _cogl_bitmap_new_from_buffer (cogl_buffer,
                                      format,
                                      width, height,
                                      rowstride,
                                      offset);

  texture = cogl_texture_new_from_bitmap (bmp, flags, internal_format);

  cogl_object_unref (bmp);

  return texture;
}

unsigned int
cogl_texture_get_width (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return 0;

  tex = COGL_TEXTURE (handle);

  return tex->vtable->get_width (tex);
}

unsigned int
cogl_texture_get_height (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return 0;

  tex = COGL_TEXTURE (handle);

  return tex->vtable->get_height (tex);
}

CoglPixelFormat
cogl_texture_get_format (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return COGL_PIXEL_FORMAT_ANY;

  tex = COGL_TEXTURE (handle);

  return tex->vtable->get_format (tex);
}

unsigned int
cogl_texture_get_rowstride (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return 0;

  /* FIXME: This function should go away. It previously just returned
     the rowstride that was used to upload the data as far as I can
     tell. This is not helpful */

  tex = COGL_TEXTURE (handle);

  /* Just guess at a suitable rowstride */
  return (_cogl_get_format_bpp (cogl_texture_get_format (tex))
          * cogl_texture_get_width (tex));
}

int
cogl_texture_get_max_waste (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return 0;

  tex = COGL_TEXTURE (handle);

  return tex->vtable->get_max_waste (tex);
}

gboolean
cogl_texture_is_sliced (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return FALSE;

  tex = COGL_TEXTURE (handle);

  return tex->vtable->is_sliced (tex);
}

/* Some CoglTextures, notably sliced textures or atlas textures when repeating
 * is used, will need to divide the coordinate space into multiple GL textures
 * (or rather; in the case of atlases duplicate a single texture in multiple
 * positions to handle repeating)
 *
 * This function helps you implement primitives using such textures by
 * invoking a callback once for each sub texture that intersects a given
 * region specified in texture coordinates.
 */
void
_cogl_texture_foreach_sub_texture_in_region (CoglHandle handle,
                                             float virtual_tx_1,
                                             float virtual_ty_1,
                                             float virtual_tx_2,
                                             float virtual_ty_2,
                                             CoglTextureSliceCallback callback,
                                             void *user_data)
{
  CoglTexture *tex = COGL_TEXTURE (handle);

  tex->vtable->foreach_sub_texture_in_region (tex,
                                              virtual_tx_1,
                                              virtual_ty_1,
                                              virtual_tx_2,
                                              virtual_ty_2,
                                              callback,
                                              user_data);
}

/* If this returns FALSE, that implies _foreach_sub_texture_in_region
 * will be needed to iterate over multiple sub textures for regions whos
 * texture coordinates extend out of the range [0,1]
 */
gboolean
_cogl_texture_can_hardware_repeat (CoglHandle handle)
{
  CoglTexture *tex = (CoglTexture *)handle;

  return tex->vtable->can_hardware_repeat (tex);
}

/* NB: You can't use this with textures comprised of multiple sub textures (use
 * cogl_texture_is_sliced() to check) since coordinate transformation for such
 * textures will be different for each slice. */
void
_cogl_texture_transform_coords_to_gl (CoglHandle handle,
                                      float *s,
                                      float *t)
{
  CoglTexture *tex = COGL_TEXTURE (handle);

  tex->vtable->transform_coords_to_gl (tex, s, t);
}

CoglTransformResult
_cogl_texture_transform_quad_coords_to_gl (CoglHandle handle,
                                           float *coords)
{
  CoglTexture *tex = COGL_TEXTURE (handle);

  return tex->vtable->transform_quad_coords_to_gl (tex, coords);
}

GLenum
_cogl_texture_get_gl_format (CoglHandle handle)
{
  CoglTexture *tex = COGL_TEXTURE (handle);

  return tex->vtable->get_gl_format (tex);
}

gboolean
cogl_texture_get_gl_texture (CoglHandle handle,
			     GLuint *out_gl_handle,
			     GLenum *out_gl_target)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return FALSE;

  tex = COGL_TEXTURE (handle);

  return tex->vtable->get_gl_texture (tex, out_gl_handle, out_gl_target);
}

void
_cogl_texture_set_filters (CoglHandle handle,
                           GLenum min_filter,
                           GLenum mag_filter)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return;

  tex = COGL_TEXTURE (handle);

  tex->vtable->set_filters (tex, min_filter, mag_filter);
}

void
_cogl_texture_pre_paint (CoglHandle handle, CoglTexturePrePaintFlags flags)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return;

  tex = COGL_TEXTURE (handle);

  tex->vtable->pre_paint (tex, flags);
}

void
_cogl_texture_ensure_non_quad_rendering (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return;

  tex = COGL_TEXTURE (handle);

  tex->vtable->ensure_non_quad_rendering (tex);
}

gboolean
_cogl_texture_set_region_from_bitmap (CoglHandle    handle,
                                      int           src_x,
                                      int           src_y,
                                      int           dst_x,
                                      int           dst_y,
                                      unsigned int  dst_width,
                                      unsigned int  dst_height,
                                      CoglBitmap   *bmp)
{
  CoglTexture *tex = COGL_TEXTURE (handle);
  GLenum       closest_gl_format;
  GLenum       closest_gl_type;
  gboolean     ret;

  /* Shortcut out early if the image is empty */
  if (dst_width == 0 || dst_height == 0)
    return TRUE;

  /* Prepare the bitmap so that it will do the premultiplication
     conversion */
  bmp = _cogl_texture_prepare_for_upload (bmp,
                                          cogl_texture_get_format (handle),
                                          NULL,
                                          NULL,
                                          &closest_gl_format,
                                          &closest_gl_type);

  ret = tex->vtable->set_region (handle,
                                 src_x, src_y,
                                 dst_x, dst_y,
                                 dst_width, dst_height,
                                 bmp);

  cogl_object_unref (bmp);

  return ret;
}

gboolean
cogl_texture_set_region (CoglHandle       handle,
			 int              src_x,
			 int              src_y,
			 int              dst_x,
			 int              dst_y,
			 unsigned int     dst_width,
			 unsigned int     dst_height,
			 int              width,
			 int              height,
			 CoglPixelFormat  format,
			 unsigned int     rowstride,
			 const guint8    *data)
{
  CoglBitmap *source_bmp;
  gboolean    ret;

  /* Check for valid format */
  if (format == COGL_PIXEL_FORMAT_ANY)
    return FALSE;

  /* Rowstride from width if none specified */
  if (rowstride == 0)
    rowstride = _cogl_get_format_bpp (format) * width;

  /* Init source bitmap */
  source_bmp = _cogl_bitmap_new_from_data ((guint8 *) data,
                                           format,
                                           width,
                                           height,
                                           rowstride,
                                           NULL, /* destroy_fn */
                                           NULL); /* destroy_fn_data */

  ret = _cogl_texture_set_region_from_bitmap (handle,
                                              src_x, src_y,
                                              dst_x, dst_y,
                                              dst_width, dst_height,
                                              source_bmp);

  cogl_object_unref (source_bmp);

  return ret;
}

/* Reads back the contents of a texture by rendering it to the framebuffer
 * and reading back the resulting pixels.
 *
 * It will perform multiple renders if the texture is larger than the
 * current glViewport.
 *
 * It assumes the projection and modelview have already been setup so
 * that rendering to 0,0 with the same width and height of the viewport
 * will exactly cover the viewport.
 *
 * NB: Normally this approach isn't normally used since we can just use
 * glGetTexImage, but may be used as a fallback in some circumstances.
 */
static void
do_texture_draw_and_read (CoglHandle   handle,
                          CoglBitmap  *target_bmp,
                          float       *viewport)
{
  int         bpp;
  float       rx1, ry1;
  float       rx2, ry2;
  float       tx1, ty1;
  float       tx2, ty2;
  int         bw,  bh;
  CoglBitmap  *rect_bmp;
  unsigned int  tex_width, tex_height;

  bpp = _cogl_get_format_bpp (COGL_PIXEL_FORMAT_RGBA_8888);

  tex_width = cogl_texture_get_width (handle);
  tex_height = cogl_texture_get_height (handle);

  ry2 = 0;
  ty2 = 0;

  /* Walk Y axis until whole bitmap height consumed */
  for (bh = tex_height; bh > 0; bh -= viewport[3])
    {
      /* Rectangle Y coords */
      ry1 = ry2;
      ry2 += (bh < viewport[3]) ? bh : viewport[3];

      /* Normalized texture Y coords */
      ty1 = ty2;
      ty2 = (ry2 / (float) tex_height);

      rx2 = 0;
      tx2 = 0;

      /* Walk X axis until whole bitmap width consumed */
      for (bw = tex_width; bw > 0; bw-=viewport[2])
        {
          int width;
          int height;
          int rowstride;
          guint8 *data;

          /* Rectangle X coords */
          rx1 = rx2;
          rx2 += (bw < viewport[2]) ? bw : viewport[2];

          width = rx2 - rx1;
          height = ry2 - ry1;
          rowstride = width * bpp;

          /* Normalized texture X coords */
          tx1 = tx2;
          tx2 = (rx2 / (float) tex_width);

          /* Draw a portion of texture */
          cogl_rectangle_with_texture_coords (0, 0,
                                              rx2 - rx1,
                                              ry2 - ry1,
                                              tx1, ty1,
                                              tx2, ty2);

          data = g_malloc (height * rowstride);

          /* Read into a temporary bitmap */
          rect_bmp =
            _cogl_bitmap_new_from_data (data,
                                        COGL_PIXEL_FORMAT_RGBA_8888,
                                        width,
                                        height,
                                        rowstride,
                                        (CoglBitmapDestroyNotify) g_free,
                                        NULL);

          cogl_read_pixels (viewport[0], viewport[1],
                            width,
                            height,
                            COGL_READ_PIXELS_COLOR_BUFFER,
                            COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                            data);

          /* Copy to target bitmap */
          _cogl_bitmap_copy_subregion (rect_bmp,
                                       target_bmp,
                                       0,0,
                                       rx1,ry1,
                                       width,
                                       height);

          /* Free temp bitmap */
          cogl_object_unref (rect_bmp);
        }
    }
}

/* Reads back the contents of a texture by rendering it to the framebuffer
 * and reading back the resulting pixels.
 *
 * NB: Normally this approach isn't normally used since we can just use
 * glGetTexImage, but may be used as a fallback in some circumstances.
 */
gboolean
_cogl_texture_draw_and_read (CoglHandle   handle,
                             CoglBitmap  *target_bmp,
                             GLuint       target_gl_format,
                             GLuint       target_gl_type)
{
  int        bpp;
  CoglFramebuffer *framebuffer;
  float viewport[4];
  CoglBitmap *alpha_bmp;
  CoglMatrixStack *projection_stack;
  CoglMatrixStack *modelview_stack;
  int target_width = _cogl_bitmap_get_width (target_bmp);
  int target_height = _cogl_bitmap_get_height (target_bmp);
  int target_rowstride = _cogl_bitmap_get_rowstride (target_bmp);

  _COGL_GET_CONTEXT (ctx, FALSE);

  bpp = _cogl_get_format_bpp (COGL_PIXEL_FORMAT_RGBA_8888);

  framebuffer = cogl_get_draw_framebuffer ();
  /* Viewport needs to have some size and be inside the window for this */
  cogl_framebuffer_get_viewport4fv (framebuffer, viewport);
  if (viewport[0] <  0 || viewport[1] <  0 ||
      viewport[2] <= 0 || viewport[3] <= 0)
    return FALSE;

  /* Setup orthographic projection into current viewport (0,0 in top-left
   * corner to draw the texture upside-down so we match the way cogl_read_pixels
   * works)
   */

  projection_stack = _cogl_framebuffer_get_projection_stack (framebuffer);
  _cogl_matrix_stack_push (projection_stack);
  _cogl_matrix_stack_load_identity (projection_stack);
  _cogl_matrix_stack_ortho (projection_stack,
                            0, viewport[2],
                            viewport[3], 0,
                            0,
                            100);

  modelview_stack = _cogl_framebuffer_get_modelview_stack (framebuffer);
  _cogl_matrix_stack_push (modelview_stack);
  _cogl_matrix_stack_load_identity (modelview_stack);

  /* Direct copy operation */

  if (ctx->texture_download_pipeline == COGL_INVALID_HANDLE)
    {
      ctx->texture_download_pipeline = cogl_pipeline_new ();
      cogl_pipeline_set_blend (ctx->texture_download_pipeline,
                               "RGBA = ADD (SRC_COLOR, 0)",
                               NULL);
    }

  cogl_push_source (ctx->texture_download_pipeline);

  cogl_pipeline_set_layer_texture (ctx->texture_download_pipeline, 0, handle);

  cogl_pipeline_set_layer_combine (ctx->texture_download_pipeline,
                                   0, /* layer */
                                   "RGBA = REPLACE (TEXTURE)",
                                   NULL);

  cogl_pipeline_set_layer_filters (ctx->texture_download_pipeline, 0,
                                   COGL_PIPELINE_FILTER_NEAREST,
                                   COGL_PIPELINE_FILTER_NEAREST);

  do_texture_draw_and_read (handle, target_bmp, viewport);

  /* Check whether texture has alpha and framebuffer not */
  /* FIXME: For some reason even if ALPHA_BITS is 8, the framebuffer
     still doesn't seem to have an alpha buffer. This might be just
     a PowerVR issue.
  GLint r_bits, g_bits, b_bits, a_bits;
  GE( ctx, glGetIntegerv (GL_ALPHA_BITS, &a_bits) );
  GE( ctx, glGetIntegerv (GL_RED_BITS, &r_bits) );
  GE( ctx, glGetIntegerv (GL_GREEN_BITS, &g_bits) );
  GE( ctx, glGetIntegerv (GL_BLUE_BITS, &b_bits) );
  printf ("R bits: %d\n", r_bits);
  printf ("G bits: %d\n", g_bits);
  printf ("B bits: %d\n", b_bits);
  printf ("A bits: %d\n", a_bits); */
  if ((cogl_texture_get_format (handle) & COGL_A_BIT)/* && a_bits == 0*/)
    {
      guint8 *srcdata;
      guint8 *dstdata;
      guint8 *srcpixel;
      guint8 *dstpixel;
      int     x,y;
      int     alpha_rowstride = bpp * target_width;

      if ((dstdata = _cogl_bitmap_map (target_bmp,
                                       COGL_BUFFER_ACCESS_WRITE,
                                       COGL_BUFFER_MAP_HINT_DISCARD)) == NULL)
        return FALSE;

      srcdata = g_malloc (alpha_rowstride * target_height);

      /* Create temp bitmap for alpha values */
      alpha_bmp = _cogl_bitmap_new_from_data (srcdata,
                                              COGL_PIXEL_FORMAT_RGBA_8888,
                                              target_width, target_height,
                                              alpha_rowstride,
                                              (CoglBitmapDestroyNotify) g_free,
                                              NULL);

      /* Draw alpha values into RGB channels */
      cogl_pipeline_set_layer_combine (ctx->texture_download_pipeline,
                                       0, /* layer */
                                       "RGBA = REPLACE (TEXTURE[A])",
                                       NULL);

      do_texture_draw_and_read (handle, alpha_bmp, viewport);

      /* Copy temp R to target A */

      for (y=0; y<target_height; ++y)
        {
          for (x=0; x<target_width; ++x)
            {
              srcpixel = srcdata + x*bpp;
              dstpixel = dstdata + x*bpp;
              dstpixel[3] = srcpixel[0];
            }
          srcdata += alpha_rowstride;
          dstdata += target_rowstride;
        }

      _cogl_bitmap_unmap (target_bmp);

      cogl_object_unref (alpha_bmp);
    }

  /* Restore old state */
  _cogl_matrix_stack_pop (modelview_stack);
  _cogl_matrix_stack_pop (projection_stack);

  /* restore the original pipeline */
  cogl_pop_source ();

  return TRUE;
}

static gboolean
get_texture_bits_via_offscreen (CoglHandle      texture_handle,
                                int             x,
                                int             y,
                                int             width,
                                int             height,
                                guint8         *dst_bits,
                                unsigned int    dst_rowstride,
                                CoglPixelFormat dst_format)
{
  CoglFramebuffer *framebuffer;

  _COGL_GET_CONTEXT (ctx, FALSE);

  if (!cogl_features_available (COGL_FEATURE_OFFSCREEN))
    return FALSE;

  framebuffer = _cogl_offscreen_new_to_texture_full
                                      (texture_handle,
                                       COGL_OFFSCREEN_DISABLE_DEPTH_AND_STENCIL,
                                       0);

  if (framebuffer == NULL)
    return FALSE;

  cogl_push_framebuffer (framebuffer);

  _cogl_read_pixels_with_rowstride (x, y, width, height,
                                    COGL_READ_PIXELS_COLOR_BUFFER,
                                    dst_format, dst_bits, dst_rowstride);

  cogl_pop_framebuffer ();

  cogl_object_unref (framebuffer);

  return TRUE;
}

static gboolean
get_texture_bits_via_copy (CoglHandle      texture_handle,
                           int             x,
                           int             y,
                           int             width,
                           int             height,
                           guint8         *dst_bits,
                           unsigned int    dst_rowstride,
                           CoglPixelFormat dst_format)
{
  CoglTexture *tex = COGL_TEXTURE (texture_handle);
  unsigned int full_rowstride;
  guint8 *full_bits;
  gboolean ret = TRUE;
  int bpp;
  int full_tex_width, full_tex_height;

  full_tex_width = cogl_texture_get_width (texture_handle);
  full_tex_height = cogl_texture_get_height (texture_handle);

  bpp = _cogl_get_format_bpp (dst_format);

  full_rowstride = bpp * full_tex_width;
  full_bits = g_malloc (full_rowstride * full_tex_height);

  if (tex->vtable->get_data (tex,
                             dst_format,
                             full_rowstride,
                             full_bits))
    {
      guint8 *dst = dst_bits;
      guint8 *src = full_bits + x * bpp + y * full_rowstride;
      int i;

      for (i = 0; i < height; i++)
        {
          memcpy (dst, src, bpp * width);
          dst += dst_rowstride;
          src += full_rowstride;
        }
    }
  else
    ret = FALSE;

  g_free (full_bits);

  return ret;
}

typedef struct
{
  int         orig_width;
  int         orig_height;
  CoglBitmap *target_bmp;
  guint8     *target_bits;
  gboolean    success;
} CoglTextureGetData;

static void
texture_get_cb (CoglHandle   texture_handle,
                const float *subtexture_coords,
                const float *virtual_coords,
                void        *user_data)
{
  CoglTexture *tex = COGL_TEXTURE (texture_handle);
  CoglTextureGetData *tg_data = user_data;
  CoglPixelFormat format = _cogl_bitmap_get_format (tg_data->target_bmp);
  int bpp = _cogl_get_format_bpp (format);
  unsigned int rowstride = _cogl_bitmap_get_rowstride (tg_data->target_bmp);
  int subtexture_width = cogl_texture_get_width (texture_handle);
  int subtexture_height = cogl_texture_get_height (texture_handle);

  int x_in_subtexture = (int) (0.5 + subtexture_width * subtexture_coords[0]);
  int y_in_subtexture = (int) (0.5 + subtexture_height * subtexture_coords[1]);
  int width = ((int) (0.5 + subtexture_width * subtexture_coords[2])
               - x_in_subtexture);
  int height = ((int) (0.5 + subtexture_height * subtexture_coords[3])
                - y_in_subtexture);
  int x_in_bitmap = (int) (0.5 + tg_data->orig_width * virtual_coords[0]);
  int y_in_bitmap = (int) (0.5 + tg_data->orig_height * virtual_coords[1]);

  guint8 *dst_bits;

  if (!tg_data->success)
    return;

  dst_bits = tg_data->target_bits + x_in_bitmap * bpp + y_in_bitmap * rowstride;

  /* If we can read everything as a single slice, then go ahead and do that
   * to avoid allocating an FBO. We'll leave it up to the GL implementation to
   * do glGetTexImage as efficiently as possible. (GLES doesn't have that,
   * so we'll fall through) */
  if (x_in_subtexture == 0 && y_in_subtexture == 0 &&
      width == subtexture_width && height == subtexture_height)
    {
      if (tex->vtable->get_data (tex,
                                 format,
                                 rowstride,
                                 dst_bits))
        return;
    }

  /* Next best option is a FBO and glReadPixels */
  if (get_texture_bits_via_offscreen (texture_handle,
                                      x_in_subtexture, y_in_subtexture,
                                      width, height,
                                      dst_bits,
                                      rowstride,
                                      format))
    return;

  /* Getting ugly: read the entire texture, copy out the part we want */
  if (get_texture_bits_via_copy (texture_handle,
                                 x_in_subtexture, y_in_subtexture,
                                 width, height,
                                 dst_bits,
                                 rowstride,
                                 format))
    return;

  /* No luck, the caller will fall back to the draw-to-backbuffer and
   * read implementation */
  tg_data->success = FALSE;
}

int
cogl_texture_get_data (CoglHandle       handle,
		       CoglPixelFormat  format,
		       unsigned int     rowstride,
		       guint8          *data)
{
  CoglTexture     *tex;
  int              bpp;
  int              byte_size;
  CoglPixelFormat  closest_format;
  int              closest_bpp;
  GLenum           closest_gl_format;
  GLenum           closest_gl_type;
  CoglBitmap      *target_bmp;
  CoglBitmap      *new_bmp;
  guint8          *src;
  guint8          *dst;
  int              y;
  int              tex_width;
  int              tex_height;

  CoglTextureGetData tg_data;

  _COGL_GET_CONTEXT (ctx, 0);

  if (!cogl_is_texture (handle))
    return 0;

  tex = COGL_TEXTURE (handle);

  /* Default to internal format if none specified */
  if (format == COGL_PIXEL_FORMAT_ANY)
    format = cogl_texture_get_format (handle);

  tex_width = cogl_texture_get_width (handle);
  tex_height = cogl_texture_get_height (handle);

  /* Rowstride from texture width if none specified */
  bpp = _cogl_get_format_bpp (format);
  if (rowstride == 0)
    rowstride = tex_width * bpp;

  /* Return byte size if only that requested */
  byte_size = tex_height * rowstride;
  if (data == NULL)
    return byte_size;

  closest_format =
    ctx->texture_driver->find_best_gl_get_data_format (format,
                                                       &closest_gl_format,
                                                       &closest_gl_type);
  closest_bpp = _cogl_get_format_bpp (closest_format);

  /* Is the requested format supported? */
  if (closest_format == format)
    /* Target user data directly */
    target_bmp = _cogl_bitmap_new_from_data (data,
                                             format,
                                             tex_width,
                                             tex_height,
                                             rowstride,
                                             NULL, NULL);
  else
    {
      int target_rowstride = tex_width * closest_bpp;
      guint8 *target_data = g_malloc (tex_height * target_rowstride);
      target_bmp = _cogl_bitmap_new_from_data (target_data,
                                               closest_format,
                                               tex_width,
                                               tex_height,
                                               target_rowstride,
                                               (CoglBitmapDestroyNotify) g_free,
                                               NULL);
    }

  tg_data.orig_width = tex_width;
  tg_data.orig_height = tex_height;
  tg_data.target_bmp = target_bmp;
  tg_data.target_bits = _cogl_bitmap_map (target_bmp, COGL_BUFFER_ACCESS_WRITE,
                                          COGL_BUFFER_MAP_HINT_DISCARD);
  if (tg_data.target_bits == NULL)
    {
      cogl_object_unref (target_bmp);
      return 0;
    }
  tg_data.success = TRUE;

  /* Iterating through the subtextures allows piecing together
   * the data for a sliced texture, and allows us to do the
   * read-from-framebuffer logic here in a simple fashion rather than
   * passing offsets down through the code. */
  _cogl_texture_foreach_sub_texture_in_region (handle,
                                               0, 0, 1, 1,
                                               texture_get_cb,
                                               &tg_data);

  _cogl_bitmap_unmap (target_bmp);

  /* XXX: In some cases _cogl_texture_2d_download_from_gl may fail
   * to read back the texture data; such as for GLES which doesn't
   * support glGetTexImage, so here we fallback to drawing the
   * texture and reading the pixels from the framebuffer. */
  if (!tg_data.success)
    _cogl_texture_draw_and_read (tex, target_bmp,
                                 closest_gl_format,
                                 closest_gl_type);

  /* Was intermediate used? */
  if (closest_format != format)
    {
      guint8 *new_bmp_data;
      int new_bmp_rowstride;

      /* Convert to requested format */
      new_bmp = _cogl_bitmap_convert_format_and_premult (target_bmp,
                                                         format);

      /* Free intermediate data and return if failed */
      cogl_object_unref (target_bmp);

      if (new_bmp == NULL)
        return 0;

      new_bmp_rowstride = _cogl_bitmap_get_rowstride (new_bmp);
      new_bmp_data = _cogl_bitmap_map (new_bmp, COGL_BUFFER_ACCESS_WRITE,
                                       COGL_BUFFER_MAP_HINT_DISCARD);

      if (new_bmp_data == NULL)
        {
          cogl_object_unref (new_bmp);
          return 0;
        }

      /* Copy to user buffer */
      for (y = 0; y < tex_height; ++y)
        {
          src = new_bmp_data + y * new_bmp_rowstride;
          dst = data + y * rowstride;
          memcpy (dst, src, tex_width * bpp);
        }

      _cogl_bitmap_unmap (new_bmp);

      /* Free converted data */
      cogl_object_unref (new_bmp);
    }

  return byte_size;
}

static void
_cogl_texture_framebuffer_destroy_cb (void *user_data,
                                      void *instance)
{
  CoglTexture *tex = user_data;
  CoglFramebuffer *framebuffer = instance;

  tex->framebuffers = g_list_remove (tex->framebuffers, framebuffer);
}

void
_cogl_texture_associate_framebuffer (CoglHandle handle,
                                     CoglFramebuffer *framebuffer)
{
  CoglTexture *tex = COGL_TEXTURE (handle);
  static CoglUserDataKey framebuffer_destroy_notify_key;

  /* Note: we don't take a reference on the framebuffer here because
   * that would introduce a circular reference. */
  tex->framebuffers = g_list_prepend (tex->framebuffers, framebuffer);

  /* Since we haven't taken a reference on the framebuffer we setup
   * some private data so we will be notified if it is destroyed... */
  _cogl_object_set_user_data (COGL_OBJECT (framebuffer),
                              &framebuffer_destroy_notify_key,
                              tex,
                              _cogl_texture_framebuffer_destroy_cb);
}

const GList *
_cogl_texture_get_associated_framebuffers (CoglHandle handle)
{
  CoglTexture *tex = COGL_TEXTURE (handle);
  return tex->framebuffers;
}

void
_cogl_texture_flush_journal_rendering (CoglHandle handle)
{
  CoglTexture *tex = COGL_TEXTURE (handle);
  GList *l;

  /* It could be that a referenced texture is part of a framebuffer
   * which has an associated journal that must be flushed before it
   * can be sampled from by the current primitive... */
  for (l = tex->framebuffers; l; l = l->next)
    _cogl_framebuffer_flush_journal (l->data);
}
