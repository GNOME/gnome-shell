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

/**
 * SECTION:clutter-script
 * @short_description: Loads a scene from UI definition data
 *
 * #ClutterScript is an object used for loading and building parts or a
 * complete scenegraph from external definition data in forms of string
 * buffers or files.
 *
 * The UI definition format is JSON, the JavaScript Object Notation as
 * described by RFC 4627. #ClutterScript can load a JSON data stream,
 * parse it and build all the objects defined into it. Each object must
 * have an "id" and a "type" properties defining the name to be used
 * to retrieve it from #ClutterScript with clutter_script_get_object(),
 * and the class type to be instanciated. Every other attribute will
 * be mapped to the class properties.
 *
 * A #ClutterScript holds a reference on every object it creates from
 * the definition data, except for the stage. Every non-actor object
 * will be finalized when the #ClutterScript instance holding it will
 * be finalized, so they need to be referenced using g_object_ref() in
 * order for them to survive.
 *
 * A simple object might be defined as:
 *
 * |[
 * {
 *   "id"     : "red-button",
 *   "type"   : "ClutterRectangle",
 *   "width"  : 100,
 *   "height" : 100,
 *   "color"  : "&num;ff0000ff"
 * }
 * ]|
 *
 * This will produce a red #ClutterRectangle, 100x100 pixels wide, and
 * with a ClutterScript id of "red-button"; it can be retrieved by calling:
 *
 * |[
 * ClutterActor *red_button;
 *
 * red_button = CLUTTER_ACTOR (clutter_script_get_object (script, "red-button"));
 * ]|
 *
 * and then manipulated with the Clutter API. For every object created
 * using ClutterScript it is possible to check the id by calling
 * clutter_get_script_id().
 *
 * Packing can be represented using the "children" member, and passing an
 * array of objects or ids of objects already defined (but not packed: the
 * packing rules of Clutter still apply, and an actor cannot be packed
 * in multiple containers without unparenting it in between).
 *
 * Behaviours and timelines can also be defined inside a UI definition
 * buffer:
 *
 * |[
 * {
 *   "id"          : "rotate-behaviour",
 *   "type"        : "ClutterBehaviourRotate",
 *   "angle-start" : 0.0,
 *   "angle-end"   : 360.0,
 *   "axis"        : "z-axis",
 *   "alpha"       : {
 *     "timeline" : { "duration" : 4000, "fps" : 60, "loop" : true },
 *     "function" : "sine"
 *   }
 * }
 * ]|
 *
 * And then to apply a defined behaviour to an actor defined inside the
 * definition of an actor, the "behaviour" member can be used:
 *
 * |[
 * {
 *   "id" : "my-rotating-actor",
 *   "type" : "ClutterTexture",
 *   ...
 *   "behaviours" : [ "rotate-behaviour" ]
 * }
 * ]|
 *
 * A #ClutterAlpha belonging to a #ClutterBehaviour can only be defined
 * implicitely. A #ClutterTimeline belonging to a #ClutterAlpha can be
 * either defined implicitely or explicitely. Implicitely defined
 * #ClutterAlpha<!-- -->s and #ClutterTimeline<!-- -->s can omit the
 * <varname>id</varname> member, as well as the <varname>type</varname> member,
 * but will not be available using clutter_script_get_object() (they can,
 * however, be extracted using the #ClutterBehaviour and #ClutterAlpha
 * API respectively).
 *
 * Signal handlers can be defined inside a Clutter UI definition file and
 * then autoconnected to their respective signals using the
 * clutter_script_connect_signals() function:
 *
 * |[
 *   ...
 *   "signals" : [
 *     { "name" : "button-press-event", "handler" : "on_button_press" },
 *     {
 *       "name" : "foo-signal",
 *       "handler" : "after_foo",
 *       "after" : true
 *     },
 *   ],
 *   ...
 * ]|
 *
 * Signal handler definitions must have a "name" and a "handler" members;
 * they can also have the "after" and "swapped" boolean members (for the
 * signal connection flags %G_CONNECT_AFTER and %G_CONNECT_SWAPPED
 * respectively) and the "object" string member for calling
 * g_signal_connect_object() instead of g_signal_connect().
 *
 * Clutter reserves the following names, so classes defining properties
 * through the usual GObject registration process should avoid using these
 * names to avoid collisions:
 *
 * <programlisting><![CDATA[
 *   "id"         := the unique name of a ClutterScript object
 *   "type"       := the class literal name, also used to infer the type
 *                   function
 *   "type_func"  := the GType function name, for non-standard classes
 *   "children"   := an array of names or objects to add as children
 *   "behaviours" := an array of names or objects to apply to an actor
 *   "signals"    := an array of signal definitions to connect to an object
 *   "is-default" := a boolean flag used when defining the #ClutterStage;
 *                   if set to "true" the default stage will be used instead
 *                   of creating a new #ClutterStage instance
 * ]]></programlisting>
 *
 * #ClutterScript is available since Clutter 0.6
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

#ifdef USE_GDKPIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif

#include "clutter-actor.h"
#include "clutter-alpha.h"
#include "clutter-behaviour.h"
#include "clutter-container.h"
#include "clutter-stage.h"
#include "clutter-texture.h"

#include "clutter-script.h"
#include "clutter-script-private.h"
#include "clutter-scriptable.h"

#include "clutter-enum-types.h"
#include "clutter-private.h"
#include "clutter-debug.h"

#include "json/json-parser.h"

enum
{
  PROP_0,

  PROP_FILENAME_SET,
  PROP_FILENAME
};

#define CLUTTER_SCRIPT_GET_PRIVATE(obj) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_SCRIPT, ClutterScriptPrivate))

struct _ClutterScriptPrivate
{
  GHashTable *objects;

  guint last_merge_id;
  guint last_unknown;

  JsonParser *parser;

  gchar **search_paths;

  gchar *filename;
  guint is_filename : 1;
};

G_DEFINE_TYPE (ClutterScript, clutter_script, G_TYPE_OBJECT);

static void
warn_missing_attribute (ClutterScript *script,
                        const gchar   *id,
                        const gchar   *attribute)
{
  ClutterScriptPrivate *priv = script->priv;

  if (G_LIKELY (id))
    {
      g_warning ("%s:%d: object '%s' has no '%s' attribute",
                 priv->is_filename ? priv->filename : "<input>",
                 json_parser_get_current_line (priv->parser),
                 id,
                 attribute);
    }
  else
    {
      g_warning ("%s:%d: object has no '%s' attribute",
                 priv->is_filename ? priv->filename : "<input>",
                 json_parser_get_current_line (priv->parser),
                 attribute);
    }
}

static void
warn_invalid_value (ClutterScript *script,
                    const gchar   *attribute,
                    const gchar   *expected,
                    JsonNode      *node)
{
  ClutterScriptPrivate *priv = script->priv;

  if (G_LIKELY (node))
    {
      g_warning ("%s:%d: invalid value of type '%s' for attribute '%s':"
                 "a value of type '%s' is expected",
                 priv->is_filename ? priv->filename : "<input>",
                 json_parser_get_current_line (priv->parser),
                 json_node_type_name (node),
                 attribute,
                 expected);
    }
  else
    {
      g_warning ("%s:%d: invalid value for attribute '%s':"
                 "a value of type '%s' is expected",
                 priv->is_filename ? priv->filename : "<input>",
                 json_parser_get_current_line (priv->parser),
                 attribute,
                 expected);
    }
}

static const gchar *
get_id_from_node (JsonNode *node)
{
  JsonObject *object;

  switch (JSON_NODE_TYPE (node))
    {
    case JSON_NODE_OBJECT:
      object = json_node_get_object (node);
      if (json_object_has_member (object, "id"))
        return json_object_get_string_member (object, "id");
      break;

    case JSON_NODE_VALUE:
      return json_node_get_string (node);

    default:
      break;
    }

  return NULL;
}

static GList *
parse_children (ObjectInfo *oinfo,
                JsonNode   *node)
{
  JsonArray *array;
  GList *retval;
  guint array_len, i;

  if (JSON_NODE_TYPE (node) != JSON_NODE_ARRAY)
    return NULL;

  retval = oinfo->children;

  array = json_node_get_array (node);
  array_len = json_array_get_length (array);

  for (i = 0; i < array_len; i++)
    {
      JsonNode *child = json_array_get_element (array, i);
      const gchar *id;

      id = get_id_from_node (child);
      if (id)
        retval = g_list_prepend (retval, g_strdup (id));
    }

  return g_list_reverse (retval);
}

static GList *
parse_signals (ClutterScript *script,
               ObjectInfo    *oinfo,
               JsonNode      *node)
{
  JsonArray *array;
  GList *retval;
  guint array_len, i;

  if (JSON_NODE_TYPE (node) != JSON_NODE_ARRAY)
    {
      warn_invalid_value (script, "signals", "Array", node);
      return NULL;
    }

  retval = oinfo->signals;
  array = json_node_get_array (node);
  array_len = json_array_get_length (array);

  for (i = 0; i < array_len; i++)
    {
      JsonNode *val = json_array_get_element (array, i);
      JsonObject *object;
      SignalInfo *sinfo;
      const gchar *name;
      const gchar *handler;
      const gchar *connect;
      GConnectFlags flags = 0;

      if (JSON_NODE_TYPE (val) != JSON_NODE_OBJECT)
        {
          warn_invalid_value (script, "signals array", "Object", node);
          continue;
        }

      object = json_node_get_object (val);

      /* mandatory: "name" */
      if (!json_object_has_member (object, "name"))
        {
          warn_missing_attribute (script, NULL, "name");
          continue;
        }
      else
        {
          name = json_object_get_string_member (object, "name");
          if (!name)
            {
              warn_invalid_value (script, "name", "string", val);
              continue;
            }
        }

      /* mandatory: "handler" */
      if (!json_object_has_member (object, "handler"))
        {
          warn_missing_attribute (script, NULL, "handler");
          continue;
        }
      else
        {
          handler = json_object_get_string_member (object, "handler");
          if (!handler)
            {
              warn_invalid_value (script, "handler", "string", val);
              continue;
            }
        }

      /* optional: "object" */
      if (json_object_has_member (object, "object"))
        connect = json_object_get_string_member (object, "object");
      else
        connect = NULL;

      /* optional: "after" */
      if (json_object_has_member (object, "after"))
        {
          if (json_object_get_boolean_member (object, "after"))
            flags |= G_CONNECT_AFTER;
        }

      /* optional: "swapped" */
      if (json_object_has_member (object, "swapped"))
        {
          if (json_object_get_boolean_member (object, "swapped"))
            flags |= G_CONNECT_SWAPPED;
        }

      CLUTTER_NOTE (SCRIPT, 
                    "Parsing signal '%s' (handler:%s, object:%s, flags:%d)",
                    name,
                    handler, connect, flags);

      sinfo = g_slice_new0 (SignalInfo);
      sinfo->name = g_strdup (name);
      sinfo->handler = g_strdup (handler);
      sinfo->object = g_strdup (connect);
      sinfo->flags = flags;

      retval = g_list_prepend (retval, sinfo);
    }

  return retval;
}

