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
 *
 *
 */

/**
 * SECTION:clutter-scriptable
 * @short_description: Override the UI definition parsing
 *
 * The #ClutterScriptableIface interface exposes the UI definition parsing
 * process to external classes. By implementing this interface, a class can
 * override the UI definition parsing and transform complex data types into
 * GObject properties, or allow custom properties.
 *
 * #ClutterScriptable is available since Clutter 0.6
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "clutter-scriptable.h"
#include "clutter-script-private.h"

#include "clutter-private.h"
#include "clutter-debug.h"

GType
clutter_scriptable_get_type (void)
{
  static GType scriptable_type = 0;

  if (G_UNLIKELY (scriptable_type == 0))
    {
      scriptable_type =
        g_type_register_static_simple (G_TYPE_INTERFACE,
                                       I_("ClutterScriptable"),
                                       sizeof (ClutterScriptableIface),
                                       NULL, 0, NULL, 0);
    }

  return scriptable_type;
}

/**
 * clutter_scriptable_set_id:
 * @scriptable: a #ClutterScriptable
 * @id: the #ClutterScript id of the object
 *
 * Sets @id as the unique Clutter script it for this instance of
 * #ClutterScriptableIface.
 *
 * This name can be used by user interface designer applications to
 * define a unique name for an object constructable using the UI
 * definition language parsed by #ClutterScript.
 *
 * Since: 0.6
 */
void
clutter_scriptable_set_id (ClutterScriptable *scriptable,
                           const gchar       *id)
{
  ClutterScriptableIface *iface;

  g_return_if_fail (CLUTTER_IS_SCRIPTABLE (scriptable));
  g_return_if_fail (id != NULL);

  iface = CLUTTER_SCRIPTABLE_GET_IFACE (scriptable);
  if (iface->set_id)
    iface->set_id (scriptable, id);
  else
    g_object_set_data_full (G_OBJECT (scriptable),
                            "clutter-script-id",
                            g_strdup (id),
                            g_free);
}

/**
 * clutter_scriptable_get_id:
 * @scriptable: a #ClutterScriptable
 *
 * Retrieves the id of @scriptable set using clutter_scriptable_set_id().
 *
 * Return value: the id of the object. The returned string is owned by
 *   the scriptable object and should never be modified of freed
 *
 * Since: 0.6
 */
G_CONST_RETURN gchar *
clutter_scriptable_get_id (ClutterScriptable *scriptable)
{
  ClutterScriptableIface *iface;

  g_return_val_if_fail (CLUTTER_IS_SCRIPTABLE (scriptable), NULL);

  iface = CLUTTER_SCRIPTABLE_GET_IFACE (scriptable);
  if (iface->get_id)
    return iface->get_id (scriptable);
  else
    return g_object_get_data (G_OBJECT (scriptable), "clutter-script-id");
}

/**
 * clutter_scriptable_parse_custom_node:
 * @scriptable: a #ClutterScriptable
 * @script: the #ClutterScript creating the scriptable instance
 * @value: the generic value to be set
 * @name: the name of the node
 * @node: the JSON node to be parsed
 *
 * Parses the passed JSON node. The implementation must set the type
 * of the passed #GValue pointer using g_value_init().
 *
 * Return value: %TRUE if the node was successfully parsed, %FALSE otherwise.
 *
 * Since: 0.6
 */
gboolean
clutter_scriptable_parse_custom_node (ClutterScriptable *scriptable,
                                      ClutterScript     *script,
                                      GValue            *value,
                                      const gchar       *name,
                                      JsonNode          *node)
{
  ClutterScriptableIface *iface;

  g_return_val_if_fail (CLUTTER_IS_SCRIPTABLE (scriptable), FALSE);
  g_return_val_if_fail (CLUTTER_IS_SCRIPT (script), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (node != NULL, FALSE);

  iface = CLUTTER_SCRIPTABLE_GET_IFACE (scriptable);
  if (iface->parse_custom_node)
    return iface->parse_custom_node (scriptable, script, value, name, node);

  return FALSE;
}

/**
 * clutter_scriptable_set_custom_property:
 * @scriptable: a #ClutterScriptable
 * @script: the #ClutterScript creating the scriptable instance
 * @name: the name of the property
 * @value: the value of the property
 *
 * Overrides the common properties setting. The underlying virtual
 * function should be used when implementing custom properties.
 *
 * Since: 0.6
 */
void
clutter_scriptable_set_custom_property (ClutterScriptable *scriptable,
                                        ClutterScript     *script,
                                        const gchar       *name,
                                        const GValue      *value)
{
  ClutterScriptableIface *iface;

  g_return_if_fail (CLUTTER_IS_SCRIPTABLE (scriptable));
  g_return_if_fail (CLUTTER_IS_SCRIPT (script));
  g_return_if_fail (name != NULL);
  g_return_if_fail (value != NULL);

  iface = CLUTTER_SCRIPTABLE_GET_IFACE (scriptable);
  if (iface->set_custom_property)
    iface->set_custom_property (scriptable, script, name, value);
}
