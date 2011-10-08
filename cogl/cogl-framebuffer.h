/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifndef __COGL_FRAMEBUFFER_H
#define __COGL_FRAMEBUFFER_H

#include <cogl/cogl.h>

#include <glib.h>

#ifdef COGL_HAS_WIN32_SUPPORT
#include <windows.h>
#endif /* COGL_HAS_WIN32_SUPPORT */

#if defined (COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT)
#include <wayland-client.h>
#endif /* COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT */

G_BEGIN_DECLS

/**
 * SECTION:cogl-framebuffer
 * @short_description: A common interface for manipulating framebuffers
 *
 * Framebuffers are a collection of buffers that can be rendered too.
 * A framebuffer may be comprised of one or more color buffers, an
 * optional depth buffer and an optional stencil buffer. Other
 * configuration parameters are associated with framebuffers too such
 * as whether the framebuffer supports multi-sampling (an anti-aliasing
 * technique) or dithering.
 *
 * There are two kinds of framebuffer in Cogl, #CoglOnscreen
 * framebuffers and #CoglOffscreen framebuffers. As the names imply
 * offscreen framebuffers are for rendering something offscreen
 * (perhaps to a texture which is bound as one of the color buffers).
 * The exact semantics of onscreen framebuffers depends on the window
 * system backend that you are using, but typically you can expect
 * rendering to a #CoglOnscreen framebuffer will be immediately
 * visible to the user.
 *
 * If you want to create a new framebuffer then you should start by
 * looking at the #CoglOnscreen and #CoglOffscreen constructor
 * functions, such as cogl_offscreen_new_to_texture() or
 * cogl_onscreen_new(). The #CoglFramebuffer interface deals with
 * all aspects that are common between those two types of framebuffer.
 *
 * Setup of a new CoglFramebuffer happens in two stages. There is a
 * configuration stage where you specify all the options and ancillary
 * buffers you want associated with your framebuffer and then when you
 * are happy with the configuration you can "allocate" the framebuffer
 * using cogl_framebuffer_allocate (). Technically explicitly calling
 * cogl_framebuffer_allocate () is optional for convenience and the
 * framebuffer will automatically be allocated when you first try to
 * draw to it, but if you do the allocation manually then you can
 * also catch any possible errors that may arise from your
 * configuration.
 */

#ifdef COGL_ENABLE_EXPERIMENTAL_API
#define cogl_onscreen_new cogl_onscreen_new_EXP

#define COGL_FRAMEBUFFER(X) ((CoglFramebuffer *)(X))

#define cogl_framebuffer_allocate cogl_framebuffer_allocate_EXP
gboolean
cogl_framebuffer_allocate (CoglFramebuffer *framebuffer,
                           GError **error);

#define cogl_framebuffer_get_width cogl_framebuffer_get_width_EXP
int
cogl_framebuffer_get_width (CoglFramebuffer *framebuffer);

#define cogl_framebuffer_get_height cogl_framebuffer_get_height_EXP
int
cogl_framebuffer_get_height (CoglFramebuffer *framebuffer);

#define cogl_framebuffer_set_viewport cogl_framebuffer_set_viewport_EXP
void
cogl_framebuffer_set_viewport (CoglFramebuffer *framebuffer,
                               float x,
                               float y,
                               float width,
                               float height);

#define cogl_framebuffer_get_viewport_x cogl_framebuffer_get_viewport_x_EXP
float
cogl_framebuffer_get_viewport_x (CoglFramebuffer *framebuffer);

#define cogl_framebuffer_get_viewport_y cogl_framebuffer_get_viewport_y_EXP
float
cogl_framebuffer_get_viewport_y (CoglFramebuffer *framebuffer);

#define cogl_framebuffer_get_viewport_width cogl_framebuffer_get_viewport_width_EXP
float
cogl_framebuffer_get_viewport_width (CoglFramebuffer *framebuffer);

#define cogl_framebuffer_get_viewport_height cogl_framebuffer_get_viewport_height_EXP
float
cogl_framebuffer_get_viewport_height (CoglFramebuffer *framebuffer);