static GList *
parse_behaviours (ObjectInfo *oinfo,
                  JsonNode   *node)
{
  JsonArray *array;
  GList *retval;
  guint array_len, i;

  if (JSON_NODE_TYPE (node) != JSON_NODE_ARRAY)
    return NULL;

  retval = oinfo->behaviours;

  array = json_node_get_array (node);
  array_len = json_array_get_length (array);

  for (i = 0; i < array_len; i++)
    {
      JsonNode *child = json_array_get_element (array, i);
      const gchar *id;

      id = get_id_from_node (child);
      if (id)
        retval = g_list_prepend (retval, g_strdup (id));
    }

  return g_list_reverse (retval);
}

static ClutterTimeline *
construct_timeline (ClutterScript *script,
                    JsonObject    *object)
{
  ClutterTimeline *retval = NULL;
  ObjectInfo *oinfo;
  GList *members, *l;
  
  /* we fake an ObjectInfo so we can reuse clutter_script_construct_object()
   * here; we do not save it inside the hash table, because if this had
   * been a named object then we wouldn't have ended up here in the first
   * place
   */
  oinfo = g_slice_new0 (ObjectInfo);
  oinfo->gtype = CLUTTER_TYPE_TIMELINE;
  oinfo->id = g_strdup ("dummy");

  members = json_object_get_members (object);
  for (l = members; l != NULL; l = l->next)
    {
      const gchar *name = l->data;
      JsonNode *node = json_object_get_member (object, name);
      PropertyInfo *pinfo = g_slice_new0 (PropertyInfo);

      pinfo->name = g_strdelimit (g_strdup (name), G_STR_DELIMITERS, '-');
      pinfo->node = node;

      oinfo->properties = g_list_prepend (oinfo->properties, pinfo);
    }

  g_list_free (members);
  
  retval = CLUTTER_TIMELINE (clutter_script_construct_object (script, oinfo));

  /* we transfer ownership to the alpha function later */
  oinfo->is_toplevel = FALSE;
  object_info_free (oinfo);

  return retval;
}

/* define the names of the animation modes to match the ones
 * that developers might be more accustomed to
 */
static const struct
{
  const gchar *name;
  ClutterAnimationMode mode;
} animation_modes[] = {
  { "linear", CLUTTER_LINEAR },
  { "easeInQuad", CLUTTER_EASE_IN_QUAD },
  { "easeOutQuad", CLUTTER_EASE_OUT_QUAD },
  { "easeInOutQuad", CLUTTER_EASE_IN_OUT_QUAD },
  { "easeInCubic", CLUTTER_EASE_IN_CUBIC },
  { "easeOutCubic", CLUTTER_EASE_OUT_CUBIC },
  { "easeInOutCubic", CLUTTER_EASE_IN_OUT_CUBIC },
  { "easeInQuart", CLUTTER_EASE_IN_QUART },
  { "easeOutQuart", CLUTTER_EASE_OUT_QUART },
  { "easeInOutQuart", CLUTTER_EASE_IN_OUT_QUART },
  { "easeInQuint", CLUTTER_EASE_IN_QUINT },
  { "easeOutQuint", CLUTTER_EASE_OUT_QUINT },
  { "easeInOutQuint", CLUTTER_EASE_IN_OUT_QUINT },
  { "easeInSine", CLUTTER_EASE_IN_SINE },
  { "easeOutSine", CLUTTER_EASE_OUT_SINE },
  { "easeInOutSine", CLUTTER_EASE_IN_OUT_SINE },
  { "easeInExpo", CLUTTER_EASE_IN_EXPO },
  { "easeOutExpo", CLUTTER_EASE_OUT_EXPO },
  { "easeInOutExpo", CLUTTER_EASE_IN_OUT_EXPO },
  { "easeInCirc", CLUTTER_EASE_IN_CIRC },
  { "easeOutCirc", CLUTTER_EASE_OUT_CIRC },
  { "easeInOutCirc", CLUTTER_EASE_IN_OUT_CIRC },
  { "easeInElastic", CLUTTER_EASE_IN_ELASTIC },
  { "easeOutElastic", CLUTTER_EASE_OUT_ELASTIC },
  { "easeInOutElastic", CLUTTER_EASE_IN_OUT_ELASTIC },
  { "easeInBack", CLUTTER_EASE_IN_BACK },
  { "easeOutBack", CLUTTER_EASE_OUT_BACK },
  { "easeInOutBack", CLUTTER_EASE_IN_OUT_BACK },
  { "easeInBounce", CLUTTER_EASE_IN_BOUNCE },
  { "easeOutBounce", CLUTTER_EASE_OUT_BOUNCE },
  { "easeInOutBounce", CLUTTER_EASE_IN_OUT_BOUNCE },
};

static const gint n_animation_modes = G_N_ELEMENTS (animation_modes);

static ClutterAnimationMode
resolve_animation_mode (const gchar *name)
{
  gint i, res = 0;

  for (i = 0; i < n_animation_modes; i++)
    {
      if (strcmp (animation_modes[i].name, name) == 0)
        return animation_modes[i].mode;
    }

  if (clutter_script_enum_from_string (CLUTTER_TYPE_ANIMATION_MODE,
                                       name, &res))
    return res;

  g_warning ("Unable to find the animation mode '%s'", name);

  return CLUTTER_CUSTOM_MODE;
}

static ClutterAlphaFunc
resolve_alpha_func (const gchar *name)
{
  static GModule *module = NULL;
  ClutterAlphaFunc func;

  CLUTTER_NOTE (SCRIPT, "Looking up '%s' alpha function", name);

  if (G_UNLIKELY (!module))
    module = g_module_open (NULL, G_MODULE_BIND_LAZY);

  if (g_module_symbol (module, name, (gpointer) &func))
    {
      CLUTTER_NOTE (SCRIPT, "Found '%s' alpha function in the symbols table",
                    name);
      return func;
    }

  return NULL;
}

