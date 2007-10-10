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
 * A simple object might be defined as:
 *
 * <programlisting>
 * {
 *   "id"     : "red-button",
 *   "type"   : "ClutterRectangle",
 *   "width"  : 100,
 *   "height" : 100,
 *   "color"  : "0xff0000ff"
 * }
 * </programlisting>
 *
 * This will produce a red #ClutterRectangle, 100x100 pixels wide, and
 * with a name of "red-button"; it can be retrieved by calling:
 *
 * <programlisting>
 * ClutterActor *red_button;
 *
 * red_button = CLUTTER_ACTOR (clutter_script_get_object (script, "red-button"));
 * </programlisting>
 *
 * and then manipulated with the Clutter API.
 *
 * Packing can be represented using the "children" member, and passing an
 * array of objects or ids of objects already defined (but not packed: the
 * packing rules of Clutter still apply).
 *
 * Behaviours and timelines can also be defined inside a UI definition
 * buffer:
 *
 * <programlisting>
 * {
 *   "id"          : "rotate-behaviour",
 *   "type"        : "ClutterBehaviourRotate",
 *   "angle-begin" : 0.0,
 *   "angle-end"   : 360.0,
 *   "axis"        : "z-axis",
 *   "alpha"       : {
 *     "timeline" : { "num-frames" : 240, "fps" : 60, "loop" : true },
 *     "function" : "sine"
 *   }
 * }
 * </programlisting>
 *
 * And then to apply a defined behaviour to an actor defined inside the
 * definition of an actor, the "behaviour" member can be used:
 *
 * <programlisting>
 * {
 *   "id" : "my-rotating-actor",
 *   "type" : "ClutterTexture",
 *   ...
 *   "behaviours" : [ "rotate-behaviour" ]
 * }
 * </programlisting>
 *
 * #ClutterScript is available since Clutter 0.6
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <glib.h>
#include <glib-object.h>

#include "clutter-actor.h"
#include "clutter-alpha.h"
#include "clutter-behaviour.h"
#include "clutter-container.h"
#include "clutter-stage.h"

#include "clutter-script.h"
#include "clutter-script-private.h"

#include "clutter-private.h"
#include "clutter-debug.h"

#include "json/json-parser.h"

#define CLUTTER_SCRIPT_GET_PRIVATE(obj) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_SCRIPT, ClutterScriptPrivate))

struct _ClutterScriptPrivate
{
  GHashTable *objects;

  guint last_merge_id;

  JsonParser *parser;
  ObjectInfo *current;

  gchar *filename;
  guint is_filename : 1;
};

G_DEFINE_TYPE (ClutterScript, clutter_script, G_TYPE_OBJECT);

static void object_info_free (gpointer data);

static GType
resolve_type (const gchar *symbol)
{
  static GModule *module = NULL;
  GTypeGetFunc func;
  GType gtype = G_TYPE_INVALID;

  if (!module)
    module = g_module_open (NULL, 0);
  
  if (g_module_symbol (module, symbol, (gpointer)&func))
    gtype = func ();
  
  return gtype;
}

/* tries to map a name in camel case into the corresponding get_type()
 * function, e.g.:
 *
 *   ClutterRectangle -> clutter_rectangle_get_type
 *   ClutterCloneTexture -> clutter_clone_texture_get_type
 *
 * taken from GTK+, gtkbuilder.c
 */
static GType
resolve_type_lazily (const gchar *name)
{
  static GModule *module = NULL;
  GTypeGetFunc func;
  GString *symbol_name = g_string_new ("");
  char c, *symbol;
  int i;
  GType gtype = G_TYPE_INVALID;

  if (!module)
    module = g_module_open (NULL, 0);
  
  for (i = 0; name[i] != '\0'; i++)
    {
      c = name[i];
      /* skip if uppercase, first or previous is uppercase */
      if ((c == g_ascii_toupper (c) &&
           i > 0 && name[i-1] != g_ascii_toupper (name[i-1])) ||
          (i > 2 && name[i]   == g_ascii_toupper (name[i]) &&
           name[i-1] == g_ascii_toupper (name[i-1]) &&
           name[i-2] == g_ascii_toupper (name[i-2])))
        g_string_append_c (symbol_name, '_');
      g_string_append_c (symbol_name, g_ascii_tolower (c));
    }
  g_string_append (symbol_name, "_get_type");
  
  symbol = g_string_free (symbol_name, FALSE);

  if (g_module_symbol (module, symbol, (gpointer)&func))
    gtype = func ();
  
  g_free (symbol);

  return gtype;
}

