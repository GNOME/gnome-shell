/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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

#ifndef _HAVE_CLUTTER_TEXTURE_H
#define _HAVE_CLUTTER_TEXTURE_H

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <clutter/clutter-element.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_TEXTURE clutter_texture_get_type()

#define CLUTTER_TEXTURE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_TEXTURE, ClutterTexture))

#define CLUTTER_TEXTURE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_TEXTURE, ClutterTextureClass))

#define CLUTTER_IS_TEXTURE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_TEXTURE))

#define CLUTTER_IS_TEXTURE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_TEXTURE))

#define CLUTTER_TEXTURE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_TEXTURE, ClutterTextureClass))

typedef struct ClutterTexturePrivate ClutterTexturePrivate ;
typedef struct _ClutterTexture      ClutterTexture;
typedef struct _ClutterTextureClass ClutterTextureClass;

struct _ClutterTexture
{
  ClutterElement         parent;

  ClutterTexturePrivate *priv;
}; 

struct _ClutterTextureClass 
{
  ClutterElementClass parent_class;

  void (*size_change) (ClutterTexture *texture, gint width, gint height);
  void (*pixbuf_change) (ClutterTexture *texture );
}; 

GType clutter_texture_get_type (void);

ClutterElement*
clutter_texture_new_from_pixbuf (GdkPixbuf *pixbuf);

ClutterElement*
clutter_texture_new (void);

void
clutter_texture_set_pixbuf (ClutterTexture *texture, GdkPixbuf *pixbuf);

GdkPixbuf*
clutter_texture_get_pixbuf (ClutterTexture* texture);

void
clutter_texture_get_base_size (ClutterTexture *texture, 
			       gint           *width,
			       gint           *height);

/* Below mainly for subclassed texture based elements */

void
clutter_texture_bind_tile (ClutterTexture *texture, gint index);

void
clutter_texture_get_n_tiles (ClutterTexture *texture, 
			     gint           *n_x_tiles,
			     gint           *n_y_tiles);

void
clutter_texture_get_x_tile_detail (ClutterTexture *texture, 
				   gint            x_index,
				   gint           *pos,
				   gint           *size,
				   gint           *waste);

void
clutter_texture_get_y_tile_detail (ClutterTexture *texture, 
				   gint            y_index,
				   gint           *pos,
				   gint           *size,
				   gint           *waste);

gboolean
clutter_texture_has_generated_tiles (ClutterTexture *texture);

gboolean
clutter_texture_is_tiled (ClutterTexture *texture);

G_END_DECLS

#endif
