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
#include "cogl-material.h"
#include "cogl-context.h"
#include "cogl-handle.h"
#include "cogl-primitives.h"

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

  return obj->klass->type == _cogl_handle_texture_2d_sliced_get_type ();
    //|| obj->klass->type == _cogl_handle_texture_3d_get_type ();
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
_cogl_texture_bitmap_free (CoglTexture *tex)
{
  if (tex->bitmap.data != NULL && tex->bitmap_owner)
    g_free (tex->bitmap.data);

  tex->bitmap.data = NULL;
  tex->bitmap_owner = FALSE;
}

void
_cogl_texture_bitmap_swap (CoglTexture     *tex,
			   CoglBitmap      *new_bitmap)
{
  if (tex->bitmap.data != NULL && tex->bitmap_owner)
    g_free (tex->bitmap.data);

  tex->bitmap = *new_bitmap;
  tex->bitmap_owner = TRUE;
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

  switch (tex->type)
    {
      case COGL_TEXTURE_TYPE_2D_SLICED:
        _cogl_texture_2d_sliced_set_wrap_mode_parameter (
                                                  (CoglTexture2DSliced *)tex,
                                                  wrap_mode);
        break;
    }
}

gboolean
_cogl_texture_bitmap_prepare (CoglTexture     *tex,
			      CoglPixelFormat  internal_format)
{
  CoglBitmap        new_bitmap;
  CoglPixelFormat   new_data_format;
  gboolean          success;

  /* Was there any internal conversion requested?
   * By default Cogl will use a premultiplied internal format. Later we will
   * add control over this. */
  if (internal_format == COGL_PIXEL_FORMAT_ANY)
    {
      if ((tex->bitmap.format & COGL_A_BIT) &&
          tex->bitmap.format != COGL_PIXEL_FORMAT_A_8)
        internal_format = tex->bitmap.format | COGL_PREMULT_BIT;
      else
        internal_format = tex->bitmap.format;
    }

  /* Find closest format accepted by GL */
  new_data_format = _cogl_pixel_format_to_gl (internal_format,
					      &tex->gl_intformat,
					      &tex->gl_format,
					      &tex->gl_type);

  /* Convert to internal format */
  if (new_data_format != tex->bitmap.format)
    {
      success = _cogl_bitmap_convert_and_premult (&tex->bitmap,
						  &new_bitmap,
						  new_data_format);

      if (!success)
	return FALSE;

      /* Update texture with new data */
      _cogl_texture_bitmap_swap (tex, &new_bitmap);
    }

  return TRUE;
}

void
_cogl_texture_free (CoglTexture *tex)
{
  _cogl_texture_bitmap_free (tex);
}

CoglHandle
cogl_texture_new_with_size (guint            width,
			    guint            height,
                            CoglTextureFlags flags,
			    CoglPixelFormat  internal_format)
{
  return _cogl_texture_2d_sliced_new_with_size (width,
                                                height,
                                                flags,
                                                internal_format);
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
  return _cogl_texture_2d_sliced_new_from_data (width,
					        height,
					        flags,
					        format,
					        internal_format,
					        rowstride,
					        data);
}

