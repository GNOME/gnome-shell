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

#define CLUTTER_TYPE_SCRIPT_PARSER      (clutter_script_parser_get_type ())
#define CLUTTER_SCRIPT_PARSER(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_SCRIPT_PARSER, ClutterScriptParser))
#define CLUTTER_IS_SCRIPT_PARSER(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_SCRIPT_PARSER))

typedef struct _ClutterScriptParser     ClutterScriptParser;
typedef struct _JsonParserClass         ClutterScriptParserClass;

struct _ClutterScriptParser
{
  JsonParser parent_instance;

  /* back reference */
  ClutterScript *script;
};

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

  guint is_child : 1;
} PropertyInfo;

typedef struct {
  gchar *name;
  gchar *handler;
  gchar *object;

  GConnectFlags flags;
} SignalInfo;

void property_info_free (gpointer data);

GType clutter_script_parser_get_type (void) G_GNUC_CONST;

gboolean clutter_script_parse_node        (ClutterScript *script,
                                           GValue        *value,
                                           const gchar   *name,
                                           JsonNode      *node,
                                           GParamSpec    *pspec);

GType    clutter_script_get_type_from_symbol (const gchar *symbol);
GType    clutter_script_get_type_from_class  (const gchar *name);

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

void _clutter_script_construct_object (ClutterScript *script,
                                       ObjectInfo    *oinfo);
void _clutter_script_apply_properties (ClutterScript *script,
                                       ObjectInfo    *oinfo);

gchar *_clutter_script_generate_fake_id (ClutterScript *script);

void _clutter_script_warn_missing_attribute (ClutterScript *script,
                                             const gchar   *id,
                                             const gchar   *attribute);

void _clutter_script_warn_invalid_value (ClutterScript *script,
                                         const gchar   *attribute,
                                         const gchar   *expected,
                                         JsonNode      *node);

ObjectInfo *_clutter_script_get_object_info (ClutterScript *script,
                                             const gchar   *script_id);

guint _clutter_script_get_last_merge_id (ClutterScript *script);

void _clutter_script_add_object_info (ClutterScript *script,
                                      ObjectInfo    *oinfo);

G_END_DECLS

#endif /* __CLUTTER_SCRIPT_PRIVATE_H__ */
