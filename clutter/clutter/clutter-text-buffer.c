/* clutter-text-buffer.c
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-text-buffer.h"
#include "clutter-marshal.h"
#include "clutter-private.h"

#include <string.h>

/**
 * SECTION:clutter-text-buffer
 * @title: ClutterTextBuffer
 * @short_description: Text buffer for ClutterText
 *
 * The #ClutterTextBuffer class contains the actual text displayed in a
 * #ClutterText widget.
 *
 * A single #ClutterTextBuffer object can be shared by multiple #ClutterText
 * widgets which will then share the same text content, but not the cursor
 * position, visibility attributes, icon etc.
 *
 * #ClutterTextBuffer may be derived from. Such a derived class might allow
 * text to be stored in an alternate location, such as non-pageable memory,
 * useful in the case of important passwords. Or a derived class could
 * integrate with an application's concept of undo/redo.
 *
 * Since: 1.10
 */

/* Initial size of buffer, in bytes */
#define MIN_SIZE 16

enum {
  PROP_0,
  PROP_TEXT,
  PROP_LENGTH,
  PROP_MAX_LENGTH,
  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST] = { NULL, };

enum {
  INSERTED_TEXT,
  DELETED_TEXT,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _ClutterTextBufferPrivate
{
  gint  max_length;

  /* Only valid if this class is not derived */
  gchar *normal_text;
  gsize  normal_text_size;
  gsize  normal_text_bytes;
  guint  normal_text_chars;
};

G_DEFINE_TYPE_WITH_PRIVATE (ClutterTextBuffer, clutter_text_buffer, G_TYPE_OBJECT)

/* --------------------------------------------------------------------------------
 * DEFAULT IMPLEMENTATIONS OF TEXT BUFFER
 *
 * These may be overridden by a derived class, behavior may be changed etc...
 * The normal_text and normal_text_xxxx fields may not be valid when
 * this class is derived from.
 */

/* Overwrite a memory that might contain sensitive information. */
static void
trash_area (gchar *area,
            gsize  len)
{
  volatile gchar *varea = (volatile gchar *)area;
  while (len-- > 0)
    *varea++ = 0;
}

static const gchar*
clutter_text_buffer_normal_get_text (ClutterTextBuffer *buffer,
                                  gsize          *n_bytes)
{
  if (n_bytes)
    *n_bytes = buffer->priv->normal_text_bytes;
  if (!buffer->priv->normal_text)
      return "";
  return buffer->priv->normal_text;
}

static guint
clutter_text_buffer_normal_get_length (ClutterTextBuffer *buffer)
{
  return buffer->priv->normal_text_chars;
}

static guint
clutter_text_buffer_normal_insert_text (ClutterTextBuffer *buffer,
                                     guint           position,
                                     const gchar    *chars,
                                     guint           n_chars)
{
  ClutterTextBufferPrivate *pv = buffer->priv;
  gsize prev_size;
  gsize n_bytes;
  gsize at;

  n_bytes = g_utf8_offset_to_pointer (chars, n_chars) - chars;

  /* Need more memory */
  if (n_bytes + pv->normal_text_bytes + 1 > pv->normal_text_size)
    {
      gchar *et_new;

      prev_size = pv->normal_text_size;

      /* Calculate our new buffer size */
      while (n_bytes + pv->normal_text_bytes + 1 > pv->normal_text_size)
        {
          if (pv->normal_text_size == 0)
            pv->normal_text_size = MIN_SIZE;
          else
            {
              if (2 * pv->normal_text_size < CLUTTER_TEXT_BUFFER_MAX_SIZE)
                pv->normal_text_size *= 2;
              else
                {
                  pv->normal_text_size = CLUTTER_TEXT_BUFFER_MAX_SIZE;
                  if (n_bytes > pv->normal_text_size - pv->normal_text_bytes - 1)
                    {
                      n_bytes = pv->normal_text_size - pv->normal_text_bytes - 1;
                      n_bytes = g_utf8_find_prev_char (chars, chars + n_bytes + 1) - chars;
                      n_chars = g_utf8_strlen (chars, n_bytes);
                    }
                  break;
                }
            }
        }

      /* Could be a password, so can't leave stuff in memory. */
      et_new = g_malloc (pv->normal_text_size);
      memcpy (et_new, pv->normal_text, MIN (prev_size, pv->normal_text_size));
      trash_area (pv->normal_text, prev_size);
      g_free (pv->normal_text);
      pv->normal_text = et_new;
    }

  /* Actual text insertion */
  at = g_utf8_offset_to_pointer (pv->normal_text, position) - pv->normal_text;
  g_memmove (pv->normal_text + at + n_bytes, pv->normal_text + at, pv->normal_text_bytes - at);
  memcpy (pv->normal_text + at, chars, n_bytes);

  /* Book keeping */
  pv->normal_text_bytes += n_bytes;
  pv->normal_text_chars += n_chars;
  pv->normal_text[pv->normal_text_bytes] = '\0';

  clutter_text_buffer_emit_inserted_text (buffer, position, chars, n_chars);
  return n_chars;
}

static guint
clutter_text_buffer_normal_delete_text (ClutterTextBuffer *buffer,
                                     guint           position,
                                     guint           n_chars)
{
  ClutterTextBufferPrivate *pv = buffer->priv;
  gsize start, end;

  if (position > pv->normal_text_chars)
    position = pv->normal_text_chars;
  if (position + n_chars > pv->normal_text_chars)
    n_chars = pv->normal_text_chars - position;

  if (n_chars > 0)
    {
      start = g_utf8_offset_to_pointer (pv->normal_text, position) - pv->normal_text;
      end = g_utf8_offset_to_pointer (pv->normal_text, position + n_chars) - pv->normal_text;

      g_memmove (pv->normal_text + start, pv->normal_text + end, pv->normal_text_bytes + 1 - end);
      pv->normal_text_chars -= n_chars;
      pv->normal_text_bytes -= (end - start);

      /*
       * Could be a password, make sure we don't leave anything sensitive after
       * the terminating zero.  Note, that the terminating zero already trashed
       * one byte.
       */
      trash_area (pv->normal_text + pv->normal_text_bytes + 1, end - start - 1);

      clutter_text_buffer_emit_deleted_text (buffer, position, n_chars);
    }

  return n_chars;
}

/* --------------------------------------------------------------------------------
 *
 */

static void
clutter_text_buffer_real_inserted_text (ClutterTextBuffer *buffer,
                                     guint           position,
                                     const gchar    *chars,
                                     guint           n_chars)
{
  g_object_notify (G_OBJECT (buffer), "text");
  g_object_notify (G_OBJECT (buffer), "length");
}

static void
clutter_text_buffer_real_deleted_text (ClutterTextBuffer *buffer,
                                    guint           position,
                                    guint           n_chars)
{
  g_object_notify (G_OBJECT (buffer), "text");
  g_object_notify (G_OBJECT (buffer), "length");
}

/* --------------------------------------------------------------------------------
 *
 */

static void
clutter_text_buffer_init (ClutterTextBuffer *self)
{
  self->priv = clutter_text_buffer_get_instance_private (self);

  self->priv->normal_text = NULL;
  self->priv->normal_text_chars = 0;
  self->priv->normal_text_bytes = 0;
  self->priv->normal_text_size = 0;
}

static void
clutter_text_buffer_finalize (GObject *obj)
{
  ClutterTextBuffer *buffer = CLUTTER_TEXT_BUFFER (obj);
  ClutterTextBufferPrivate *pv = buffer->priv;

  if (pv->normal_text)
    {
      trash_area (pv->normal_text, pv->normal_text_size);
      g_free (pv->normal_text);
      pv->normal_text = NULL;
      pv->normal_text_bytes = pv->normal_text_size = 0;
      pv->normal_text_chars = 0;
    }

  G_OBJECT_CLASS (clutter_text_buffer_parent_class)->finalize (obj);
}

static void
clutter_text_buffer_set_property (GObject      *obj,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ClutterTextBuffer *buffer = CLUTTER_TEXT_BUFFER (obj);

  switch (prop_id)
    {
    case PROP_MAX_LENGTH:
      clutter_text_buffer_set_max_length (buffer, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

static void
clutter_text_buffer_get_property (GObject    *obj,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ClutterTextBuffer *buffer = CLUTTER_TEXT_BUFFER (obj);

  switch (prop_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, clutter_text_buffer_get_text (buffer));
      break;
    case PROP_LENGTH:
      g_value_set_uint (value, clutter_text_buffer_get_length (buffer));
      break;
    case PROP_MAX_LENGTH:
      g_value_set_int (value, clutter_text_buffer_get_max_length (buffer));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

static void
clutter_text_buffer_class_init (ClutterTextBufferClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = clutter_text_buffer_finalize;
  gobject_class->set_property = clutter_text_buffer_set_property;
  gobject_class->get_property = clutter_text_buffer_get_property;

  klass->get_text = clutter_text_buffer_normal_get_text;
  klass->get_length = clutter_text_buffer_normal_get_length;
  klass->insert_text = clutter_text_buffer_normal_insert_text;
  klass->delete_text = clutter_text_buffer_normal_delete_text;

  klass->inserted_text = clutter_text_buffer_real_inserted_text;
  klass->deleted_text = clutter_text_buffer_real_deleted_text;

  /**
   * ClutterTextBuffer:text:
   *
   * The contents of the buffer.
   *
   * Since: 1.10
   */
  obj_props[PROP_TEXT] =
      g_param_spec_string ("text",
                           P_("Text"),
                           P_("The contents of the buffer"),
                           "",
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * ClutterTextBuffer:length:
   *
   * The length (in characters) of the text in buffer.
   *
   * Since: 1.10
   */
  obj_props[PROP_LENGTH] =
      g_param_spec_uint ("length",
                         P_("Text length"),
                         P_("Length of the text currently in the buffer"),
                         0, CLUTTER_TEXT_BUFFER_MAX_SIZE, 0,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * ClutterTextBuffer:max-length:
   *
   * The maximum length (in characters) of the text in the buffer.
   *
   * Since: 1.10
   */
  obj_props[PROP_MAX_LENGTH] =
      g_param_spec_int ("max-length",
                        P_("Maximum length"),
                        P_("Maximum number of characters for this entry. Zero if no maximum"),
                        0, CLUTTER_TEXT_BUFFER_MAX_SIZE, 0,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);

  /**
   * ClutterTextBuffer::inserted-text:
   * @buffer: a #ClutterTextBuffer
   * @position: the position the text was inserted at.
   * @chars: The text that was inserted.
   * @n_chars: The number of characters that were inserted.
   *
   * This signal is emitted after text is inserted into the buffer.
   *
   * Since: 1.10
   */
  signals[INSERTED_TEXT] =
    g_signal_new (I_("inserted-text"),
                  CLUTTER_TYPE_TEXT_BUFFER,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (ClutterTextBufferClass, inserted_text),
                  NULL, NULL,
                  _clutter_marshal_VOID__UINT_STRING_UINT,
                  G_TYPE_NONE, 3,
                  G_TYPE_UINT,
                  G_TYPE_STRING,
                  G_TYPE_UINT);

  /**
   * ClutterTextBuffer::deleted-text:
   * @buffer: a #ClutterTextBuffer
   * @position: the position the text was deleted at.
   * @n_chars: The number of characters that were deleted.
   *
   * This signal is emitted after text is deleted from the buffer.
   *
   * Since: 1.10
   */
  signals[DELETED_TEXT] =
    g_signal_new (I_("deleted-text"),
                  CLUTTER_TYPE_TEXT_BUFFER,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (ClutterTextBufferClass, deleted_text),
                  NULL, NULL,
                  _clutter_marshal_VOID__UINT_UINT,
                  G_TYPE_NONE, 2,
                  G_TYPE_UINT,
                  G_TYPE_UINT);
}

/* --------------------------------------------------------------------------------
 *
 */

/**
 * clutter_text_buffer_new:
 *
 * Create a new ClutterTextBuffer object.
 *
 * Return value: A new ClutterTextBuffer object.
 *
 * Since: 1.10
 **/
ClutterTextBuffer*
clutter_text_buffer_new (void)
{
  return g_object_new (CLUTTER_TYPE_TEXT_BUFFER, NULL);
}


/**
 * clutter_text_buffer_new_with_text:
 * @text: (allow-none): initial buffer text
 * @text_len: initial buffer text length, or -1 for null-terminated.
 *
 * Create a new ClutterTextBuffer object with some text.
 *
 * Return value: A new ClutterTextBuffer object.
 *
 * Since: 1.10
 **/
ClutterTextBuffer*
clutter_text_buffer_new_with_text (const gchar   *text,
                                   gssize         text_len)
{
  ClutterTextBuffer *buffer;
  buffer = clutter_text_buffer_new ();
  clutter_text_buffer_set_text (buffer, text, text_len);
  return buffer;
}


/**
 * clutter_text_buffer_get_length:
 * @buffer: a #ClutterTextBuffer
 *
 * Retrieves the length in characters of the buffer.
 *
 * Return value: The number of characters in the buffer.
 *
 * Since: 1.10
 **/
guint
clutter_text_buffer_get_length (ClutterTextBuffer *buffer)
{
  ClutterTextBufferClass *klass;

  g_return_val_if_fail (CLUTTER_IS_TEXT_BUFFER (buffer), 0);

  klass = CLUTTER_TEXT_BUFFER_GET_CLASS (buffer);
  g_return_val_if_fail (klass->get_length != NULL, 0);

  return (*klass->get_length) (buffer);
}

/**
 * clutter_text_buffer_get_bytes:
 * @buffer: a #ClutterTextBuffer
 *
 * Retrieves the length in bytes of the buffer.
 * See clutter_text_buffer_get_length().
 *
 * Return value: The byte length of the buffer.
 *
 * Since: 1.10
 **/
gsize
clutter_text_buffer_get_bytes (ClutterTextBuffer *buffer)
{
  ClutterTextBufferClass *klass;
  gsize bytes = 0;

  g_return_val_if_fail (CLUTTER_IS_TEXT_BUFFER (buffer), 0);

  klass = CLUTTER_TEXT_BUFFER_GET_CLASS (buffer);
  g_return_val_if_fail (klass->get_text != NULL, 0);

  (*klass->get_text) (buffer, &bytes);
  return bytes;
}

/**
 * clutter_text_buffer_get_text:
 * @buffer: a #ClutterTextBuffer
 *
 * Retrieves the contents of the buffer.
 *
 * The memory pointer returned by this call will not change
 * unless this object emits a signal, or is finalized.
 *
 * Return value: a pointer to the contents of the widget as a
 *      string. This string points to internally allocated
 *      storage in the buffer and must not be freed, modified or
 *      stored.
 *
 * Since: 1.10
 **/
const gchar*
clutter_text_buffer_get_text (ClutterTextBuffer *buffer)
{
  ClutterTextBufferClass *klass;

  g_return_val_if_fail (CLUTTER_IS_TEXT_BUFFER (buffer), NULL);

  klass = CLUTTER_TEXT_BUFFER_GET_CLASS (buffer);
  g_return_val_if_fail (klass->get_text != NULL, NULL);

  return (*klass->get_text) (buffer, NULL);
}

/**
 * clutter_text_buffer_set_text:
 * @buffer: a #ClutterTextBuffer
 * @chars: the new text
 * @n_chars: the number of characters in @text, or -1
 *
 * Sets the text in the buffer.
 *
 * This is roughly equivalent to calling clutter_text_buffer_delete_text()
 * and clutter_text_buffer_insert_text().
 *
 * Note that @n_chars is in characters, not in bytes.
 *
 * Since: 1.10
 **/
void
clutter_text_buffer_set_text (ClutterTextBuffer *buffer,
                              const gchar       *chars,
                              gint               n_chars)
{
  g_return_if_fail (CLUTTER_IS_TEXT_BUFFER (buffer));
  g_return_if_fail (chars != NULL);

  g_object_freeze_notify (G_OBJECT (buffer));
  clutter_text_buffer_delete_text (buffer, 0, -1);
  clutter_text_buffer_insert_text (buffer, 0, chars, n_chars);
  g_object_thaw_notify (G_OBJECT (buffer));
}

/**
 * clutter_text_buffer_set_max_length:
 * @buffer: a #ClutterTextBuffer
 * @max_length: the maximum length of the entry buffer, or 0 for no maximum.
 *   (other than the maximum length of entries.) The value passed in will
 *   be clamped to the range [ 0, %CLUTTER_TEXT_BUFFER_MAX_SIZE ].
 *
 * Sets the maximum allowed length of the contents of the buffer. If
 * the current contents are longer than the given length, then they
 * will be truncated to fit.
 *
 * Since: 1.10
 **/
void
clutter_text_buffer_set_max_length (ClutterTextBuffer *buffer,
                                    gint               max_length)
{
  g_return_if_fail (CLUTTER_IS_TEXT_BUFFER (buffer));

  max_length = CLAMP (max_length, 0, CLUTTER_TEXT_BUFFER_MAX_SIZE);

  if (max_length > 0 && clutter_text_buffer_get_length (buffer) > max_length)
    clutter_text_buffer_delete_text (buffer, max_length, -1);

  buffer->priv->max_length = max_length;
  g_object_notify (G_OBJECT (buffer), "max-length");
}

/**
 * clutter_text_buffer_get_max_length:
 * @buffer: a #ClutterTextBuffer
 *
 * Retrieves the maximum allowed length of the text in
 * @buffer. See clutter_text_buffer_set_max_length().
 *
 * Return value: the maximum allowed number of characters
 *               in #ClutterTextBuffer, or 0 if there is no maximum.
 *
 * Since: 1.10
 */
gint
clutter_text_buffer_get_max_length (ClutterTextBuffer *buffer)
{
  g_return_val_if_fail (CLUTTER_IS_TEXT_BUFFER (buffer), 0);
  return buffer->priv->max_length;
}

/**
 * clutter_text_buffer_insert_text:
 * @buffer: a #ClutterTextBuffer
 * @position: the position at which to insert text.
 * @chars: the text to insert into the buffer.
 * @n_chars: the length of the text in characters, or -1
 *
 * Inserts @n_chars characters of @chars into the contents of the
 * buffer, at position @position.
 *
 * If @n_chars is negative, then characters from chars will be inserted
 * until a null-terminator is found. If @position or @n_chars are out of
 * bounds, or the maximum buffer text length is exceeded, then they are
 * coerced to sane values.
 *
 * Note that the position and length are in characters, not in bytes.
 *
 * Returns: The number of characters actually inserted.
 *
 * Since: 1.10
 */
guint
clutter_text_buffer_insert_text (ClutterTextBuffer *buffer,
                                 guint              position,
                                 const gchar       *chars,
                                 gint               n_chars)
{
  ClutterTextBufferClass *klass;
  ClutterTextBufferPrivate *pv;
  guint length;

  g_return_val_if_fail (CLUTTER_IS_TEXT_BUFFER (buffer), 0);

  length = clutter_text_buffer_get_length (buffer);
  pv = buffer->priv;

  if (n_chars < 0)
    n_chars = g_utf8_strlen (chars, -1);

  /* Bring position into bounds */
  if (position > length)
    position = length;

  /* Make sure not entering too much data */
  if (pv->max_length > 0)
    {
      if (length >= pv->max_length)
        n_chars = 0;
      else if (length + n_chars > pv->max_length)
        n_chars -= (length + n_chars) - pv->max_length;
    }

  klass = CLUTTER_TEXT_BUFFER_GET_CLASS (buffer);
  g_return_val_if_fail (klass->insert_text != NULL, 0);

  return (klass->insert_text) (buffer, position, chars, n_chars);
}

/**
 * clutter_text_buffer_delete_text:
 * @buffer: a #ClutterTextBuffer
 * @position: position at which to delete text
 * @n_chars: number of characters to delete
 *
 * Deletes a sequence of characters from the buffer. @n_chars characters are
 * deleted starting at @position. If @n_chars is negative, then all characters
 * until the end of the text are deleted.
 *
 * If @position or @n_chars are out of bounds, then they are coerced to sane
 * values.
 *
 * Note that the positions are specified in characters, not bytes.
 *
 * Returns: The number of characters deleted.
 *
 * Since: 1.10
 */
guint
clutter_text_buffer_delete_text (ClutterTextBuffer *buffer,
                                 guint              position,
                                 gint               n_chars)
{
  ClutterTextBufferClass *klass;
  guint length;

  g_return_val_if_fail (CLUTTER_IS_TEXT_BUFFER (buffer), 0);

  length = clutter_text_buffer_get_length (buffer);
  if (n_chars < 0)
    n_chars = length;
  if (position > length)
    position = length;
  if (position + n_chars > length)
    n_chars = length - position;

  klass = CLUTTER_TEXT_BUFFER_GET_CLASS (buffer);
  g_return_val_if_fail (klass->delete_text != NULL, 0);

  return (klass->delete_text) (buffer, position, n_chars);
}

/**
 * clutter_text_buffer_emit_inserted_text:
 * @buffer: a #ClutterTextBuffer
 * @position: position at which text was inserted
 * @chars: text that was inserted
 * @n_chars: number of characters inserted
 *
 * Emits the #ClutterTextBuffer::inserted-text signal on @buffer.
 *
 * Used when subclassing #ClutterTextBuffer
 *
 * Since: 1.10
 */
void
clutter_text_buffer_emit_inserted_text (ClutterTextBuffer *buffer,
                                        guint              position,
                                        const gchar       *chars,
                                        guint              n_chars)
{
  g_return_if_fail (CLUTTER_IS_TEXT_BUFFER (buffer));
  g_signal_emit (buffer, signals[INSERTED_TEXT], 0, position, chars, n_chars);
}

/**
 * clutter_text_buffer_emit_deleted_text:
 * @buffer: a #ClutterTextBuffer
 * @position: position at which text was deleted
 * @n_chars: number of characters deleted
 *
 * Emits the #ClutterTextBuffer::deleted-text signal on @buffer.
 *
 * Used when subclassing #ClutterTextBuffer
 *
 * Since: 1.10
 */
void
clutter_text_buffer_emit_deleted_text (ClutterTextBuffer *buffer,
                                       guint              position,
                                       guint              n_chars)
{
  g_return_if_fail (CLUTTER_IS_TEXT_BUFFER (buffer));
  g_signal_emit (buffer, signals[DELETED_TEXT], 0, position, n_chars);
}
