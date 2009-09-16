/*
 * nbtk-clipboard.h: clipboard object
 *
 * Copyright 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Written by: Thomas Wood <thomas.wood@intel.com>
 *
 */

#ifndef _NBTK_CLIPBOARD_H
#define _NBTK_CLIPBOARD_H

#include <glib-object.h>

G_BEGIN_DECLS

#define NBTK_TYPE_CLIPBOARD nbtk_clipboard_get_type()

#define NBTK_CLIPBOARD(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  NBTK_TYPE_CLIPBOARD, NbtkClipboard))

#define NBTK_CLIPBOARD_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  NBTK_TYPE_CLIPBOARD, NbtkClipboardClass))

#define NBTK_IS_CLIPBOARD(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  NBTK_TYPE_CLIPBOARD))

#define NBTK_IS_CLIPBOARD_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  NBTK_TYPE_CLIPBOARD))

#define NBTK_CLIPBOARD_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  NBTK_TYPE_CLIPBOARD, NbtkClipboardClass))

typedef struct _NbtkClipboard NbtkClipboard;
typedef struct _NbtkClipboardClass NbtkClipboardClass;
typedef struct _NbtkClipboardPrivate NbtkClipboardPrivate;

/**
 * NbtkClipboard:
 *
 * The contents of this structure is private and should only be accessed using
 * the provided API.
 */
struct _NbtkClipboard
{
  /*< private >*/
  GObject parent;
  NbtkClipboardPrivate *priv;
};

struct _NbtkClipboardClass
{
  GObjectClass parent_class;
};

/**
 * NbtkClipboardCallbackFunc:
 * @clipboard: A #NbtkClipboard
 * @text: text from the clipboard
 * @user_data: user data
 *
 * Callback function called when text is retrieved from the clipboard.
 */
typedef void (*NbtkClipboardCallbackFunc) (NbtkClipboard *clipboard,
                                           const gchar   *text,
                                           gpointer       user_data);

GType nbtk_clipboard_get_type (void);

NbtkClipboard* nbtk_clipboard_get_default (void);
void nbtk_clipboard_get_text (NbtkClipboard *clipboard, NbtkClipboardCallbackFunc callback, gpointer user_data);
void nbtk_clipboard_set_text (NbtkClipboard *clipboard, const gchar *text);

G_END_DECLS

#endif /* _NBTK_CLIPBOARD_H */
