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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifndef __COGL_TEXTURE_PIXMAP_X11_H
#define __COGL_TEXTURE_PIXMAP_X11_H

/* NB: this is a top-level header that can be included directly but we
 * want to be careful not to define __COGL_H_INSIDE__ when this is
 * included internally while building Cogl itself since
 * __COGL_H_INSIDE__ is used in headers to guard public vs private api
 * definitions
 */
#ifndef COGL_COMPILATION

/* Note: When building Cogl .gir we explicitly define
 * __COGL_H_INSIDE__ */
#ifndef __COGL_H_INSIDE__
#define __COGL_H_INSIDE__
#define __COGL_MUST_UNDEF_COGL_H_INSIDE__
#endif

#endif /* COGL_COMPILATION */

#include <cogl/cogl-context.h>

COGL_BEGIN_DECLS

#ifdef COGL_ENABLE_EXPERIMENTAL_API

/**
 * SECTION:cogl-texture-pixmap-x11
 * @short_description: Functions for creating and manipulating 2D meta
 *                     textures derived from X11 pixmaps.
 *
 * These functions allow high-level meta textures (See the
 * #CoglMetaTexture interface) that derive their contents from an X11
 * pixmap.
 */

typedef struct _CoglTexturePixmapX11 CoglTexturePixmapX11;

#define COGL_TEXTURE_PIXMAP_X11(X) ((CoglTexturePixmapX11 *)X)

typedef enum
{
  COGL_TEXTURE_PIXMAP_X11_DAMAGE_RAW_RECTANGLES,
  COGL_TEXTURE_PIXMAP_X11_DAMAGE_DELTA_RECTANGLES,
  COGL_TEXTURE_PIXMAP_X11_DAMAGE_BOUNDING_BOX,
  COGL_TEXTURE_PIXMAP_X11_DAMAGE_NON_EMPTY
} CoglTexturePixmapX11ReportLevel;

/**
 * COGL_TEXTURE_PIXMAP_X11_ERROR:
 *
 * #CoglError domain for texture-pixmap-x11 errors.
 *
 * Since: 1.10
 */
#define COGL_TEXTURE_PIXMAP_X11_ERROR (cogl_texture_pixmap_x11_error_quark ())

/**
 * CoglTexturePixmapX11Error:
 * @COGL_TEXTURE_PIXMAP_X11_ERROR_X11: An X11 protocol error
 *
 * Error codes that can be thrown when performing texture-pixmap-x11
 * operations.
 *
 * Since: 1.10
 */
typedef enum {
  COGL_TEXTURE_PIXMAP_X11_ERROR_X11,
} CoglTexturePixmapX11Error;

uint32_t cogl_texture_pixmap_x11_error_quark (void);

/**
 * cogl_texture_pixmap_x11_new:
 * @context: A #CoglContext
 * @pixmap: A X11 pixmap ID
 * @automatic_updates: Whether to automatically copy the contents of
 * the pixmap to the texture.
 * @error: A #CoglError for exceptions
 *
 * Creates a texture that contains the contents of @pixmap. If
 * @automatic_updates is %TRUE then Cogl will attempt to listen for
 * damage events on the pixmap and automatically update the texture
 * when it changes.
 *
 * Return value: a new #CoglTexturePixmapX11 instance
 *
 * Since: 1.10
 * Stability: Unstable
 */
CoglTexturePixmapX11 *
cogl_texture_pixmap_x11_new (CoglContext *context,
                             uint32_t pixmap,
                             CoglBool automatic_updates,
                             CoglError **error);

/**
 * cogl_texture_pixmap_x11_update_area:
 * @texture: A #CoglTexturePixmapX11 instance
 * @x: x coordinate of the area to update
 * @y: y coordinate of the area to update
 * @width: width of the area to update
 * @height: height of the area to update
 *
 * Forces an update of the given @texture so that it is refreshed with
 * the contents of the pixmap that was given to
 * cogl_texture_pixmap_x11_new().
 *
 * Since: 1.4
 * Stability: Unstable
 */
void
cogl_texture_pixmap_x11_update_area (CoglTexturePixmapX11 *texture,
                                     int x,
                                     int y,
                                     int width,
                                     int height);

/**
 * cogl_texture_pixmap_x11_is_using_tfp_extension:
 * @texture: A #CoglTexturePixmapX11 instance
 *
 * Checks whether the given @texture is using the
 * GLX_EXT_texture_from_pixmap or similar extension to copy the
 * contents of the pixmap to the texture.  This extension is usually
 * implemented as zero-copy operation so it implies the updates are
 * working efficiently.
 *
 * Return value: %TRUE if the texture is using an efficient extension
 *   and %FALSE otherwise
 *
 * Since: 1.4
 * Stability: Unstable
 */
CoglBool
cogl_texture_pixmap_x11_is_using_tfp_extension (CoglTexturePixmapX11 *texture);

/**
 * cogl_texture_pixmap_x11_set_damage_object:
 * @texture: A #CoglTexturePixmapX11 instance
 * @damage: A X11 Damage object or 0
 * @report_level: The report level which describes how to interpret
 *   the damage events. This should match the level that the damage
 *   object was created with.
 *
 * Sets the damage object that will be used to track automatic updates
 * to the @texture. Damage tracking can be disabled by passing 0 for
 * @damage. Otherwise this damage will replace the one used if %TRUE
 * was passed for automatic_updates to cogl_texture_pixmap_x11_new().
 *
 * Note that Cogl will subtract from the damage region as it processes
 * damage events.
 *
 * Since: 1.4
 * Stability: Unstable
 */
void
cogl_texture_pixmap_x11_set_damage_object (CoglTexturePixmapX11 *texture,
                                           uint32_t damage,
                                           CoglTexturePixmapX11ReportLevel
                                                                  report_level);

/**
 * cogl_is_texture_pixmap_x11:
 * @object: A pointer to a #CoglObject
 *
 * Checks whether @object points to a #CoglTexturePixmapX11 instance.
 *
 * Return value: %TRUE if the object is a #CoglTexturePixmapX11, and
 *   %FALSE otherwise
 *
 * Since: 1.4
 * Stability: Unstable
 */
CoglBool
cogl_is_texture_pixmap_x11 (void *object);

#endif /* COGL_ENABLE_EXPERIMENTAL_API */

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

#endif /* __COGL_TEXTURE_PIXMAP_X11_H */