static ClutterAlphaFunc
resolve_alpha_func (const gchar *name)
{
  static GModule *module = NULL;
  ClutterAlphaFunc func;
  GString *symbol_name = g_string_new ("");
  char c, *symbol;
  int i;

  if (!module)
    module = g_module_open (NULL, 0);
  
  CLUTTER_NOTE (SCRIPT, "Looking for `%s' alpha function", name);
  
  if (g_module_symbol (module, name, (gpointer) &func))
    return func;

  g_string_append (symbol_name, "clutter_");

  for (i = 0; name[i] != '\0'; i++)
    {
      c = name[i];
      
      if (name[i] == '-')
        g_string_append_c (symbol_name, '_');
      else
        g_string_append_c (symbol_name, g_ascii_tolower (name[i]));
    }
  g_string_append (symbol_name, "_func");
  
  symbol = g_string_free (symbol_name, FALSE);

  CLUTTER_NOTE (SCRIPT, "Looking for `%s' alpha function", symbol);

  if (!g_module_symbol (module, symbol, (gpointer)&func))
    func = NULL;

  g_free (symbol);

  return func;
}

static void
warn_missing_attribute (ClutterScript *script,
                        const gchar   *id,
                        const gchar   *attribute)
{
  ClutterScriptPrivate *priv = script->priv;

  if (G_LIKELY (id))
    {
      g_warning ("%s: %d: object `%s' has no `%s' attribute",
                 priv->is_filename ? priv->filename : "<input>",
                 json_parser_get_current_line (priv->parser),
                 id,
                 attribute);
    }
  else
    {
      g_warning ("%s: %d: object has no `%s' attribute",
                 priv->is_filename ? priv->filename : "<input>",
                 json_parser_get_current_line (priv->parser),
                 attribute);
    }
}

static void
warn_invalid_value (ClutterScript *script,
                    const gchar   *attribute,
                    JsonNode      *node)
{
  ClutterScriptPrivate *priv = script->priv;

  if (G_LIKELY (node))
    {
      g_warning ("%s: %d: invalid value of type `%s' for attribute `%s'",
                 priv->is_filename ? priv->filename : "<input>",
                 json_parser_get_current_line (priv->parser),
                 json_node_type_name (node),
                 attribute);
    }
  else
    {
      g_warning ("%s: %d: invalid value for attribute `%s'",
                 priv->is_filename ? priv->filename : "<input>",
                 json_parser_get_current_line (priv->parser),
                 attribute);
    }
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

  members = json_object_get_members (object);
  for (l = members; l != NULL; l = l->next)
    {
      const gchar *name = l->data;
      JsonNode *node = json_object_get_member (object, name);

      if (JSON_NODE_TYPE (node) == JSON_NODE_VALUE)
        {
          PropertyInfo *pinfo = g_slice_new (PropertyInfo);
          GValue value = { 0, };

          pinfo->property_name = g_strdup (name);

          json_node_get_value (node, &value);
          g_value_init (&pinfo->value, G_VALUE_TYPE (&value));
          g_value_transform (&value, &pinfo->value);
          g_value_unset (&value);

          oinfo->properties = g_list_prepend (oinfo->properties, pinfo);
        }
    }
  g_list_free (members);
  
  retval = CLUTTER_TIMELINE (clutter_script_construct_object (script, oinfo));

  /* it needs to survive */
  g_object_ref (retval);
  object_info_free (oinfo);

  return retval;
}