#define cogl_framebuffer_get_viewport4fv cogl_framebuffer_get_viewport4fv_EXP
void
cogl_framebuffer_get_viewport4fv (CoglFramebuffer *framebuffer,
                                  float *viewport);

/**
 * cogl_framebuffer_get_red_bits:
 * @framebuffer: a pointer to a #CoglFramebuffer
 *
 * Retrieves the number of red bits of @framebuffer
 *
 * Return value: the number of bits
 *
 * Since: 1.8
 * Stability: unstable
 */
#define cogl_framebuffer_get_red_bits cogl_framebuffer_get_red_bits_EXP
int
cogl_framebuffer_get_red_bits (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_get_green_bits:
 * @framebuffer: a pointer to a #CoglFramebuffer
 *
 * Retrieves the number of green bits of @framebuffer
 *
 * Return value: the number of bits
 *
 * Since: 1.8
 * Stability: unstable
 */
#define cogl_framebuffer_get_green_bits cogl_framebuffer_get_green_bits_EXP
int
cogl_framebuffer_get_green_bits (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_get_blue_bits:
 * @framebuffer: a pointer to a #CoglFramebuffer
 *
 * Retrieves the number of blue bits of @framebuffer
 *
 * Return value: the number of bits
 *
 * Since: 1.8
 * Stability: unstable
 */
#define cogl_framebuffer_get_blue_bits cogl_framebuffer_get_blue_bits_EXP
int
cogl_framebuffer_get_blue_bits (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_get_alpha_bits:
 * @framebuffer: a pointer to a #CoglFramebuffer
 *
 * Retrieves the number of alpha bits of @framebuffer
 *
 * Return value: the number of bits
 *
 * Since: 1.8
 * Stability: unstable
 */
#define cogl_framebuffer_get_alpha_bits cogl_framebuffer_get_alpha_bits_EXP
int
cogl_framebuffer_get_alpha_bits (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_get_dither_enabled:
 * @framebuffer: a pointer to a #CoglFramebuffer
 *
 * Returns whether dithering has been requested for the given @framebuffer.
 * See cogl_framebuffer_set_dither_enabled() for more details about dithering.
 *
 * <note>This may return %TRUE even when the underlying @framebuffer
 * display pipeline does not support dithering. This value only represents
 * the user's request for dithering.</note>
 *
 * Return value: %TRUE if dithering has been requested or %FALSE if not.
 */
gboolean
cogl_framebuffer_get_dither_enabled (CoglFramebuffer *framebuffer);

/**
 * cogl_framebuffer_set_dither_enabled:
 * @framebuffer: a pointer to a #CoglFramebuffer
 * @dither_enabled: %TRUE to enable dithering or %FALSE to disable
 *
 * Enables or disabled dithering if supported by the hardware.
 *
 * Dithering is a hardware dependent technique to increase the visible
 * color resolution beyond what the underlying hardware supports by playing
 * tricks with the colors placed into the framebuffer to give the illusion
 * of other colors. (For example this can be compared to half-toning used
 * by some news papers to show varying levels of grey even though their may
 * only be black and white are available).
 *
 * If the current display pipeline for @framebuffer does not support dithering
 * then this has no affect.
 *
 * Dithering is enabled by default.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_framebuffer_set_dither_enabled (CoglFramebuffer *framebuffer,
                                     gboolean dither_enabled);

#define cogl_framebuffer_get_color_mask cogl_framebuffer_get_color_mask_EXP
/**
 * cogl_framebuffer_get_color_mask:
 * @framebuffer: a pointer to a #CoglFramebuffer
 *
 * Gets the current #CoglColorMask of which channels would be written to the
 * current framebuffer. Each bit set in the mask means that the
 * corresponding color would be written.
 *
 * Returns: A #CoglColorMask
 * Since: 1.8
 * Stability: unstable
 */
CoglColorMask
cogl_framebuffer_get_color_mask (CoglFramebuffer *framebuffer);

#define cogl_framebuffer_set_color_mask cogl_framebuffer_set_color_mask_EXP
/**
 * cogl_framebuffer_set_color_mask:
 * @framebuffer: a pointer to a #CoglFramebuffer
 * @color_mask: A #CoglColorMask of which color channels to write to
 *              the current framebuffer.
 *
 * Defines a bit mask of which color channels should be written to the
 * given @framebuffer. If a bit is set in @color_mask that means that
 * color will be written.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_framebuffer_set_color_mask (CoglFramebuffer *framebuffer,
                                 CoglColorMask color_mask);

#define cogl_framebuffer_get_color_format cogl_framebuffer_get_color_format_EXP
/**
 * cogl_framebuffer_get_color_format:
 * @framebuffer: A #CoglFramebuffer framebuffer
 *
 * Queries the common #CoglPixelFormat of all color buffers attached
 * to this framebuffer. For an offscreen framebuffer created with
 * cogl_offscreen_new_to_texture() this will correspond to the format
 * of the texture.
 *
 * Since: 1.8
 * Stability: unstable
 */
CoglPixelFormat
cogl_framebuffer_get_color_format (CoglFramebuffer *framebuffer);

#define cogl_framebuffer_set_samples_per_pixel \
  cogl_framebuffer_set_samples_per_pixel_EXP
/**
 * cogl_framebuffer_set_samples_per_pixel:
 * @framebuffer: A #CoglFramebuffer framebuffer
 * @n: The minimum number of samples per pixel
 *
 * Requires that when rendering to @framebuffer then @n point samples
 * should be made per pixel which will all contribute to the final
 * resolved color for that pixel. The idea is that the hardware aims
 * to get quality similar to what you would get if you rendered
 * everything twice as big (for 4 samples per pixel) and then scaled
 * that image back down with filtering. It can effectively remove the
 * jagged edges of polygons and should be more efficient than if you
 * were to manually render at a higher resolution and downscale
 * because the hardware is often able to take some shortcuts. For
 * example the GPU may only calculate a single texture sample for all
 * points of a single pixel, and for tile based architectures all the
 * extra sample data (such as depth and stencil samples) may be
 * handled on-chip and so avoid increased demand on system memory
 * bandwidth.
 *
 * By default this value is usually set to 0 and that is referred to
 * as "single-sample" rendering. A value of 1 or greater is referred
 * to as "multisample" rendering.
 *
 * <note>There are some semantic differences between single-sample
 * rendering and multisampling with just 1 point sample such as it
 * being redundant to use the cogl_framebuffer_resolve_samples() and
 * cogl_framebuffer_resolve_samples_region() apis with single-sample
 * rendering.</note>
 *
 * <note>It's recommended that
 * cogl_framebuffer_resolve_samples_region() be explicitly used at the
 * end of rendering to a point sample buffer to minimize the number of
 * samples that get resolved. By default Cogl will implicitly resolve
 * all framebuffer samples but if only a small region of a
 * framebuffer has changed this can lead to redundant work being
 * done.</note>
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_framebuffer_set_samples_per_pixel (CoglFramebuffer *framebuffer,
                                        int samples_per_pixel);

#define cogl_framebuffer_get_samples_per_pixel \
  cogl_framebuffer_get_samples_per_pixel_EXP
/**
 * cogl_framebuffer_get_samples_per_pixel:
 * @framebuffer: A #CoglFramebuffer framebuffer
 *
 * Gets the number of points that are sampled per-pixel when
 * rasterizing geometry. Usually by default this will return 0 which
 * means that single-sample not multisample rendering has been chosen.
 * When using a GPU supporting multisample rendering it's possible to
 * increase the number of samples per pixel using
 * cogl_framebuffer_set_samples_per_pixel().
 *
 * Calling cogl_framebuffer_get_samples_per_pixel() before the
 * framebuffer has been allocated will simply return the value set
 * using cogl_framebuffer_set_samples_per_pixel(). After the
 * framebuffer has been allocated the value will reflect the actual
 * number of samples that will be made by the GPU.
 *
 * Returns: The number of point samples made per pixel when
 *          rasterizing geometry or 0 if single-sample rendering
 *          has been chosen.
 *
 * Since: 1.10
 * Stability: unstable
 */
int
cogl_framebuffer_get_samples_per_pixel (CoglFramebuffer *framebuffer);


#define cogl_framebuffer_resolve_samples \
  cogl_framebuffer_resolve_samples_EXP
/**
 * cogl_framebuffer_resolve_samples:
 * @framebuffer: A #CoglFramebuffer framebuffer
 *
 * When point sample rendering (also known as multisample rendering)
 * has been enabled via cogl_framebuffer_set_samples_per_pixel()
 * then you can optionally call this function (or
 * cogl_framebuffer_resolve_samples_region()) to explicitly resolve
 * the point samples into values for the final color buffer.
 *
 * Some GPUs will implicitly resolve the point samples during
 * rendering and so this function is effectively a nop, but with other
 * architectures it is desirable to defer the resolve step until the
 * end of the frame.
 *
 * Since Cogl will automatically ensure samples are resolved if the
 * target color buffer is used as a source this API only needs to be
 * used if explicit control is desired - perhaps because you want to
 * ensure that the resolve is completed in advance to avoid later
 * having to wait for the resolve to complete.
 *
 * If you are performing incremental updates to a framebuffer you
 * should consider using cogl_framebuffer_resolve_samples_region()
 * instead to avoid resolving redundant pixels.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_framebuffer_resolve_samples (CoglFramebuffer *framebuffer);

#define cogl_framebuffer_resolve_samples_region \
  cogl_framebuffer_resolve_samples_region_EXP
/**
 * cogl_framebuffer_resolve_samples_region:
 * @framebuffer: A #CoglFramebuffer framebuffer
 * @x: top-left x coordinate of region to resolve
 * @y: top-left y coordinate of region to resolve
 * @width: width of region to resolve
 * @height: height of region to resolve
 *
 * When point sample rendering (also known as multisample rendering)
 * has been enabled via cogl_framebuffer_set_samples_per_pixel()
 * then you can optionally call this function (or
 * cogl_framebuffer_resolve_samples()) to explicitly resolve the point
 * samples into values for the final color buffer.
 *
 * Some GPUs will implicitly resolve the point samples during
 * rendering and so this function is effectively a nop, but with other
 * architectures it is desirable to defer the resolve step until the
 * end of the frame.
 *
 * Use of this API is recommended if incremental, small updates to
 * a framebuffer are being made because by default Cogl will
 * implicitly resolve all the point samples of the framebuffer which
 * can result in redundant work if only a small number of samples have
 * changed.
 *
 * Because some GPUs implicitly resolve point samples this function
 * only guarantees that at-least the region specified will be resolved
 * and if you have rendered to a larger region then it's possible that
 * other samples may be implicitly resolved.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_framebuffer_resolve_samples_region (CoglFramebuffer *framebuffer,
                                         int x,
                                         int y,
                                         int width,
                                         int height);

#define cogl_framebuffer_get_context cogl_framebuffer_get_context_EXP
/**
 * @framebuffer: A #CoglFramebuffer
 *
 * Can be used to query the #CoglContext a given @framebuffer was
 * instantiated within. This is the #CoglContext that was passed to
 * cogl_onscreen_new() for example.
 *
 * Return value: The #CoglContext that the given @framebuffer was
 *               instantiated within.
 * Since: 1.8
 * Stability: unstable
 */
CoglContext *
cogl_framebuffer_get_context (CoglFramebuffer *framebuffer);

#define cogl_framebuffer_clear cogl_framebuffer_clear_EXP
/**
 * cogl_framebuffer_clear:
 * @framebuffer: A #CoglFramebuffer
 * @buffers: A mask of #CoglBufferBit<!-- -->'s identifying which auxiliary
 *   buffers to clear
 * @color: The color to clear the color buffer too if specified in
 *         @buffers.
 *
 * Clears all the auxiliary buffers identified in the @buffers mask, and if
 * that includes the color buffer then the specified @color is used.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_framebuffer_clear (CoglFramebuffer *framebuffer,
                        unsigned long buffers,
                        const CoglColor *color);

#define cogl_framebuffer_clear4f cogl_framebuffer_clear4f_EXP
/**
 * cogl_framebuffer_clear4f:
 * @framebuffer: A #CoglFramebuffer
 * @buffers: A mask of #CoglBufferBit<!-- -->'s identifying which auxiliary
 *   buffers to clear
 * @red: The red component of color to clear the color buffer too if
 *       specified in @buffers.
 * @green: The green component of color to clear the color buffer too if
 *         specified in @buffers.
 * @blue: The blue component of color to clear the color buffer too if
 *        specified in @buffers.
 * @alpha: The alpha component of color to clear the color buffer too if
 *         specified in @buffers.
 *
 * Clears all the auxiliary buffers identified in the @buffers mask, and if
 * that includes the color buffer then the specified @color is used.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_framebuffer_clear4f (CoglFramebuffer *framebuffer,
                          unsigned long buffers,
                          float red,
                          float green,
                          float blue,
                          float alpha);

/* XXX: Should we take an n_buffers + buffer id array instead of using
 * the CoglBufferBits type which doesn't seem future proof? */
#define cogl_framebuffer_discard_buffers cogl_framebuffer_discard_buffers_EXP
/**
 * cogl_framebuffer_discard_buffers:
 * @framebuffer: A #CoglFramebuffer
 * @buffers: A #CoglBufferBit mask of which ancillary buffers you want
 *           to discard.
 *
 * Declares that the specified @buffers no longer need to be referenced
 * by any further rendering commands. This can be an important
 * optimization to avoid subsequent frames of rendering depending on
 * the results of a previous frame.
 *
 * For example; some tile-based rendering GPUs are able to avoid allocating and
 * accessing system memory for the depth and stencil buffer so long as these
 * buffers are not required as input for subsequent frames and that can save a
 * significant amount of memory bandwidth used to save and restore their
 * contents to system memory between frames.
 *
 * It is currently considered an error to try and explicitly discard the color
 * buffer by passing %COGL_BUFFER_BIT_COLOR. This is because the color buffer is
 * already implicitly discard when you finish rendering to a #CoglOnscreen
 * framebuffer, and it's not meaningful to try and discard the color buffer of
 * a #CoglOffscreen framebuffer since they are single-buffered.
 *
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_framebuffer_discard_buffers (CoglFramebuffer *framebuffer,
                                  unsigned long buffers);

/**
 * cogl_framebuffer_finish:
 * @framebuffer: A #CoglFramebuffer pointer
 *
 * This blocks the CPU until all pending rendering associated with the
 * specified framebuffer has completed. It's very rare that developers should
 * ever need this level of synchronization with the GPU and should never be
 * used unless you clearly understand why you need to explicitly force
 * synchronization.
 *
 * One example might be for benchmarking purposes to be sure timing
 * measurements reflect the time that the GPU is busy for not just the time it
 * takes to queue rendering commands.
 *
 * Stability: unstable
 * Since: 1.10
 */
void
cogl_framebuffer_finish (CoglFramebuffer *framebuffer);

#define cogl_get_draw_framebuffer cogl_get_draw_framebuffer_EXP
CoglFramebuffer *
cogl_get_draw_framebuffer (void);

#endif /* COGL_ENABLE_EXPERIMENTAL_API */

/* XXX: Note these are defined outside the COGL_ENABLE_EXPERIMENTAL_API guard since
 * otherwise the glib-mkenums stuff will get upset. */

#define cogl_framebuffer_error_quark cogl_framebuffer_error_quark_EXP
GQuark
cogl_framebuffer_error_quark (void);

#define COGL_FRAMEBUFFER_ERROR (cogl_framebuffer_error_quark ())

typedef enum { /*< prefix=COGL_FRAMEBUFFER_ERROR >*/
  COGL_FRAMEBUFFER_ERROR_ALLOCATE
} CoglFramebufferError;

G_END_DECLS

#endif /* __COGL_FRAMEBUFFER_H */

