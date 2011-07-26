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
#include <cairo.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_CAIRO_TEXTURE              (clutter_cairo_texture_get_type ())
#define CLUTTER_CAIRO_TEXTURE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_CAIRO_TEXTURE, ClutterCairoTexture))
#define CLUTTER_CAIRO_TEXTURE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_CAIRO_TEXTURE, ClutterCairoTextureClass))
#define CLUTTER_IS_CAIRO_TEXTURE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_CAIRO_TEXTURE))
#define CLUTTER_IS_CAIRO_TEXTURE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_CAIRO_TEXTURE))
#define CLUTTER_CAIRO_TEXTURE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_CAIRO_TEXTURE, ClutterCairoTextureClass))

/**
 * CLUTTER_CAIRO_FORMAT_ARGB32:
 *
 * The #CoglPixelFormat to be used when uploading image data from
 * and to a Cairo image surface using %CAIRO_FORMAT_ARGB32 and
 * %CAIRO_FORMAT_RGB24 as #cairo_format_t.
 *
 * Since: 1.8
 */

/* Cairo stores the data in native byte order as ARGB but Cogl's pixel
 * formats specify the actual byte order. Therefore we need to use a
 * different format depending on the architecture
 */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define CLUTTER_CAIRO_FORMAT_ARGB32     (COGL_PIXEL_FORMAT_BGRA_8888_PRE)
#else
#define CLUTTER_CAIRO_FORMAT_ARGB32     (COGL_PIXEL_FORMAT_ARGB_8888_PRE)
#endif

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
 * @create_surface: class handler for the #ClutterCairoTexture::create-surface
 *   signal
 * @draw: class handler for the #ClutterCairoTexture::draw signal
 *
 * The #ClutterCairoTextureClass struct contains only private data.
 *
 * Since: 1.0
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

GType clutter_cairo_texture_get_type (void) G_GNUC_CONST;

ClutterActor *  clutter_cairo_texture_new                       (guint                  width,
                                                                 guint                  height);

#ifndef CLUTTER_DISABLE_DEPRECATED
cairo_t *       clutter_cairo_texture_create_region             (ClutterCairoTexture   *self,
                                                                 gint                   x_offset,
                                                                 gint                   y_offset,
                                                                 gint                   width,
                                                                 gint                   height);
cairo_t *       clutter_cairo_texture_create                    (ClutterCairoTexture   *self);
#endif /* CLUTTER_DISABLE_DEPRECATED */

void            clutter_cairo_texture_set_surface_size          (ClutterCairoTexture   *self,
                                                                 guint                  width,
                                                                 guint                  height);
void            clutter_cairo_texture_get_surface_size          (ClutterCairoTexture   *self,
                                                                 guint                 *width,
                                                                 guint                 *height);
void            clutter_cairo_texture_set_auto_resize           (ClutterCairoTexture   *self,
                                                                 gboolean               value);
gboolean        clutter_cairo_texture_get_auto_resize           (ClutterCairoTexture   *self);

void            clutter_cairo_texture_clear                     (ClutterCairoTexture   *self);

void            clutter_cairo_texture_invalidate_rectangle      (ClutterCairoTexture   *self,
                                                                 cairo_rectangle_int_t *rect);
void            clutter_cairo_texture_invalidate                (ClutterCairoTexture   *self);

void            clutter_cairo_set_source_color                  (cairo_t               *cr,
						                 const ClutterColor    *color);

G_END_DECLS

#endif /* __CLUTTER_CAIRO_TEXTURE_H__ */
