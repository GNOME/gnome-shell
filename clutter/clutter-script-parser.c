#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "clutter-actor.h"
#include "clutter-behaviour.h"
#include "clutter-container.h"

#include "clutter-script.h"
#include "clutter-script-private.h"
#include "clutter-scriptable.h"

#include "clutter-debug.h"
#include "clutter-private.h"

GType
clutter_script_get_type_from_symbol (const gchar *symbol)
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

GType
clutter_script_get_type_from_class (const gchar *name)
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
clutter_script_flags_from_string (GType        type, 
                                  const gchar *string,
                                  gint        *flags_value)
{
  GFlagsClass *fclass;
  gchar *endptr, *prevptr;
  guint i, j, ret, value;
  gchar *flagstr;
  GFlagsValue *fv;
  const gchar *flag;
  gunichar ch;
  gboolean eos;

  g_return_val_if_fail (G_TYPE_IS_FLAGS (type), 0);
  g_return_val_if_fail (string != 0, 0);

  ret = TRUE;
  
  value = strtoul (string, &endptr, 0);
  if (endptr != string) /* parsed a number */
    *flags_value = value;
  else
    {
      fclass = g_type_class_ref (type);

      flagstr = g_strdup (string);
      for (value = i = j = 0; ; i++)
	{
	  
	  eos = flagstr[i] == '\0';
	  
	  if (!eos && flagstr[i] != '|')
	    continue;
	  
	  flag = &flagstr[j];
	  endptr = &flagstr[i];
	  
	  if (!eos)
	    {
	      flagstr[i++] = '\0';
	      j = i;
	    }
	  
	  /* trim spaces */
	  for (;;)
	    {
	      ch = g_utf8_get_char (flag);
	      if (!g_unichar_isspace (ch))
		break;
	      flag = g_utf8_next_char (flag);
	    }
	  
	  while (endptr > flag)
	    {
	      prevptr = g_utf8_prev_char (endptr);
	      ch = g_utf8_get_char (prevptr);
	      if (!g_unichar_isspace (ch))
		break;
	      endptr = prevptr;
	    }
	  
	  if (endptr > flag)
	    {
	      *endptr = '\0';
	      fv = g_flags_get_value_by_name (fclass, flag);
	      
	      if (!fv)
		fv = g_flags_get_value_by_nick (fclass, flag);
	      
	      if (fv)
		value |= fv->value;
	      else
		{
		  ret = FALSE;
		  break;
		}
	    }
	  
	  if (eos)
	    {
	      *flags_value = value;
	      break;
	    }
	}
      
      g_free (flagstr);
      
      g_type_class_unref (fclass);
    }

  return ret;
}

static gboolean
parse_knot_from_array (JsonArray   *array,
                       ClutterKnot *knot)
{
  JsonNode *val;

  if (json_array_get_length (array) < 2)
    return FALSE;

  val = json_array_get_element (array, 0);
  if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE)
    knot->x = json_node_get_int (val);

  val = json_array_get_element (array, 1);
  if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE)
    knot->y = json_node_get_int (val);

  return TRUE;
}

static gboolean
parse_knot_from_object (JsonObject  *object,
                        ClutterKnot *knot)
{
  JsonNode *val;

  if (json_object_get_size (object) < 2)
    return FALSE;

  val = json_object_get_member (object, "x");
  if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE)
    knot->x = json_node_get_int (val);

  val = json_object_get_member (object, "y");
  if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE)
    knot->y = json_node_get_int (val);

  return TRUE;
}

gboolean
clutter_script_parse_knot (ClutterScript *script,
                           JsonNode      *node,
                           ClutterKnot   *knot)
{
  g_return_val_if_fail (CLUTTER_IS_SCRIPT (script), FALSE);
  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (knot != NULL, FALSE);

  switch (JSON_NODE_TYPE (node))
    {
    case JSON_NODE_ARRAY:
      return parse_knot_from_array (json_node_get_array (node), knot);

    case JSON_NODE_OBJECT:
      return parse_knot_from_object (json_node_get_object (node), knot);

    default:
      break;
    }

  return FALSE;
}