static PropertyInfo *
parse_member_to_property (ClutterScript *script,
                          ObjectInfo    *info,
                          const gchar   *name,
                          JsonNode      *node)
{
  PropertyInfo *retval = NULL;
  GValue value = { 0, };

  switch (JSON_NODE_TYPE (node))
    {
    case JSON_NODE_VALUE:
      retval = g_slice_new (PropertyInfo);
      retval->property_name = g_strdup (name);

      json_node_get_value (node, &value);
      g_value_init (&retval->value, G_VALUE_TYPE (&value));
      g_value_transform (&value, &retval->value);
      g_value_unset (&value);
      break;

    case JSON_NODE_OBJECT:
      if (strcmp (name, "alpha") == 0)
        {
          JsonObject *object = json_node_get_object (node);
          ClutterAlpha *alpha = NULL;
          ClutterTimeline *timeline = NULL;
          ClutterAlphaFunc func = NULL;
          JsonNode *val;

          retval = g_slice_new (PropertyInfo);
          retval->property_name = g_strdup (name);

          alpha = clutter_alpha_new ();
          
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
                timeline = construct_timeline (script, json_node_get_object (val));
            }

          val = json_object_get_member (object, "function");
          if (val && json_node_get_string (val) != NULL)
            func = resolve_alpha_func (json_node_get_string (val));

          alpha = g_object_new (CLUTTER_TYPE_ALPHA,
                                "timeline", timeline,
                                NULL);
          clutter_alpha_set_func (alpha, func, NULL, NULL);

          g_value_init (&retval->value, CLUTTER_TYPE_ALPHA);
          g_value_set_object (&retval->value, G_OBJECT (alpha));
        }
      break;

    case JSON_NODE_ARRAY:
      if (strcmp (name, "margin") == 0)
        {
          JsonArray *array = json_node_get_array (node);
          JsonNode *val;
          gint i;
          ClutterMargin margin = { 0, };

          /* this is quite evil indeed */
          for (i = 0; i < json_array_get_length (array); i++)
            {
              val = json_array_get_element (array, i);
              switch (i)
                {
                case 0:
                  margin.top = CLUTTER_UNITS_FROM_INT (json_node_get_int (val));
                  break;
                case 1:
                  margin.right = CLUTTER_UNITS_FROM_INT (json_node_get_int (val));
                  break;
                case 2:
                  margin.bottom = CLUTTER_UNITS_FROM_INT (json_node_get_int (val));
                  break;
                case 3:
                  margin.left = CLUTTER_UNITS_FROM_INT (json_node_get_int (val));
                  break;
                }
            }

          retval = g_slice_new (PropertyInfo);
          retval->property_name = g_strdup (name);
          g_value_init (&retval->value, CLUTTER_TYPE_MARGIN);
          g_value_set_boxed (&retval->value, &margin);
        }
      else if (strcmp (name, "padding") == 0)
        {
          JsonArray *array = json_node_get_array (node);
          JsonNode *val;
          gint i;
          ClutterPadding padding = { 0, };

          /* this is quite evil indeed */
          for (i = 0; i < json_array_get_length (array); i++)
            {
              val = json_array_get_element (array, i);
              switch (i)
                {
                case 0:
                  padding.top = CLUTTER_UNITS_FROM_INT (json_node_get_int (val));
                  break;
                case 1:
                  padding.right = CLUTTER_UNITS_FROM_INT (json_node_get_int (val));
                  break;
                case 2:
                  padding.bottom = CLUTTER_UNITS_FROM_INT (json_node_get_int (val));
                  break;
                case 3:
                  padding.left = CLUTTER_UNITS_FROM_INT (json_node_get_int (val));
                  break;
                }
            }

          retval = g_slice_new (PropertyInfo);
          retval->property_name = g_strdup (name);
          g_value_init (&retval->value, CLUTTER_TYPE_PADDING);
          g_value_set_boxed (&retval->value, &padding);
        }
      else if (strcmp (name, "clip") == 0)
        {
          JsonArray *array = json_node_get_array (node);
          JsonNode *val;
          gint i;
          ClutterGeometry geom = { 0, };

          /* this is quite evil indeed */
          for (i = 0; i < json_array_get_length (array); i++)
            {
              val = json_array_get_element (array, i);
              switch (i)
                {
                case 0: geom.x      = json_node_get_int (val); break;
                case 1: geom.y      = json_node_get_int (val); break;
                case 2: geom.width  = json_node_get_int (val); break;
                case 3: geom.height = json_node_get_int (val); break;
                }
            }

          retval = g_slice_new (PropertyInfo);
          retval->property_name = g_strdup (name);
          g_value_init (&retval->value, CLUTTER_TYPE_GEOMETRY);
          g_value_set_boxed (&retval->value, &geom);
        }
      else if ((strcmp (name, "children") == 0) ||
               (strcmp (name, "behaviours") == 0))
        {
          JsonArray *array = json_node_get_array (node);
          JsonNode *val;
          gint i, array_len;
          GList *children;

          children = NULL;
          array_len = json_array_get_length (array);
          for (i = 0; i < array_len; i++)
            {
              JsonObject *object;

              val = json_array_get_element (array, i);
              
              switch (JSON_NODE_TYPE (val))
                {
                case JSON_NODE_OBJECT:
                  object = json_node_get_object (val);

                  if (json_object_has_member (object, "id") &&
                      json_object_has_member (object, "type"))
                    {
                      JsonNode *id = json_object_get_member (object, "id");

                      children = g_list_prepend (children,
                                                 json_node_dup_string (id));
                    }
                  break;
                
                case JSON_NODE_VALUE:
                  if (json_node_get_string (val))
                    children = g_list_prepend (children,
                                               json_node_dup_string (val));
                  break;

                default:
                  warn_invalid_value (script, name, val);
                  break;
                }
            }
          
          if (name[0] == 'c') /* children */
            info->children = children;
          else
            info->behaviours = children;
        }
      break;

    case JSON_NODE_NULL:
      break;
    }

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
  GList *members, *l;

  /* ignore any non-named object */
  if (!json_object_has_member (object, "id"))
    return;

  if (!json_object_has_member (object, "type"))
    {
      val = json_object_get_member (object, "id");

      warn_missing_attribute (script, json_node_get_string (val), "type");
      return;
    }

  oinfo = g_slice_new0 (ObjectInfo);
  
  val = json_object_get_member (object, "id");
  oinfo->id = json_node_dup_string (val);

  val = json_object_get_member (object, "type");
  oinfo->class_name = json_node_dup_string (val);

  if (json_object_has_member (object, "type_func"))
    {
      val = json_object_get_member (object, "type_func");
      oinfo->type_func = json_node_dup_string (val);
    }

  oinfo->is_toplevel = FALSE;

  members = json_object_get_members (object);
  for (l = members; l; l = l->next)
    {
      const gchar *name = l->data;

      val = json_object_get_member (object, name);

      if (strcmp (name, "id") == 0 ||
          strcmp (name, "type_func") == 0 ||
          strcmp (name, "type") == 0)
        continue;
      else
        {
          PropertyInfo *pinfo;

          pinfo = parse_member_to_property (script, oinfo, name, val);
          if (!pinfo)
            continue;

          oinfo->properties = g_list_prepend (oinfo->properties, pinfo);

          CLUTTER_NOTE (SCRIPT, "Added property `%s' (type:%s) for class `%s'",
                        pinfo->property_name,
                        g_type_name (G_VALUE_TYPE (&pinfo->value)),
                        oinfo->class_name);
        }
    }
  g_list_free (members);

  CLUTTER_NOTE (SCRIPT, "Added object `%s' (type:%s) with %d properties",
                oinfo->id,
                oinfo->class_name,
                g_list_length (oinfo->properties));

  g_hash_table_replace (priv->objects, g_strdup (oinfo->id), oinfo);
}

