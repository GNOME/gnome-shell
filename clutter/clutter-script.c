#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <glib.h>
#include <glib-object.h>

#include "clutter-actor.h"
#include "clutter-stage.h"
#include "clutter-container.h"

#include "clutter-scriptable.h"
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

static PropertyInfo *
parse_member_to_property (ClutterScript *script,
                          ObjectInfo    *info,
                          const gchar   *name,
                          JsonNode      *node)
{
  PropertyInfo *retval;
  GValue value = { 0, };

  retval = g_slice_new (PropertyInfo);
  retval->property_name = g_strdup (name);
  
  switch (JSON_NODE_TYPE (node))
    {
    case JSON_NODE_VALUE:
      json_node_get_value (node, &value);
      g_value_init (&retval->value, G_VALUE_TYPE (&value));
      g_value_copy (&value, &retval->value);
      g_value_unset (&value);
      break;

    case JSON_NODE_OBJECT:
      break;

    case JSON_NODE_ARRAY:
      if (strcmp (name, "geometry") == 0)
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

          g_value_init (&retval->value, CLUTTER_TYPE_GEOMETRY);
          g_value_set_boxed (&retval->value, &geom);
        }
      else if (strcmp (name, "children") == 0)
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
                  warn_invalid_value (script, "children", val);
                  break;
                }
            }

          g_value_init (&retval->value, G_TYPE_POINTER);
          g_value_set_pointer (&retval->value, children);
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

  members = json_object_get_members (object);
  for (l = members; l; l = l->next)
    {
      const gchar *name = l->data;

      val = json_object_get_member (object, name);

      if (strcmp (name, "id") == 0 || strcmp (name, "type") == 0)
        continue;
      else
        {
          PropertyInfo *pinfo;

          pinfo = parse_member_to_property (script, oinfo, name, val);
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

static gboolean
translate_property (const gchar  *name,
                    const GValue *src,
                    GValue       *dest)
{
  if (strcmp (name, "color") == 0)
    {
      ClutterColor color = { 0, };
      const gchar *color_str;

      if (G_VALUE_HOLDS (src, G_TYPE_STRING))
        color_str = g_value_get_string (src);
      else
        color_str = NULL;

      clutter_color_parse (color_str, &color);

      g_value_init (dest, CLUTTER_TYPE_COLOR);
      g_value_set_boxed (dest, &color);

      return TRUE;
    }
  else if (strcmp (name, "pixbuf") == 0)
    {
      GdkPixbuf *pixbuf = NULL;
      const gchar *path;

      if (G_VALUE_HOLDS (src, G_TYPE_STRING))
        path = g_value_get_string (src);
      else
        path = NULL;

      if (path && g_path_is_absolute (path))
        {
          GError *error = NULL;

          pixbuf = gdk_pixbuf_new_from_file (path, &error);
          if (error)
            {
              g_warning ("Unable to open pixbuf at path `%s': %s",
                         path,
                         error->message);
              g_error_free (error);
            }
        }

      g_value_init (dest, GDK_TYPE_PIXBUF);
      g_value_set_object (dest, pixbuf);

      return TRUE;
    }

  return FALSE;
}

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

      if (strcmp (pinfo->property_name, "children") == 0)
        continue;

      pspec = g_object_class_find_property (oclass, pinfo->property_name);
      if (!pspec)
        {
          g_warning ("Unknown property `%s' for class `%s'",
                     pinfo->property_name,
                     g_type_name (oinfo->gtype));
          continue;
        }

      param.name = pinfo->property_name;
      if (!translate_property (param.name, &pinfo->value, &param.value))
        {
          g_value_init (&param.value, G_PARAM_SPEC_VALUE_TYPE (pspec));
          g_value_copy (&pinfo->value, &param.value);
        }

      g_array_append_val (parameters, param);
    }

  *n_params = parameters->len;
  *params = (GParameter *) g_array_free (parameters, FALSE);

  g_type_class_unref (oclass);
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

      /* "children" is a fake property: we use it so we can construct
       * the list of children of a given container
       */
      if (strcmp (name, "children") == 0)
        {
          GList *children = g_value_get_pointer (&pinfo->value);
          
          /* we know ClutterStage is a ClutterContainer */
          add_children (script, CLUTTER_CONTAINER (oinfo->object), children);

          /* unset, so we don't leak it later */
          g_list_foreach (children, (GFunc) g_free, NULL);
          g_list_free (children);
          g_value_set_pointer (&pinfo->value, NULL);

          continue;
        }

      pspec = g_object_class_find_property (oclass, name);
      if (!pspec)
        {
          g_warning ("Unknown property `%s' for class `ClutterStage'",
                     name);
          continue;
        }

      if (!translate_property (name, &pinfo->value, &value))
        {
          g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
          g_value_copy (&pinfo->value, &value);
        }

      g_object_set_property (oinfo->object, name, &value);

      g_value_unset (&value);
    }

  g_type_class_unref (oclass);

  g_object_set_data_full (oinfo->object, "clutter-script-name",
                          g_strdup (oinfo->id),
                          g_free);

  return oinfo->object;
}

GObject *
clutter_script_construct_object (ClutterScript *script,
                                 ObjectInfo    *oinfo)
{
  GType gtype;
  guint n_params, i;
  GParameter *params;

  if (oinfo->object)
    return oinfo->object;

  gtype = resolve_type_lazily (oinfo->class_name);
  if (gtype == G_TYPE_INVALID)
    return NULL;

  /* the stage is a special case: it's a singleton, it cannot
   * be created by the user and it's owned by the backend. hence,
   * we cannot follow the usual pattern here
   */
  if (g_type_is_a (gtype, CLUTTER_TYPE_STAGE))
    return construct_stage (script, oinfo);

  oinfo->gtype = gtype;
  params = NULL;
  translate_properties (script, oinfo, &n_params, &params);

  CLUTTER_NOTE (SCRIPT, "Creating instance for type `%s' (params:%d)",
                g_type_name (gtype),
                n_params);

  oinfo->object = g_object_newv (gtype, n_params, params);
  g_object_set_data_full (oinfo->object, "clutter-script-name",
                          g_strdup (oinfo->id),
                          g_free);

  for (i = 0; i < n_params; i++)
    {
      GParameter param = params[i];
      g_value_unset (&param.value);
    }

  g_free (params);

  return oinfo->object;
}

static void
json_parse_end (JsonParser *parser,
                gpointer    user_data)
{
  ClutterScript *script = user_data;
  ClutterScriptPrivate *priv = script->priv;
  GList *objects, *l;

  objects = g_hash_table_get_values (priv->objects);
  for (l = objects; l; l = l->next)
    {
      ObjectInfo *oinfo = l->data;

      oinfo->object = clutter_script_construct_object (script, oinfo);
    }
  g_list_free (objects);
}

static void
object_info_free (gpointer data)
{
  if (G_LIKELY (data))
    {
      ObjectInfo *oinfo = data;
      GList *l;

      g_free (oinfo->class_name);
      g_free (oinfo->id);

      for (l = oinfo->properties; l; l = l->next)
        {
          PropertyInfo *pinfo = l->data;

          g_free (pinfo->property_name);
          g_value_unset (&pinfo->value);
        }
      g_list_free (oinfo->properties);

      if (oinfo->object)
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

ClutterScript *
clutter_script_new (void)
{
  return g_object_new (CLUTTER_TYPE_SCRIPT, NULL);
}

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

  return oinfo->object;
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
clutter_script_value_from_data (ClutterScript  *script,
                                GType           gtype,
                                const gchar    *data,
                                GValue         *value,
                                GError        **error)
{
  return FALSE;
}
