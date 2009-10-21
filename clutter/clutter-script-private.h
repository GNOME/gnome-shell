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
#include "clutter-json.h"
#include "clutter-color.h"
#include "clutter-types.h"
#include "clutter-script.h"

G_BEGIN_DECLS

typedef GType (* GTypeGetFunc) (void);

typedef struct {
  gchar *id;
  gchar *class_name;
  gchar *type_func;

  GList *properties;
  GList *children;
  GList *behaviours;
  GList *signals;

  GType gtype;
  GObject *object;

  guint merge_id;

  guint is_stage_default : 1;
  guint is_toplevel      : 1;
  guint has_unresolved   : 1;
  guint is_unmerged      : 1;
} ObjectInfo;

void object_info_free (gpointer data);

typedef struct {
  gchar *name;
  JsonNode *node;
  GParamSpec *pspec;
} PropertyInfo;

typedef struct {
  gchar *name;
  gchar *handler;
  gchar *object;

  GConnectFlags flags;
} SignalInfo;

void property_info_free (gpointer data);

gboolean clutter_script_parse_node        (ClutterScript *script,
                                           GValue        *value,
                                           const gchar   *name,
                                           JsonNode      *node,
                                           GParamSpec    *pspec);

GType    clutter_script_get_type_from_symbol (const gchar *symbol);
GType    clutter_script_get_type_from_class  (const gchar *name);

GObject *clutter_script_construct_object  (ClutterScript *script,
                                           ObjectInfo    *info);

gulong   clutter_script_resolve_animation_mode (const gchar *namer);

gboolean clutter_script_enum_from_string  (GType          gtype,
                                           const gchar   *string,
                                           gint          *enum_value);
gboolean clutter_script_flags_from_string (GType          gtype,
                                           const gchar   *string,
                                           gint          *flags_value);

gboolean clutter_script_parse_knot        (ClutterScript   *script,
                                           JsonNode        *node,
                                           ClutterKnot     *knot);
gboolean clutter_script_parse_geometry    (ClutterScript   *script,
                                           JsonNode        *node,
                                           ClutterGeometry *geometry);
gboolean clutter_script_parse_color       (ClutterScript   *script,
                                           JsonNode        *node,
                                           ClutterColor    *color);
GObject *clutter_script_parse_alpha       (ClutterScript   *script,
                                           JsonNode        *node);

G_END_DECLS

#endif /* __CLUTTER_SCRIPT_PRIVATE_H__ */
