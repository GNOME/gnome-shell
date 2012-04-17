/*
 * Clutter
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By: Emmanuele Bassi <ebassi@linux.intel.com>
 *              Matthew Allum <mallum@o-hand.com>
 *              Chris Lord <chris@o-hand.com>
 *              Iain Holmes <iain@o-hand.com>
 *              Neil Roberts <neil@linux.intel.com>
 *
 * Copyright (C) 2008, 2009, 2010  Intel Corporation.
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
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_CAIRO_TEXTURE_H__
#define __CLUTTER_CAIRO_TEXTURE_H__

#include <clutter/clutter-texture.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_CAIRO_TEXTURE              (clutter_cairo_texture_get_type ())
#define CLUTTER_CAIRO_TEXTURE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_CAIRO_TEXTURE, ClutterCairoTexture))
#define CLUTTER_CAIRO_TEXTURE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_CAIRO_TEXTURE, ClutterCairoTextureClass))
#define CLUTTER_IS_CAIRO_TEXTURE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_CAIRO_TEXTURE))
#define CLUTTER_IS_CAIRO_TEXTURE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_CAIRO_TEXTURE))
#define CLUTTER_CAIRO_TEXTURE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_CAIRO_TEXTURE, ClutterCairoTextureClass))

typedef struct _ClutterCairoTexture             ClutterCairoTexture;
typedef struct _ClutterCairoTextureClass        ClutterCairoTextureClass;
typedef struct _ClutterCairoTexturePrivate      ClutterCairoTexturePrivate;

/**
 * ClutterCairoTexture:
 *
 * The #ClutterCairoTexture struct contains only private data.
 *
 * Since: 1.0
 *
 * Deprecated: 1.12: Use #ClutterCanvas instead
 */
struct _ClutterCairoTexture
{
  /*< private >*/
  ClutterTexture parent_instance;

  ClutterCairoTexturePrivate *priv;
};

/**
 * ClutterCairoTextureClass:
 * @create_surface: class handler for the #ClutterCairoTexture::create-surface
 *   signal
 * @draw: class handler for the #ClutterCairoTexture::draw signal
 *
 * The #ClutterCairoTextureClass struct contains only private data.
 *
 * Since: 1.0
 *
 * Deprecated: 1.12: Use #ClutterCanvas instead
 */
struct _ClutterCairoTextureClass
{
  /*< private >*/
  ClutterTextureClass parent_class;

  /*< public >*/
  cairo_surface_t *(* create_surface) (ClutterCairoTexture *texture,
                                       guint                width,
                                       guint                height);

  gboolean         (* draw)           (ClutterCairoTexture *texture,
                                       cairo_t             *cr);

  /*< private >*/
  void (*_clutter_cairo_3) (void);
  void (*_clutter_cairo_4) (void);
};

CLUTTER_DEPRECATED_IN_1_12_FOR(clutter_canvas_get_type)
GType clutter_cairo_texture_get_type (void) G_GNUC_CONST;

CLUTTER_DEPRECATED_IN_1_12_FOR(clutter_canvas_new)
ClutterActor *  clutter_cairo_texture_new                       (guint                  width,
                                                                 guint                  height);

CLUTTER_DEPRECATED_IN_1_12_FOR(clutter_canvas_set_size)
void            clutter_cairo_texture_set_surface_size          (ClutterCairoTexture   *self,
                                                                 guint                  width,
                                                                 guint                  height);
CLUTTER_DEPRECATED_IN_1_12_FOR(clutter_canvas_get_size)
void            clutter_cairo_texture_get_surface_size          (ClutterCairoTexture   *self,
                                                                 guint                 *width,
                                                                 guint                 *height);
CLUTTER_DEPRECATED_IN_1_12
void            clutter_cairo_texture_set_auto_resize           (ClutterCairoTexture   *self,
                                                                 gboolean               value);
CLUTTER_DEPRECATED_IN_1_12
gboolean        clutter_cairo_texture_get_auto_resize           (ClutterCairoTexture   *self);

CLUTTER_DEPRECATED_IN_1_12
void            clutter_cairo_texture_clear                     (ClutterCairoTexture   *self);

CLUTTER_DEPRECATED_IN_1_12
void            clutter_cairo_texture_invalidate_rectangle      (ClutterCairoTexture   *self,
                                                                 cairo_rectangle_int_t *rect);
CLUTTER_DEPRECATED_IN_1_12
void            clutter_cairo_texture_invalidate                (ClutterCairoTexture   *self);

CLUTTER_DEPRECATED_IN_1_8_FOR(clutter_cairo_texture_invalidate_rectangle)
cairo_t *       clutter_cairo_texture_create_region             (ClutterCairoTexture   *self,
                                                                 gint                   x_offset,
                                                                 gint                   y_offset,
                                                                 gint                   width,
                                                                 gint                   height);

CLUTTER_DEPRECATED_IN_1_8_FOR(clutter_cairo_texture_invalidate)
cairo_t *       clutter_cairo_texture_create                    (ClutterCairoTexture   *self);

G_END_DECLS

#endif /* __CLUTTER_CAIRO_TEXTURE_DEPRECATED_H__ */
