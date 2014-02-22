/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2014 Intel Corporation.
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

#ifndef __COGL_FRAMEBUFFER_DEPRECATED_H__
#define __COGL_FRAMEBUFFER_DEPRECATED_H__

#include <cogl/cogl-macros.h>

/**
 * cogl_set_framebuffer:
 * @buffer: A #CoglFramebuffer object, either onscreen or offscreen.
 *
 * This redirects all subsequent drawing to the specified framebuffer. This can
 * either be an offscreen buffer created with cogl_offscreen_new_to_texture ()
 * or in the future it may be an onscreen framebuffers too.
 *
 * Since: 1.2
 * Deprecated: 1.16: The latest drawing apis take explicit
 *                   #CoglFramebuffer arguments so this stack of
 *                   framebuffers shouldn't be used anymore.
 */
COGL_DEPRECATED_IN_1_16
void
cogl_set_framebuffer (CoglFramebuffer *buffer);

/**
 * cogl_push_framebuffer:
 * @buffer: A #CoglFramebuffer object, either onscreen or offscreen.
 *
 * Redirects all subsequent drawing to the specified framebuffer. This can
 * either be an offscreen buffer created with cogl_offscreen_new_to_texture ()
 * or in the future it may be an onscreen framebuffer too.
 *
 * You should understand that a framebuffer owns the following state:
 * <itemizedlist>
 *  <listitem><simpara>The projection matrix</simpara></listitem>
 *  <listitem><simpara>The modelview matrix stack</simpara></listitem>
 *  <listitem><simpara>The viewport</simpara></listitem>
 *  <listitem><simpara>The clip stack</simpara></listitem>
 * </itemizedlist>
 * So these items will automatically be saved and restored when you
 * push and pop between different framebuffers.
 *
 * Also remember a newly allocated framebuffer will have an identity matrix for
 * the projection and modelview matrices which gives you a coordinate space
 * like OpenGL with (-1, -1) corresponding to the top left of the viewport,
 * (1, 1) corresponding to the bottom right and +z coming out towards the
 * viewer.
 *
 * If you want to set up a coordinate space like Clutter does with (0, 0)
 * corresponding to the top left and (framebuffer_width, framebuffer_height)
 * corresponding to the bottom right you can do so like this:
 *
 * |[
 * static void
 * setup_viewport (unsigned int width,
 *                 unsigned int height,
 *                 float fovy,
 *                 float aspect,
 *                 float z_near,
 *                 float z_far)
 * {
 *   float z_camera;
 *   CoglMatrix projection_matrix;
 *   CoglMatrix mv_matrix;
 *
 *   cogl_set_viewport (0, 0, width, height);
 *   cogl_perspective (fovy, aspect, z_near, z_far);
 *
 *   cogl_get_projection_matrix (&amp;projection_matrix);
 *   z_camera = 0.5 * projection_matrix.xx;
 *
 *   cogl_matrix_init_identity (&amp;mv_matrix);
 *   cogl_matrix_translate (&amp;mv_matrix, -0.5f, -0.5f, -z_camera);
 *   cogl_matrix_scale (&amp;mv_matrix, 1.0f / width, -1.0f / height, 1.0f / width);
 *   cogl_matrix_translate (&amp;mv_matrix, 0.0f, -1.0 * height, 0.0f);
 *   cogl_set_modelview_matrix (&amp;mv_matrix);
 * }
 *
 * static void
 * my_init_framebuffer (ClutterStage *stage,
 *                      CoglFramebuffer *framebuffer,
 *                      unsigned int framebuffer_width,
 *                      unsigned int framebuffer_height)
 * {
 *   ClutterPerspective perspective;
 *
 *   clutter_stage_get_perspective (stage, &perspective);
 *
 *   cogl_push_framebuffer (framebuffer);
 *   setup_viewport (framebuffer_width,
 *                   framebuffer_height,
 *                   perspective.fovy,
 *                   perspective.aspect,
 *                   perspective.z_near,
 *                   perspective.z_far);
 * }
 * ]|
 *
 * The previous framebuffer can be restored by calling cogl_pop_framebuffer()
 *
 * Since: 1.2
 * Deprecated: 1.16: The latest drawing apis take explicit
 *                   #CoglFramebuffer arguments so this stack of
 *                   framebuffers shouldn't be used anymore.
 */
COGL_DEPRECATED_IN_1_16
void
cogl_push_framebuffer (CoglFramebuffer *buffer);

