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

#include <clutter/clutter-actor.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_TEXTURE            (clutter_texture_get_type ())
#define CLUTTER_TEXTURE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_TEXTURE, ClutterTexture))
#define CLUTTER_TEXTURE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_TEXTURE, ClutterTextureClass))
#define CLUTTER_IS_TEXTURE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_TEXTURE))
#define CLUTTER_IS_TEXTURE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_TEXTURE))
#define CLUTTER_TEXTURE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_TEXTURE, ClutterTextureClass))

typedef struct _ClutterTexture        ClutterTexture;
typedef struct _ClutterTextureClass   ClutterTextureClass;
typedef struct _ClutterTexturePrivate ClutterTexturePrivate;

struct _ClutterTexture
{
  ClutterActor         parent;

  ClutterTexturePrivate *priv;
};

struct _ClutterTextureClass
{
  ClutterActorClass parent_class;

  void (*size_change)   (ClutterTexture *texture, 
		         gint            width, 
		         gint            height);
  void (*pixbuf_change) (ClutterTexture *texture);

  /* padding, for future expansion */
  void (*_clutter_texture1) (void);
  void (*_clutter_texture2) (void);
  void (*_clutter_texture3) (void);
  void (*_clutter_texture4) (void);
  void (*_clutter_texture5) (void);
  void (*_clutter_texture6) (void);
};

typedef enum ClutterTextureFlags
{
    CLUTTER_TEXTURE_RGB_FLAG_BGR     = (1<<1),
    CLUTTER_TEXTURE_RGB_FLAG_PREMULT = (1<<2),
    CLUTTER_TEXTURE_YUV_FLAG_YUV2    = (1<<3)
    /* FIXME: add compressed types ? */
} ClutterTextureFlags;

GType clutter_texture_get_type (void) G_GNUC_CONST;

ClutterActor *clutter_texture_new             (void);
ClutterActor *clutter_texture_new_from_pixbuf (GdkPixbuf      *pixbuf);
gboolean      clutter_texture_set_from_rgb_data   (ClutterTexture *texture,
						   const guchar   *data,
						   gboolean        has_alpha,
						   gint            width,
						   gint            height,
						   gint            rowstride,
						   gint            bpp,
						   ClutterTextureFlags  flags,
						   GError        **error);
gboolean      clutter_texture_set_from_yuv_data   (ClutterTexture *texture,
						   const guchar   *data,
						   gint            width,
						   gint            height,
						   ClutterTextureFlags  flags,
						   GError        **error);
gboolean      clutter_texture_set_pixbuf          (ClutterTexture *texture,
						   GdkPixbuf      *pixbuf,
						   GError        **error);
GdkPixbuf *   clutter_texture_get_pixbuf      (ClutterTexture *texture);
void          clutter_texture_get_base_size   (ClutterTexture *texture,
                                               gint           *width,
                                               gint           *height);

/* Below mainly for subclassed texture based actors */

void clutter_texture_bind_tile               (ClutterTexture *texture,
                                              gint            index);
void clutter_texture_get_n_tiles             (ClutterTexture *texture,
                                              gint           *n_x_tiles,
                                              gint           *n_y_tiles);
void clutter_texture_get_x_tile_detail       (ClutterTexture *texture, 
				              gint            x_index,
				              gint           *pos,
				              gint           *size,
				              gint           *waste);
void clutter_texture_get_y_tile_detail       (ClutterTexture *texture, 
				              gint            y_index,
				              gint           *pos,
				              gint           *size,
				              gint           *waste);
gboolean clutter_texture_has_generated_tiles (ClutterTexture *texture);
gboolean clutter_texture_is_tiled            (ClutterTexture *texture);

G_END_DECLS

#endif /* _HAVE_CLUTTER_TEXTURE_H */