GObject *
clutter_script_parse_alpha (ClutterScript *script,
                            JsonNode      *node)
{
  GObject *retval = NULL;
  JsonObject *object;
  ClutterTimeline *timeline = NULL;
  ClutterAlphaFunc alpha_func = NULL;
  ClutterAnimationMode mode = CLUTTER_CUSTOM_MODE;
  JsonNode *val;
  gboolean unref_timeline = FALSE;

  if (JSON_NODE_TYPE (node) != JSON_NODE_OBJECT)
    return NULL;

  object = json_node_get_object (node);
  
  val = json_object_get_member (object, "timeline");
  if (val)
    {
      if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE &&
          json_node_get_string (val) != NULL)
        {
          const gchar *id = json_node_get_string (val);

          timeline =
            CLUTTER_TIMELINE (clutter_script_get_object (script, id));
        }
      else if (JSON_NODE_TYPE (val) == JSON_NODE_OBJECT)
        {
          timeline = construct_timeline (script, json_node_get_object (val));
          unref_timeline = TRUE;
        }
    }

  val = json_object_get_member (object, "mode");
  if (val && json_node_get_string (val) != NULL)
    mode = resolve_animation_mode (json_node_get_string (val));

  if (mode == CLUTTER_CUSTOM_MODE)
    {
      val = json_object_get_member (object, "function");
      if (val && json_node_get_string (val) != NULL)
        {
          alpha_func = resolve_alpha_func (json_node_get_string (val));
          if (!alpha_func)
            {
              g_warning ("Unable to find the function '%s' in the "
                         "Clutter alpha functions or the symbols table",
                         json_node_get_string (val));
            }
        }
    }

  CLUTTER_NOTE (SCRIPT, "Parsed alpha: %s timeline (%p) (mode:%d, func:%p)",
                unref_timeline ? "implicit" : "explicit",
                timeline ? timeline : 0x0,
                mode != CLUTTER_CUSTOM_MODE ? mode : 0,
                alpha_func ? alpha_func : 0x0);

  retval = g_object_new (CLUTTER_TYPE_ALPHA, NULL);

  if (mode != CLUTTER_CUSTOM_MODE)
    clutter_alpha_set_mode (CLUTTER_ALPHA (retval), mode);

  if (alpha_func != NULL)
    clutter_alpha_set_func (CLUTTER_ALPHA (retval), alpha_func, NULL, NULL);

  clutter_alpha_set_timeline (CLUTTER_ALPHA (retval), timeline);
  if (unref_timeline)
    g_object_unref (timeline);

  return retval;
}

static void
json_object_end (JsonParser *parser,
                 JsonObject *object,
                 gpointer    user_data)
{
  ClutterScript *script = user_data;
  ClutterScriptPrivate *priv = script->priv;
  ObjectInfo *oinfo;
  JsonNode *val;
  const gchar *id;
  GList *members, *l;

  if (!json_object_has_member (object, "id"))
    {
      gchar *fake;

      if (!json_object_has_member (object, "type"))
        return;

      fake = g_strdup_printf ("script-%d-%d",
                              priv->last_merge_id,
                              priv->last_unknown++);

      val = json_node_new (JSON_NODE_VALUE);
      json_node_set_string (val, fake);
      json_object_set_member (object, "id", val);

      g_free (fake);
    }
      
  if (!json_object_has_member (object, "type"))
    {
      val = json_object_get_member (object, "id");

      warn_missing_attribute (script, json_node_get_string (val), "type");
      return;
    }

  val = json_object_get_member (object, "id");
  id = json_node_get_string (val);

  oinfo = g_hash_table_lookup (priv->objects, id);
  if (G_LIKELY (!oinfo))
    {
      oinfo = g_slice_new0 (ObjectInfo);
      oinfo->merge_id = priv->last_merge_id;
      oinfo->id = g_strdup (id);

      val = json_object_get_member (object, "type");
      oinfo->class_name = json_node_dup_string (val);
  
      if (json_object_has_member (object, "type_func"))
        {
          val = json_object_get_member (object, "type_func");
          oinfo->type_func = json_node_dup_string (val);

          json_object_remove_member (object, "type_func");
        }
    }

  if (json_object_has_member (object, "children"))
    {
      val = json_object_get_member (object, "children");
      oinfo->children = parse_children (oinfo, val);

      json_object_remove_member (object, "children");
    }

  if (json_object_has_member (object, "behaviours"))
    {
      val = json_object_get_member (object, "behaviours");
      oinfo->behaviours = parse_behaviours (oinfo, val);

      json_object_remove_member (object, "behaviours");
    }

  if (json_object_has_member (object, "signals"))
    {
      val = json_object_get_member (object, "signals");
      oinfo->signals = parse_signals (script, oinfo, val);

      json_object_remove_member (object, "signals");
    }

  if (strcmp (oinfo->class_name, "ClutterStage") == 0 &&
      json_object_has_member (object, "is-default"))
    {
      oinfo->is_stage_default =
        json_object_get_boolean_member (object, "is-default");

      json_object_remove_member (object, "is-default");
    }
  else
    oinfo->is_stage_default = FALSE;

  oinfo->is_toplevel = FALSE;
  oinfo->is_unmerged = FALSE;
  oinfo->has_unresolved = TRUE;

  members = json_object_get_members (object);
  for (l = members; l; l = l->next)
    {
      const gchar *name = l->data;
      PropertyInfo *pinfo;
      JsonNode *node;

      /* we have already parsed these */
      if (strcmp (name, "id") == 0 || strcmp (name, "type") == 0)
        continue;

      node = json_object_get_member (object, name);

      pinfo = g_slice_new (PropertyInfo);

      pinfo->name = g_strdup (name);
      pinfo->node = node;
      pinfo->pspec = NULL;

      oinfo->properties = g_list_prepend (oinfo->properties, pinfo);
    }

  g_list_free (members);

  CLUTTER_NOTE (SCRIPT,
                "Added object '%s' (type:%s, id:%d, props:%d, signals:%d)",
                oinfo->id,
                oinfo->class_name,
                oinfo->merge_id,
                g_list_length (oinfo->properties),
                g_list_length (oinfo->signals));

  g_hash_table_steal (priv->objects, oinfo->id);
  g_hash_table_insert (priv->objects, oinfo->id, oinfo);

  oinfo->object = clutter_script_construct_object (script, oinfo);
}

