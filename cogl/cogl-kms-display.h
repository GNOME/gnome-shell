
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_KMS_DISPLAY_H__
#define __COGL_KMS_DISPLAY_H__

#include <cogl/cogl-types.h>
#include <cogl/cogl-display.h>

#include <xf86drmMode.h>

COGL_BEGIN_DECLS

/**
 * cogl_kms_display_queue_modes_reset:
 * @display: A #CoglDisplay
 *
 * Asks Cogl to explicitly reset the crtc output modes at the next
 * #CoglOnscreen swap_buffers request. For applications that support
 * VT switching they may want to re-assert the output modes when
 * switching back to the applications VT since the modes are often not
 * correctly restored automatically.
 *
 * <note>The @display must have been either explicitly setup via
 * cogl_display_setup() or implicitily setup by having created a
 * context using the @display</note>
 *
 * Since: 2.0
 * Stability: unstable
 */
void
cogl_kms_display_queue_modes_reset (CoglDisplay *display);

typedef struct {
  uint32_t id;
  uint32_t x, y;
  drmModeModeInfo mode;

  uint32_t *connectors;
  uint32_t  count;
} CoglKmsCrtc;

/**
 * cogl_kms_display_set_layout:
 * @onscreen: a #CoglDisplay
 * @width: the framebuffer width
 * @height: the framebuffer height
 * @crtcs: the array of #CoglKmsCrtc structure with the desired CRTC layout
 *
 * Configures @display to use a framebuffer sized @width x @height, covering
 * the CRTCS in @crtcs.
 * @width and @height must be within the driver framebuffer limits, and @crtcs
 * must be valid KMS API IDs.
 *
 * Calling this function overrides the automatic mode setting done by Cogl,
 * and for this reason must be called before the first call to cogl_onscreen_swap_buffers().
 *
 * If you want to restore the default behaviour, you can call this function
 * with @width and @height set to -1.
 *
 * Stability: unstable
 */
CoglBool
cogl_kms_display_set_layout (CoglDisplay *display,
                             int width,
                             int height,
                             CoglKmsCrtc **crtcs,
                             int n_crtcs,
                             CoglError **error);

COGL_END_DECLS
#endif /* __COGL_KMS_DISPLAY_H__ */
