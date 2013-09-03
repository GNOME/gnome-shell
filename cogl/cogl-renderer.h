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

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_RENDERER_H__
#define __COGL_RENDERER_H__

#include <cogl/cogl-types.h>
#include <cogl/cogl-onscreen-template.h>
#include <cogl/cogl-error.h>
#include <cogl/cogl-output.h>

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-renderer
 * @short_description: Choosing a means to render
 *
 * A #CoglRenderer represents a means to render. It encapsulates the
 * selection of an underlying driver, such as OpenGL or OpenGL-ES and
 * a selection of a window system binding API such as GLX, or EGL or
 * WGL.
 *
 * A #CoglRenderer has two states, "unconnected" and "connected". When
 * a renderer is first instantiated using cogl_renderer_new() it is
 * unconnected so that it can be configured and constraints can be
 * specified for how the backend driver and window system should be
 * chosen.
 *
 * After configuration a #CoglRenderer can (optionally) be explicitly
 * connected using cogl_renderer_connect() which allows for the
 * handling of connection errors so that fallback configurations can
 * be tried if necessary. Applications that don't support any
 * fallbacks though can skip using cogl_renderer_connect() and leave
 * Cogl to automatically connect the renderer.
 *
 * Once you have a configured #CoglRenderer it can be used to create a
 * #CoglDisplay object using cogl_display_new().
 *
 * <note>Many applications don't need to explicitly use
 * cogl_renderer_new() or cogl_display_new() and can just jump
 * straight to cogl_context_new() and pass a %NULL display argument so
 * Cogl will automatically connect and setup a renderer and
 * display.</note>
 */


/**
 * COGL_RENDERER_ERROR:
 *
 * An error domain for exceptions reported by Cogl
 */
#define COGL_RENDERER_ERROR cogl_renderer_error_quark ()

uint32_t
cogl_renderer_error_quark (void);

typedef struct _CoglRenderer CoglRenderer;

/**
 * cogl_is_renderer:
 * @object: A #CoglObject pointer
 *
 * Determines if the given @object is a #CoglRenderer
 *
 * Return value: %TRUE if @object is a #CoglRenderer, else %FALSE.
 * Since: 1.10
 * Stability: unstable
 */
CoglBool
cogl_is_renderer (void *object);

/**
 * cogl_renderer_new:
 *
 * Instantiates a new (unconnected) #CoglRenderer object. A
 * #CoglRenderer represents a means to render. It encapsulates the
 * selection of an underlying driver, such as OpenGL or OpenGL-ES and
 * a selection of a window system binding API such as GLX, or EGL or
 * WGL.
 *
 * While the renderer is unconnected it can be configured so that
 * applications may specify backend constraints, such as "must use
 * x11" for example via cogl_renderer_add_constraint().
 *
 * There are also some platform specific configuration apis such
 * as cogl_xlib_renderer_set_foreign_display() that may also be
 * used while the renderer is unconnected.
 *
 * Once the renderer has been configured, then it may (optionally) be
 * explicitly connected using cogl_renderer_connect() which allows
 * errors to be handled gracefully and potentially fallback
 * configurations can be tried out if there are initial failures.
 *
 * If a renderer is not explicitly connected then cogl_display_new()
 * will automatically connect the renderer for you. If you don't
 * have any code to deal with error/fallback situations then its fine
 * to just let Cogl do the connection for you.
 *
 * Once you have setup your renderer then the next step is to create a
 * #CoglDisplay using cogl_display_new().
 *
 * <note>Many applications don't need to explicitly use
 * cogl_renderer_new() or cogl_display_new() and can just jump
 * straight to cogl_context_new() and pass a %NULL display argument
 * so Cogl will automatically connect and setup a renderer and
 * display.</note>
 *
 * Return value: (transfer full): A newly created #CoglRenderer.
 *
 * Since: 1.10
 * Stability: unstable
 */
CoglRenderer *
cogl_renderer_new (void);

/* optional configuration APIs */

/**
 * CoglWinsysID:
 * @COGL_WINSYS_ID_ANY: Implies no preference for which backend is used
 * @COGL_WINSYS_ID_STUB: Use the no-op stub backend
 * @COGL_WINSYS_ID_GLX: Use the GLX window system binding API
 * @COGL_WINSYS_ID_EGL_XLIB: Use EGL with the X window system via XLib
 * @COGL_WINSYS_ID_EGL_NULL: Use EGL with the PowerVR NULL window system
 * @COGL_WINSYS_ID_EGL_GDL: Use EGL with the GDL platform
 * @COGL_WINSYS_ID_EGL_WAYLAND: Use EGL with the Wayland window system
 * @COGL_WINSYS_ID_EGL_KMS: Use EGL with the KMS platform
 * @COGL_WINSYS_ID_EGL_ANDROID: Use EGL with the Android platform
 * @COGL_WINSYS_ID_WGL: Use the Microsoft Windows WGL binding API
 * @COGL_WINSYS_ID_SDL: Use the SDL window system
 *
 * Identifies specific window system backends that Cogl supports.
 *
 * These can be used to query what backend Cogl is using or to try and
 * explicitly select a backend to use.
 */