/* the property translation sequence is split in two: the first
 * half is done in parse_member_to_property(), which translates
 * from JSON data types into valid property types. the second
 * half is done here, where property types are translated into
 * the correct type for the given property GType
 */
static gboolean
translate_property (ClutterScript *script,
                    GType          gtype,
                    const gchar   *name,
                    const GValue  *src,
                    GValue        *dest)
{
  gboolean retval = FALSE;

  /* colors have a parse function, so we can pass it the
   * string we get from the parser
   */
  if (strcmp (name, "color") == 0)
    {
      ClutterColor color = { 0, };
      const gchar *color_str;

      if (G_VALUE_HOLDS (src, G_TYPE_STRING))
        {
          color_str = g_value_get_string (src);
          clutter_color_parse (color_str, &color);
        }

      g_value_init (dest, CLUTTER_TYPE_COLOR);
      g_value_set_boxed (dest, &color);

      return TRUE;
    }

  /* pixbufs are specified using the path to the file name; the
   * path can be absolute or relative to the current directory.
   * we need to load the pixbuf from the file and print a
   * warning here in case it didn't work.
   */
  if (strcmp (name, "pixbuf") == 0)
    {
      GdkPixbuf *pixbuf = NULL;
      const gchar *string;
      gchar *path;
      GError *error;

      if (G_VALUE_HOLDS (src, G_TYPE_STRING))
        string = g_value_get_string (src);
      else
        return FALSE;

      if (g_path_is_absolute (string))
        path = g_strdup (string);
      else
        {
          if (script->priv->is_filename)
            {
              gchar *dirname;

              dirname = g_path_get_dirname (script->priv->filename);
              path = g_build_filename (dirname, string, NULL);
              
              g_free (dirname);
            }
          else
            {
              gchar *dirname;
              
              dirname = g_get_current_dir ();
              path = g_build_filename (dirname, string, NULL);

              g_free (dirname);
            }
        }

      error = NULL;
      pixbuf = gdk_pixbuf_new_from_file (path, &error);
      if (error)
        {
          g_warning ("Unable to open pixbuf at path `%s': %s",
                     path,
                     error->message);
          g_error_free (error);
          g_free (path);
          return FALSE;
        }

      CLUTTER_NOTE (SCRIPT, "Setting pixbuf [%p] from file `%s'",
                    pixbuf,
                    path);
      
      g_free (path);
      
      g_value_init (dest, GDK_TYPE_PIXBUF);
      g_value_set_object (dest, pixbuf);

      return TRUE;
    }

  CLUTTER_NOTE (SCRIPT, "Copying property `%s' to type `%s'",
                name,
                g_type_name (gtype));

  /* fall back scenario */
  g_value_init (dest, gtype);

  switch (G_TYPE_FUNDAMENTAL (gtype))
    {
    case G_TYPE_UINT:
      g_value_set_uint (dest, (guint) g_value_get_int (src));
      retval = TRUE;
      break;
    case G_TYPE_UCHAR:
      g_value_set_uchar (dest, (guchar) g_value_get_int (src));
      retval = TRUE;
      break;
    case G_TYPE_ENUM:
      /* enumeration values can be expressed using the nick field
       * of GEnumValue or the actual integer value
       */
      if (G_VALUE_HOLDS (src, G_TYPE_STRING))
        {
          const gchar *string = g_value_get_string (src);
          gint enum_value;

          if (clutter_script_enum_from_string (gtype, string, &enum_value))
            {
              g_value_set_enum (dest, enum_value);
              retval = TRUE;
            }
        }
      else if (G_VALUE_HOLDS (src, G_TYPE_INT))
        {
          g_value_set_enum (dest, g_value_get_int (src));
          retval = TRUE;
        }
      break;
    case G_TYPE_FLAGS:
      break;
    default:
      g_value_copy (src, dest);
      retval = TRUE;
      break;
    }

  return retval;
}

