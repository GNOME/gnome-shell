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
#include "cogl-texture-private.h"
#include "cogl-texture-driver.h"
#include "cogl-texture-2d-sliced-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-sub-texture-private.h"
#include "cogl-atlas-texture-private.h"
#include "cogl-material.h"
#include "cogl-context.h"
#include "cogl-handle.h"
#include "cogl-primitives.h"
#include "cogl-framebuffer-private.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

/* XXX:
 * The CoglHandle macros don't support any form of inheritance, so for
 * now we implement the CoglHandle support for the CoglTexture
 * abstract class manually.
 */

gboolean
cogl_is_texture (CoglHandle handle)
{
  CoglHandleObject *obj = (CoglHandleObject *)handle;

  if (handle == COGL_INVALID_HANDLE)
    return FALSE;

  return (obj->klass->type == _cogl_handle_texture_2d_get_type () ||
          obj->klass->type == _cogl_handle_atlas_texture_get_type () ||
          obj->klass->type == _cogl_handle_texture_2d_sliced_get_type () ||
          obj->klass->type == _cogl_handle_sub_texture_get_type ());
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
_cogl_texture_upload_data_free (CoglTextureUploadData *data)
{
  if (data->bitmap.data != NULL && data->bitmap_owner)
    g_free (data->bitmap.data);

  data->bitmap.data = NULL;
  data->bitmap_owner = FALSE;
}

void
_cogl_texture_upload_data_swap_bitmap (CoglTextureUploadData *data,
                                       CoglBitmap            *new_bitmap)
{
  if (data->bitmap.data != NULL && data->bitmap_owner)
    g_free (data->bitmap.data);

  data->bitmap = *new_bitmap;
  data->bitmap_owner = TRUE;
}

void
_cogl_texture_prep_gl_alignment_for_pixels_upload (int pixels_rowstride)
{
  if (!(pixels_rowstride & 0x7))
    GE( glPixelStorei (GL_UNPACK_ALIGNMENT, 8) );
  else if (!(pixels_rowstride & 0x3))
    GE( glPixelStorei (GL_UNPACK_ALIGNMENT, 4) );
  else if (!(pixels_rowstride & 0x1))
    GE( glPixelStorei (GL_UNPACK_ALIGNMENT, 2) );
  else
    GE( glPixelStorei (GL_UNPACK_ALIGNMENT, 1) );
}

void
_cogl_texture_prep_gl_alignment_for_pixels_download (int pixels_rowstride)
{
  if (!(pixels_rowstride & 0x7))
    GE( glPixelStorei (GL_PACK_ALIGNMENT, 8) );
  else if (!(pixels_rowstride & 0x3))
    GE( glPixelStorei (GL_PACK_ALIGNMENT, 4) );
  else if (!(pixels_rowstride & 0x1))
    GE( glPixelStorei (GL_PACK_ALIGNMENT, 2) );
  else
    GE( glPixelStorei (GL_PACK_ALIGNMENT, 1) );
}

/* FIXME: wrap modes should be set on materials not textures */
void
_cogl_texture_set_wrap_mode_parameter (CoglHandle handle,
                                       GLenum wrap_mode)
{
  CoglTexture *tex = COGL_TEXTURE (handle);

  tex->vtable->set_wrap_mode_parameter (tex, wrap_mode);
}

gboolean
_cogl_texture_upload_data_prepare_format
                                    (CoglTextureUploadData *data,
                                     CoglPixelFormat       *internal_format)
{
  /* Was there any internal conversion requested?
   * By default Cogl will use a premultiplied internal format. Later we will
   * add control over this. */
  if (*internal_format == COGL_PIXEL_FORMAT_ANY)
    {
      if ((data->bitmap.format & COGL_A_BIT) &&
          data->bitmap.format != COGL_PIXEL_FORMAT_A_8)
        *internal_format = data->bitmap.format | COGL_PREMULT_BIT;
      else
        *internal_format = data->bitmap.format;
    }

  /* Find closest format accepted by GL */
  *internal_format = _cogl_pixel_format_to_gl (*internal_format,
                                               &data->gl_intformat,
                                               &data->gl_format,
                                               &data->gl_type);

  return TRUE;
}

gboolean
_cogl_texture_upload_data_convert (CoglTextureUploadData *data,
                                   CoglPixelFormat internal_format)
{
  CoglBitmap        new_bitmap;
  gboolean          success;

  /* Convert to internal format */
  if (internal_format != data->bitmap.format)
    {
      success = _cogl_bitmap_convert_and_premult (&data->bitmap,
						  &new_bitmap,
						  internal_format);

      if (!success)
	return FALSE;

      /* Update texture with new data */
      _cogl_texture_upload_data_swap_bitmap (data, &new_bitmap);
    }

  return TRUE;
}

gboolean
_cogl_texture_upload_data_prepare (CoglTextureUploadData *data,
                                   CoglPixelFormat       internal_format)
{
  return (_cogl_texture_upload_data_prepare_format (data, &internal_format) &&
          _cogl_texture_upload_data_convert (data, internal_format));
}

/* This is like CoglSpanIter except it deals with floats and it
   effectively assumes there is only one span from 0.0 to 1.0 */
typedef struct _CoglTextureIter
{
  gfloat pos, end, next_pos;
  gboolean flipped;
  gfloat t_1, t_2;
} CoglTextureIter;

static void
_cogl_texture_iter_update (CoglTextureIter *iter)
{
  gfloat t_2;
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
                              gfloat t_1, gfloat t_2)
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
cogl_texture_new_with_size (guint            width,
			    guint            height,
                            CoglTextureFlags flags,
			    CoglPixelFormat  internal_format)
{
  CoglHandle tex;

  /* First try creating a fast-path non-sliced texture */
  tex = _cogl_texture_2d_new_with_size (width, height, flags, internal_format);

  /* If it fails resort to sliced textures */
  if (tex == COGL_INVALID_HANDLE)
    tex = _cogl_texture_2d_sliced_new_with_size (width,
                                                 height,
                                                 flags,
                                                 internal_format);

  return tex;
}

CoglHandle
cogl_texture_new_from_data (guint             width,
			    guint             height,
                            CoglTextureFlags  flags,
			    CoglPixelFormat   format,
			    CoglPixelFormat   internal_format,
			    guint             rowstride,
			    const guchar     *data)
{
  CoglBitmap bitmap;

  if (format == COGL_PIXEL_FORMAT_ANY)
    return COGL_INVALID_HANDLE;

  if (data == NULL)
    return COGL_INVALID_HANDLE;

  /* Rowstride from width if not given */
  if (rowstride == 0)
    rowstride = width * _cogl_get_format_bpp (format);

  /* Wrap the data into a bitmap */
  bitmap.width = width;
  bitmap.height = height;
  bitmap.data = (guchar *) data;
  bitmap.format = format;
  bitmap.rowstride = rowstride;

  return cogl_texture_new_from_bitmap (&bitmap, flags, internal_format);
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
                                               internal_format)))
    return tex;

  /* Otherwise create a sliced texture */
  return _cogl_texture_2d_sliced_new_from_bitmap (bmp_handle,
                                                  flags,
                                                  internal_format);
}

