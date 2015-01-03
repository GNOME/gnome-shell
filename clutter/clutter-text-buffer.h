/* clutter-text-buffer.h
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_TEXT_BUFFER_H__
#define __CLUTTER_TEXT_BUFFER_H__

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_TEXT_BUFFER            (clutter_text_buffer_get_type ())
#define CLUTTER_TEXT_BUFFER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_TEXT_BUFFER, ClutterTextBuffer))
#define CLUTTER_TEXT_BUFFER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_TEXT_BUFFER, ClutterTextBufferClass))
#define CLUTTER_IS_TEXT_BUFFER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_TEXT_BUFFER))
#define CLUTTER_IS_TEXT_BUFFER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_TEXT_BUFFER))
#define CLUTTER_TEXT_BUFFER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_TEXT_BUFFER, ClutterTextBufferClass))

/**
 * CLUTTER_TEXT_BUFFER_MAX_SIZE:
 *
 * Maximum size of text buffer, in bytes.
 *
 * Since: 1.10
 */
#define CLUTTER_TEXT_BUFFER_MAX_SIZE        G_MAXUSHORT

typedef struct _ClutterTextBuffer            ClutterTextBuffer;
typedef struct _ClutterTextBufferClass       ClutterTextBufferClass;
typedef struct _ClutterTextBufferPrivate     ClutterTextBufferPrivate;

/**
 * ClutterTextBuffer:
 *
 * The #ClutterTextBuffer structure contains private
 * data and it should only be accessed using the provided API.
 *
 * Since: 1.10
 */
struct _ClutterTextBuffer
{
  /*< private >*/
  GObject parent_instance;

  ClutterTextBufferPrivate *priv;
};

/**
 * ClutterTextBufferClass:
 * @inserted_text: default handler for the #ClutterTextBuffer::inserted-text signal
 * @deleted_text: default hanlder for the #ClutterTextBuffer::deleted-text signal
 * @get_text: virtual function
 * @get_length: virtual function
 * @insert_text: virtual function
 * @delete_text: virtual function
 *
 * The #ClutterTextBufferClass structure contains
 * only private data.
 *
 * Since: 1.10
 */
struct _ClutterTextBufferClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
  /* Signals */
  void         (*inserted_text)          (ClutterTextBuffer *buffer,
                                          guint              position,
                                          const gchar       *chars,
                                          guint              n_chars);

  void         (*deleted_text)           (ClutterTextBuffer *buffer,
                                          guint              position,
                                          guint              n_chars);

  /* Virtual Methods */
  const gchar* (*get_text)               (ClutterTextBuffer *buffer,
                                          gsize             *n_bytes);

  guint        (*get_length)             (ClutterTextBuffer *buffer);

  guint        (*insert_text)            (ClutterTextBuffer *buffer,
                                          guint              position,
                                          const gchar       *chars,
                                          guint              n_chars);

  guint        (*delete_text)            (ClutterTextBuffer *buffer,
                                          guint              position,
                                          guint              n_chars);

  /*< private >*/
  /* Padding for future expansion */
  void (*_clutter_reserved1) (void);
  void (*_clutter_reserved2) (void);
  void (*_clutter_reserved3) (void);
  void (*_clutter_reserved4) (void);
  void (*_clutter_reserved5) (void);
  void (*_clutter_reserved6) (void);
  void (*_clutter_reserved7) (void);
  void (*_clutter_reserved8) (void);
};

CLUTTER_AVAILABLE_IN_1_10
GType               clutter_text_buffer_get_type            (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_1_10
ClutterTextBuffer*  clutter_text_buffer_new                 (void);
CLUTTER_AVAILABLE_IN_1_10
ClutterTextBuffer*  clutter_text_buffer_new_with_text       (const gchar       *text,
                                                             gssize             text_len);

CLUTTER_AVAILABLE_IN_1_10
gsize               clutter_text_buffer_get_bytes           (ClutterTextBuffer *buffer);
CLUTTER_AVAILABLE_IN_1_10
guint               clutter_text_buffer_get_length          (ClutterTextBuffer *buffer);
CLUTTER_AVAILABLE_IN_1_10
const gchar*        clutter_text_buffer_get_text            (ClutterTextBuffer *buffer);
CLUTTER_AVAILABLE_IN_1_10
void                clutter_text_buffer_set_text            (ClutterTextBuffer *buffer,
                                                             const gchar       *chars,
                                                             gint               n_chars);
CLUTTER_AVAILABLE_IN_1_10
void                clutter_text_buffer_set_max_length      (ClutterTextBuffer *buffer,
                                                             gint               max_length);
CLUTTER_AVAILABLE_IN_1_10
gint                clutter_text_buffer_get_max_length      (ClutterTextBuffer  *buffer);

CLUTTER_AVAILABLE_IN_1_10
guint               clutter_text_buffer_insert_text         (ClutterTextBuffer *buffer,
                                                             guint              position,
                                                             const gchar       *chars,
                                                             gint               n_chars);
CLUTTER_AVAILABLE_IN_1_10
guint               clutter_text_buffer_delete_text         (ClutterTextBuffer *buffer,
                                                             guint              position,
                                                             gint               n_chars);
CLUTTER_AVAILABLE_IN_1_10
void                clutter_text_buffer_emit_inserted_text  (ClutterTextBuffer *buffer,
                                                             guint              position,
                                                             const gchar       *chars,
                                                             guint              n_chars);
CLUTTER_AVAILABLE_IN_1_10
void                clutter_text_buffer_emit_deleted_text   (ClutterTextBuffer *buffer,
                                                             guint              position,
                                                             guint              n_chars);

G_END_DECLS

#endif /* __CLUTTER_TEXT_BUFFER_H__ */