/* translates the PropertyInfo structure into a GParameter array to
 * be fed to g_object_newv()
 */
static void
translate_properties (ClutterScript  *script,
                      ObjectInfo     *oinfo,
                      guint          *n_params,
                      GParameter    **params)
{
  GList *l;
  GParamSpec *pspec;
  GObjectClass *oclass;
  GArray *parameters;

  oclass = g_type_class_ref (oinfo->gtype);
  g_assert (oclass != NULL);

  parameters = g_array_new (FALSE, FALSE, sizeof (GParameter));

  for (l = oinfo->properties; l; l = l->next)
    {
      PropertyInfo *pinfo = l->data;
      GParameter param = { NULL };

      pspec = g_object_class_find_property (oclass, pinfo->property_name);
      if (!pspec)
        {
          g_warning ("Unknown property `%s' for class `%s'",
                     pinfo->property_name,
                     g_type_name (oinfo->gtype));
          continue;
        }

      param.name = pinfo->property_name;
      if (!translate_property (script, G_PARAM_SPEC_VALUE_TYPE (pspec),
                               param.name,
                               &pinfo->value,
                               &param.value))
        {
          g_warning ("Unable to set property `%s' for class `%s'",
                     pinfo->property_name,
                     g_type_name (oinfo->gtype));
          continue;
        }

      g_array_append_val (parameters, param);
    }

  *n_params = parameters->len;
  *params = (GParameter *) g_array_free (parameters, FALSE);

  g_type_class_unref (oclass);
}