CoglHandle
cogl_texture_new_from_file (const gchar       *filename,
                            CoglTextureFlags   flags,
                            CoglPixelFormat    internal_format,
                            GError           **error)
{
  CoglHandle bmp;
  CoglHandle handle;

  g_return_val_if_fail (error == NULL || *error == NULL, COGL_INVALID_HANDLE);

  bmp = cogl_bitmap_new_from_file (filename, error);
  if (bmp == COGL_INVALID_HANDLE)
    return COGL_INVALID_HANDLE;

  handle = cogl_texture_new_from_bitmap (bmp, flags, internal_format);
  cogl_handle_unref (bmp);

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
  return _cogl_texture_2d_sliced_new_from_foreign (gl_handle,
                                                   gl_target,
                                                   width,
                                                   height,
                                                   x_pot_waste,
                                                   y_pot_waste,
                                                   format);
}

CoglHandle
cogl_texture_new_from_sub_texture (CoglHandle full_texture,
                                   gint       sub_x,
                                   gint       sub_y,
                                   gint       sub_width,
                                   gint       sub_height)
{
  return _cogl_sub_texture_new (full_texture, sub_x, sub_y,
                                sub_width, sub_height);
}

guint
cogl_texture_get_width (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return 0;

  tex = COGL_TEXTURE (handle);

  return tex->vtable->get_width (tex);
}

guint
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

guint
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

gint
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

gboolean
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
_cogl_texture_ensure_mipmaps (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return;

  tex = COGL_TEXTURE (handle);

  tex->vtable->ensure_mipmaps (tex);
}