typedef enum
{
  COGL_WINSYS_ID_ANY,
  COGL_WINSYS_ID_STUB,
  COGL_WINSYS_ID_GLX,
  COGL_WINSYS_ID_EGL_XLIB,
  COGL_WINSYS_ID_EGL_NULL,
  COGL_WINSYS_ID_EGL_GDL,
  COGL_WINSYS_ID_EGL_WAYLAND,
  COGL_WINSYS_ID_EGL_KMS,
  COGL_WINSYS_ID_EGL_ANDROID,
  COGL_WINSYS_ID_WGL,
  COGL_WINSYS_ID_SDL
} CoglWinsysID;

/**
 * cogl_renderer_set_winsys_id:
 * @renderer: A #CoglRenderer
 * @winsys_id: An ID of the winsys you explicitly want to use.
 *
 * This allows you to explicitly select a winsys backend to use instead
 * of letting Cogl automatically select a backend.
 *
 * if you select an unsupported backend then cogl_renderer_connect()
 * will fail and report an error.
 *
 * This may only be called on an un-connected #CoglRenderer.
 */
void
cogl_renderer_set_winsys_id (CoglRenderer *renderer,
                             CoglWinsysID winsys_id);

/**
 * cogl_renderer_get_winsys_id:
 * @renderer: A #CoglRenderer
 *
 * Queries which window system backend Cogl has chosen to use.
 *
 * This may only be called on a connected #CoglRenderer.
 *
 * Returns: The #CoglWinsysID corresponding to the chosen window
 *          system backend.
 */
CoglWinsysID
cogl_renderer_get_winsys_id (CoglRenderer *renderer);

/**
 * cogl_renderer_get_n_fragment_texture_units:
 * @renderer: A #CoglRenderer
 *
 * Queries how many texture units can be used from fragment programs
 *
 * Returns: the number of texture image units.
 *
 * Since: 1.8
 * Stability: Unstable
 */
int
cogl_renderer_get_n_fragment_texture_units (CoglRenderer *renderer);

/**
 * cogl_renderer_check_onscreen_template:
 * @renderer: A #CoglRenderer
 * @onscreen_template: A #CoglOnscreenTemplate
 * @error: A pointer to a #CoglError for reporting exceptions
 *
 * Tests if a given @onscreen_template can be supported with the given
 * @renderer.
 *
 * Return value: %TRUE if the @onscreen_template can be supported,
 *               else %FALSE.
 * Since: 1.10
 * Stability: unstable
 */
CoglBool
cogl_renderer_check_onscreen_template (CoglRenderer *renderer,
                                       CoglOnscreenTemplate *onscreen_template,
                                       CoglError **error);

/* Final connection API */

/**
 * cogl_renderer_connect:
 * @renderer: An unconnected #CoglRenderer
 * @error: a pointer to a #CoglError for reporting exceptions
 *
 * Connects the configured @renderer. Renderer connection isn't a
 * very active process, it basically just means validating that
 * any given constraint criteria can be satisfied and that a
 * usable driver and window system backend can be found.
 *
 * Return value: %TRUE if there was no error while connecting the
 *               given @renderer. %FALSE if there was an error.
 * Since: 1.10
 * Stability: unstable
 */
CoglBool
cogl_renderer_connect (CoglRenderer *renderer, CoglError **error);

/**
 * CoglRendererConstraint:
 * @COGL_RENDERER_CONSTRAINT_USES_X11: Require the renderer to be X11 based
 * @COGL_RENDERER_CONSTRAINT_USES_XLIB: Require the renderer to be X11
 *                                      based and use Xlib
 * @COGL_RENDERER_CONSTRAINT_USES_EGL: Require the renderer to be EGL based
 * @COGL_RENDERER_CONSTRAINT_SUPPORTS_COGL_GLES2: Require that the
 *    renderer supports creating a #CoglGLES2Context via
 *    cogl_gles2_context_new(). This can be used to integrate GLES 2.0
 *    code into Cogl based applications.
 *
 * These constraint flags are hard-coded features of the different renderer
 * backends. Sometimes a platform may support multiple rendering options which
 * Cogl will usually choose from automatically. Some of these features are
 * important to higher level applications and frameworks though, such as
 * whether a renderer is X11 based because an application might only support
 * X11 based input handling. An application might also need to ensure EGL is
 * used internally too if they depend on access to an EGLDisplay for some
 * purpose.
 *
 * Applications should ideally minimize how many of these constraints
 * they depend on to ensure maximum portability.
 *
 * Since: 1.10
 * Stability: unstable
 */
