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

#ifndef __CLUTTER_GLX_TEXTURE_PIXMAP_H__
#define __CLUTTER_GLX_TEXTURE_PIXMAP_H__

#include <glib.h>
#include <glib-object.h>
#include <clutter/x11/clutter-x11-texture-pixmap.h>

#include <GL/glx.h>

G_BEGIN_DECLS

#define CLUTTER_GLX_TYPE_TEXTURE_PIXMAP            (clutter_glx_texture_pixmap_get_type ())
#define CLUTTER_GLX_TEXTURE_PIXMAP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_GLX_TYPE_TEXTURE_PIXMAP, ClutterGLXTexturePixmap))
#define CLUTTER_GLX_TEXTURE_PIXMAP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_GLX_TYPE_TEXTURE_PIXMAP, ClutterGLXTexturePixmapClass))
#define CLUTTER_GLX_IS_TEXTURE_PIXMAP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_GLX_TYPE_TEXTURE_PIXMAP))
#define CLUTTER_GLX_IS_TEXTURE_PIXMAP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_GLX_TYPE_TEXTURE_PIXMAP))
#define CLUTTER_GLX_TEXTURE_PIXMAP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_GLX_TYPE_TEXTURE_PIXMAP, ClutterGLXTexturePixmapClass))

typedef struct _ClutterGLXTexturePixmap        ClutterGLXTexturePixmap;
typedef struct _ClutterGLXTexturePixmapClass   ClutterGLXTexturePixmapClass;
typedef struct _ClutterGLXTexturePixmapPrivate ClutterGLXTexturePixmapPrivate;

struct _ClutterGLXTexturePixmapClass
{
  ClutterX11TexturePixmapClass   parent_class;
};

struct _ClutterGLXTexturePixmap
{
  ClutterX11TexturePixmap         parent;

  ClutterGLXTexturePixmapPrivate *priv;
};

GType clutter_glx_texture_pixmap_get_type (void);

ClutterActor * clutter_glx_texture_pixmap_new (void);

ClutterActor * clutter_glx_texture_pixmap_new_with_pixmap (Pixmap pixmap);

ClutterActor * clutter_glx_texture_pixmap_new_with_window (Window window);

gboolean       clutter_glx_texture_pixmap_using_extension (ClutterGLXTexturePixmap *texture);

G_END_DECLS

#endif
