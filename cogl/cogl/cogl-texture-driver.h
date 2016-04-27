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

#ifndef __COGL_TEXTURE_DRIVER_H
#define __COGL_TEXTURE_DRIVER_H

typedef struct _CoglTextureDriver CoglTextureDriver;

struct _CoglTextureDriver
{
  /*
   * A very small wrapper around glGenTextures() that ensures we default to
   * non-mipmap filters when creating textures. This is to save some memory as
   * the driver will not allocate room for the mipmap tree.
   */
  GLuint
  (* gen) (CoglContext *ctx,
           GLenum gl_target,
           CoglPixelFormat internal_format);

  /*
   * This sets up the glPixelStore state for an upload to a destination with
   * the same size, and with no offset.
   */
  /* NB: GLES can't upload a sub region of pixel data from a larger source
   * buffer which is why this interface is limited. The GL driver has a more
   * flexible version of this function that is uses internally */
  void
  (* prep_gl_for_pixels_upload) (CoglContext *ctx,
                                 int pixels_rowstride,
                                 int pixels_bpp);

  /*
   * This uploads a sub-region from source_bmp to a single GL texture
   * handle (i.e a single CoglTexture slice)
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
  CoglBool
  (* upload_subregion_to_gl) (CoglContext *ctx,
                              CoglTexture *texture,
                              CoglBool is_foreign,
                              int src_x,
                              int src_y,
                              int dst_x,
                              int dst_y,
                              int width,
                              int height,
                              int level,
                              CoglBitmap *source_bmp,
                              GLuint source_gl_format,
                              GLuint source_gl_type,
                              CoglError **error);

  /*
   * Replaces the contents of the GL texture with the entire bitmap. On
   * GL this just directly calls glTexImage2D, but under GLES it needs
   * to copy the bitmap if the rowstride is not a multiple of a possible
   * alignment value because there is no GL_UNPACK_ROW_LENGTH
   */
  CoglBool
  (* upload_to_gl) (CoglContext *ctx,
                    GLenum gl_target,
                    GLuint gl_handle,
                    CoglBool is_foreign,
                    CoglBitmap *source_bmp,
                    GLint internal_gl_format,
                    GLuint source_gl_format,
                    GLuint source_gl_type,
                    CoglError **error);

  /*
   * Replaces the contents of the GL texture with the entire bitmap. The
   * width of the texture is inferred from the bitmap. The height and
   * depth of the texture is given directly. The 'image_height' (which
   * is the number of rows between images) is inferred by dividing the
   * height of the bitmap by the depth.
   */
  CoglBool
  (* upload_to_gl_3d) (CoglContext *ctx,
                       GLenum gl_target,
                       GLuint gl_handle,
                       CoglBool is_foreign,
                       GLint height,
                       GLint depth,
                       CoglBitmap *source_bmp,
                       GLint internal_gl_format,
                       GLuint source_gl_format,
                       GLuint source_gl_type,
                       CoglError **error);

  /*
   * This sets up the glPixelStore state for an download to a destination with
   * the same size, and with no offset.
   */
  /* NB: GLES can't download pixel data into a sub region of a larger
   * destination buffer, the GL driver has a more flexible version of
   * this function that it uses internally. */
  void
  (* prep_gl_for_pixels_download) (CoglContext *ctx,
                                   int image_width,
                                   int pixels_rowstride,
                                   int pixels_bpp);

  /*
   * This driver abstraction is needed because GLES doesn't support
   * glGetTexImage (). On GLES this currently just returns FALSE which
   * will lead to a generic fallback path being used that simply
   * renders the texture and reads it back from the framebuffer. (See
   * _cogl_texture_draw_and_read () )
   */
  CoglBool
  (* gl_get_tex_image) (CoglContext *ctx,
                        GLenum gl_target,
                        GLenum dest_gl_format,
                        GLenum dest_gl_type,
                        uint8_t *dest);

  /*
   * It may depend on the driver as to what texture sizes are supported...
   */
  CoglBool
  (* size_supported) (CoglContext *ctx,
                      GLenum gl_target,
                      GLenum gl_intformat,
                      GLenum gl_format,
                      GLenum gl_type,
                      int width,
                      int height);

  CoglBool
  (* size_supported_3d) (CoglContext *ctx,
                         GLenum gl_target,
                         GLenum gl_format,
                         GLenum gl_type,
                         int width,
                         int height,
                         int depth);

  /*
   * This driver abstraction is needed because GLES doesn't support setting
   * a texture border color.
   */
  void
  (* try_setting_gl_border_color) (CoglContext *ctx,
                                   GLuint gl_target,
                                   const GLfloat *transparent_color);

  /*
   * It may depend on the driver as to what texture targets may be used when
   * creating a foreign texture. E.g. OpenGL supports ARB_texture_rectangle
   * but GLES doesn't
   */
  CoglBool
  (* allows_foreign_gl_target) (CoglContext *ctx,
                                GLenum gl_target);

  /*
   * The driver may impose constraints on what formats can be used to store
   * texture data read from textures. For example GLES currently only supports
   * RGBA_8888, and so we need to manually convert the data if the final
   * destination has another format.
   */
  CoglPixelFormat
  (* find_best_gl_get_data_format) (CoglContext     *context,
                                    CoglPixelFormat format,
                                    GLenum *closest_gl_format,
                                    GLenum *closest_gl_type);
};

#endif /* __COGL_TEXTURE_DRIVER_H */