static ClutterUnit
get_units_from_node (JsonNode *node)
{
  ClutterUnit retval = 0;
  GValue value = { 0, };

  if (JSON_NODE_TYPE (node) != JSON_NODE_VALUE)
    return 0;

  json_node_get_value (node, &value);
  switch (G_VALUE_TYPE (&value))
    {
    case G_TYPE_INT:
      retval = CLUTTER_UNITS_FROM_INT (g_value_get_int (&value));
      break;

    default:
      break;
    }

  return retval;
}

gboolean
clutter_script_parse_padding (ClutterScript  *script,
                              JsonNode       *node,
                              ClutterPadding *padding)
{
  JsonArray *array;
  gint array_len, i;

  g_return_val_if_fail (CLUTTER_IS_SCRIPT (script), FALSE);
  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (padding != NULL, FALSE);

  if (JSON_NODE_TYPE (node) != JSON_NODE_ARRAY)
    return FALSE;

  array = json_node_get_array (node);
  array_len = json_array_get_length (array);
  
  for (i = 0; i < array_len; i++)
    {
      JsonNode *val = json_array_get_element (array, i);
      ClutterUnit units = get_units_from_node (val);

      switch (i)
        {
        case 0:
          padding->top = units; 
          padding->right = padding->top;
          padding->bottom = padding->top;
          padding->left = padding->top;
          break;
        
        case 1:
          padding->right = padding->left = units; 
          break;

        case 2:
          padding->bottom = units; 
          break;

        case 3:
          padding->left = units; 
          break;
        }
    }

  return TRUE;
}

gboolean
clutter_script_parse_margin (ClutterScript *script,
                             JsonNode      *node,
                             ClutterMargin *margin)
{
  JsonArray *array;
  gint array_len, i;

  g_return_val_if_fail (CLUTTER_IS_SCRIPT (script), FALSE);
  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (margin != NULL, FALSE);

  if (JSON_NODE_TYPE (node) != JSON_NODE_ARRAY)
    return FALSE;

  array = json_node_get_array (node);
  array_len = json_array_get_length (array);
  
  for (i = 0; i < array_len; i++)
    {
      JsonNode *val = json_array_get_element (array, i);
      ClutterUnit units = get_units_from_node (val);

      switch (i)
        {
        case 0:
          margin->top = units; 
          margin->right = margin->top;
          margin->bottom = margin->top;
          margin->left = margin->top;
          break;
        
        case 1:
          margin->right = margin->left = units; 
          break;

        case 2:
          margin->bottom = units; 
          break;

        case 3:
          margin->left = units; 
          break;
        }
    }

  return TRUE;
}

static gboolean
parse_geometry_from_array (JsonArray       *array,
                           ClutterGeometry *geometry)
{
  JsonNode *val;

  if (json_array_get_length (array) < 4)
    return FALSE;

  val = json_array_get_element (array, 0);
  if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE)
    geometry->x = json_node_get_int (val);

  val = json_array_get_element (array, 1);
  if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE)
    geometry->y = json_node_get_int (val);

  val = json_array_get_element (array, 2);
  if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE)
    geometry->width = json_node_get_int (val);

  val = json_array_get_element (array, 3);
  if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE)
    geometry->height = json_node_get_int (val);

  return TRUE;
}

static gboolean
parse_geometry_from_object (JsonObject      *object,
                            ClutterGeometry *geometry)
{
  JsonNode *val;

  if (json_object_get_size (object) < 4)
    return FALSE;

  val = json_object_get_member (object, "x");
  if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE)
    geometry->x = json_node_get_int (val);

  val = json_object_get_member (object, "y");
  if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE)
    geometry->y = json_node_get_int (val);

  val = json_object_get_member (object, "width");
  if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE)
    geometry->width = json_node_get_int (val);

  val = json_object_get_member (object, "height");
  if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE)
    geometry->height = json_node_get_int (val);

  return TRUE;
}

gboolean
clutter_script_parse_geometry (ClutterScript   *script,
                               JsonNode        *node,
                               ClutterGeometry *geometry)
{
  g_return_val_if_fail (CLUTTER_IS_SCRIPT (script), FALSE);
  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (geometry != NULL, FALSE);

  switch (JSON_NODE_TYPE (node))
    {
    case JSON_NODE_ARRAY:
      return parse_geometry_from_array (json_node_get_array (node), geometry);

    case JSON_NODE_OBJECT:
      return parse_geometry_from_object (json_node_get_object (node), geometry);

    default:
      break;
    }

  return FALSE;
}
