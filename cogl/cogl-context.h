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

#ifndef __COGL_CONTEXT_H__
#define __COGL_CONTEXT_H__

#include <cogl/cogl-defines.h>
#include <cogl/cogl-display.h>
#ifdef COGL_HAS_EGL_PLATFORM_ANDROID_SUPPORT
#include <android/native_window.h>
#endif

G_BEGIN_DECLS

/**
 * SECTION:cogl-context
 * @short_description: The top level application context.
 *
 * A #CoglContext is the top most sandbox of Cogl state for an
 * application or toolkit. Its main purpose is to act as a sandbox
 * for the memory management of state objects. Normally an application
 * will only create a single context since there is no way to share
 * resources between contexts.
 *
 * For those familiar with OpenGL or perhaps Cairo it should be
 * understood that unlike these APIs a Cogl context isn't a rendering
 * context as such. In other words Cogl doesn't aim to provide a state
 * machine style model for configuring rendering parameters. Most
 * rendering state in Cogl is directly associated with user managed
 * objects called pipelines and geometry is drawn with a specific
 * pipeline object to a framebuffer object and those 3 things fully
 * define the state for drawing. This is an important part of Cogl's
 * design since it helps you write orthogonal rendering components
 * that can all access the same GPU without having to worry about
 * what state other components have left you with.
 */

typedef struct _CoglContext	      CoglContext;

#define COGL_CONTEXT(OBJECT) ((CoglContext *)OBJECT)

/**
 * cogl_context_new:
 * @display: A #CoglDisplay pointer
 * @error: A GError return location.
 *
 * Creates a new #CoglContext which acts as an application sandbox
 * for any state objects that are allocated.
 *
 * Return value: (transfer full): A newly allocated #CoglContext
 * Since: 1.8
 * Stability: unstable
 */
CoglContext *
cogl_context_new (CoglDisplay *display,
                  GError **error);

/**
 * cogl_context_get_display:
 * @context: A #CoglContext pointer
 *
 * Retrieves the #CoglDisplay that is internally associated with the
 * given @context. This will return the same #CoglDisplay that was
 * passed to cogl_context_new() or if %NULL was passed to
 * cogl_context_new() then this function returns a pointer to the
 * display that was automatically setup internally.
 *
 * Return value: (transfer none): The #CoglDisplay associated with the
 *               given @context.
 * Since: 1.8
 * Stability: unstable
 */
CoglDisplay *
cogl_context_get_display (CoglContext *context);

#ifdef COGL_HAS_EGL_SUPPORT
/**
 * cogl_egl_context_get_egl_display:
 * @context: A #CoglContext pointer
 *
 * If you have done a runtime check to determine that Cogl is using
 * EGL internally then this API can be used to retrieve the EGLDisplay
 * handle that was setup internally. The result is undefined if Cogl
 * is not using EGL.
 *
 * Return value: The internally setup EGLDisplay handle.
 * Since: 1.8
 * Stability: unstable
 */
EGLDisplay
cogl_egl_context_get_egl_display (CoglContext *context);
#endif

#ifdef COGL_HAS_EGL_PLATFORM_ANDROID_SUPPORT
/**
 * cogl_android_set_native_window:
 * @window: A native Android window
 *
 * Allows Android applications to inform Cogl of the native window
 * that they have been given which Cogl can render too. On Android
 * this API must be used before creating a #CoglRenderer, #CoglDisplay
 * and #CoglContext.
 *
 * Since: 1.8
 * Stability: unstable
 */
void
cogl_android_set_native_window (ANativeWindow *window);
#endif

/**
 * cogl_is_context:
 * @object: An object or %NULL
 *
 * Gets whether the given object references an existing context object.
 *
 * Return value: %TRUE if the handle references a #CoglContext,
 *   %FALSE otherwise
 *
 * Since: 1.10
 * Stability: Unstable
 */
gboolean
cogl_is_context (void *object);

G_END_DECLS

#endif /* __COGL_CONTEXT_H__ */