static void
apply_behaviours (ClutterScript *script,
                  ClutterActor  *actor,
                  GList         *behaviours)
{
  GObject *object;
  GList *l;

  for (l = behaviours; l != NULL; l = l->next)
    {
      const gchar *name = l->data;

      object = clutter_script_get_object (script, name);
      if (!object)
        {
          ObjectInfo *oinfo;

          oinfo = g_hash_table_lookup (script->priv->objects, name);
          if (oinfo)
            object = clutter_script_construct_object (script, oinfo);
          else
            continue;
        }

      CLUTTER_NOTE (SCRIPT, "Applying behaviour `%s' to actor of type `%s'",
                    name,
                    g_type_name (G_OBJECT_TYPE (actor)));

      clutter_behaviour_apply (CLUTTER_BEHAVIOUR (object), actor);
    }
}

static void
add_children (ClutterScript    *script,
              ClutterContainer *container,
              GList            *children)
{
  GObject *object;
  GList *l;

  for (l = children; l != NULL; l = l->next)
    {
      const gchar *name = l->data;

      object = clutter_script_get_object (script, name);
      if (!object)
        {
          ObjectInfo *oinfo;

          oinfo = g_hash_table_lookup (script->priv->objects, name);
          if (oinfo)
            object = clutter_script_construct_object (script, oinfo);
          else
            continue;
        }

      CLUTTER_NOTE (SCRIPT, "Adding children `%s' to actor of type `%s'",
                    name,
                    g_type_name (G_OBJECT_TYPE (container)));

      clutter_container_add_actor (container, CLUTTER_ACTOR (object));
    }
}

static GObject *
construct_stage (ClutterScript *script,
                 ObjectInfo    *oinfo)
{
  GObjectClass *oclass = g_type_class_ref (CLUTTER_TYPE_STAGE);
  GList *l;

  if (oinfo->object)
    return oinfo->object;

  oinfo->object = G_OBJECT (clutter_stage_get_default ());

  for (l = oinfo->properties; l; l = l->next)
    {
      PropertyInfo *pinfo = l->data;
      const gchar *name = pinfo->property_name;
      GParamSpec *pspec;
      GValue value = { 0, };

      pspec = g_object_class_find_property (oclass, name);
      if (!pspec)
        {
          g_warning ("Unknown property `%s' for class `ClutterStage'",
                     name);
          continue;
        }

      if (!translate_property (script, G_PARAM_SPEC_VALUE_TYPE (pspec),
                               name,
                               &pinfo->value,
                               &value))
        {
          g_warning ("Unable to set property `%s' for class `%s'",
                     pinfo->property_name,
                     g_type_name (oinfo->gtype));
          continue;
        }

      g_object_set_property (oinfo->object, name, &value);

      g_value_unset (&value);
    }

  g_type_class_unref (oclass);

  if (oinfo->children)
    {
      /* we know ClutterStage is a ClutterContainer */
      add_children (script,
                    CLUTTER_CONTAINER (oinfo->object),
                    oinfo->children);
    }

  g_object_set_data_full (oinfo->object, "clutter-script-name",
                          g_strdup (oinfo->id),
                          g_free);

  return oinfo->object;
}

