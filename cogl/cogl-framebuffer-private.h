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

#ifndef __COGL_FRAMEBUFFER_PRIVATE_H
#define __COGL_FRAMEBUFFER_PRIVATE_H

#include "cogl-object-private.h"
#include "cogl-matrix-stack.h"
#include "cogl-clip-state-private.h"
#include "cogl-journal-private.h"

#ifdef COGL_HAS_XLIB_SUPPORT
#include <X11/Xlib.h>
#endif

#ifdef COGL_HAS_GLX_SUPPORT
#include <GL/glx.h>
#include <GL/glxext.h>
#endif

#ifdef COGL_HAS_WIN32_SUPPORT
#include <windows.h>
#endif

typedef enum _CoglFramebufferType {
  COGL_FRAMEBUFFER_TYPE_ONSCREEN,
  COGL_FRAMEBUFFER_TYPE_OFFSCREEN
} CoglFramebufferType;

struct _CoglFramebuffer
{
  CoglObject          _parent;
  CoglContext        *context;
  CoglFramebufferType  type;
  int                 width;
  int                 height;
  /* Format of the pixels in the framebuffer (including the expected
     premult state) */
  CoglPixelFormat     format;
  gboolean            allocated;

  CoglMatrixStack    *modelview_stack;
  CoglMatrixStack    *projection_stack;
  float               viewport_x;
  float               viewport_y;
  float               viewport_width;
  float               viewport_height;

  CoglClipState       clip_state;

  gboolean            dirty_bitmasks;
  int                 red_bits;
  int                 blue_bits;
  int                 green_bits;
  int                 alpha_bits;

  /* We journal the textured rectangles we want to submit to OpenGL so
   * we have an oppertunity to batch them together into less draw
   * calls. */
  CoglJournal        *journal;

  /* The scene of a given framebuffer may depend on images in other
   * framebuffers... */
  GList              *deps;

  /* As part of an optimization for reading-back single pixels from a
   * framebuffer in some simple cases where the geometry is still
   * available in the journal we need to track the bounds of the last
   * region cleared, its color and we need to track when something
   * does in fact draw to that region so it is no longer clear.
   */
  float               clear_color_red;
  float               clear_color_green;
  float               clear_color_blue;
  float               clear_color_alpha;
  int                 clear_clip_x0;
  int                 clear_clip_y0;
  int                 clear_clip_x1;
  int                 clear_clip_y1;
  gboolean            clear_clip_dirty;
};

typedef struct _CoglOffscreen
{
  CoglFramebuffer  _parent;
  GLuint          fbo_handle;
  GSList          *renderbuffers;
  CoglHandle      texture;
} CoglOffscreen;

/* Flags to pass to _cogl_offscreen_new_to_texture_full */
typedef enum
{
  COGL_OFFSCREEN_DISABLE_DEPTH_AND_STENCIL = 1
} CoglOffscreenFlags;

#define COGL_OFFSCREEN(X) ((CoglOffscreen *)(X))

struct _CoglOnscreen
{
  CoglFramebuffer  _parent;

#ifdef COGL_HAS_X11_SUPPORT
  guint32 foreign_xid;
  CoglOnscreenX11MaskCallback foreign_update_mask_callback;
  void *foreign_update_mask_data;
#endif

#ifdef COGL_HAS_WIN32_SUPPORT
  HWND foreign_hwnd;
#endif

  gboolean swap_throttled;

  void *winsys;
};

void
_cogl_framebuffer_state_init (void);

void
_cogl_framebuffer_winsys_update_size (CoglFramebuffer *framebuffer,
                                      int width, int height);

void
_cogl_clear4f (unsigned long buffers,
               float red,
               float green,
               float blue,
               float alpha);

void
_cogl_framebuffer_clear (CoglFramebuffer *framebuffer,
                         unsigned long buffers,
                         const CoglColor *color);

void
_cogl_framebuffer_clear4f (CoglFramebuffer *framebuffer,
                           unsigned long buffers,
                           float red,
                           float green,
                           float blue,
                           float alpha);

void
_cogl_framebuffer_dirty (CoglFramebuffer *framebuffer);

CoglClipState *
_cogl_framebuffer_get_clip_state (CoglFramebuffer *framebuffer);

/*
 * _cogl_framebuffer_get_clip_stack:
 * @framebuffer: A #CoglFramebuffer
 *
 * Gets a pointer to the current clip stack. This can be used to later
 * return to the same clip stack state with
 * _cogl_framebuffer_set_clip_stack(). A reference is not taken on the
 * stack so if you want to keep it you should call
 * _cogl_clip_stack_ref().
 *
 * Return value: a pointer to the @framebuffer clip stack.
 */
CoglClipStack *
_cogl_framebuffer_get_clip_stack (CoglFramebuffer *framebuffer);

/*
 * _cogl_framebuffer_set_clip_stack:
 * @framebuffer: A #CoglFramebuffer
 * @stack: a pointer to the replacement clip stack
 *
 * Replaces the @framebuffer clip stack with @stack.
 */
void
_cogl_framebuffer_set_clip_stack (CoglFramebuffer *framebuffer,
                                  CoglClipStack *stack);

CoglMatrixStack *
_cogl_framebuffer_get_modelview_stack (CoglFramebuffer *framebuffer);

