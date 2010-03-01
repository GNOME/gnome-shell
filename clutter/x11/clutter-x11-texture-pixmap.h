/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Johan Bilien  <johan.bilien@nokia.com>
 *
 * Copyright (C) 2007 OpenedHand
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

#ifndef __CLUTTER_X11_TEXTURE_PIXMAP_H__
#define __CLUTTER_X11_TEXTURE_PIXMAP_H__

#include <glib.h>
#include <glib-object.h>
#include <clutter/clutter.h>
#include <X11/Xlib.h>

G_BEGIN_DECLS

#define CLUTTER_X11_TYPE_TEXTURE_PIXMAP            (clutter_x11_texture_pixmap_get_type ())
#define CLUTTER_X11_TEXTURE_PIXMAP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_X11_TYPE_TEXTURE_PIXMAP, ClutterX11TexturePixmap))
#define CLUTTER_X11_TEXTURE_PIXMAP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_X11_TYPE_TEXTURE_PIXMAP, ClutterX11TexturePixmapClass))
#define CLUTTER_X11_IS_TEXTURE_PIXMAP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_X11_TYPE_TEXTURE_PIXMAP))
#define CLUTTER_X11_IS_TEXTURE_PIXMAP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_X11_TYPE_TEXTURE_PIXMAP))
#define CLUTTER_X11_TEXTURE_PIXMAP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_X11_TYPE_TEXTURE_PIXMAP, ClutterX11TexturePixmapClass))

typedef struct _ClutterX11TexturePixmap        ClutterX11TexturePixmap;
typedef struct _ClutterX11TexturePixmapClass   ClutterX11TexturePixmapClass;
typedef struct _ClutterX11TexturePixmapPrivate ClutterX11TexturePixmapPrivate;

struct _ClutterX11TexturePixmapClass
{
  ClutterTextureClass    parent_class;

  void                  (*update_area)    (ClutterX11TexturePixmap *texture,
                                           gint                     x,
                                           gint                     y,
                                           gint                     width,
                                           gint                     height);
};

struct _ClutterX11TexturePixmap
{
  ClutterTexture                  parent;

  ClutterX11TexturePixmapPrivate *priv;
};

GType clutter_x11_texture_pixmap_get_type (void);
ClutterActor * clutter_x11_texture_pixmap_new (void);

ClutterActor * clutter_x11_texture_pixmap_new_with_pixmap (Pixmap      pixmap);

ClutterActor * clutter_x11_texture_pixmap_new_with_window (Window      window);

void  clutter_x11_texture_pixmap_set_pixmap (ClutterX11TexturePixmap  *texture,
                                             Pixmap                    pixmap);

void  clutter_x11_texture_pixmap_set_window (ClutterX11TexturePixmap *texture,
                                             Window                   window,
                                             gboolean                 automatic);
void  clutter_x11_texture_pixmap_sync_window (ClutterX11TexturePixmap *texture);

void  clutter_x11_texture_pixmap_update_area (ClutterX11TexturePixmap *texture,
                                              gint                     x,
                                              gint                     y,
                                              gint                     width,
                                              gint                     height);

void  clutter_x11_texture_pixmap_set_automatic (ClutterX11TexturePixmap *texture,
                                                gboolean                 setting);

G_END_DECLS

#endif
