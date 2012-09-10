/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
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

#ifndef __COGL_DRIVER_H
#define __COGL_DRIVER_H

#include "cogl-context.h"
#include "cogl-offscreen.h"
#include "cogl-framebuffer-private.h"

typedef struct _CoglDriverVtable CoglDriverVtable;

struct _CoglDriverVtable
{
  /* TODO: factor this out since this is OpenGL specific and
   * so can be ignored by non-OpenGL drivers. */
  CoglBool
  (* pixel_format_from_gl_internal) (CoglContext *context,
                                     GLenum gl_int_format,
                                     CoglPixelFormat *out_format);

  /* TODO: factor this out since this is OpenGL specific and
   * so can be ignored by non-OpenGL drivers. */
  CoglPixelFormat
  (* pixel_format_to_gl) (CoglContext *context,
                          CoglPixelFormat format,
                          GLenum *out_glintformat,
                          GLenum *out_glformat,
                          GLenum *out_gltype);

  CoglBool
  (* update_features) (CoglContext *context,
                       CoglError **error);

  CoglBool
  (* offscreen_allocate) (CoglOffscreen *offscreen,
                          CoglError **error);

  void
  (* offscreen_free) (CoglOffscreen *offscreen);

  void
  (* framebuffer_flush_state) (CoglFramebuffer *draw_buffer,
                               CoglFramebuffer *read_buffer,
                               CoglFramebufferState state);

  void
  (* framebuffer_clear) (CoglFramebuffer *framebuffer,
                         unsigned long buffers,
                         float red,
                         float green,
                         float blue,
                         float alpha);

  void
  (* framebuffer_query_bits) (CoglFramebuffer *framebuffer,
                              int *red,
                              int *green,
                              int *blue,
                              int *alpha);

  void
  (* framebuffer_finish) (CoglFramebuffer *framebuffer);

  void
  (* framebuffer_discard_buffers) (CoglFramebuffer *framebuffer,
                                   unsigned long buffers);

  void
  (* framebuffer_draw_attributes) (CoglFramebuffer *framebuffer,
                                   CoglPipeline *pipeline,
                                   CoglVerticesMode mode,
                                   int first_vertex,
                                   int n_vertices,
                                   CoglAttribute **attributes,
                                   int n_attributes,
                                   CoglDrawFlags flags);

  void
  (* framebuffer_draw_indexed_attributes) (CoglFramebuffer *framebuffer,
                                           CoglPipeline *pipeline,
                                           CoglVerticesMode mode,
                                           int first_vertex,
                                           int n_vertices,
                                           CoglIndices *indices,
                                           CoglAttribute **attributes,
                                           int n_attributes,
                                           CoglDrawFlags flags);

  /* Destroys any driver specific resources associated with the given
   * 2D texture. */
  void
  (* texture_2d_free) (CoglTexture2D *tex_2d);

  /* Returns TRUE if the driver can support creating a 2D texture with
   * the given geometry and specified internal format.
   */
  CoglBool
  (* texture_2d_can_create) (CoglContext *ctx,
                             int width,
                             int height,
                             CoglPixelFormat internal_format);

  /* Initializes driver private state before allocating any specific
   * storage for a 2D texture, where base texture and texture 2D
   * members will already be initialized before passing control to
   * the driver.
   */
  void
  (* texture_2d_init) (CoglTexture2D *tex_2d);

  /* Instantiates a new CoglTexture2D object with un-initialized
   * storage for a given size and internal format */
  CoglTexture2D *
  (* texture_2d_new_with_size) (CoglContext *ctx,
                                int width,
                                int height,
                                CoglPixelFormat internal_format,
                                CoglError **error);

  /* Instantiates a new CoglTexture2D object with storage initialized
   * with the contents of the given bitmap, using the specified
   * internal format.
   */
  CoglTexture2D *
  (* texture_2d_new_from_bitmap) (CoglBitmap *bmp,
                                  CoglPixelFormat internal_format,
                                  CoglError **error);

#if defined (COGL_HAS_EGL_SUPPORT) && defined (EGL_KHR_image_base)
  /* Instantiates a new CoglTexture2D object with storage initialized
   * with the contents of the given EGL image.
   *
   * This is optional for drivers to support
   */
  CoglTexture2D *
  (* egl_texture_2d_new_from_image) (CoglContext *ctx,
                                     int width,
                                     int height,
                                     CoglPixelFormat format,
                                     EGLImageKHR image,
                                     CoglError **error);
#endif

  /* Initialize the specified region of storage of the given texture
   * with the contents of the specified framebuffer region
   */
  void
  (* texture_2d_copy_from_framebuffer) (CoglTexture2D *tex_2d,
                                        CoglFramebuffer *src_fb,
                                        int dst_x,
                                        int dst_y,
                                        int src_x,
                                        int src_y,
                                        int width,
                                        int height);

  /* If the given texture has a corresponding OpenGL texture handle
   * then return that.
   *
   * This is optional
   */
  unsigned int
  (* texture_2d_get_gl_handle) (CoglTexture2D *tex_2d);

  /* Update all mipmap levels > 0 */
  void
  (* texture_2d_generate_mipmap) (CoglTexture2D *tex_2d);

  /* Initialize the specified region of storage of the given texture
   * with the contents of the specified bitmap region
   */
  void
  (* texture_2d_copy_from_bitmap) (CoglTexture2D *tex_2d,
                                   CoglBitmap *bitmap,
                                   int dst_x,
                                   int dst_y,
                                   int src_x,
                                   int src_y,
                                   int width,
                                   int height);

  /* Reads back the full contents of the given texture and write it to
   * @data in the given @format and with the given @rowstride.
   *
   * This is optional
   */
  void
  (* texture_2d_get_data) (CoglTexture2D *tex_2d,
                           CoglPixelFormat format,
                           unsigned int rowstride,
                           uint8_t *data);
};

#endif /* __COGL_DRIVER_H */