CoglHandle
cogl_texture_new_from_bitmap (CoglHandle       bmp_handle,
                              CoglTextureFlags flags,
                              CoglPixelFormat  internal_format)
{
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
  return _cogl_texture_2d_sliced_new_from_file (filename,
                                                flags,
                                                internal_format,
                                                error);
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

guint
cogl_texture_get_width (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return 0;

  tex = COGL_TEXTURE (handle);

  return tex->bitmap.width;
}

guint
cogl_texture_get_height (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return 0;

  tex = COGL_TEXTURE (handle);

  return tex->bitmap.height;
}

CoglPixelFormat
cogl_texture_get_format (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return COGL_PIXEL_FORMAT_ANY;

  tex = COGL_TEXTURE (handle);

  return tex->bitmap.format;
}

guint
cogl_texture_get_rowstride (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return 0;

  tex = COGL_TEXTURE (handle);

  return tex->bitmap.rowstride;
}

gint
cogl_texture_get_max_waste (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return 0;

  tex = COGL_TEXTURE (handle);

  switch (tex->type)
    {
      case COGL_TEXTURE_TYPE_2D_SLICED:
        return _cogl_texture_2d_sliced_get_max_waste (handle);
    }

  g_return_val_if_reached (0);
}

gboolean
cogl_texture_is_sliced (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return FALSE;

  tex = COGL_TEXTURE (handle);

  switch (tex->type)
    {
      case COGL_TEXTURE_TYPE_2D_SLICED:
        return _cogl_texture_2d_sliced_is_sliced (handle);
    }

  g_return_val_if_reached (FALSE);
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

  switch (tex->type)
    {
      case COGL_TEXTURE_TYPE_2D_SLICED:
        {
          CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
          _cogl_texture_2d_sliced_foreach_sub_texture_in_region (tex_2ds,
                                                                 virtual_tx_1,
                                                                 virtual_ty_1,
                                                                 virtual_tx_2,
                                                                 virtual_ty_2,
                                                                 callback,
                                                                 user_data);
          break;
        }
    }
}

/* If this returns FALSE, that implies _foreach_sub_texture_in_region
 * will be needed to iterate over multiple sub textures for regions whos
 * texture coordinates extend out of the range [0,1]
 */
gboolean
_cogl_texture_can_hardware_repeat (CoglHandle handle)
{
  CoglTexture *tex = (CoglTexture *)handle;

#if HAVE_COGL_GL
  /* TODO: COGL_TEXTURE_TYPE_2D_RECTANGLE */
  if (tex->gl_target == GL_TEXTURE_RECTANGLE_ARB)
    return FALSE;
#endif

  switch (tex->type)
    {
      case COGL_TEXTURE_TYPE_2D_SLICED:
        {
          CoglTexture2DSliced *tex_2ds = COGL_TEXTURE_2D_SLICED (tex);
          return _cogl_texture_2d_sliced_can_hardware_repeat (tex_2ds);
        }
    }

  g_return_val_if_reached (FALSE);
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

  switch (tex->type)
    {
      case COGL_TEXTURE_TYPE_2D_SLICED:
        return _cogl_texture_2d_sliced_transform_coords_to_gl (
                                          COGL_TEXTURE_2D_SLICED (tex), s, t);
    }
}

GLenum
_cogl_texture_get_internal_gl_format (CoglHandle handle)
{
  CoglTexture *tex = COGL_TEXTURE (handle);

  return tex->gl_intformat;
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

  switch (tex->type)
    {
      case COGL_TEXTURE_TYPE_2D_SLICED:
        return _cogl_texture_2d_sliced_get_gl_texture (handle,
                                                       out_gl_handle,
                                                       out_gl_target);
    }

  g_return_val_if_reached (FALSE);
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

  switch (tex->type)
    {
      case COGL_TEXTURE_TYPE_2D_SLICED:
        _cogl_texture_2d_sliced_set_filters (handle, min_filter, mag_filter);
        break;
    }
}

void
_cogl_texture_ensure_mipmaps (CoglHandle handle)
{
  CoglTexture *tex;

  if (!cogl_is_texture (handle))
    return;

  tex = COGL_TEXTURE (handle);

  switch (tex->type)
    {
      case COGL_TEXTURE_TYPE_2D_SLICED:
        _cogl_texture_2d_sliced_ensure_mipmaps (handle);
        break;
    }
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

  switch (tex->type)
    {
      case COGL_TEXTURE_TYPE_2D_SLICED:
        return _cogl_texture_2d_sliced_set_region (handle,
					           src_x,
					           src_y,
                                                   dst_x,
                                                   dst_y,
                                                   dst_width,
                                                   dst_height,
                                                   width,
                                                   height,
                                                   format,
                                                   rowstride,
                                                   data);
    }

  g_return_val_if_reached (FALSE);
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
do_texture_draw_and_read (CoglTexture *tex,
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
  CoglHandle  handle;

  handle = (CoglHandle) tex;
  bpp = _cogl_get_format_bpp (COGL_PIXEL_FORMAT_RGBA_8888);

  ry1 = 0; ry2 = 0;
  ty1 = 0; ty2 = 0;

  /* Walk Y axis until whole bitmap height consumed */
  for (bh = tex->bitmap.height; bh > 0; bh -= viewport[3])
    {
      /* Rectangle Y coords */
      ry1 = ry2;
      ry2 += (bh < viewport[3]) ? bh : viewport[3];

      /* Normalized texture Y coords */
      ty1 = ty2;
      ty2 = (ry2 / (float)tex->bitmap.height);

      rx1 = 0; rx2 = 0;
      tx1 = 0; tx2 = 0;

      /* Walk X axis until whole bitmap width consumed */
      for (bw = tex->bitmap.width; bw > 0; bw-=viewport[2])
        {
          /* Rectangle X coords */
          rx1 = rx2;
          rx2 += (bw < viewport[2]) ? bw : viewport[2];

          /* Normalized texture X coords */
          tx1 = tx2;
          tx2 = (rx2 / (float)tex->bitmap.width);

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
_cogl_texture_draw_and_read (CoglTexture *tex,
                             CoglBitmap  *target_bmp,
                             GLuint       target_gl_format,
                             GLuint       target_gl_type)
{
  gint       bpp;
  GLint      viewport[4];
  CoglBitmap alpha_bmp;
  CoglHandle prev_source;

  _COGL_GET_CONTEXT (ctx, FALSE);

  bpp = _cogl_get_format_bpp (COGL_PIXEL_FORMAT_RGBA_8888);

  /* Viewport needs to have some size and be inside the window for this */
  GE( glGetIntegerv (GL_VIEWPORT, viewport));
  if (viewport[0] <  0 || viewport[1] <  0 ||
      viewport[2] <= 0 || viewport[3] <= 0)
    return FALSE;

  /* Setup orthographic projection into current viewport (0,0 in bottom-left
   * corner to draw the texture upside-down so we match the way glReadPixels
   * works)
   */

  _cogl_set_current_matrix (COGL_MATRIX_PROJECTION);
  _cogl_current_matrix_push ();
  _cogl_current_matrix_identity ();

  _cogl_current_matrix_ortho (0, (float)(viewport[2]),
                              0, (float)(viewport[3]),
                              (float)(0),
                              (float)(100));

  _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
  _cogl_current_matrix_push ();
  _cogl_current_matrix_identity ();

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

  cogl_material_set_layer (ctx->texture_download_material, 0, tex);

  cogl_material_set_layer_combine (ctx->texture_download_material,
                                   0, /* layer */
                                   "RGBA = REPLACE (TEXTURE)",
                                   NULL);

  do_texture_draw_and_read (tex, target_bmp, viewport);

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
  if ((tex->bitmap.format & COGL_A_BIT)/* && a_bits == 0*/)
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

      do_texture_draw_and_read (tex, &alpha_bmp, viewport);

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
  _cogl_set_current_matrix (COGL_MATRIX_PROJECTION);
  _cogl_current_matrix_pop ();
  _cogl_set_current_matrix (COGL_MATRIX_MODELVIEW);
  _cogl_current_matrix_pop ();

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

  switch (tex->type)
    {
      case COGL_TEXTURE_TYPE_2D_SLICED:
        return _cogl_texture_2d_sliced_get_data (handle,
                                                 format,
                                                 rowstride,
                                                 data);
    }

  g_return_val_if_reached (0);
}

