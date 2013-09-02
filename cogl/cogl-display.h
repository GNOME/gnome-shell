/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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
 * Authors:
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_DISPLAY_H__
#define __COGL_DISPLAY_H__

#include <cogl/cogl-renderer.h>
#include <cogl/cogl-onscreen-template.h>

#ifdef COGL_HAS_GTYPE_SUPPORT
#include <glib-object.h>
#endif

COGL_BEGIN_DECLS

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
#include <libgdl.h>
#endif

/**
 * SECTION:cogl-display
 * @short_description: Common aspects of a display pipeline
 *
 * The basic intention for this object is to let the application
 * configure common display preferences before creating a context, and
 * there are a few different aspects to this...
 *
 * Firstly there are options directly relating to the physical display
 * pipeline that is currently being used including the digital to
 * analogue conversion hardware and the screens the user sees.
 *
 * Another aspect is that display options may constrain or affect how
 * onscreen framebuffers should later be configured. The original
 * rationale for the display object in fact was to let us handle GLX
 * and EGLs requirements that framebuffers must be "compatible" with
 * the config associated with the current context meaning we have to
 * force the user to describe how they would like to create their
 * onscreen windows before we can choose a suitable fbconfig and
 * create a GLContext.
 */

typedef struct _CoglDisplay	      CoglDisplay;

#define COGL_DISPLAY(OBJECT) ((CoglDisplay *)OBJECT)

#ifdef COGL_HAS_GTYPE_SUPPORT
/**
 * cogl_display_get_gtype:
 *
 * Returns: a #GType that can be used with the GLib type system.
 */
GType cogl_display_get_gtype (void);
#endif

/**
 * cogl_display_new:
 * @renderer: A #CoglRenderer
 * @onscreen_template: A #CoglOnscreenTemplate
 *
 * Explicitly allocates a new #CoglDisplay object to encapsulate the
 * common state of the display pipeline that applies to the whole
 * application.
 *
 * <note>Many applications don't need to explicitly use
 * cogl_display_new() and can just jump straight to cogl_context_new()
 * and pass a %NULL display argument so Cogl will automatically
 * connect and setup a renderer and display.</note>
 *
 * A @display can only be made for a specific choice of renderer which
 * is why this takes the @renderer argument.
 *
 * A common use for explicitly allocating a display object is to
 * define a template for allocating onscreen framebuffers which is
 * what the @onscreen_template argument is for, or alternatively
 * you can use cogl_display_set_onscreen_template().
 *
 * When a display is first allocated via cogl_display_new() it is in a
 * mutable configuration mode. It's designed this way so we can
 * extend the apis available for configuring a display without
 * requiring huge numbers of constructor arguments.
 *
 * When you have finished configuring a display object you can
 * optionally call cogl_display_setup() to explicitly apply the
 * configuration and check for errors. Alternaitvely you can pass the
 * display to cogl_context_new() and Cogl will implicitly apply your
 * configuration but if there are errors then the application will
 * abort with a message. For simple applications with no fallback
 * options then relying on the implicit setup can be fine.
 *
 * Return value: (transfer full): A newly allocated #CoglDisplay
 *               object in a mutable configuration mode.
 * Since: 1.10
 * Stability: unstable
 */
CoglDisplay *
cogl_display_new (CoglRenderer *renderer,
                  CoglOnscreenTemplate *onscreen_template);

/**
 * cogl_display_get_renderer:
 * @display: a #CoglDisplay
 *
 * Queries the #CoglRenderer associated with the given @display.
 *
 * Return value: (transfer none): The associated #CoglRenderer
 *
 * Since: 1.10
 * Stability: unstable
 */
CoglRenderer *
cogl_display_get_renderer (CoglDisplay *display);

/**
 * cogl_display_set_onscreen_template:
 * @display: a #CoglDisplay
 * @onscreen_template: A template for creating #CoglOnscreen framebuffers
 *
 * Specifies a template for creating #CoglOnscreen framebuffers.
 *
 * Depending on the system, the constraints for creating #CoglOnscreen
 * framebuffers need to be known before setting up a #CoglDisplay because the
 * final setup of the display may constrain how onscreen framebuffers may be
 * allocated. If Cogl knows how an application wants to allocate onscreen
 * framebuffers then it can try to make sure to setup the display accordingly.
 *
 * Since: 1.16
 * Stability: unstable
 */
void
cogl_display_set_onscreen_template (CoglDisplay *display,
                                    CoglOnscreenTemplate *onscreen_template);

/**
 * cogl_display_setup:
 * @display: a #CoglDisplay
 * @error: return location for a #CoglError
 *
 * Explicitly sets up the given @display object. Use of this api is
 * optional since Cogl will internally setup the display if not done
 * explicitly.
 *
 * When a display is first allocated via cogl_display_new() it is in a
 * mutable configuration mode. This allows us to extend the apis
 * available for configuring a display without requiring huge numbers
 * of constructor arguments.
 *
 * Its possible to request a configuration that might not be
 * supportable on the current system and so this api provides a means
 * to apply the configuration explicitly but if it fails then an
 * exception will be returned so you can handle the error gracefully
 * and perhaps fall back to an alternative configuration.
 *
 * If you instead rely on Cogl implicitly calling cogl_display_setup()
 * for you then if there is an error with the configuration you won't
 * get an opportunity to handle that and the application may abort
 * with a message.  For simple applications that don't have any
 * fallback options this behaviour may be fine.
 *
 * Return value: Returns %TRUE if there was no error, else it returns
 *               %FALSE and returns an exception via @error.
 * Since: 1.10
 * Stability: unstable
 */
CoglBool
cogl_display_setup (CoglDisplay *display,
                    CoglError **error);

#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
/**
 * cogl_gdl_display_set_plane:
 * @display: a #CoglDisplay
 * @plane: the GDL plane id
 *
 * Request that Cogl output to a specific GDL overlay @plane.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_gdl_display_set_plane (CoglDisplay *display,
                            gdl_plane_id_t plane);
#endif

/**
 * cogl_is_display:
 * @object: A #CoglObject pointer
 *
 * Gets whether the given object references a #CoglDisplay.
 *
 * Return value: %TRUE if the object references a #CoglDisplay
 *   and %FALSE otherwise.
 * Since: 1.10
 * Stability: unstable
 */
CoglBool
cogl_is_display (void *object);

COGL_END_DECLS

#endif /* __COGL_DISPLAY_H__ */

