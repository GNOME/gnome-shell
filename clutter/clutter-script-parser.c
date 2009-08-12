/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd
 * Copyright (C) 2009 Intel Corportation
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
 *
 * Original author:
 *
 *      Emmanuele Bassi <ebassi@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <gmodule.h>

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
    module = g_module_open (NULL, G_MODULE_BIND_LAZY);
  
  if (g_module_symbol (module, symbol, (gpointer)&func))
    gtype = func ();
  
  return gtype;
}

GType
clutter_script_get_type_from_class (const gchar *name)
{
  static GModule *module = NULL;
  GString *symbol_name = g_string_sized_new (64);
  GType gtype = G_TYPE_INVALID;
  GTypeGetFunc func;
  gchar *symbol;
  gint i;

  if (G_UNLIKELY (!module))
    module = g_module_open (NULL, G_MODULE_BIND_LAZY);
  
  for (i = 0; name[i] != '\0'; i++)
    {
      gchar c = name[i];

      /* the standard naming policy for GObject-based libraries
       * is:
       *
       *   NAME := INITIAL_WORD WORD+
       *   INITIAL_WORD := [A-Z][a-z0-9]*
       *   WORD := [A-Z]{1,2}[a-z0-9]+ | [A-Z]{2,}
       *
       * for instance:
       *
       *   GString -> g_string
       *   GtkCTree -> gtk_ctree
       *   ClutterX11TexturePixmap -> clutter_x11_texture_pixmap
       *
       * see:
       *
       * http://mail.gnome.org/archives/gtk-devel-list/2007-June/msg00022.html
       *
       * and:
       *
       * http://git.gnome.org/cgit/gtk+/plain/gtk/gtkbuilderparser.c
       */

      if ((c == g_ascii_toupper (c) &&
           i > 0 && name[i - 1] != g_ascii_toupper (name[i - 1])) ||
          (i > 2 && name[i] == g_ascii_toupper (name[i]) &&
           name[i - 1] == g_ascii_toupper (name[i - 1]) &&
           name[i - 2] == g_ascii_toupper (name[i - 2])))
        g_string_append_c (symbol_name, '_');

      g_string_append_c (symbol_name, g_ascii_tolower (c));
    }

  g_string_append (symbol_name, "_get_type");
  
  symbol = g_string_free (symbol_name, FALSE);

  if (g_module_symbol (module, symbol, (gpointer)&func))
    {
      CLUTTER_NOTE (SCRIPT, "Type function: %s", symbol);
      gtype = func ();
    }
  
  g_free (symbol);

  return gtype;
}

/*
 * clutter_script_enum_from_string:
 * @type: a #GType for an enumeration type
 * @string: the enumeration value as a string
 * @enum_value: return location for the enumeration value as an integer
 *
 * Converts an enumeration value inside @string into a numeric
 * value and places it into @enum_value.
 *
 * The enumeration value can be an integer, the enumeration nick
 * or the enumeration name, as part of the #GEnumValue structure.
 *
 * Return value: %TRUE if the conversion was successfull.
 */
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
  gchar *endptr, *prevptr;
  guint i, j, ret, value;
  gchar *flagstr;
  GFlagsValue *fv;
  const gchar *flag;

  g_return_val_if_fail (G_TYPE_IS_FLAGS (type), 0);
  g_return_val_if_fail (string != 0, 0);

  ret = TRUE;
  
  value = strtoul (string, &endptr, 0);
  if (endptr != string) /* parsed a number */
    *flags_value = value;
  else
    {
      GFlagsClass *fclass;

      fclass = g_type_class_ref (type);

      flagstr = g_strdup (string);
      for (value = i = j = 0; ; i++)
	{
          gboolean eos = (flagstr[i] == '\0') ? TRUE : FALSE;
	  
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
	      gunichar ch = g_utf8_get_char (flag);
	      if (!g_unichar_isspace (ch))
		break;

	      flag = g_utf8_next_char (flag);
	    }
	  
	  while (endptr > flag)
	    {
              gunichar ch;

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

static gboolean
parse_color_from_array (JsonArray    *array,
                        ClutterColor *color)
{
  JsonNode *val;

  if (json_array_get_length (array) < 4)
    return FALSE;

  val = json_array_get_element (array, 0);
  if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE)
    color->red = CLAMP (json_node_get_int (val), 0, 255);

  val = json_array_get_element (array, 1);
  if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE)
    color->green = CLAMP (json_node_get_int (val), 0, 255);

  val = json_array_get_element (array, 2);
  if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE)
    color->blue = CLAMP (json_node_get_int (val), 0, 255);

  val = json_array_get_element (array, 3);
  if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE)
    color->alpha = CLAMP (json_node_get_int (val), 0, 255);

  return TRUE;
}

static gboolean
parse_color_from_object (JsonObject   *object,
                         ClutterColor *color)
{
  JsonNode *val;

  if (json_object_get_size (object) < 4)
    return FALSE;

  val = json_object_get_member (object, "red");
  if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE)
    color->red = CLAMP (json_node_get_int (val), 0, 255);

  val = json_object_get_member (object, "green");
  if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE)
    color->green = CLAMP (json_node_get_int (val), 0, 255);

  val = json_object_get_member (object, "blue");
  if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE)
    color->blue = CLAMP (json_node_get_int (val), 0, 255);

  val = json_object_get_member (object, "alpha");
  if (JSON_NODE_TYPE (val) == JSON_NODE_VALUE)
    color->alpha = CLAMP (json_node_get_int (val), 0, 255);

  return TRUE;
}

gboolean
clutter_script_parse_color (ClutterScript *script,
                            JsonNode      *node,
                            ClutterColor  *color)
{
  g_return_val_if_fail (CLUTTER_IS_SCRIPT (script), FALSE);
  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (color != NULL, FALSE);

  switch (JSON_NODE_TYPE (node))
    {
    case JSON_NODE_ARRAY:
      return parse_color_from_array (json_node_get_array (node), color);

    case JSON_NODE_OBJECT:
      return parse_color_from_object (json_node_get_object (node), color);

    default:
      break;
    }

  return FALSE;
}