CoglMatrixStack *
_cogl_framebuffer_get_projection_stack (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_add_dependency (CoglFramebuffer *framebuffer,
                                  CoglFramebuffer *dependency);

void
_cogl_framebuffer_remove_all_dependencies (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_flush_journal (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_flush_dependency_journals (CoglFramebuffer *framebuffer);

gboolean
_cogl_framebuffer_try_fast_read_pixel (CoglFramebuffer *framebuffer,
                                       int x,
                                       int y,
                                       CoglReadPixelsFlags source,
                                       CoglPixelFormat format,
                                       guint8 *pixel);

typedef enum _CoglFramebufferFlushFlags
{
  /* XXX: When using this, that imples you are going to manually load the
   * modelview matrix (via glLoadMatrix). _cogl_matrix_stack_flush_to_gl wont
   * be called for framebuffer->modelview_stack, and the modelview_stack will
   * also be marked as dirty. */
  COGL_FRAMEBUFFER_FLUSH_SKIP_MODELVIEW =     1L<<0,
  /* Similarly this flag implies you are going to flush the clip state
     yourself */
  COGL_FRAMEBUFFER_FLUSH_SKIP_CLIP_STATE =    1L<<1,
  /* When using this all that will be updated is the glBindFramebuffer
   * state and corresponding winsys state to make the framebuffer
   * current if it is a CoglOnscreen framebuffer. */
  COGL_FRAMEBUFFER_FLUSH_BIND_ONLY =          1L<<2
} CoglFramebufferFlushFlags;

void
_cogl_framebuffer_flush_state (CoglFramebuffer *draw_buffer,
                               CoglFramebuffer *read_buffer,
                               CoglFramebufferFlushFlags flags);

CoglFramebuffer *
_cogl_get_read_framebuffer (void);

GSList *
_cogl_create_framebuffer_stack (void);

void
_cogl_free_framebuffer_stack (GSList *stack);

/*
 * _cogl_offscreen_new_to_texture_full:
 * @texhandle: A handle to the texture to target
 * @create_flags: Flags specifying how to create the FBO
 * @level: The mipmap level within the texture to target
 *
 * Creates a new offscreen buffer which will target the given
 * texture. By default the buffer will have a depth and stencil
 * buffer. This can be disabled by passing
 * %COGL_OFFSCREEN_DISABLE_DEPTH_AND_STENCIL in @create_flags.
 *
 * Return value: the new CoglOffscreen object.
 */
CoglHandle
_cogl_offscreen_new_to_texture_full (CoglHandle texhandle,
                                     CoglOffscreenFlags create_flags,
                                     unsigned int level);

/*
 * _cogl_push_framebuffers:
 * @draw_buffer: A pointer to the buffer used for drawing
 * @read_buffer: A pointer to the buffer used for reading back pixels
 *
 * Redirects drawing and reading to the specified framebuffers as in
 * cogl_push_framebuffer() except that it allows the draw and read
 * buffer to be different. The buffers are pushed as a pair so that
 * they can later both be restored with a single call to
 * cogl_pop_framebuffer().
 */
void
_cogl_push_framebuffers (CoglFramebuffer *draw_buffer,
                         CoglFramebuffer *read_buffer);

/*
 * _cogl_blit_framebuffer:
 * @src_x: Source x position
 * @src_y: Source y position
 * @dst_x: Destination x position
 * @dst_y: Destination y position
 * @width: Width of region to copy
 * @height: Height of region to copy
 *
 * This blits a region of the color buffer of the current draw buffer
 * to the current read buffer. The draw and read buffers can be set up
 * using _cogl_push_framebuffers(). This function should only be
 * called if the COGL_FEATURE_OFFSCREEN_BLIT feature is
 * advertised. The two buffers must both be offscreen and have the
 * same format.
 *
 * Note that this function differs a lot from the glBlitFramebuffer
 * function provided by the GL_EXT_framebuffer_blit extension. Notably
 * it doesn't support having different sizes for the source and
 * destination rectangle. This isn't supported by the corresponding
 * GL_ANGLE_framebuffer_blit extension on GLES2.0 and it doesn't seem
 * like a particularly useful feature. If the application wanted to
 * scale the results it may make more sense to draw a primitive
 * instead.
 *
 * We can only really support blitting between two offscreen buffers
 * for this function on GLES2.0. This is because we effectively render
 * upside down to offscreen buffers to maintain Cogl's representation
 * of the texture coordinate system where 0,0 is the top left of the
 * texture. If we were to blit from an offscreen to an onscreen buffer
 * then we would need to mirror the blit along the x-axis but the GLES
 * extension does not support this.
 *
 * The GL function is documented to be affected by the scissor. This
 * function therefore ensure that an empty clip stack is flushed
 * before performing the blit which means the scissor is effectively
 * ignored.
 *
 * The function also doesn't support specifying the buffers to copy
 * and instead only the color buffer is copied. When copying the depth
 * or stencil buffers the extension on GLES2.0 only supports copying
 * the full buffer which would be awkward to document with this
 * API. If we wanted to support that feature it may be better to have
 * a separate function to copy the entire buffer for a given mask.
 */
void
_cogl_blit_framebuffer (unsigned int src_x,
                        unsigned int src_y,
                        unsigned int dst_x,
                        unsigned int dst_y,
                        unsigned int width,
                        unsigned int height);

CoglOnscreen *
_cogl_onscreen_new (void);

#endif /* __COGL_FRAMEBUFFER_PRIVATE_H */

