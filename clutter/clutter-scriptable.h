/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Emmanuele Bassi  <ebassi@openedhand.com>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_SCRIPTABLE_H__
#define __CLUTTER_SCRIPTABLE_H__

#include <glib-object.h>
#include <json-glib/json-glib.h>
#include <clutter/clutter-script.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_SCRIPTABLE                 (clutter_scriptable_get_type ())
#define CLUTTER_SCRIPTABLE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_SCRIPTABLE, ClutterScriptable))
#define CLUTTER_IS_SCRIPTABLE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_SCRIPTABLE))
#define CLUTTER_SCRIPTABLE_GET_IFACE(obj)       (G_TYPE_INSTANCE_GET_INTERFACE ((obj), CLUTTER_TYPE_SCRIPTABLE, ClutterScriptableIface))

typedef struct _ClutterScriptable               ClutterScriptable;
typedef struct _ClutterScriptableIface          ClutterScriptableIface;

/**
 * ClutterScriptable:
 *
 * #ClutterScriptable is an opaque structure whose members cannot be directly
 * accessed
 *
 * Since: 0.6
 */

/**
 * ClutterScriptableIface:
 * @set_id: virtual function for setting the id of a scriptable object
 * @get_id: virtual function for getting the id of a scriptable object
 * @parse_custom_node: virtual function for parsing complex data containers
 *   into GObject properties
 * @set_custom_property: virtual function for setting a custom property
 *
 * Interface for implementing "scriptable" objects. An object implementing
 * this interface can override the parsing and properties setting sequence
 * when loading a UI definition data with #ClutterScript
 *
 * Since: 0.6
 */
struct _ClutterScriptableIface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/
  void         (* set_id)              (ClutterScriptable *scriptable,
                                        const gchar       *id_);
  const gchar *(* get_id)              (ClutterScriptable *scriptable);

  gboolean     (* parse_custom_node)   (ClutterScriptable *scriptable,
                                        ClutterScript     *script,
                                        GValue            *value,
                                        const gchar       *name,
                                        JsonNode          *node);
  void         (* set_custom_property) (ClutterScriptable *scriptable,
                                        ClutterScript     *script,
                                        const gchar       *name,
                                        const GValue      *value);
};

CLUTTER_AVAILABLE_IN_ALL
GType                 clutter_scriptable_get_type            (void) G_GNUC_CONST;

CLUTTER_AVAILABLE_IN_ALL
void                  clutter_scriptable_set_id              (ClutterScriptable *scriptable,
                                                              const gchar       *id_);
CLUTTER_AVAILABLE_IN_ALL
const gchar *         clutter_scriptable_get_id              (ClutterScriptable *scriptable);
CLUTTER_AVAILABLE_IN_ALL
gboolean              clutter_scriptable_parse_custom_node   (ClutterScriptable *scriptable,
                                                              ClutterScript     *script,
                                                              GValue            *value,
                                                              const gchar       *name,
                                                              JsonNode          *node);
CLUTTER_AVAILABLE_IN_ALL
void                  clutter_scriptable_set_custom_property (ClutterScriptable *scriptable,
                                                              ClutterScript     *script,
                                                              const gchar       *name,
                                                              const GValue      *value);

G_END_DECLS

#endif /* __CLUTTER_SCRIPTABLE_H__ */
