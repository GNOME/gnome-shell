/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Collabora Ltd.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  Tomeu Vizoso <tomeu.vizoso@collabora.com>
 *  Robert Bragg <robert@linux.intel.com>
 *
 */

#ifndef __COGL_GLES2_H__
#define __COGL_GLES2_H__

/* NB: cogl-gles2.h is a top-level header that can be included directly
 * but we want to be careful not to define __COGL_H_INSIDE__ when this
 * is included internally while building Cogl itself since
 * __COGL_H_INSIDE__ is used in headers to guard public vs private
 * api definitions
 */
#ifndef COGL_COMPILATION

/* Note: When building Cogl .gir we explicitly define
 * __COGL_H_INSIDE__ */
#ifndef __COGL_H_INSIDE__
#define __COGL_H_INSIDE__
#define __COGL_MUST_UNDEF_COGL_H_INSIDE__
#endif

#endif /* COGL_COMPILATION */

#include <cogl/cogl-defines.h>
#include <cogl/cogl-context.h>
#include <cogl/cogl-framebuffer.h>
#include <cogl/cogl-texture.h>
#include <cogl/cogl-texture-2d.h>

/* CoglGLES2Vtable depends on GLES 2.0 typedefs being available but we
 * want to be careful that the public api doesn't expose arbitrary
 * system GL headers as part of the Cogl API so although when building
 * internally we consistently refer to the system headers to avoid
 * conflicts we only expose the minimal set of GLES 2.0 types and enums
 * publicly.
 */
#ifdef COGL_COMPILATION
#include "cogl-gl-header.h"
#else
#include <cogl/cogl-gles2-types.h>
#endif

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-gles2
 * @short_description: A portable api to access OpenGLES 2.0
 *
 * Cogl provides portable access to the OpenGLES api through a single
 * library that is able to smooth over inconsistencies between the
 * different vendor drivers for OpenGLES in a single place.
 *
 * The api is designed to allow Cogl to transparently implement the
 * api on top of other drivers, such as OpenGL, D3D or on Cogl's own
 * drawing api so even if your platform doesn't come with an
 * OpenGLES 2.0 api Cogl may still be able to expose the api to your
 * application.
 *
 * Since Cogl is a library and not an api specification it is possible
 * to add OpenGLES 2.0 api features to Cogl which can immidiately
 * benefit developers regardless of what platform they are running on.
 *
 * With this api it's possible to re-use existing OpenGLES 2.0 code
 * within applications that are rendering with the Cogl API and also
 * it's possible for applications that render using OpenGLES 2.0 to
 * incorporate content rendered with Cogl.
 *
 * Applications can check for OpenGLES 2.0 api support by checking for
 * %COGL_FEATURE_ID_GLES2_CONTEXT support with cogl_has_feature().
 *
 * Since: 1.12
 * Stability: unstable
 */

/**
 * CoglGLES2Context:
 *
 * Represents an OpenGLES 2.0 api context used as a sandbox for
 * OpenGLES 2.0 state. This is comparable to an EGLContext for those
 * who have used OpenGLES 2.0 with EGL before.
 *
 * Since: 1.12
 * Stability: unstable
 */
typedef struct _CoglGLES2Context CoglGLES2Context;

/**
 * CoglGLES2Vtable:
 *
 * Provides function pointers for the full OpenGLES 2.0 api. The
 * api must be accessed this way and not by directly calling
 * symbols of any system OpenGLES 2.0 api.
 *
 * Since: 1.12
 * Stability: unstable
 */
typedef struct _CoglGLES2Vtable CoglGLES2Vtable;

struct _CoglGLES2Vtable
{
  /*< private >*/
#define COGL_EXT_BEGIN(name, \
                       min_gl_major, min_gl_minor, \
                       gles_availability, \
                       extension_suffixes, extension_names)

#define COGL_EXT_FUNCTION(ret, name, args) \
  ret (* name) args;

#define COGL_EXT_END()

#include <cogl/gl-prototypes/cogl-gles2-functions.h>

#undef COGL_EXT_BEGIN
#undef COGL_EXT_FUNCTION
#undef COGL_EXT_END
};

uint32_t
_cogl_gles2_context_error_quark (void);

/**
 * COGL_GLES2_CONTEXT_ERROR:
 *
 * An error domain for runtime exceptions relating to the
 * cogl_gles2_context api.
 *
 * Since: 2.0
 * Stability: unstable
 */
#define COGL_GLES2_CONTEXT_ERROR (_cogl_gles2_context_error_quark ())