gboolean
clutter_script_parse_node (ClutterScript *script,
                           GValue        *value,
                           const gchar   *name,
                           JsonNode      *node,
                           GParamSpec    *pspec)
{
  GValue node_value = { 0, };
  gboolean retval = FALSE;

  g_return_val_if_fail (CLUTTER_IS_SCRIPT (script), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (node != NULL, FALSE);

  switch (JSON_NODE_TYPE (node))
    {
    case JSON_NODE_OBJECT:
      if (!pspec)
        return FALSE;
      else
        {
          g_value_init (value, G_PARAM_SPEC_VALUE_TYPE (pspec));

          if (G_VALUE_HOLDS (value, CLUTTER_TYPE_ALPHA))
            {
              GObject *alpha;

              alpha = clutter_script_parse_alpha (script, node);
              if (alpha)
                {
                  g_value_set_object (value, alpha);
                  return TRUE;
                }
            }
          else if (G_VALUE_HOLDS (value, CLUTTER_TYPE_KNOT))
            {
              ClutterKnot knot = { 0, };

              /* knot := { "x" : (int), "y" : (int) } */

              if (clutter_script_parse_knot (script, node, &knot))
                {
                  g_value_set_boxed (value, &knot);
                  return TRUE;
                }
            }
          else if (G_VALUE_HOLDS (value, CLUTTER_TYPE_GEOMETRY))
            {
              ClutterGeometry geom = { 0, };

              /* geometry := {
               *        "x" : (int),
               *        "y" : (int),
               *        "width" : (int),
               *        "height" : (int)
               * }
               */

              if (clutter_script_parse_geometry (script, node, &geom))
                {
                  g_value_set_boxed (value, &geom);
                  return TRUE;
                }
            }
          else if (CLUTTER_VALUE_HOLDS_COLOR (value))
            {
              ClutterColor color = { 0, };

              /* color := {
               *        "red" : (int),
               *        "green" : (int),
               *        "blue" : (int),
               *        "alpha" : (int)
               * }
               */

              if (clutter_script_parse_color (script, node, &color))
                {
                  g_value_set_boxed (value, &color);
                  return TRUE;
                }
            }
         }
      return FALSE;

    case JSON_NODE_ARRAY:
      if (!pspec)
        return FALSE;
      else
        {
          g_value_init (value, G_PARAM_SPEC_VALUE_TYPE (pspec));

          if (G_VALUE_HOLDS (value, CLUTTER_TYPE_KNOT))
            {
              ClutterKnot knot = { 0, };

              /* knot := [ (int), (int) ] */

              if (clutter_script_parse_knot (script, node, &knot))
                {
                  g_value_set_boxed (value, &knot);
                  return TRUE;
                }
            }
          else if (G_VALUE_HOLDS (value, CLUTTER_TYPE_GEOMETRY))
            {
              ClutterGeometry geom = { 0, };

              /* geometry := [ (int), (int), (int), (int) ] */

              if (clutter_script_parse_geometry (script, node, &geom))
                {
                  g_value_set_boxed (value, &geom);
                  return TRUE;
                }
            }
          else if (CLUTTER_VALUE_HOLDS_COLOR (value))
            {
              ClutterColor color = { 0, };

              /* color := [ (int), (int), (int), (int) ] */

              if (clutter_script_parse_color (script, node, &color))
                {
                  g_value_set_boxed (value, &color);
                  return TRUE;
                }
            }
          else if (G_VALUE_HOLDS (value, G_TYPE_STRV))
            {
              JsonArray *array = json_node_get_array (node);
              guint i, array_len = json_array_get_length (array);
              GPtrArray *str_array = g_ptr_array_sized_new (array_len);

              /* strv := [ (str), (str), ... ] */

              for (i = 0; i < array_len; i++)
                {
                  JsonNode *val = json_array_get_element (array, i);

                  if (JSON_NODE_TYPE (val) != JSON_NODE_VALUE &&
                      json_node_get_string (val) == NULL)
                    continue;

                  g_ptr_array_add (str_array,
                                   (gpointer) json_node_get_string (val));
                }

              g_value_set_boxed (value, str_array->pdata);
              g_ptr_array_free (str_array, TRUE);

              return TRUE;
            }
        }
      return FALSE;

    case JSON_NODE_NULL:
      return FALSE;

    case JSON_NODE_VALUE:
      json_node_get_value (node, &node_value);

      if (pspec)
        g_value_init (value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      else
        g_value_init (value, G_VALUE_TYPE (&node_value));

      switch (G_TYPE_FUNDAMENTAL (G_VALUE_TYPE (value)))
        {
        /* fundamental JSON types */
        case G_TYPE_INT64:
        case G_TYPE_DOUBLE:
        case G_TYPE_STRING:
        case G_TYPE_BOOLEAN:
          g_value_copy (&node_value, value);
          retval = TRUE;
          break;

        case G_TYPE_INT:
          g_value_set_int (value, g_value_get_int64 (&node_value));
          retval = TRUE;
          break;

        case G_TYPE_UINT:
          g_value_set_uint (value, (guint) g_value_get_int64 (&node_value));
          retval = TRUE;
          break;

        case G_TYPE_ULONG:
          g_value_set_ulong (value, (gulong) g_value_get_int64 (&node_value));
          retval = TRUE;
          break;

        case G_TYPE_UCHAR:
          g_value_set_uchar (value, (guchar) g_value_get_int64 (&node_value));
          retval = TRUE;
          break;

        case G_TYPE_FLOAT:
          if (G_VALUE_HOLDS (&node_value, G_TYPE_DOUBLE))
            {
              g_value_set_float (value, g_value_get_double (&node_value));
              retval = TRUE;
            }
          else if (G_VALUE_HOLDS (&node_value, G_TYPE_INT64))
            {
              g_value_set_float (value, g_value_get_int64 (&node_value));
              retval = TRUE;
            }
          break;

        case G_TYPE_ENUM:
          if (G_VALUE_HOLDS (&node_value, G_TYPE_INT64))
            {
              g_value_set_enum (value, g_value_get_int64 (&node_value));
              retval = TRUE;
            }
          else if (G_VALUE_HOLDS (&node_value, G_TYPE_STRING))
            {
              gint enum_value;

              retval = clutter_script_enum_from_string (G_VALUE_TYPE (value),
                                                        g_value_get_string (&node_value),
                                                        &enum_value);
              if (retval)
                g_value_set_enum (value, enum_value);
            }
          break;

        case G_TYPE_FLAGS:
          if (G_VALUE_HOLDS (&node_value, G_TYPE_INT64))
            {
              g_value_set_flags (value, g_value_get_int64 (&node_value));
              retval = TRUE;
            }
          else if (G_VALUE_HOLDS (&node_value, G_TYPE_STRING))
            {
              gint flags_value;

              retval = clutter_script_flags_from_string (G_VALUE_TYPE (value),
                                                         g_value_get_string (&node_value),
                                                         &flags_value);
              if (retval)
                g_value_set_flags (value, flags_value);
            }
          break;

        case G_TYPE_BOXED:
          if (G_VALUE_HOLDS (value, CLUTTER_TYPE_COLOR))
            {
              if (G_VALUE_HOLDS (&node_value, G_TYPE_STRING))
                {
                  const gchar *str = g_value_get_string (&node_value);
                  ClutterColor color = { 0, };

                  if (str && str[0] != '\0')
                    clutter_color_from_string (&color, str);

                  g_value_set_boxed (value, &color);
                  retval = TRUE;
                }
            }
          break;

        case G_TYPE_OBJECT:
#ifdef USE_GDKPIXBUF
          if (G_VALUE_HOLDS (value, GDK_TYPE_PIXBUF))
            {
              if (G_VALUE_HOLDS (&node_value, G_TYPE_STRING))
                {
                  const gchar *str = g_value_get_string (&node_value);
                  GdkPixbuf *pixbuf = NULL;
                  gchar *path;
                  GError *error;

                  if (g_path_is_absolute (str))
                    path = g_strdup (str);
                  else
                    {
                      gchar *dirname = NULL;

                      if (script->priv->is_filename)
                        dirname = g_path_get_dirname (script->priv->filename);
                      else
                        dirname = g_get_current_dir ();

                      path = g_build_filename (dirname, str, NULL);
                      g_free (dirname);
                    }

                  error = NULL;
                  pixbuf = gdk_pixbuf_new_from_file (path, &error);
                  if (error)
                    {
                      g_warning ("Unable to open image at path '%s': %s",
                                 path,
                                 error->message);
                      g_error_free (error);
                    }
                  else
                    {
                      g_value_take_object (value, pixbuf);
                      retval = TRUE;
                    }

                  g_free (path);
                }
            }
          else
            {
              if (G_VALUE_HOLDS (&node_value, G_TYPE_STRING))
                {
                  const gchar *str = g_value_get_string (&node_value);
                  GObject *object = clutter_script_get_object (script, str);
                  if (object)
                    {
                      g_value_set_object (value, object);
                      retval = TRUE;
                    }
                }
            }
          break;
#endif

        default:
          retval = FALSE;
          break;
        }

      g_value_unset (&node_value);
      break;
    }

  return retval;
}

static GList *
clutter_script_translate_parameters (ClutterScript  *script,
                                     GObject        *object,
                                     const gchar    *name,
                                     GList          *properties,
                                     GArray        **params)
{
  ClutterScriptable *scriptable = NULL;
  ClutterScriptableIface *iface = NULL;
  GList *l, *unparsed;
  gboolean parse_custom = FALSE;

  *params = g_array_new (FALSE, FALSE, sizeof (GParameter));

  if (CLUTTER_IS_SCRIPTABLE (object))
    {
      scriptable = CLUTTER_SCRIPTABLE (object);
      iface = CLUTTER_SCRIPTABLE_GET_IFACE (scriptable);

      if (iface->parse_custom_node)
        parse_custom = TRUE;
    }

  unparsed = NULL;

  for (l = properties; l != NULL; l = l->next)
    {
      PropertyInfo *pinfo = l->data;
      GParameter param = { NULL };
      gboolean res = FALSE;

      CLUTTER_NOTE (SCRIPT, "Parsing %s property (id:%s)",
                    pinfo->pspec ? "regular" : "custom",
                    pinfo->name);

      if (parse_custom)
        res = iface->parse_custom_node (scriptable, script, &param.value,
                                        pinfo->name,
                                        pinfo->node);

      if (!res)
        res = clutter_script_parse_node (script, &param.value,
                                         pinfo->name,
                                         pinfo->node,
                                         pinfo->pspec);

      if (!res)
        {
          unparsed = g_list_prepend (unparsed, pinfo);
          continue;
        }

      param.name = g_strdup (pinfo->name);
      
      g_array_append_val (*params, param);

      property_info_free (pinfo);
    }

  g_list_free (properties);

  return unparsed;
}

static GList *
clutter_script_construct_parameters (ClutterScript  *script,
                                     GType           gtype,
                                     const gchar    *name,
                                     GList          *properties,
                                     GArray        **construct_params)
{
  GObjectClass *klass;
  GList *l, *unparsed;

  klass = g_type_class_ref (gtype);
  g_assert (klass != NULL);

  *construct_params = g_array_new (FALSE, FALSE, sizeof (GParameter));

  unparsed = NULL;

  for (l = properties; l != NULL; l = l->next)
    {
      PropertyInfo *pinfo = l->data;
      GParameter param = { NULL };
      GParamSpec *pspec = NULL;

      /* we allow custom property names for classes, so if we
       * don't find a corresponding GObject property for this
       * class we just skip it and let the class itself deal
       * with it later on
       */
      pspec = g_object_class_find_property (klass, pinfo->name);
      if (pspec)
        pinfo->pspec = g_param_spec_ref (pspec);
      else
        {
          pinfo->pspec = NULL;
          unparsed = g_list_prepend (unparsed, pinfo);
          continue;
        }

      if (!(pspec->flags & G_PARAM_CONSTRUCT_ONLY))
        {
          unparsed = g_list_prepend (unparsed, pinfo);
          continue;
        }

      param.name = g_strdup (pinfo->name);
      
      if (!clutter_script_parse_node (script, &param.value,
                                      pinfo->name,
                                      pinfo->node,
                                      pinfo->pspec))
        {
          unparsed = g_list_prepend (unparsed, pinfo);
          continue;
        }

      g_array_append_val (*construct_params, param);

      property_info_free (pinfo);
    }

  g_list_free (properties);

  g_type_class_unref (klass);

  return unparsed;
}

static void
apply_behaviours (ClutterScript *script,
                  ClutterActor  *actor,
                  ObjectInfo    *oinfo)
{
  GObject *object;
  GList *l, *unresolved;

  unresolved = NULL;
  for (l = oinfo->behaviours; l != NULL; l = l->next)
    {
      const gchar *name = l->data;

      object = clutter_script_get_object (script, name);
      if (!object)
        {
          ObjectInfo *behaviour_info;

          behaviour_info = g_hash_table_lookup (script->priv->objects, name);
          if (behaviour_info)
            object = clutter_script_construct_object (script, behaviour_info);
        }

      if (!object)
        {
          unresolved = g_list_prepend (unresolved, g_strdup (name));
          continue;
        }

      CLUTTER_NOTE (SCRIPT, "Applying behaviour '%s' to actor of type '%s'",
                    name,
                    g_type_name (G_OBJECT_TYPE (actor)));

      clutter_behaviour_apply (CLUTTER_BEHAVIOUR (object), actor);
    }

  g_list_foreach (oinfo->behaviours, (GFunc) g_free, NULL);
  g_list_free (oinfo->behaviours);

  oinfo->behaviours = unresolved;
}

static void
add_children (ClutterScript    *script,
              ClutterContainer *container,
              ObjectInfo       *oinfo)
{
  GObject *object;
  GList *l, *unresolved;

  unresolved = NULL;
  for (l = oinfo->children; l != NULL; l = l->next)
    {
      const gchar *name = l->data;

      object = clutter_script_get_object (script, name);
      if (!object)
        {
          ObjectInfo *child_info;

          child_info = g_hash_table_lookup (script->priv->objects, name);
          if (child_info)
            object = clutter_script_construct_object (script, child_info);
        }

      if (!object)
        {
          unresolved = g_list_prepend (unresolved, g_strdup (name));
          continue;
        }

      CLUTTER_NOTE (SCRIPT, "Adding children '%s' to actor of type '%s'",
                    name,
                    g_type_name (G_OBJECT_TYPE (container)));

      clutter_container_add_actor (container, CLUTTER_ACTOR (object));
    }

  g_list_foreach (oinfo->children, (GFunc) g_free, NULL);
  g_list_free (oinfo->children);

  oinfo->children = unresolved;
}

/* top-level classes: these classes are the roots of the
 * hiearchy; some of them must be unreferenced, whilst
 * others are owned by other instances
 */
static const struct
{
  const gchar *type_name;
  guint is_toplevel : 1;
} clutter_toplevels[] = {
  { "ClutterActor",          FALSE },
  { "ClutterAlpha",          FALSE },
  { "ClutterBehaviour",      TRUE  },
  { "ClutterEffectTemplate", TRUE  },
  { "ClutterModel",          TRUE  },
  { "ClutterScore",          TRUE  },
  { "ClutterTimeline",       TRUE  }
};

static guint n_clutter_toplevels = G_N_ELEMENTS (clutter_toplevels);

GObject *
clutter_script_construct_object (ClutterScript *script,
                                 ObjectInfo    *oinfo)
{
  GObject *object;
  guint i;
  GArray *params;
  GArray *construct_params;
  ClutterScriptable *scriptable = NULL;
  ClutterScriptableIface *iface = NULL;
  gboolean set_custom_property = FALSE;

  g_return_val_if_fail (CLUTTER_IS_SCRIPT (script), NULL);
  g_return_val_if_fail (oinfo != NULL, NULL);

  /* we have completely updated the object */
  if (oinfo->object && !oinfo->has_unresolved)
    return oinfo->object;

  if (oinfo->gtype == G_TYPE_INVALID)
    {
      if (G_UNLIKELY (oinfo->type_func))
        oinfo->gtype = clutter_script_get_type_from_symbol (oinfo->type_func);
      else
        oinfo->gtype = clutter_script_get_type_from_name (script, oinfo->class_name);
    }

  if (G_UNLIKELY (oinfo->gtype == G_TYPE_INVALID))
    return NULL;

  if (oinfo->object)
    object = oinfo->object;
  else if (oinfo->gtype == CLUTTER_TYPE_STAGE && oinfo->is_stage_default)
    {
      /* the default stage is a complex beast: we cannot create it using
       * g_object_newv() but we need clutter_script_construct_parameters()
       * to add the GParamSpec to the PropertyInfo pspec member, so
       * that we don't have to implement every complex property (like
       * the "color" one) directly inside the ClutterStage class.
       */
      oinfo->properties =
        clutter_script_construct_parameters (script,
                                             oinfo->gtype,
                                             oinfo->id,
                                             oinfo->properties,
                                             &construct_params);

      object = G_OBJECT (clutter_stage_get_default ());

      for (i = 0; i < construct_params->len; i++)
        {
          GParameter *param = &g_array_index (construct_params, GParameter, i);

          g_free ((gchar *) param->name);
          g_value_unset (&param->value);
        }

      g_array_free (construct_params, TRUE);
    }
  else
    {
      /* every other object: first, we get the construction parameters */
      oinfo->properties =
        clutter_script_construct_parameters (script,
                                             oinfo->gtype,
                                             oinfo->id,
                                             oinfo->properties,
                                             &construct_params);

      object = g_object_newv (oinfo->gtype,
                              construct_params->len,
                              (GParameter *) construct_params->data);

      for (i = 0; i < construct_params->len; i++)
        {
          GParameter *param = &g_array_index (construct_params, GParameter, i);
      
          g_free ((gchar *) param->name);
          g_value_unset (&param->value);
        }

      g_array_free (construct_params, TRUE);
   }

  /* shortcut, to avoid typechecking every time */
  if (CLUTTER_IS_SCRIPTABLE (object))
    {
      scriptable = CLUTTER_SCRIPTABLE (object);
      iface = CLUTTER_SCRIPTABLE_GET_IFACE (scriptable);

      if (iface->set_custom_property)
        set_custom_property = TRUE;
    }

  /* then we get the rest of the parameters, asking the object itself
   * to translate them for us, if we cannot do that
   */
  oinfo->properties = clutter_script_translate_parameters (script,
                                                           object,
                                                           oinfo->id,
                                                           oinfo->properties,
                                                           &params);

  /* consume all the properties we could translate in this pass */
  for (i = 0; i < params->len; i++)
    {
      GParameter *param = &g_array_index (params, GParameter, i);

      CLUTTER_NOTE (SCRIPT,
                    "Setting %s property '%s' (type:%s) to object '%s' (id:%s)",
                    set_custom_property ? "custom" : "regular",
                    param->name,
                    g_type_name (G_VALUE_TYPE (&param->value)),
                    g_type_name (oinfo->gtype),
                    oinfo->id);

      if (set_custom_property)
        iface->set_custom_property (scriptable, script,
                                    param->name,
                                    &param->value);
      else
        g_object_set_property (object, param->name, &param->value);

      g_free ((gchar *) param->name);
      g_value_unset (&param->value);
    }
  g_array_free (params, TRUE);

  for (i = 0; i < n_clutter_toplevels; i++)
    {
      const gchar *t_name = clutter_toplevels[i].type_name;
      GType t_type;
      
      t_type = clutter_script_get_type_from_name (script, t_name);
      if (g_type_is_a (oinfo->gtype, t_type))
        {
          oinfo->is_toplevel = clutter_toplevels[i].is_toplevel;
          break;
        }
    }

  /* XXX - at the moment, we are adding the children (and constructing
   * the scenegraph) after we applied all the properties of an object;
   * this usually ensures that an object is fully constructed before
   * it is added to its parent. unfortunately, this also means that
   * children cannot reference the parent's state inside their own
   * definition.
   *
   * see bug:
   *   http://bugzilla.openedhand.com/show_bug.cgi?id=1042
   */

  if (oinfo->children && CLUTTER_IS_CONTAINER (object))
    add_children (script, CLUTTER_CONTAINER (object), oinfo);

  if (oinfo->behaviours && CLUTTER_IS_ACTOR (object))
    apply_behaviours (script, CLUTTER_ACTOR (object), oinfo);

  if (oinfo->properties || oinfo->children || oinfo->behaviours)
    oinfo->has_unresolved = TRUE;
  else
    oinfo->has_unresolved = FALSE;

  if (scriptable)
    clutter_scriptable_set_id (scriptable, oinfo->id);
  else
    g_object_set_data_full (object, "clutter-script-id",
                            g_strdup (oinfo->id),
                            g_free);

  return object;
}

static void
construct_each_object (gpointer key,
                       gpointer value,
                       gpointer data)
{
  ClutterScript *script = data;
  ObjectInfo *oinfo = value;

  if (!oinfo->object)
    oinfo->object = clutter_script_construct_object (script, oinfo);
}

static void
json_parse_end (JsonParser *parser,
                gpointer    user_data)
{
  ClutterScript *script = user_data;
  ClutterScriptPrivate *priv = script->priv;

  g_hash_table_foreach (priv->objects, construct_each_object, script);
}

static GType
clutter_script_real_get_type_from_name (ClutterScript *script,
                                        const gchar   *type_name)
{
  GType gtype;

  gtype = g_type_from_name (type_name);
  if (gtype != G_TYPE_INVALID)
    return gtype;

  return clutter_script_get_type_from_class (type_name);
}

void
property_info_free (gpointer data)
{
  if (G_LIKELY (data))
    {
      PropertyInfo *pinfo = data;

      if (pinfo->pspec)
        g_param_spec_unref (pinfo->pspec);

      g_free (pinfo->name);

      g_slice_free (PropertyInfo, pinfo);
    }
}

void
signal_info_free (gpointer data)
{
  if (G_LIKELY (data))
    {
      SignalInfo *sinfo = data;

      g_free (sinfo->name);
      g_free (sinfo->handler);
      g_free (sinfo->object);

      g_slice_free (SignalInfo, sinfo);
    }
}

void
object_info_free (gpointer data)
{
  if (G_LIKELY (data))
    {
      ObjectInfo *oinfo = data;

      g_free (oinfo->id);
      g_free (oinfo->class_name);
      g_free (oinfo->type_func);

      g_list_foreach (oinfo->properties, (GFunc) property_info_free, NULL);
      g_list_free (oinfo->properties);

      g_list_foreach (oinfo->signals, (GFunc) signal_info_free, NULL);
      g_list_free (oinfo->signals);

      /* these are ids */
      g_list_foreach (oinfo->children, (GFunc) g_free, NULL);
      g_list_free (oinfo->children);

      g_list_foreach (oinfo->behaviours, (GFunc) g_free, NULL);
      g_list_free (oinfo->behaviours);

      /* we unref top-level objects and leave the actors alone,
       * unless we are unmerging in which case we have to destroy
       * the actor to unparent them
       */
      if (oinfo->object)
        {
          if (oinfo->is_unmerged)
            {
              if (oinfo->is_toplevel)
                g_object_unref (oinfo->object);
              else
                {
                  /* destroy every actor, unless it's the default stage */
                  if (oinfo->is_stage_default == FALSE &&
                      CLUTTER_IS_ACTOR (oinfo->object))
                    clutter_actor_destroy (CLUTTER_ACTOR (oinfo->object));
                }
            }
          else
            {
              if (oinfo->is_toplevel)
                g_object_unref (oinfo->object);
            }

          oinfo->object = NULL;
        }

      g_slice_free (ObjectInfo, oinfo);
    }
}

static void
clutter_script_finalize (GObject *gobject)
{
  ClutterScriptPrivate *priv = CLUTTER_SCRIPT_GET_PRIVATE (gobject);

  g_object_unref (priv->parser);
  g_hash_table_destroy (priv->objects);
  g_strfreev (priv->search_paths);
  g_free (priv->filename);

  G_OBJECT_CLASS (clutter_script_parent_class)->finalize (gobject);
}

static void
clutter_script_get_property (GObject    *gobject,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ClutterScript *script = CLUTTER_SCRIPT (gobject);

  switch (prop_id)
    {
    case PROP_FILENAME_SET:
      g_value_set_boolean (value, script->priv->is_filename);
      break;
    case PROP_FILENAME:
      g_value_set_string (value, script->priv->filename);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_script_class_init (ClutterScriptClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ClutterScriptPrivate));

  klass->get_type_from_name = clutter_script_real_get_type_from_name;

  gobject_class->get_property = clutter_script_get_property;
  gobject_class->finalize = clutter_script_finalize;

  /**
   * ClutterScript:filename-set:
   *
   * Whether the ClutterScript:filename property is set. If this property
   * is %TRUE then the currently parsed data comes from a file, and the
   * file name is stored inside the ClutterScript:filename property.
   *
   * Since: 0.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_FILENAME_SET,
                                   g_param_spec_boolean ("filename-set",
                                                         "Filename Set",
                                                         "Whether the :filename property is set",
                                                         FALSE,
                                                         CLUTTER_PARAM_READABLE));
  /**
   * ClutterScript:filename:
   *
   * The path of the currently parsed file. If ClutterScript:filename-set
   * is %FALSE then the value of this property is undefined.
   *
   * Since: 0.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_FILENAME,
                                   g_param_spec_string ("filename",
                                                        "Filename",
                                                        "The path of the currently parsed file",
                                                        NULL,
                                                        CLUTTER_PARAM_READABLE));
}

static void
clutter_script_init (ClutterScript *script)
{
  ClutterScriptPrivate *priv;

  script->priv = priv = CLUTTER_SCRIPT_GET_PRIVATE (script);

  priv->parser = json_parser_new ();
  g_signal_connect (priv->parser,
                    "object-end", G_CALLBACK (json_object_end),
                    script);
  g_signal_connect (priv->parser,
                    "parse-end", G_CALLBACK (json_parse_end),
                    script);

  priv->is_filename = FALSE;
  priv->last_merge_id = 0;

  priv->objects = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         NULL,
                                         object_info_free);
}

/**
 * clutter_script_new:
 *
 * Creates a new #ClutterScript instance. #ClutterScript can be used
 * to load objects definitions for scenegraph elements, like actors,
 * or behavioural elements, like behaviours and timelines. The
 * definitions must be encoded using the JavaScript Object Notation (JSON)
 * language.
 *
 * Return value: the newly created #ClutterScript instance. Use
 *   g_object_unref() when done.
 *
 * Since: 0.6
 */
ClutterScript *
clutter_script_new (void)
{
  return g_object_new (CLUTTER_TYPE_SCRIPT, NULL);
}

/**
 * clutter_script_load_from_file:
 * @script: a #ClutterScript
 * @filename: the full path to the definition file
 * @error: return location for a #GError, or %NULL
 *
 * Loads the definitions from @filename into @script and merges with
 * the currently loaded ones, if any.
 *
 * Return value: on error, zero is returned and @error is set
 *   accordingly. On success, the merge id for the UI definitions is
 *   returned. You can use the merge id with clutter_script_unmerge().
 *
 * Since: 0.6
 */
guint
clutter_script_load_from_file (ClutterScript  *script,
                               const gchar    *filename,
                               GError        **error)
{
  ClutterScriptPrivate *priv;
  GError *internal_error;

  g_return_val_if_fail (CLUTTER_IS_SCRIPT (script), 0);
  g_return_val_if_fail (filename != NULL, 0);

  priv = script->priv;

  g_free (priv->filename);
  priv->filename = g_strdup (filename);
  priv->is_filename = TRUE;
  priv->last_merge_id += 1;

  internal_error = NULL;
  json_parser_load_from_file (priv->parser, filename, &internal_error);
  if (internal_error)
    {
      g_propagate_error (error, internal_error);
      priv->last_merge_id -= 1;
      return 0;
    }

  return priv->last_merge_id;
}

/**
 * clutter_script_load_from_data:
 * @script: a #ClutterScript
 * @data: a buffer containing the definitions
 * @length: the length of the buffer, or -1 if @data is a NUL-terminated
 *   buffer
 * @error: return location for a #GError, or %NULL
 *
 * Loads the definitions from @data into @script and merges with
 * the currently loaded ones, if any.
 *
 * Return value: on error, zero is returned and @error is set
 *   accordingly. On success, the merge id for the UI definitions is
 *   returned. You can use the merge id with clutter_script_unmerge().
 *
 * Since: 0.6
 */
guint
clutter_script_load_from_data (ClutterScript  *script,
                               const gchar    *data,
                               gssize          length,
                               GError        **error)
{
  ClutterScriptPrivate *priv;
  GError *internal_error;

  g_return_val_if_fail (CLUTTER_IS_SCRIPT (script), 0);
  g_return_val_if_fail (data != NULL, 0);

  if (length < 0)
    length = strlen (data);

  priv = script->priv;

  g_free (priv->filename);
  priv->filename = NULL;
  priv->is_filename = FALSE;
  priv->last_merge_id += 1;

  internal_error = NULL;
  json_parser_load_from_data (priv->parser, data, length, &internal_error);
  if (internal_error)
    {
      g_propagate_error (error, internal_error);
      priv->last_merge_id -= 1;
      return 0;
    }

  return priv->last_merge_id;
}

/**
 * clutter_script_get_object:
 * @script: a #ClutterScript
 * @name: the name of the object to retrieve
 *
 * Retrieves the object bound to @name. This function does not increment
 * the reference count of the returned object.
 *
 * Return value: : (transfer none): the named object, or %NULL if no object
 *   with the given name was available
 *
 * Since: 0.6
 */
GObject *
clutter_script_get_object (ClutterScript *script,
                           const gchar   *name)
{
  ObjectInfo *oinfo;

  g_return_val_if_fail (CLUTTER_IS_SCRIPT (script), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  oinfo = g_hash_table_lookup (script->priv->objects, name);
  if (!oinfo)
    return NULL;

  return clutter_script_construct_object (script, oinfo);
}

static gint
clutter_script_get_objects_valist (ClutterScript *script,
                                   const gchar   *first_name,
                                   va_list        args)
{
  gint retval = 0;
  const gchar *name;

  name = first_name;
  while (name)
    {
      GObject **obj = NULL;
      
      obj = va_arg (args, GObject**);

      *obj = clutter_script_get_object (script, name);
      if (*obj)
        retval += 1;

      name = va_arg (args, gchar*);
    }

  return retval;
}

/**
 * clutter_script_get_objects:
 * @script: a #ClutterScript
 * @first_name: the name of the first object to retrieve
 * @Varargs: return location for a #GObject, then additional names, ending
 *   with %NULL
 *
 * Retrieves a list of objects for the given names. After @script, object
 * names/return location pairs should be listed, with a %NULL pointer
 * ending the list, like:
 *
 * <informalexample><programlisting>
 *   GObject *my_label, *a_button, *main_timeline;
 *
 *   clutter_script_get_objects (script,
 *                               "my-label", &amp;my_label,
 *                               "a-button", &amp;a_button,
 *                               "main-timeline", &amp;main_timeline,
 *                               NULL);
 * </programlisting></informalexample>
 *
 * Note: This function does not increment the reference count of the
 * returned objects.
 *
 * Return value: the number of objects returned.
 *
 * Since: 0.6
 */
gint
clutter_script_get_objects (ClutterScript *script,
                            const gchar   *first_name,
                            ...)
{
  gint retval;
  va_list var_args;

  g_return_val_if_fail (CLUTTER_IS_SCRIPT (script), 0);
  g_return_val_if_fail (first_name != NULL, 0);

  va_start (var_args, first_name);
  retval = clutter_script_get_objects_valist (script, first_name, var_args);
  va_end (var_args);

  return retval;
}

typedef struct {
  ClutterScript *script;
  guint merge_id;
  GSList *ids;
} UnmergeData;

static void
remove_by_merge_id (gpointer key,
                    gpointer value,
                    gpointer data)
{
  gchar *name = key;
  ObjectInfo *oinfo = value;
  UnmergeData *unmerge_data = data;

  if (oinfo->merge_id == unmerge_data->merge_id)
    {
      CLUTTER_NOTE (SCRIPT,
                    "Unmerging object (id:%s, type:%s, merge-id:%d)",
                    oinfo->id,
                    oinfo->class_name,
                    oinfo->merge_id);

      unmerge_data->ids = g_slist_prepend (unmerge_data->ids, g_strdup (name));
      oinfo->is_unmerged = TRUE;
    }
}

/**
 * clutter_script_unmerge_objects:
 * @script: a #ClutterScript
 * @merge_id: merge id returned when loading a UI definition
 *
 * Unmerges the objects identified by @merge_id.
 *
 * Since: 0.6
 */
void
clutter_script_unmerge_objects (ClutterScript *script,
                                guint          merge_id)
{
  ClutterScriptPrivate *priv;
  UnmergeData data;
  GSList *l;

  g_return_if_fail (CLUTTER_IS_SCRIPT (script));
  g_return_if_fail (merge_id > 0);

  priv = script->priv;

  data.script = script;
  data.merge_id = merge_id;
  data.ids = NULL;
  g_hash_table_foreach (priv->objects, remove_by_merge_id, &data);

  for (l = data.ids; l != NULL; l = l->next)
    g_hash_table_remove (priv->objects, l->data);

  g_slist_foreach (data.ids, (GFunc) g_free, NULL);
  g_slist_free (data.ids);

  clutter_script_ensure_objects (script);
}

/**
 * clutter_script_ensure_objects:
 * @script: a #ClutterScript
 *
 * Ensure that every object defined inside @script is correctly
 * constructed. You should rarely need to use this function.
 *
 * Since: 0.6
 */
void
clutter_script_ensure_objects (ClutterScript *script)
{
  ClutterScriptPrivate *priv;

  g_return_if_fail (CLUTTER_IS_SCRIPT (script));

  priv = script->priv;
  g_hash_table_foreach (priv->objects, construct_each_object, script);  
}

/**
 * clutter_script_get_type_from_name:
 * @script: a #ClutterScript
 * @type_name: name of the type to look up
 *
 * Looks up a type by name, using the virtual function that 
 * #ClutterScript has for that purpose. This function should
 * rarely be used.
 *
 * Return value: the type for the requested type name, or
 *   %G_TYPE_INVALID if not corresponding type was found.
 *
 * Since: 0.6
 */
GType
clutter_script_get_type_from_name (ClutterScript *script,
                                   const gchar   *type_name)
{
  g_return_val_if_fail (CLUTTER_IS_SCRIPT (script), G_TYPE_INVALID);
  g_return_val_if_fail (type_name != NULL, G_TYPE_INVALID);

  return CLUTTER_SCRIPT_GET_CLASS (script)->get_type_from_name (script, type_name);
}

/**
 * clutter_get_script_id:
 * @gobject: a #GObject
 *
 * Retrieves the Clutter script id, if any.
 *
 * Return value: the script id, or %NULL if @object was not defined inside
 *   a UI definition file. The returned string is owned by the object and
 *   should never be modified or freed.
 *
 * Since: 0.6
 */
G_CONST_RETURN gchar *
clutter_get_script_id (GObject *gobject)
{
  g_return_val_if_fail (G_IS_OBJECT (gobject), NULL);

  if (CLUTTER_IS_SCRIPTABLE (gobject))
    return clutter_scriptable_get_id (CLUTTER_SCRIPTABLE (gobject));
  else
    return g_object_get_data (gobject, "clutter-script-id");
}

typedef struct {
  GModule *module;
  gpointer data;
} ConnectData;

/* default signal connection code */
static void
clutter_script_default_connect (ClutterScript *script,
                                GObject       *gobject,
                                const gchar   *signal_name,
                                const gchar   *signal_handler,
                                GObject       *connect_gobject,
                                GConnectFlags  flags,
                                gpointer       user_data)
{
  ConnectData *data = user_data;
  GCallback function;

  if (!data->module)
    return;

  if (!g_module_symbol (data->module, signal_handler, (gpointer) &function))
    {
      g_warning ("Could not find a signal handler '%s' for signal '%s::%s'",
                 signal_handler,
                 connect_gobject ? G_OBJECT_TYPE_NAME (connect_gobject)
                                 : G_OBJECT_TYPE_NAME (gobject),
                 signal_name);
      return;
    }

  CLUTTER_NOTE (SCRIPT,
                "connecting %s::%s to %s (afetr:%s, swapped:%s, object:%s)",
                (connect_gobject ? G_OBJECT_TYPE_NAME (connect_gobject)
                                 : G_OBJECT_TYPE_NAME (gobject)),
                signal_name,
                signal_handler,
                (flags & G_CONNECT_AFTER) ? "true" : "false",
                (flags & G_CONNECT_SWAPPED) ? "true" : "false",
                (connect_gobject ? G_OBJECT_TYPE_NAME (connect_gobject)
                                 : "<none>"));

  if (connect_gobject != NULL)
    g_signal_connect_object (gobject,
                             signal_name, function,
                             connect_gobject,
                             flags);
  else
    g_signal_connect_data (gobject,
                           signal_name, function,
                           data->data,
                           NULL,
                           flags);
}

/**
 * clutter_script_connect_signals:
 * @script: a #ClutterScript
 * @user_data: data to be passed to the signal handlers, or %NULL
 *
 * Connects all the signals defined into a UI definition file to their
 * handlers.
 *
 * This method invokes clutter_script_connect_signals_full() internally
 * and uses  #GModule's introspective features (by opening the current
 * module's scope) to look at the application's symbol table.
 * 
 * Note that this function will not work if #GModule is not supported by
 * the platform Clutter is running on.
 *
 * Since: 0.6
 */
void
clutter_script_connect_signals (ClutterScript *script,
                                gpointer       user_data)
{
  ConnectData *cd;

  g_return_if_fail (CLUTTER_IS_SCRIPT (script));

  if (!g_module_supported ())
    {
      g_critical ("clutter_script_connect_signals() requires a working "
                  "GModule support from GLib");
      return;
    }

  cd = g_new (ConnectData, 1);
  cd->module = g_module_open (NULL, G_MODULE_BIND_LAZY);
  cd->data = user_data;

  clutter_script_connect_signals_full (script,
                                       clutter_script_default_connect,
                                       cd);

  g_module_close (cd->module);

  g_free (cd);
}

typedef struct {
  ClutterScript *script;
  ClutterScriptConnectFunc func;
  gpointer user_data;
} SignalConnectData;

static void
connect_each_object (gpointer key,
                     gpointer value,
                     gpointer data)
{
  SignalConnectData *connect_data = data;
  ClutterScript *script = connect_data->script;
  ObjectInfo *oinfo = value;
  GObject *object = oinfo->object;
  GList *unresolved, *l;

  if (G_UNLIKELY (!oinfo->object))
    oinfo->object = clutter_script_construct_object (script, oinfo);

  unresolved = NULL;
  for (l = oinfo->signals; l != NULL; l = l->next)
    {
      SignalInfo *sinfo = l->data;
      GObject *connect_object = NULL;

      if (sinfo->object)
        connect_object = clutter_script_get_object (script, sinfo->object);

      if (sinfo->object && !connect_object)
        unresolved = g_list_prepend (unresolved, sinfo);
      else
        {
          connect_data->func (script, object,
                              sinfo->name,
                              sinfo->handler,
                              connect_object,
                              sinfo->flags,
                              connect_data->user_data);

          signal_info_free (sinfo);
        }
    }

  /* keep the unresolved signal handlers around, in case
   * clutter_script_connect_signals() is called multiple
   * times (e.g. after a UI definition merge)
   */
  g_list_free (oinfo->signals);
  oinfo->signals = unresolved;
}

/**
 * clutter_script_connect_signals_full:
 * @script: a #ClutterScript
 * @func: signal connection function
 * @user_data: data to be passed to the signal handlers, or %NULL
 *
 * Connects all the signals defined into a UI definition file to their
 * handlers.
 *
 * This function allows to control how the signal handlers are
 * going to be connected to their respective signals. It is meant
 * primarily for language bindings to allow resolving the function
 * names using the native API.
 *
 * Applications should use clutter_script_connect_signals().
 *
 * Since: 0.6
 */
void
clutter_script_connect_signals_full (ClutterScript            *script,
                                     ClutterScriptConnectFunc  func,
                                     gpointer                  user_data)
{
  SignalConnectData data;

  g_return_if_fail (CLUTTER_IS_SCRIPT (script));
  g_return_if_fail (func != NULL);

  data.script = script;
  data.func = func;
  data.user_data = user_data;

  g_hash_table_foreach (script->priv->objects, connect_each_object, &data);
}

GQuark
clutter_script_error_quark (void)
{
  return g_quark_from_static_string ("clutter-script-error");
}

/**
 * clutter_script_add_search_paths:
 * @script: a #ClutterScript
 * @paths: an array of strings containing different search paths
 * @n_paths: the length of the passed array
 *
 * Adds @paths to the list of search paths held by @script.
 *
 * The search paths are used by clutter_script_lookup_filename(), which
 * can be used to define search paths for the textures source file name
 * or other custom, file-based properties.
 *
 * Since: 0.8
 */
void
clutter_script_add_search_paths (ClutterScript       *script,
                                 const gchar * const  paths[],
                                 gsize                n_paths)
{
  ClutterScriptPrivate *priv;
  gchar **old_paths, **new_paths;
  gsize old_paths_len, i;
  gsize iter = 0;

  g_return_if_fail (CLUTTER_IS_SCRIPT (script));
  g_return_if_fail (paths != NULL);
  g_return_if_fail (n_paths > 0);

  priv = script->priv;

  if (priv->search_paths)
    {
      old_paths     = priv->search_paths;
      old_paths_len = g_strv_length (old_paths);
    }
  else
    {
      old_paths     = NULL;
      old_paths_len = 0;
    }

  new_paths = g_new0 (gchar*, old_paths_len + n_paths + 1);

  for (i = 0, iter = 0; i < old_paths_len; i++, iter++)
    new_paths[iter] = g_strdup (old_paths[i]);

  for (i = 0; i < n_paths; i++, iter++)
    new_paths[iter] = g_strdup (paths[i]);

  CLUTTER_NOTE (SCRIPT,
                "Added %" G_GSIZE_FORMAT " new search paths (new size: %d)",
                n_paths,
                g_strv_length (new_paths));

  priv->search_paths = new_paths;

  if (old_paths)
    g_strfreev (old_paths);
}

/**
 * clutter_script_lookup_filename:
 * @script: a #ClutterScript
 * @filename: the name of the file to lookup
 *
 * Looks up @filename inside the search paths of @script. If @filename
 * is found, its full path will be returned .
 *
 * Return value: the full path of @filename or %NULL if no path was
 *   found.
 *
 * Since: 0.8
 */
gchar *
clutter_script_lookup_filename (ClutterScript *script,
                                const gchar   *filename)
{
  ClutterScriptPrivate *priv;
  gchar *dirname;
  gchar *retval;

  g_return_val_if_fail (CLUTTER_IS_SCRIPT (script), FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  if (g_path_is_absolute (filename))
    return g_strdup (filename);

  priv = script->priv;

  if (priv->search_paths)
    {
      gsize paths_len, i;

      paths_len = g_strv_length (priv->search_paths);
      for (i = 0; i < paths_len; i++)
        {
          retval = g_build_filename (priv->search_paths[i], filename, NULL);
          if (g_file_test (retval, G_FILE_TEST_EXISTS))
            return retval;
          else
            {
              g_free (retval);
              retval = NULL;
            }
        }
    }

  /* Fall back to assuming relative to our script */
  if (priv->is_filename)
    dirname = g_path_get_dirname (script->priv->filename);
  else
    dirname = g_get_current_dir ();
  
  retval = g_build_filename (dirname, filename, NULL);
  if (!g_file_test (retval, G_FILE_TEST_EXISTS))
    {
      g_free (retval);
      retval = NULL;
    }
  
  g_free (dirname);

  return retval;
}

/**
 * clutter_script_list_objects:
 * @script: a #ClutterScript
 *
 * Retrieves all the objects created by @script.
 *
 * Note: this function does not increment the reference count of the
 * objects it returns.
 *
 * Return value: (transfer container) (element-type GObject): a list of #GObject<!-- -->s,
 *   or %NULL. The objects are owned by the #ClutterScript instance. Use g_list_free() on the
 *   returned value when done.
 *
 * Since: 0.8.2
 */
GList *
clutter_script_list_objects (ClutterScript *script)
{
  GList *objects, *l;
  GList *retval;

  g_return_val_if_fail (CLUTTER_IS_SCRIPT (script), NULL);

  clutter_script_ensure_objects (script);
  if (!script->priv->objects)
    return NULL;

  retval = NULL;
  objects = g_hash_table_get_values (script->priv->objects);
  for (l = objects; l != NULL; l = l->next)
    {
      ObjectInfo *oinfo = l->data;

      if (oinfo->object)
        retval = g_list_prepend (retval, oinfo->object);
    }

  g_list_free (objects);

  return retval;
}
