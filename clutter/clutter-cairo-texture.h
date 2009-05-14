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
 * Copyright (C) 2008  Intel Corporation.
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
#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_CAIRO_TEXTURE_H__
#define __CLUTTER_CAIRO_TEXTURE_H__

#include <clutter/clutter-texture.h>
#include <cairo.h>

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
 */
struct _ClutterCairoTexture
{
  /*< private >*/
  ClutterTexture parent_instance;

  ClutterCairoTexturePrivate *priv;
};

/**
 * ClutterCairoTextureClass:
 *
 * The #ClutterCairoTextureClass struct contains only private data.
 *
 * Since: 1.0
 */
struct _ClutterCairoTextureClass
{
  /*< private >*/
  ClutterTextureClass parent_class;

  void (*_clutter_cairo_1) (void);
  void (*_clutter_cairo_2) (void);
  void (*_clutter_cairo_3) (void);
  void (*_clutter_cairo_4) (void);
};

GType         clutter_cairo_texture_get_type         (void) G_GNUC_CONST;
ClutterActor *clutter_cairo_texture_new              (guint                width,
                                                      guint                height);
cairo_t *     clutter_cairo_texture_create_region    (ClutterCairoTexture *self,
                                                      gint                 x_offset,
                                                      gint                 y_offset,
                                                      gint                 width,
                                                      gint                 height);
cairo_t *     clutter_cairo_texture_create           (ClutterCairoTexture *self);
void          clutter_cairo_texture_set_surface_size (ClutterCairoTexture *self,
                                                      guint                width,
                                                      guint                height);
void          clutter_cairo_texture_get_surface_size (ClutterCairoTexture *self,
                                                      guint               *width,
                                                      guint               *height);

void          clutter_cairo_texture_clear            (ClutterCairoTexture *self);

void          clutter_cairo_set_source_color         (cairo_t             *cr,
						      const ClutterColor  *color);

G_END_DECLS

#endif /* __CLUTTER_CAIRO_TEXTURE_H__ */