/**
 * CoglGLES2ContextError:
 * @COGL_GLES2_CONTEXT_ERROR_UNSUPPORTED: Creating GLES2 contexts
 *    isn't supported. Applications should use cogl_has_feature() to
 *    check for the %COGL_FEATURE_ID_GLES2_CONTEXT.
 * @COGL_GLES2_CONTEXT_ERROR_DRIVER: An underlying driver error
 *    occured.
 *
 * Error codes that relate to the cogl_gles2_context api.
 */
typedef enum { /*< prefix=COGL_GLES2_CONTEXT_ERROR >*/
  COGL_GLES2_CONTEXT_ERROR_UNSUPPORTED,
  COGL_GLES2_CONTEXT_ERROR_DRIVER
} CoglGLES2ContextError;

/**
 * cogl_gles2_context_new:
 * @ctx: A #CoglContext
 * @error: A pointer to a #CoglError for returning exceptions
 *
 * Allocates a new OpenGLES 2.0 context that can be used to render to
 * #CoglOffscreen framebuffers (Rendering to #CoglOnscreen
 * framebuffers is not currently supported).
 *
 * To actually access the OpenGLES 2.0 api itself you need to use
 * cogl_gles2_context_get_vtable(). You should not try to directly link
 * to and use the symbols provided by the a system OpenGLES 2.0
 * driver.
 *
 * Once you have allocated an OpenGLES 2.0 context you can make it
 * current using cogl_push_gles2_context(). For those familiar with
 * using the EGL api, this serves a similar purpose to eglMakeCurrent.
 *
 * <note>Before using this api applications can check for OpenGLES 2.0
 * api support by checking for %COGL_FEATURE_ID_GLES2_CONTEXT support
 * with cogl_has_feature(). This function will return %FALSE and
 * return an %COGL_GLES2_CONTEXT_ERROR_UNSUPPORTED error if the
 * feature isn't available.</note>
 *
 * Since: 2.0
 * Return value: A newly allocated #CoglGLES2Context or %NULL if there
 *               was an error and @error will be updated in that case.
 * Stability: unstable
 */
CoglGLES2Context *
cogl_gles2_context_new (CoglContext *ctx, CoglError **error);

/**
 * cogl_gles2_context_get_vtable:
 * @gles2_ctx: A #CoglGLES2Context allocated with
 *             cogl_gles2_context_new()
 *
 * Queries the OpenGLES 2.0 api function pointers that should be
 * used for rendering with the given @gles2_ctx.
 *
 * <note>You should not try to directly link to and use the symbols
 * provided by any system OpenGLES 2.0 driver.</note>
 *
 * Since: 2.0
 * Return value: A pointer to a #CoglGLES2Vtable providing pointers
 *               to functions for the full OpenGLES 2.0 api.
 * Stability: unstable
 */
const CoglGLES2Vtable *
cogl_gles2_context_get_vtable (CoglGLES2Context *gles2_ctx);

/**
 * cogl_push_gles2_context:
 * @ctx: A #CoglContext
 * @gles2_ctx: A #CoglGLES2Context allocated with
 *             cogl_gles2_context_new()
 * @read_buffer: A #CoglFramebuffer to access to read operations
 *               such as glReadPixels. (must be a #CoglOffscreen
 *               framebuffer currently)
 * @write_buffer: A #CoglFramebuffer to access for drawing operations
 *                such as glDrawArrays. (must be a #CoglOffscreen
 *               framebuffer currently)
 * @error: A pointer to a #CoglError for returning exceptions
 *
 * Pushes the given @gles2_ctx onto a stack associated with @ctx so
 * that the OpenGLES 2.0 api can be used instead of the Cogl
 * rendering apis to read and write to the specified framebuffers.
 *
 * Usage of the api available through a #CoglGLES2Vtable is only
 * allowed between cogl_push_gles2_context() and
 * cogl_pop_gles2_context() calls.
 *
 * If there is a runtime problem with switching over to the given
 * @gles2_ctx then this function will return %FALSE and return
 * an error through @error.
 *
 * Since: 2.0
 * Return value: %TRUE if operation was successfull or %FALSE
 *               otherwise and @error will be updated.
 * Stability: unstable
 */
CoglBool
cogl_push_gles2_context (CoglContext *ctx,
                         CoglGLES2Context *gles2_ctx,
                         CoglFramebuffer *read_buffer,
                         CoglFramebuffer *write_buffer,
                         CoglError **error);

/**
 * cogl_pop_gles2_context:
 * @ctx: A #CoglContext
 *
 * Restores the previously active #CoglGLES2Context if there
 * were nested calls to cogl_push_gles2_context() or otherwise
 * restores the ability to render with the Cogl api instead
 * of OpenGLES 2.0.
 *
 * The behaviour is undefined if calls to cogl_pop_gles2_context()
 * are not balenced with the number of corresponding calls to
 * cogl_push_gles2_context().
 *
 * Since: 2.0
 * Stability: unstable
 */