GObject *
clutter_script_construct_object (ClutterScript *script,
                                 ObjectInfo    *oinfo)
{
  guint n_params, i;
  GParameter *params;

  if (oinfo->object)
    return oinfo->object;

  if (oinfo->gtype == G_TYPE_INVALID)
    {
      if (oinfo->type_func)
        oinfo->gtype = resolve_type (oinfo->type_func);
      else
        oinfo->gtype = resolve_type_lazily (oinfo->class_name);

      if (oinfo->gtype == G_TYPE_INVALID)
        return NULL;
    }

  /* the stage is a special case: it's a singleton, it cannot
   * be created by the user and it's owned by the backend. hence,
   * we cannot follow the usual pattern here
   */
  if (g_type_is_a (oinfo->gtype, CLUTTER_TYPE_STAGE))
    return construct_stage (script, oinfo);

  params = NULL;
  translate_properties (script, oinfo, &n_params, &params);

  CLUTTER_NOTE (SCRIPT, "Creating instance for type `%s' (params:%d)",
                g_type_name (oinfo->gtype),
                n_params);

  oinfo->object = g_object_newv (oinfo->gtype, n_params, params);

  for (i = 0; i < n_params; i++)
    {
      GParameter param = params[i];
      g_value_unset (&param.value);
    }

  g_free (params);

  if (CLUTTER_IS_CONTAINER (oinfo->object) && oinfo->children)
    add_children (script, CLUTTER_CONTAINER (oinfo->object), oinfo->children); 

  if (CLUTTER_IS_ACTOR (oinfo->object) && oinfo->behaviours)
    apply_behaviours (script, CLUTTER_ACTOR (oinfo->object), oinfo->behaviours);

  if (CLUTTER_IS_BEHAVIOUR (oinfo->object) ||
      CLUTTER_IS_TIMELINE (oinfo->object))
    oinfo->is_toplevel = TRUE;

  if (oinfo->id)
    g_object_set_data_full (oinfo->object, "clutter-script-name",
                            g_strdup (oinfo->id),
                            g_free);

  return oinfo->object;
}

static void
for_each_object (gpointer key,
                 gpointer value,
                 gpointer data)
{
  ClutterScript *script = data;
  ObjectInfo *oinfo = value;

  clutter_script_construct_object (script, oinfo);
}

static void
json_parse_end (JsonParser *parser,
                gpointer    user_data)
{
  ClutterScript *script = user_data;
  ClutterScriptPrivate *priv = script->priv;

  g_hash_table_foreach (priv->objects, for_each_object, script);
}

static void
object_info_free (gpointer data)
{
  if (G_LIKELY (data))
    {
      ObjectInfo *oinfo = data;
      GList *l;

      g_free (oinfo->id);
      g_free (oinfo->class_name);
      g_free (oinfo->type_func);

      for (l = oinfo->properties; l; l = l->next)
        {
          PropertyInfo *pinfo = l->data;

          g_free (pinfo->property_name);
          g_value_unset (&pinfo->value);
        }
      g_list_free (oinfo->properties);

      /* these are ids */
      g_list_foreach (oinfo->children, (GFunc) g_free, NULL);
      g_list_free (oinfo->children);

      g_list_foreach (oinfo->behaviours, (GFunc) g_free, NULL);
      g_list_free (oinfo->behaviours);

      if (oinfo->is_toplevel && oinfo->object)
        g_object_unref (oinfo->object);

      g_slice_free (ObjectInfo, oinfo);
    }
}

static void
clutter_script_finalize (GObject *gobject)
{
  ClutterScriptPrivate *priv = CLUTTER_SCRIPT_GET_PRIVATE (gobject);

  g_object_unref (priv->parser);
  g_hash_table_destroy (priv->objects);
  g_free (priv->filename);

  G_OBJECT_CLASS (clutter_script_parent_class)->finalize (gobject);
}

