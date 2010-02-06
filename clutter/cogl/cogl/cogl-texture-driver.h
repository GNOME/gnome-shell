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

#ifndef __COGL_TEXTURE_DRIVER_H
#define __COGL_TEXTURE_DRIVER_H

/*
 * Basically just a wrapper around glBindTexture, but the GLES2 backend
 * for example also wants to know about the internal format so it can
 * identify when alpha only textures are bound.
 */
void
_cogl_texture_driver_bind (GLenum gl_target, GLuint gl_handle, GLenum gl_intformat);

/*
 * This sets up the glPixelStore state for an upload to a destination with
 * the same size, and with no offset.
 */
/* NB: GLES can't upload a sub region of pixel data from a larger source
 * buffer which is why this interface is limited. The GL driver has a more
 * flexible version of this function that is uses internally */
void
_cogl_texture_driver_prep_gl_for_pixels_upload (int pixels_rowstride,
                                                int pixels_bpp);

/*
 * This uploads a sub-region from source_bmp to a single GL texture handle (i.e
 * a single CoglTexture slice)
 *
 * It also updates the array of tex->first_pixels[slice_index] if
 * dst_{x,y} == 0
 *
 * The driver abstraction is in place because GLES doesn't support the pixel
 * store options required to source from a subregion, so for GLES we have
 * to manually create a transient source bitmap.
 *
 * XXX: sorry for the ridiculous number of arguments :-(
 */
void
_cogl_texture_driver_upload_subregion_to_gl (GLenum       gl_target,
                                             GLuint       gl_handle,
                                             int          src_x,
                                             int          src_y,
                                             int          dst_x,
                                             int          dst_y,
                                             int          width,
                                             int          height,
                                             CoglBitmap  *source_bmp,
				             GLuint       source_gl_format,
				             GLuint       source_gl_type);

/*
 * Replaces the contents of the GL texture with the entire bitmap. On
 * GL this just directly calls glTexImage2D, but under GLES it needs
 * to copy the bitmap if the rowstride is not a multiple of a possible
 * alignment value because there is no GL_UNPACK_ROW_LENGTH
 */
void
_cogl_texture_driver_upload_to_gl (GLenum       gl_target,
                                   GLuint       gl_handle,
                                   CoglBitmap  *source_bmp,
                                   GLint        internal_gl_format,
                                   GLuint       source_gl_format,
                                   GLuint       source_gl_type);

/*
 * This sets up the glPixelStore state for an download to a destination with
 * the same size, and with no offset.
 */
/* NB: GLES can't download pixel data into a sub region of a larger destination
 * buffer, the GL driver has a more flexible version of this function that it
 * uses internally. */
void
_cogl_texture_driver_prep_gl_for_pixels_download (int pixels_rowstride,
                                                  int pixels_bpp);

/*
 * This driver abstraction is in place because GLES doesn't have a sane way to
 * download data from a texture so you litterally render the texture to the
 * backbuffer, and retrive the data using glReadPixels :-(
 */
gboolean
_cogl_texture_driver_download_from_gl (CoglTexture *tex,
				       CoglBitmap  *target_bmp,
				       GLuint       target_gl_format,
				       GLuint       target_gl_type);

/*
 * This driver abstraction is needed because GLES doesn't support glGetTexImage
 * (). On GLES this currently just returns FALSE which will lead to a generic
 * fallback path being used that simply renders the texture and reads it back
 * from the framebuffer. (See _cogl_texture_draw_and_read () )
 */
gboolean
_cogl_texture_driver_gl_get_tex_image (GLenum  gl_target,
                                       GLenum  dest_gl_format,
                                       GLenum  dest_gl_type,
                                       guint8 *dest);

/*
 * It may depend on the driver as to what texture sizes are supported...
 */
gboolean
_cogl_texture_driver_size_supported (GLenum gl_target,
			             GLenum gl_format,
			             GLenum gl_type,
			             int    width,
			             int    height);

/*
 * This driver abstraction is needed because GLES doesn't support setting
 * a texture border color.
 */
void
_cogl_texture_driver_try_setting_gl_border_color (
                                              GLuint         gl_target,
                                              const GLfloat *transparent_color);

/*
 * XXX: this should live in cogl/{gl,gles}/cogl.c
 */
gboolean
_cogl_pixel_format_from_gl_internal (GLenum            gl_int_format,
				     CoglPixelFormat  *out_format);

/*
 * XXX: this should live in cogl/{gl,gles}/cogl.c
 */
CoglPixelFormat
_cogl_pixel_format_to_gl (CoglPixelFormat  format,
			  GLenum          *out_glintformat,
			  GLenum          *out_glformat,
			  GLenum          *out_gltype);

/*
 * It may depend on the driver as to what texture targets may be used when
 * creating a foreign texture. E.g. OpenGL supports ARB_texture_rectangle
 * but GLES doesn't
 */
gboolean
_cogl_texture_driver_allows_foreign_gl_target (GLenum gl_target);

/*
 * glGenerateMipmap semantics may need to be emulated for some drivers. E.g. by
 * enabling auto mipmap generation an re-loading a number of known texels.
 */
void
_cogl_texture_driver_gl_generate_mipmaps (GLenum texture_target);

/*
 * The driver may impose constraints on what formats can be used to store
 * texture data read from textures. For example GLES currently only supports
 * RGBA_8888, and so we need to manually convert the data if the final
 * destination has another format.
 */
CoglPixelFormat
_cogl_texture_driver_find_best_gl_get_data_format (
                                             CoglPixelFormat  format,
                                             GLenum          *closest_gl_format,
                                             GLenum          *closest_gl_type);

#endif /* __COGL_TEXTURE_DRIVER_H */