/**
 * cogl_pop_framebuffer:
 *
 * Restores the framebuffer that was previously at the top of the stack.
 * All subsequent drawing will be redirected to this framebuffer.
 *
 * Since: 1.2
 * Deprecated: 1.16: The latest drawing apis take explicit
 *                   #CoglFramebuffer arguments so this stack of
 *                   framebuffers shouldn't be used anymore.
 */
COGL_DEPRECATED_IN_1_16
void
cogl_pop_framebuffer (void);

/**
 * cogl_set_draw_buffer:
 * @target: A #CoglBufferTarget that specifies what kind of framebuffer you
 *          are setting as the render target.
 * @offscreen: If you are setting a framebuffer of type COGL_OFFSCREEN_BUFFER
 *             then this is a CoglHandle for the offscreen buffer.
 *
 * Redirects all subsequent drawing to the specified framebuffer. This
 * can either be an offscreen buffer created with
 * cogl_offscreen_new_to_texture () or you can revert to your original
 * on screen window buffer.
 *
 * Deprecated: 1.16: The latest drawing apis take explicit
 *                   #CoglFramebuffer arguments so this stack of
 *                   framebuffers shouldn't be used anymore.
 */
COGL_DEPRECATED_IN_1_16
void
cogl_set_draw_buffer (CoglBufferTarget target,
                      CoglHandle offscreen);

/**
 * cogl_push_draw_buffer:
 *
 * Save cogl_set_draw_buffer() state.
 *
 * Deprecated: 1.16: The latest drawing apis take explicit
 *                   #CoglFramebuffer arguments so this stack of
 *                   framebuffers shouldn't be used anymore.
 */
COGL_DEPRECATED_IN_1_16
void
cogl_push_draw_buffer (void);

/**
 * cogl_pop_draw_buffer:
 *
 * Restore cogl_set_draw_buffer() state.
 *
 * Deprecated: 1.16: The latest drawing apis take explicit
 *                   #CoglFramebuffer arguments so this stack of
 *                   framebuffers shouldn't be used anymore.
 */
COGL_DEPRECATED_IN_1_16
void
cogl_pop_draw_buffer (void);

/**
 * cogl_read_pixels:
 * @x: The window x position to start reading from
 * @y: The window y position to start reading from
 * @width: The width of the rectangle you want to read
 * @height: The height of the rectangle you want to read
 * @source: Identifies which auxillary buffer you want to read
 *          (only COGL_READ_PIXELS_COLOR_BUFFER supported currently)
 * @format: The pixel format you want the result in
 *          (only COGL_PIXEL_FORMAT_RGBA_8888 supported currently)
 * @pixels: The location to write the pixel data.
 *
 * This reads a rectangle of pixels from the current framebuffer where
 * position (0, 0) is the top left. The pixel at (x, y) is the first
 * read, and the data is returned with a rowstride of (width * 4).
 *
 * Currently Cogl assumes that the framebuffer is in a premultiplied
 * format so if @format is non-premultiplied it will convert it. To
 * read the pixel values without any conversion you should either
 * specify a format that doesn't use an alpha channel or use one of
 * the formats ending in PRE.
 *
 * Deprecated: 1.16: Use cogl_framebuffer_read_pixels() instead
 */
COGL_DEPRECATED_IN_1_16_FOR (cogl_framebuffer_read_pixels)
void
cogl_read_pixels (int x,
                  int y,
                  int width,
                  int height,
                  CoglReadPixelsFlags source,
                  CoglPixelFormat format,
                  uint8_t *pixels);


/* XXX: Since this api was marked unstable, maybe we can just
 * remove this api if we can't find anyone is using it. */
/**
 * cogl_framebuffer_get_color_format:
 * @framebuffer: A #CoglFramebuffer framebuffer
 *
 * Queries the common #CoglPixelFormat of all color buffers attached
 * to this framebuffer. For an offscreen framebuffer created with
 * cogl_offscreen_new_with_texture() this will correspond to the format
 * of the texture.
 *
 * This API is deprecated because it is missleading to report a
 * #CoglPixelFormat for the internal format of the @framebuffer since
 * #CoglPixelFormat is such a precise format description and it's
 * only the set of components and the premultiplied alpha status
 * that is really known.
 *
 * Since: 1.8
 * Stability: unstable
 * Deprecated 1.18: Removed since it is misleading
 */
COGL_DEPRECATED_IN_1_18
CoglPixelFormat
cogl_framebuffer_get_color_format (CoglFramebuffer *framebuffer);

#endif /* __COGL_FRAMEBUFFER_DEPRECATED_H__ */