typedef enum
{
  COGL_RENDERER_CONSTRAINT_USES_X11 = (1 << 0),
  COGL_RENDERER_CONSTRAINT_USES_XLIB = (1 << 1),
  COGL_RENDERER_CONSTRAINT_USES_EGL = (1 << 2),
  COGL_RENDERER_CONSTRAINT_SUPPORTS_COGL_GLES2 = (1 << 3)
} CoglRendererConstraint;


/**
 * cogl_renderer_add_constraint:
 * @renderer: An unconnected #CoglRenderer
 * @constraint: A #CoglRendererConstraint to add
 *
 * This adds a renderer selection @constraint.
 *
 * Applications should ideally minimize how many of these constraints they
 * depend on to ensure maximum portability.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_renderer_add_constraint (CoglRenderer *renderer,
                              CoglRendererConstraint constraint);

/**
 * cogl_renderer_remove_constraint:
 * @renderer: An unconnected #CoglRenderer
 * @constraint: A #CoglRendererConstraint to remove
 *
 * This removes a renderer selection @constraint.
 *
 * Applications should ideally minimize how many of these constraints they
 * depend on to ensure maximum portability.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_renderer_remove_constraint (CoglRenderer *renderer,
                                 CoglRendererConstraint constraint);

/**
 * CoglDriver:
 * @COGL_DRIVER_ANY: Implies no preference for which driver is used
 * @COGL_DRIVER_NOP: A No-Op driver.
 * @COGL_DRIVER_GL: An OpenGL driver.
 * @COGL_DRIVER_GL3: An OpenGL driver using the core GL 3.1 profile
 * @COGL_DRIVER_GLES1: An OpenGL ES 1.1 driver.
 * @COGL_DRIVER_GLES2: An OpenGL ES 2.0 driver.
 * @COGL_DRIVER_WEBGL: A WebGL driver.
 *
 * Identifiers for underlying hardware drivers that may be used by
 * Cogl for rendering.
 *
 * Since: 1.10
 * Stability: unstable
 */
typedef enum
{
  COGL_DRIVER_ANY,
  COGL_DRIVER_NOP,
  COGL_DRIVER_GL,
  COGL_DRIVER_GL3,
  COGL_DRIVER_GLES1,
  COGL_DRIVER_GLES2,
  COGL_DRIVER_WEBGL
} CoglDriver;

/**
 * cogl_renderer_set_driver:
 * @renderer: An unconnected #CoglRenderer
 *
 * Requests that Cogl should try to use a specific underlying driver
 * for rendering.
 *
 * If you select an unsupported driver then cogl_renderer_connect()
 * will fail and report an error. Most applications should not
 * explicitly select a driver and should rely on Cogl automatically
 * choosing the driver.
 *
 * This may only be called on an un-connected #CoglRenderer.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_renderer_set_driver (CoglRenderer *renderer,
                          CoglDriver driver);

/**
 * cogl_renderer_get_driver:
 * @renderer: A connected #CoglRenderer
 *
 * Queries what underlying driver is being used by Cogl.
 *
 * This may only be called on a connected #CoglRenderer.
 *
 * Since: 1.10
 * Stability: unstable
 */
CoglDriver
cogl_renderer_get_driver (CoglRenderer *renderer);

/**
 * CoglOutputCallback:
 * @output: The current display output being iterated
 * @user_data: The user pointer passed to
 *             cogl_renderer_foreach_output()
 *
 * A callback type that can be passed to
 * cogl_renderer_foreach_output() for iterating display outputs for a
 * given renderer.
 *
 * Since: 1.14
 * Stability: Unstable
 */
typedef void (*CoglOutputCallback) (CoglOutput *output, void *user_data);

/**
 * cogl_renderer_foreach_output:
 * @renderer: A connected #CoglRenderer
 * @callback: A #CoglOutputCallback to be called for each display output
 * @user_data: A user pointer to be passed to @callback
 *
 * Iterates all known display outputs for the given @renderer and
 * passes a corresponding #CoglOutput pointer to the given @callback
 * for each one, along with the given @user_data.
 *
 * Since: 1.14
 * Stability: Unstable
 */
void
cogl_renderer_foreach_output (CoglRenderer *renderer,
                              CoglOutputCallback callback,
                              void *user_data);

COGL_END_DECLS

#endif /* __COGL_RENDERER_H__ */