static void
clutter_script_class_init (ClutterScriptClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ClutterScriptPrivate));

  gobject_class->finalize = clutter_script_finalize;
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
                                         g_free,
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
 *   accordingly. On success, a positive integer is returned.
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

  internal_error = NULL;
  json_parser_load_from_file (priv->parser, filename, &internal_error);
  if (internal_error)
    {
      g_propagate_error (error, internal_error);
      return 0;
    }
  else
    priv->last_merge_id += 1;

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
 *   accordingly. On success, a positive integer is returned.
 *
 * Since: 0.6
 */
guint
clutter_script_load_from_data (ClutterScript  *script,
                               const gchar    *data,
                               gsize           length,
                               GError        **error)
{
  ClutterScriptPrivate *priv;
  GError *internal_error;

  g_return_val_if_fail (CLUTTER_IS_SCRIPT (script), 0);
  g_return_val_if_fail (data != NULL, 0);

  priv = script->priv;

  g_free (priv->filename);
  priv->filename = NULL;
  priv->is_filename = FALSE;

  internal_error = NULL;
  json_parser_load_from_data (priv->parser, data, length, &internal_error);
  if (internal_error)
    {
      g_propagate_error (error, internal_error);
      return 0;
    }
  else
    priv->last_merge_id += 1;

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
 * Return value: the named object, or %NULL if no object with the
 *   given name was available
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

static GList *
clutter_script_get_objects_valist (ClutterScript *script,
                                   const gchar   *first_name,
                                   va_list        args)
{
  GList *retval = NULL;
  const gchar *name;

  name = first_name;
  while (name)
    {
      retval =
        g_list_prepend (retval, clutter_script_get_object (script, name));

      name = va_arg (args, gchar*);
    }

  return g_list_reverse (retval);
}

/**
 * clutter_script_get_objects:
 * @script: a #ClutterScript
 * @first_name: the name of the first object to retrieve
 * @Varargs: a %NULL-terminated list of names
 *
 * Retrieves a list of objects for the given names. This function does
 * not increment the reference count of the returned objects.
 *
 * Return value: a newly allocated #GList containing the found objects,
 *   or %NULL. Use g_list_free() when done using it.
 *
 * Since: 0.6
 */
GList *
clutter_script_get_objects (ClutterScript *script,
                            const gchar   *first_name,
                            ...)
{
  GList *retval = NULL;
  va_list var_args;

  g_return_val_if_fail (CLUTTER_IS_SCRIPT (script), NULL);
  g_return_val_if_fail (first_name != NULL, NULL);

  va_start (var_args, first_name);
  retval = clutter_script_get_objects_valist (script, first_name, var_args);
  va_end (var_args);

  return retval;
}

gboolean
clutter_script_enum_from_string (GType        type, 
                                 const gchar *string,
                                 gint        *enum_value)
{
  GEnumClass *eclass;
  GEnumValue *ev;
  gchar *endptr;
  gint value;
  gboolean retval = TRUE;
  
  g_return_val_if_fail (G_TYPE_IS_ENUM (type), 0);
  g_return_val_if_fail (string != NULL, 0);
  
  value = strtoul (string, &endptr, 0);
  if (endptr != string) /* parsed a number */
    *enum_value = value;
  else
    {
      eclass = g_type_class_ref (type);
      ev = g_enum_get_value_by_name (eclass, string);
      if (!ev)
	ev = g_enum_get_value_by_nick (eclass, string);

      if (ev)
	*enum_value = ev->value;
      else
        retval = FALSE;
      
      g_type_class_unref (eclass);
    }
  
  return retval;
}

gboolean
clutter_script_value_from_data (ClutterScript  *script,
                                GType           gtype,
                                const gchar    *data,
                                GValue         *value,
                                GError        **error)
{
  return FALSE;
}

GQuark
clutter_script_error_quark (void)
{
  return g_quark_from_static_string ("clutter-script-error");
}
