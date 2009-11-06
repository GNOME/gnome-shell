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
 *     "timeline" : { "duration" : 4000, "loop" : true },
 *     "mode"     : "easeInSine"
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
 * implicitly like in the example above, or explicitly by setting the
 * "alpha" property to point to a previously defined #ClutterAlpha, e.g.:
 *
 * |[
 * {
 *   "id"          : "rotate-behaviour",
 *   "type"        : "ClutterBehaviourRotate",
 *   "angle-start" : 0.0,
 *   "angle-end"   : 360.0,
 *   "axis"        : "z-axis",
 *   "alpha"       : {
 *     "id"       : "rotate-alpha",
 *     "type"     : "ClutterAlpha",
 *     "timeline" : {
 *       "id"       : "rotate-timeline",
 *       "type      : "ClutterTimeline",
 *       "duration" : 4000,
 *       "loop"     : true
 *     },
 *     "function" : "custom_sine_alpha"
 *   }
 * }
 * ]|
 *
 * Implicitely defined #ClutterAlpha<!-- -->s and #ClutterTimeline<!-- -->s
 * can omit the <varname>id</varname> member, as well as the
 * <varname>type</varname> member, but will not be available using
 * clutter_script_get_object() (they can, however, be extracted using the
 * #ClutterBehaviour and #ClutterAlpha API respectively).
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

  ClutterScriptParser *parser;

  gchar **search_paths;

  gchar *filename;
  guint is_filename : 1;
};

G_DEFINE_TYPE (ClutterScript, clutter_script, G_TYPE_OBJECT);

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

      if (pinfo->node)
        json_node_free (pinfo->node);

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

  priv->parser = g_object_new (CLUTTER_TYPE_SCRIPT_PARSER, NULL);
  priv->parser->script = script;

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
  json_parser_load_from_file (JSON_PARSER (priv->parser),
                              filename,
                              &internal_error);
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
  json_parser_load_from_data (JSON_PARSER (priv->parser),
                                           data, length,
                                           &internal_error);
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

  _clutter_script_construct_object (script, oinfo);
  _clutter_script_apply_properties (script, oinfo);

  return oinfo->object;
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

static void
construct_each_objects (gpointer key,
                        gpointer value,
                        gpointer user_data)
{
  ClutterScript *script = user_data;
  ObjectInfo *oinfo = value;

  /* we have unfinished business */
  if (oinfo->has_unresolved)
    {
      /* this should not happen, but resilence is
       * a good thing in a parser
       */
      if (oinfo->object == NULL)
        _clutter_script_construct_object (script, oinfo);

      /* this will take care of setting up properties,
       * adding children and applying behaviours
       */
      _clutter_script_apply_properties (script, oinfo);
    }
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
  g_hash_table_foreach (priv->objects, construct_each_objects, script);
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
  cd->module = g_module_open (NULL, 0);
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

  _clutter_script_construct_object (script, oinfo);

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

/*
 * _clutter_script_generate_fake_id:
 * @script: a #ClutterScript
 *
 * Generates a fake id string for object definitions without
 * an "id" member
 *
 * Return value: a newly-allocated string containing the fake
 *   id. Use g_free() to free the resources allocated by the
 *   returned value
 *
 */
gchar *
_clutter_script_generate_fake_id (ClutterScript *script)
{
  ClutterScriptPrivate *priv = script->priv;

  return g_strdup_printf ("script-%d-%d",
                          priv->last_merge_id,
                          priv->last_unknown++);
}

/*
 * _clutter_script_warn_missing_attribute:
 * @script: a #ClutterScript
 * @id: the id of an object definition, or %NULL
 * @attribute: the expected attribute
 *
 * Emits a warning, using GLib's log facilities, for a missing
 * @attribute in an object definition, pointing to the current
 * location of the #ClutterScriptParser
 */
void
_clutter_script_warn_missing_attribute (ClutterScript *script,
                                        const gchar   *id,
                                        const gchar   *attribute)
{
  ClutterScriptPrivate *priv = script->priv;
  JsonParser *parser = JSON_PARSER (priv->parser);
  gint current_line = json_parser_get_current_line (parser);

  if (id != NULL && *id != '\0')
    {
      g_warning ("%s:%d: object '%s' has no '%s' attribute",
                 priv->is_filename ? priv->filename : "<input>",
                 current_line,
                 id,
                 attribute);
    }
  else
    {
      g_warning ("%s:%d: object has no '%s' attribute",
                 priv->is_filename ? priv->filename : "<input>",
                 current_line,
                 attribute);
    }
}

/*
 * _clutter_script_warn_invalid_value:
 * @script: a #ClutterScript
 * @attribute: the attribute with the invalid value
 * @expected: a string with the expected value
 * @node: a #JsonNode containing the value
 *
 * Emits a warning, using GLib's log facilities, for an invalid
 * value found when parsing @attribute, pointing to the current
 * location of the #ClutterScriptParser
 */
void
_clutter_script_warn_invalid_value (ClutterScript *script,
                                    const gchar   *attribute,
                                    const gchar   *expected,
                                    JsonNode      *node)
{
  ClutterScriptPrivate *priv = script->priv;
  JsonParser *parser = JSON_PARSER (priv->parser);
  gint current_line = json_parser_get_current_line (parser);

  if (node != NULL)
    {
      g_warning ("%s:%d: invalid value of type '%s' for attribute '%s':"
                 "a value of type '%s' is expected",
                 priv->is_filename ? priv->filename : "<input>",
                 current_line,
                 json_node_type_name (node),
                 attribute,
                 expected);
    }
  else
    {
      g_warning ("%s:%d: invalid value for attribute '%s':"
                 "a value of type '%s' is expected",
                 priv->is_filename ? priv->filename : "<input>",
                 current_line,
                 attribute,
                 expected);
    }
}

/*
 * _clutter_script_get_object_info:
 * @script: a #ClutterScript
 * @script_id: the id of the object definition
 *
 * Retrieves the #ObjectInfo for the given @script_id
 *
 * Return value: a #ObjectInfo or %NULL
 */
ObjectInfo *
_clutter_script_get_object_info (ClutterScript *script,
                                 const gchar   *script_id)
{
  ClutterScriptPrivate *priv = script->priv;

  return g_hash_table_lookup (priv->objects, script_id);
}

/*
 * _clutter_script_get_last_merge_id:
 * @script: a #ClutterScript
 *
 * Retrieves the last merge id of @script. The merge id
 * should be stored inside an #ObjectInfo. If you need
 * a unique fake id for object definitions with an "id"
 * member, consider using _clutter_script_generate_fake_id()
 * instead
 *
 * Return value: the last merge id
 */
guint
_clutter_script_get_last_merge_id (ClutterScript *script)
{
  return script->priv->last_merge_id;
}

/*
 * _clutter_script_add_object_info:
 * @script: a #ClutterScript
 * @oinfo: a #ObjectInfo
 *
 * Adds @oinfo inside the objects list held by @script
 */
void
_clutter_script_add_object_info (ClutterScript *script,
                                 ObjectInfo    *oinfo)
{
  ClutterScriptPrivate *priv = script->priv;

  g_hash_table_steal (priv->objects, oinfo->id);
  g_hash_table_insert (priv->objects, oinfo->id, oinfo);
}
