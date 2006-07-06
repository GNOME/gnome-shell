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

#ifndef _HAVE_CLUTTER_VIDEO_TEXTURE_H
#define _HAVE_CLUTTER_VIDEO_TEXTURE_H

#include <glib-object.h>
#include <clutter/clutter-actor.h>
#include <clutter/clutter-texture.h>
#include <clutter/clutter-media.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_VIDEO_TEXTURE clutter_video_texture_get_type()

#define CLUTTER_VIDEO_TEXTURE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_VIDEO_TEXTURE, ClutterVideoTexture))

#define CLUTTER_VIDEO_TEXTURE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_VIDEO_TEXTURE, ClutterVideoTextureClass))

#define CLUTTER_IS_VIDEO_TEXTURE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_VIDEO_TEXTURE))

#define CLUTTER_IS_VIDEO_TEXTURE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_VIDEO_TEXTURE))

#define CLUTTER_VIDEO_TEXTURE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_VIDEO_TEXTURE, ClutterVideoTextureClass))

typedef struct _ClutterVideoTexture        ClutterVideoTexture;
typedef struct _ClutterVideoTextureClass   ClutterVideoTextureClass;
typedef struct _ClutterVideoTexturePrivate ClutterVideoTexturePrivate;

/* #define CLUTTER_VIDEO_TEXTURE_ERROR clutter_video_texture_error_quark() */

struct _ClutterVideoTexture
{
  ClutterTexture              parent;
  ClutterVideoTexturePrivate *priv;
}; 

struct _ClutterVideoTextureClass 
{
  ClutterTextureClass parent_class;

  /* Signals */
  void (* tag_list_available) (ClutterVideoTexture *video_texture,
			       GstTagList          *tag_list);
  void (* eos)                (ClutterVideoTexture *video_texture);
  void (* error)              (ClutterVideoTexture *video_texture,
			       GError              *error);

  /* Future padding */
  void (* _clutter_reserved1) (void);
  void (* _clutter_reserved2) (void);
  void (* _clutter_reserved3) (void);
  void (* _clutter_reserved4) (void);
  void (* _clutter_reserved5) (void);
  void (* _clutter_reserved6) (void);
}; 

GType         clutter_video_texture_get_type (void) G_GNUC_CONST;
ClutterActor *clutter_video_texture_new      (void);

G_END_DECLS

#endif
