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

#ifndef _HAVE_CLUTTER_LABEL_H
#define _HAVE_CLUTTER_LABEL_H

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <clutter/clutter-texture.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_LABEL clutter_label_get_type()

#define CLUTTER_LABEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_LABEL, ClutterLabel))

#define CLUTTER_LABEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_LABEL, ClutterLabelClass))

#define CLUTTER_IS_LABEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_LABEL))

#define CLUTTER_IS_LABEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_LABEL))

#define CLUTTER_LABEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_LABEL, ClutterLabelClass))

typedef struct _ClutterLabel ClutterLabel;
typedef struct _ClutterLabelPrivate ClutterLabelPrivate ;
typedef struct _ClutterLabelClass ClutterLabelClass;

struct _ClutterLabel
{
  ClutterTexture         parent;

  /*< private >*/
  ClutterLabelPrivate   *priv;
};

struct _ClutterLabelClass 
{
  /*< private >*/
  ClutterTextureClass parent_class;

  void (*_clutter_label_1) (void);
  void (*_clutter_label_2) (void);
  void (*_clutter_label_3) (void);
  void (*_clutter_label_4) (void);
}; 

GType clutter_label_get_type (void);

ClutterElement*
clutter_label_new_with_text (const gchar *font_desc, const gchar *text);

ClutterElement*
clutter_label_new (void);

void
clutter_label_set_text (ClutterLabel *label, const gchar *text);

void
clutter_label_set_font (ClutterLabel *label, const gchar *desc);

void
clutter_label_set_color (ClutterLabel *label, guint32 pixel);

void
clutter_label_set_text_extents (ClutterLabel *label, 
				gint          width,
				gint          height);

G_END_DECLS

#endif
