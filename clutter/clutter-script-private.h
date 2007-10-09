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

#ifndef __CLUTTER_SCRIPT_PRIVATE_H__
#define __CLUTTER_SCRIPT_PRIVATE_H__

#include <glib-object.h>
#include "clutter-script.h"

G_BEGIN_DECLS

typedef GType (* GTypeGetFunc) (void);

typedef struct {
  gchar *class_name;
  gchar *id;

  GList *properties;
  GList *children;

  GType gtype;
  GObject *object;
} ObjectInfo;

typedef struct {
  gchar *property_name;
  GValue value;
} PropertyInfo;

GObject *clutter_script_construct_object (ClutterScript *script,
                                          ObjectInfo    *info);

gboolean clutter_script_enum_from_string (GType          gtype,
                                          const gchar   *string,
                                          gint          *enum_value);

G_END_DECLS

#endif /* __CLUTTER_SCRIPT_PRIVATE_H__ */