void
_cogl_texture_ensure_non_quad_rendering (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return;

  tex = COGL_TEXTURE (handle);

  return tex->vtable->ensure_non_quad_rendering (tex);
}

gboolean
cogl_texture_set_region (CoglHandle       handle,
			 gint             src_x,
			 gint             src_y,
			 gint             dst_x,
			 gint             dst_y,
			 guint            dst_width,
			 guint            dst_height,
			 gint             width,
			 gint             height,
			 CoglPixelFormat  format,
			 guint            rowstride,
			 const guchar    *data)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return FALSE;

  tex = COGL_TEXTURE (handle);

  return tex->vtable->set_region (tex,
                                  src_x, src_y,
                                  dst_x, dst_y,
                                  dst_width, dst_height,
                                  width, height,
                                  format,
                                  rowstride,
                                  data);
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
                          GLint       *viewport)
{
  gint        bpp;
  float       rx1, ry1;
  float       rx2, ry2;
  float       tx1, ty1;
  float       tx2, ty2;
  int         bw,  bh;
  CoglBitmap  rect_bmp;
  guint       tex_width, tex_height;

  bpp = _cogl_get_format_bpp (COGL_PIXEL_FORMAT_RGBA_8888);

  tex_width = cogl_texture_get_width (handle);
  tex_height = cogl_texture_get_height (handle);

  ry1 = 0; ry2 = 0;
  ty1 = 0; ty2 = 0;

  /* Walk Y axis until whole bitmap height consumed */
  for (bh = tex_height; bh > 0; bh -= viewport[3])
    {
      /* Rectangle Y coords */
      ry1 = ry2;
      ry2 += (bh < viewport[3]) ? bh : viewport[3];

      /* Normalized texture Y coords */
      ty1 = ty2;
      ty2 = (ry2 / (float) tex_height);

      rx1 = 0; rx2 = 0;
      tx1 = 0; tx2 = 0;

      /* Walk X axis until whole bitmap width consumed */
      for (bw = tex_width; bw > 0; bw-=viewport[2])
        {
          /* Rectangle X coords */
          rx1 = rx2;
          rx2 += (bw < viewport[2]) ? bw : viewport[2];

          /* Normalized texture X coords */
          tx1 = tx2;
          tx2 = (rx2 / (float) tex_width);

          /* Draw a portion of texture */
          cogl_rectangle_with_texture_coords (0, 0,
                                              rx2 - rx1,
                                              ry2 - ry1,
                                              tx1, ty1,
                                              tx2, ty2);

          /* Read into a temporary bitmap */
          rect_bmp.format = COGL_PIXEL_FORMAT_RGBA_8888;
          rect_bmp.width = rx2 - rx1;
          rect_bmp.height = ry2 - ry1;
          rect_bmp.rowstride = bpp * rect_bmp.width;
          rect_bmp.data = (guchar*) g_malloc (rect_bmp.rowstride *
                                              rect_bmp.height);

          _cogl_texture_driver_prep_gl_for_pixels_download (rect_bmp.rowstride,
                                                            bpp);
          GE( glReadPixels (viewport[0], viewport[1],
                            rect_bmp.width,
                            rect_bmp.height,
                            GL_RGBA, GL_UNSIGNED_BYTE,
                            rect_bmp.data) );

          /* Copy to target bitmap */
          _cogl_bitmap_copy_subregion (&rect_bmp,
                                       target_bmp,
                                       0,0,
                                       rx1,ry1,
                                       rect_bmp.width,
                                       rect_bmp.height);

          /* Free temp bitmap */
          g_free (rect_bmp.data);
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
  gint       bpp;
  CoglHandle framebuffer;
  int        viewport[4];
  CoglBitmap alpha_bmp;
  CoglHandle prev_source;
  CoglMatrixStack *projection_stack;
  CoglMatrixStack *modelview_stack;

  _COGL_GET_CONTEXT (ctx, FALSE);

  bpp = _cogl_get_format_bpp (COGL_PIXEL_FORMAT_RGBA_8888);

  framebuffer = _cogl_get_framebuffer ();
  /* Viewport needs to have some size and be inside the window for this */
  _cogl_framebuffer_get_viewport4fv (framebuffer, viewport);
  if (viewport[0] <  0 || viewport[1] <  0 ||
      viewport[2] <= 0 || viewport[3] <= 0)
    return FALSE;

  /* Setup orthographic projection into current viewport (0,0 in bottom-left
   * corner to draw the texture upside-down so we match the way glReadPixels
   * works)
   */

  projection_stack = _cogl_framebuffer_get_projection_stack (framebuffer);
  _cogl_matrix_stack_push (projection_stack);
  _cogl_matrix_stack_load_identity (projection_stack);
  _cogl_matrix_stack_ortho (projection_stack,
                            0, (float)(viewport[2]),
                            0, (float)(viewport[3]),
                            (float)(0),
                            (float)(100));

  modelview_stack = _cogl_framebuffer_get_modelview_stack (framebuffer);
  _cogl_matrix_stack_push (modelview_stack);
  _cogl_matrix_stack_load_identity (modelview_stack);

  /* Direct copy operation */

  if (ctx->texture_download_material == COGL_INVALID_HANDLE)
    {
      ctx->texture_download_material = cogl_material_new ();
      cogl_material_set_blend (ctx->texture_download_material,
                               "RGBA = ADD (SRC_COLOR, 0)",
                               NULL);
    }

  prev_source = cogl_handle_ref (ctx->source_material);
  cogl_set_source (ctx->texture_download_material);

  cogl_material_set_layer (ctx->texture_download_material, 0, handle);

  cogl_material_set_layer_combine (ctx->texture_download_material,
                                   0, /* layer */
                                   "RGBA = REPLACE (TEXTURE)",
                                   NULL);

  do_texture_draw_and_read (handle, target_bmp, viewport);

  /* Check whether texture has alpha and framebuffer not */
  /* FIXME: For some reason even if ALPHA_BITS is 8, the framebuffer
     still doesn't seem to have an alpha buffer. This might be just
     a PowerVR issue.
  GLint r_bits, g_bits, b_bits, a_bits;
  GE( glGetIntegerv (GL_ALPHA_BITS, &a_bits) );
  GE( glGetIntegerv (GL_RED_BITS, &r_bits) );
  GE( glGetIntegerv (GL_GREEN_BITS, &g_bits) );
  GE( glGetIntegerv (GL_BLUE_BITS, &b_bits) );
  printf ("R bits: %d\n", r_bits);
  printf ("G bits: %d\n", g_bits);
  printf ("B bits: %d\n", b_bits);
  printf ("A bits: %d\n", a_bits); */
  if ((cogl_texture_get_format (handle) & COGL_A_BIT)/* && a_bits == 0*/)
    {
      guchar *srcdata;
      guchar *dstdata;
      guchar *srcpixel;
      guchar *dstpixel;
      gint    x,y;

      /* Create temp bitmap for alpha values */
      alpha_bmp.format = COGL_PIXEL_FORMAT_RGBA_8888;
      alpha_bmp.width = target_bmp->width;
      alpha_bmp.height = target_bmp->height;
      alpha_bmp.rowstride = bpp * alpha_bmp.width;
      alpha_bmp.data = (guchar*) g_malloc (alpha_bmp.rowstride *
                                           alpha_bmp.height);

      /* Draw alpha values into RGB channels */
      cogl_material_set_layer_combine (ctx->texture_download_material,
                                       0, /* layer */
                                       "RGBA = REPLACE (TEXTURE[A])",
                                       NULL);

      do_texture_draw_and_read (handle, &alpha_bmp, viewport);

      /* Copy temp R to target A */
      srcdata = alpha_bmp.data;
      dstdata = target_bmp->data;

      for (y=0; y<target_bmp->height; ++y)
        {
          for (x=0; x<target_bmp->width; ++x)
            {
              srcpixel = srcdata + x*bpp;
              dstpixel = dstdata + x*bpp;
              dstpixel[3] = srcpixel[0];
            }
          srcdata += alpha_bmp.rowstride;
          dstdata += target_bmp->rowstride;
        }

      g_free (alpha_bmp.data);
    }

  /* Restore old state */
  _cogl_matrix_stack_pop (modelview_stack);
  _cogl_matrix_stack_pop (projection_stack);

  /* restore the original material */
  cogl_set_source (prev_source);
  cogl_handle_unref (prev_source);

  return TRUE;
}

gint
cogl_texture_get_data (CoglHandle       handle,
		       CoglPixelFormat  format,
		       guint            rowstride,
		       guchar          *data)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return FALSE;

  tex = COGL_TEXTURE (handle);

  return tex->vtable->get_data (handle, format, rowstride, data);
}

