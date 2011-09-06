/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2010 Intel Corporation.
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
 * Authors:
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_DISPLAY_H__
#define __COGL_DISPLAY_H__

#include <cogl/cogl-renderer.h>
#include <cogl/cogl-onscreen-template.h>

G_BEGIN_DECLS

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
#include <libgdl.h>
#endif

/**
 * SECTION:cogl-display
 * @short_description: Represents a display pipeline
 *
 * TODO: We still need to decide if we really need this object or if
 * it's enough to just have the CoglSwapChain CoglOnscreenTemplate
 * objects.
 *
 * The basic intention is for this object to let the application
 * specify its display preferences before creating a context, and
 * there are a few different aspects to this...
 *
 * Firstly there is the physical display pipeline that is currently
 * being used including the digital to analogue conversion hardware
 * and the screen the user sees. Although we don't have a plan to
 * expose all the advanced features of arbitrary display hardware with
 * a Cogl API, some backends may want to expose limited control over
 * this hardware via Cogl and simpler features like providing a list
 * of modes to choose from in a UI could be nice too.
 *
 * Another aspect is that the display configuration may be tightly
 * related to how onscreen framebuffers should be configured. In fact
 * one of the early rationals for this object was to let us handle
 * GLX's requirement that framebuffers must be "compatible" with the
 * fbconfig associated with the current context meaning we have to
 * force the user to describe how they would like to create their
 * onscreen windows before we can choose a suitable fbconfig and
 * create a GLContext.
 *
 * TODO: continue this thought process and come to a decision...
 */

typedef struct _CoglDisplay	      CoglDisplay;

#define COGL_DISPLAY(OBJECT) ((CoglDisplay *)OBJECT)

#define cogl_display_new cogl_display_new_EXP
CoglDisplay *
cogl_display_new (CoglRenderer *renderer,
                  CoglOnscreenTemplate *onscreen_template);

#define cogl_display_get_renderer cogl_display_get_renderer_EXP
CoglRenderer *
cogl_display_get_renderer (CoglDisplay *display);

#define cogl_display_setup cogl_display_setup_EXP
gboolean
cogl_display_setup (CoglDisplay *display,
                    GError **error);

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
#define cogl_gdl_display_set_plane \
  cogl_gdl_display_set_plane_EXP
void
cogl_gdl_display_set_plane (CoglDisplay *display,
                            gdl_plane_id_t plane);
#endif

#ifdef COGL_HAS_WAYLAND_EGL_SERVER_SUPPORT
void
cogl_wayland_display_set_compositor_display (CoglDisplay *display,
                                          struct wl_display *wayland_display);
#endif

G_END_DECLS

#endif /* __COGL_DISPLAY_H__ */

