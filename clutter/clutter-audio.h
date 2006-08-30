/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Jorn Baayen  <jorn@openedhand.com>
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

#ifndef _HAVE_CLUTTER_AUDIO_H
#define _HAVE_CLUTTER_AUDIO_H

#include <glib-object.h>
#include <clutter/clutter-media.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_AUDIO clutter_audio_get_type()

#define CLUTTER_AUDIO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_AUDIO, ClutterAudio))

#define CLUTTER_AUDIO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_AUDIO, ClutterAudioClass))

#define CLUTTER_IS_AUDIO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_AUDIO))

#define CLUTTER_IS_AUDIO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_AUDIO))

#define CLUTTER_AUDIO_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_AUDIO, ClutterAudioClass))

typedef struct _ClutterAudio        ClutterAudio;
typedef struct _ClutterAudioClass   ClutterAudioClass;
typedef struct _ClutterAudioPrivate ClutterAudioPrivate;

/* #define CLUTTER_AUDIO_ERROR clutter_audio_error_quark() */

struct _ClutterAudio
{
  GObject              parent;
  ClutterAudioPrivate *priv;
}; 

struct _ClutterAudioClass 
{
  GObjectClass parent_class;

  /* Future padding */
  void (* _clutter_reserved1) (void);
  void (* _clutter_reserved2) (void);
  void (* _clutter_reserved3) (void);
  void (* _clutter_reserved4) (void);
  void (* _clutter_reserved5) (void);
  void (* _clutter_reserved6) (void);
}; 

GType         clutter_audio_get_type (void) G_GNUC_CONST;
ClutterAudio *clutter_audio_new      (void);

G_END_DECLS

#endif