void
cogl_pop_gles2_context (CoglContext *ctx);

/**
 * cogl_gles2_get_current_vtable:
 *
 * Returns the OpenGL ES 2.0 api vtable for the currently pushed
 * #CoglGLES2Context (last pushed with cogl_push_gles2_context()) or
 * %NULL if no #CoglGLES2Context has been pushed.
 *
 * Return value: The #CoglGLES2Vtable for the currently pushed
 *               #CoglGLES2Context or %NULL if none has been pushed.
 * Since: 2.0
 * Stability: unstable
 */
CoglGLES2Vtable *
cogl_gles2_get_current_vtable (void);

/**
 * cogl_gles2_texture_2d_new_from_handle:
 * @ctx: A #CoglContext
 * @gles2_ctx: A #CoglGLES2Context allocated with
 *             cogl_gles2_context_new()
 * @handle: An OpenGL ES 2.0 texture handle created with
 *          glGenTextures()
 * @width: Width of the texture to allocate
 * @height: Height of the texture to allocate
 * @format: The format of the texture
 *
 * Creates a #CoglTexture2D from an OpenGL ES 2.0 texture handle that
 * was created within the given @gles2_ctx via glGenTextures(). The
 * texture needs to have been associated with the GL_TEXTURE_2D target.
 *
 * <note>This interface is only intended for sharing textures to read
 * from.  The behaviour is undefined if the texture is modified using
 * the Cogl api.</note>
 *
 * <note>Applications should only pass this function handles that were
 * created via a #CoglGLES2Vtable or via libcogl-gles2 and not pass
 * handles created directly using the system's native libGLESv2
 * api.</note>
 *
 * Since: 2.0
 * Stability: unstable
 */
CoglTexture2D *
cogl_gles2_texture_2d_new_from_handle (CoglContext *ctx,
                                       CoglGLES2Context *gles2_ctx,
                                       unsigned int handle,
                                       int width,
                                       int height,
                                       CoglPixelFormat format);

/**
 * cogl_gles2_texture_get_handle:
 * @texture: A #CoglTexture
 * @handle: A return location for an OpenGL ES 2.0 texture handle
 * @target: A return location for an OpenGL ES 2.0 texture target
 *
 * Gets an OpenGL ES 2.0 texture handle for a #CoglTexture that can
 * then be referenced by a #CoglGLES2Context. As well as returning
 * a texture handle the texture's target (such as GL_TEXTURE_2D) is
 * also returned.
 *
 * If the #CoglTexture can not be shared with a #CoglGLES2Context then
 * this function will return %FALSE.
 *
 * This api does not affect the lifetime of the CoglTexture and you
 * must take care not to reference the returned handle after the
 * original texture has been freed.
 *
 * <note>This interface is only intended for sharing textures to read
 * from.  The behaviour is undefined if the texture is modified by a
 * GLES2 context.</note>
 *
 * <note>This function will only return %TRUE for low-level
 * #CoglTexture<!-- -->s such as #CoglTexture2D or #CoglTexture3D but
 * not for high level meta textures such as
 * #CoglTexture2DSliced</note>
 *
 * <note>The handle returned should not be passed directly to a system
 * OpenGL ES 2.0 library, the handle is only intended to be used via
 * a #CoglGLES2Vtable or via libcogl-gles2.</note>
 *
 * Return value: %TRUE if a handle and target could be returned
 *               otherwise %FALSE is returned.
 * Since: 2.0
 * Stability: unstable
 */
CoglBool
cogl_gles2_texture_get_handle (CoglTexture *texture,
                               unsigned int *handle,
                               unsigned int *target);

/**
 * cogl_is_gles2_context:
 * @object: A #CoglObject pointer
 *
 * Gets whether the given object references a #CoglGLES2Context.
 *
 * Return value: %TRUE if the object references a #CoglGLES2Context
 *   and %FALSE otherwise.
 * Since: 2.0
 * Stability: unstable
 */
CoglBool
cogl_is_gles2_context (void *object);

COGL_END_DECLS

/* The gobject introspection scanner seems to parse public headers in
 * isolation which means we need to be extra careful about how we
 * define and undefine __COGL_H_INSIDE__ used to detect when internal
 * headers are incorrectly included by developers. In the gobject
 * introspection case we have to manually define __COGL_H_INSIDE__ as
 * a commandline argument for the scanner which means we must be
 * careful not to undefine it in a header...
 */
#ifdef __COGL_MUST_UNDEF_COGL_H_INSIDE__
#undef __COGL_H_INSIDE__
#undef __COGL_MUST_UNDEF_COGL_H_INSIDE__
#endif

#endif /* __COGL_GLES2_H__ */

