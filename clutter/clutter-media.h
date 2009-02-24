/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By: Matthew Allum  <mallum@openedhand.com>
 *              Emmanuele Bassi <ebassi@linux.intel.com>
 *
 * Copyright (C) 2006 OpenedHand
 * Copyright (C) 2009 Intel Corp.
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

#ifndef __CLUTTER_MEDIA_H__
#define __CLUTTER_MEDIA_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_MEDIA                      (clutter_media_get_type ())
#define CLUTTER_MEDIA(obj)                      (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_MEDIA, ClutterMedia))
#define CLUTTER_IS_MEDIA(obj)                   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_MEDIA))
#define CLUTTER_MEDIA_GET_INTERFACE(obj)        (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CLUTTER_TYPE_MEDIA, ClutterMediaIface))

typedef struct _ClutterMedia            ClutterMedia; /* dummy typedef */
typedef struct _ClutterMediaIface       ClutterMediaIface;

struct _ClutterMediaIface
{
  /*< private >*/
  GTypeInterface base_iface;

  /* signals */
  void (* eos)   (ClutterMedia *media);
  void (* error) (ClutterMedia *media,
		  const GError *error);
};

GType clutter_media_get_type (void) G_GNUC_CONST;

void     clutter_media_set_uri          (ClutterMedia *media,
                                         const gchar  *uri);
gchar *  clutter_media_get_uri          (ClutterMedia *media);
void     clutter_media_set_filename     (ClutterMedia *media,
                                         const gchar  *filename);

void     clutter_media_set_playing      (ClutterMedia *media,
                                         gboolean      playing);
gboolean clutter_media_get_playing      (ClutterMedia *media);
void     clutter_media_set_progress     (ClutterMedia *media,
                                         gdouble       progress);
gdouble  clutter_media_get_progress     (ClutterMedia *media);
void     clutter_media_set_audio_volume (ClutterMedia *media,
                                         gdouble       volume);
gdouble  clutter_media_get_audio_volume (ClutterMedia *media);
gboolean clutter_media_get_can_seek     (ClutterMedia *media);
gdouble  clutter_media_get_buffer_fill  (ClutterMedia *media);
guint    clutter_media_get_duration     (ClutterMedia *media);

G_END_DECLS

#endif /* __CLUTTER_MEDIA_H__ */
