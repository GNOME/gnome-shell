/* Metacity theme parsing */

/*
 * Copyright (C) 2001 Havoc Pennington
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>
#include "theme-parser.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>

typedef enum
{
  STATE_START,
  STATE_THEME,
  /* info section */
  STATE_INFO,
  STATE_NAME,
  STATE_AUTHOR,
  STATE_COPYRIGHT,
  STATE_DATE,
  STATE_DESCRIPTION,
  /* constants */
  STATE_CONSTANT,
  /* geometry */
  STATE_FRAME_GEOMETRY,
  STATE_DISTANCE,
  STATE_BORDER,
  /* draw ops */
  STATE_DRAW_OPS,
  STATE_LINE,
  STATE_RECTANGLE,
  STATE_ARC,
  STATE_CLIP,
  STATE_TINT,
  STATE_GRADIENT,
  STATE_IMAGE,
  STATE_GTK_ARROW,
  STATE_GTK_BOX,
  STATE_GTK_VLINE,
  STATE_ICON,
  STATE_TITLE,
  STATE_INCLUDE, /* include another draw op list */
  STATE_TILE,    /* tile another draw op list */
  /* sub-parts of gradient */
  STATE_COLOR,
  /* frame style */
  STATE_FRAME_STYLE,
  STATE_PIECE,
  STATE_BUTTON,
  /* style set */
  STATE_FRAME_STYLE_SET,
  STATE_FRAME,
  /* assigning style sets to windows */
  STATE_WINDOW,
  /* and menu icons */
  STATE_MENU_ICON
} ParseState;

typedef struct
{
  GSList *states;

  const char *theme_name;       /* name of theme (directory it's in) */
  char *theme_file;             /* theme filename */
  char *theme_dir;              /* dir the theme is inside */
  MetaTheme *theme;             /* theme being parsed */
  char *name;                   /* name of named thing being parsed */
  MetaFrameLayout *layout;      /* layout being parsed if any */
  MetaDrawOpList *op_list;      /* op list being parsed if any */
  MetaDrawOp *op;               /* op being parsed if any */
  MetaFrameStyle *style;        /* frame style being parsed if any */
  MetaFrameStyleSet *style_set; /* frame style set being parsed if any */
  MetaFramePiece piece;         /* position of piece being parsed */
  MetaButtonType button_type;   /* type of button/menuitem being parsed */
  MetaButtonState button_state; /* state of button being parsed */
  MetaMenuIconType menu_icon_type; /* type of menu icon being parsed */
  GtkStateType menu_icon_state; /* state of menu icon being parsed */
} ParseInfo;

static void set_error (GError             **err,
                       GMarkupParseContext *context,
                       int                  error_domain,
                       int                  error_code,
                       const char          *format,
                       ...) G_GNUC_PRINTF (5, 6);

static void add_context_to_error (GError             **err,
                                  GMarkupParseContext *context);

static void       parse_info_init (ParseInfo *info);
static void       parse_info_free (ParseInfo *info);

static void       push_state (ParseInfo  *info,
                              ParseState  state);
static void       pop_state  (ParseInfo  *info);
static ParseState peek_state (ParseInfo  *info);


static void parse_toplevel_element  (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     ParseInfo            *info,
                                     GError              **error);
static void parse_info_element      (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     ParseInfo            *info,
                                     GError              **error);
static void parse_geometry_element  (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     ParseInfo            *info,
                                     GError              **error);
static void parse_draw_op_element   (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     ParseInfo            *info,
                                     GError              **error);
static void parse_gradient_element  (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     ParseInfo            *info,
                                     GError              **error);
static void parse_style_element     (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     ParseInfo            *info,
                                     GError              **error);
static void parse_style_set_element (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     ParseInfo            *info,
                                     GError              **error);

static void parse_piece_element     (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     ParseInfo            *info,
                                     GError              **error);

static void parse_button_element    (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     ParseInfo            *info,
                                     GError              **error);

static void parse_menu_icon_element (GMarkupParseContext  *context,
                                     const gchar          *element_name,
                                     const gchar         **attribute_names,
                                     const gchar         **attribute_values,
                                     ParseInfo            *info,
                                     GError              **error);

static void start_element_handler (GMarkupParseContext  *context,
                                   const gchar          *element_name,
                                   const gchar         **attribute_names,
                                   const gchar         **attribute_values,
                                   gpointer              user_data,
                                   GError              **error);
static void end_element_handler   (GMarkupParseContext  *context,
                                   const gchar          *element_name,
                                   gpointer              user_data,
                                   GError              **error);
static void text_handler          (GMarkupParseContext  *context,
                                   const gchar          *text,
                                   gsize                 text_len,
                                   gpointer              user_data,
                                   GError              **error);

static GMarkupParser metacity_theme_parser = {
  start_element_handler,
  end_element_handler,
  text_handler,
  NULL,
  NULL
};

static void
set_error (GError             **err,
           GMarkupParseContext *context,
           int                  error_domain,
           int                  error_code,
           const char          *format,
           ...)
{
  int line, ch;
  va_list args;
  char *str;
  
  g_markup_parse_context_get_position (context, &line, &ch);

  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);

  g_set_error (err, error_domain, error_code,
               _("Line %d character %d: %s"),
               line, ch, str);

  g_free (str);
}

static void
add_context_to_error (GError             **err,
                      GMarkupParseContext *context)
{
  int line, ch;
  char *str;

  if (err == NULL || *err == NULL)
    return;

  g_markup_parse_context_get_position (context, &line, &ch);

  str = g_strdup_printf (_("Line %d character %d: %s"),
                         line, ch, (*err)->message);
  g_free ((*err)->message);
  (*err)->message = str;
}

static void
parse_info_init (ParseInfo *info)
{
  info->theme_file = NULL;
  info->states = g_slist_prepend (NULL, GINT_TO_POINTER (STATE_START));
  info->theme = NULL;
  info->name = NULL;
  info->layout = NULL;
  info->op_list = NULL;
  info->op = NULL;
  info->style = NULL;
  info->style_set = NULL;
  info->piece = META_FRAME_PIECE_LAST;
  info->button_type = META_BUTTON_TYPE_LAST;
  info->button_state = META_BUTTON_STATE_LAST;
}

static void
parse_info_free (ParseInfo *info)
{
  g_free (info->theme_file);
  g_free (info->theme_dir);

  g_slist_free (info->states);
  
  if (info->theme)
    meta_theme_free (info->theme);

  if (info->layout)
    meta_frame_layout_unref (info->layout);

  if (info->op_list)
    meta_draw_op_list_unref (info->op_list);

  if (info->op)
    meta_draw_op_free (info->op);
  
  if (info->style)
    meta_frame_style_unref (info->style);

  if (info->style_set)
    meta_frame_style_set_unref (info->style_set);
}

static void
push_state (ParseInfo  *info,
            ParseState  state)
{
  info->states = g_slist_prepend (info->states, GINT_TO_POINTER (state));
}

static void
pop_state (ParseInfo *info)
{
  g_return_if_fail (info->states != NULL);
  
  info->states = g_slist_remove (info->states, info->states->data);
}

static ParseState
peek_state (ParseInfo *info)
{
  g_return_val_if_fail (info->states != NULL, STATE_START);

  return GPOINTER_TO_INT (info->states->data);
}

#define ELEMENT_IS(name) (strcmp (element_name, (name)) == 0)

typedef struct
{
  const char  *name;
  const char **retloc;
} LocateAttr;

static gboolean
locate_attributes (GMarkupParseContext *context,
                   const char  *element_name,
                   const char **attribute_names,
                   const char **attribute_values,
                   GError     **error,
                   const char  *first_attribute_name,
                   const char **first_attribute_retloc,
                   ...)
{
  va_list args;
  const char *name;
  const char **retloc;
  int n_attrs;
#define MAX_ATTRS 24
  LocateAttr attrs[MAX_ATTRS];
  gboolean retval;
  int i;

  g_return_val_if_fail (first_attribute_name != NULL, FALSE);
  g_return_val_if_fail (first_attribute_retloc != NULL, FALSE);

  retval = TRUE;

  n_attrs = 1;
  attrs[0].name = first_attribute_name;
  attrs[0].retloc = first_attribute_retloc;
  *first_attribute_retloc = NULL;
  
  va_start (args, first_attribute_retloc);

  name = va_arg (args, const char*);
  retloc = va_arg (args, const char**);

  while (name != NULL)
    {
      g_return_val_if_fail (retloc != NULL, FALSE);

      g_assert (n_attrs < MAX_ATTRS);
      
      attrs[n_attrs].name = name;
      attrs[n_attrs].retloc = retloc;
      n_attrs += 1;
      *retloc = NULL;      

      name = va_arg (args, const char*);
      retloc = va_arg (args, const char**);
    }

  va_end (args);

  if (!retval)
    return retval;

  i = 0;
  while (attribute_names[i])
    {
      int j;
      gboolean found;

      found = FALSE;
      j = 0;
      while (j < n_attrs)
        {
          if (strcmp (attrs[j].name, attribute_names[i]) == 0)
            {
              retloc = attrs[j].retloc;

              if (*retloc != NULL)
                {
                  set_error (error, context,
                             G_MARKUP_ERROR,
                             G_MARKUP_ERROR_PARSE,
                             _("Attribute \"%s\" repeated twice on the same <%s> element"),
                             attrs[j].name, element_name);
                  retval = FALSE;
                  goto out;
                }

              *retloc = attribute_values[i];
              found = TRUE;
            }

          ++j;
        }

      if (!found)
        {
          set_error (error, context,
                     G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Attribute \"%s\" is invalid on <%s> element in this context"),
                     attribute_names[i], element_name);
          retval = FALSE;
          goto out;
        }

      ++i;
    }

 out:
  return retval;
}

static gboolean
check_no_attributes (GMarkupParseContext *context,
                     const char  *element_name,
                     const char **attribute_names,
                     const char **attribute_values,
                     GError     **error)
{
  if (attribute_names[0] != NULL)
    {
      set_error (error, context,
                 G_MARKUP_ERROR,
                 G_MARKUP_ERROR_PARSE,
                 _("Attribute \"%s\" is invalid on <%s> element in this context"),
                 attribute_names[0], element_name);
      return FALSE;
    }

  return TRUE;
}

#define MAX_REASONABLE 4096
static gboolean
parse_positive_integer (const char          *str,
                        int                 *val,
                        GMarkupParseContext *context,
                        GError             **error)
{
  char *end;
  long l;

  *val = 0;
  
  end = NULL;
  
  l = strtol (str, &end, 10);

  if (end == NULL || end == str)
    {
      set_error (error, context, G_MARKUP_ERROR,
                 G_MARKUP_ERROR_PARSE,
                 _("Could not parse \"%s\" as an integer"),
                 str);
      return FALSE;
    }

  if (*end != '\0')
    {
      set_error (error, context, G_MARKUP_ERROR,
                 G_MARKUP_ERROR_PARSE,
                 _("Did not understand trailing characters \"%s\" in string \"%s\""),
                 end, str);
      return FALSE;
    }

  if (l < 0)
    {
      set_error (error, context, G_MARKUP_ERROR,
                 G_MARKUP_ERROR_PARSE,
                 _("Integer %ld must be positive"), l);
      return FALSE;
    }

  if (l > MAX_REASONABLE)
    {
      set_error (error, context, G_MARKUP_ERROR,
                 G_MARKUP_ERROR_PARSE,
                 _("Integer %ld is too large, current max is %d"),
                 l, MAX_REASONABLE);
      return FALSE;
    }
  
  *val = (int) l;

  return TRUE;
}

static gboolean
parse_double (const char          *str,
              double              *val,
              GMarkupParseContext *context,
              GError             **error)
{
  char *end;

  *val = 0;
  
  end = NULL;
  
  *val = g_ascii_strtod (str, &end);

  if (end == NULL || end == str)
    {
      set_error (error, context, G_MARKUP_ERROR,
                 G_MARKUP_ERROR_PARSE,
                 _("Could not parse \"%s\" as a floating point number"),
                 str);
      return FALSE;
    }

  if (*end != '\0')
    {
      set_error (error, context, G_MARKUP_ERROR,
                 G_MARKUP_ERROR_PARSE,
                 _("Did not understand trailing characters \"%s\" in string \"%s\""),
                 end, str);
      return FALSE;
    }

  return TRUE;
}

static gboolean
parse_boolean (const char          *str,
               gboolean            *val,
               GMarkupParseContext *context,
               GError             **error)
{
  if (strcmp ("true", str) == 0)
    *val = TRUE;
  else if (strcmp ("false", str) == 0)
    *val = FALSE;
  else
    {
      set_error (error, context, G_MARKUP_ERROR,
                 G_MARKUP_ERROR_PARSE,
                 _("Boolean values must be \"true\" or \"false\" not \"%s\""),
                 str);
      return FALSE;
    }
  
  return TRUE;
}

static gboolean
parse_angle (const char          *str,
             double              *val,
             GMarkupParseContext *context,
             GError             **error)
{
  if (!parse_double (str, val, context, error))
    return FALSE;

  if (*val < (0.0 - 1e6) || *val > (360.0 + 1e6))
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Angle must be between 0.0 and 360.0, was %g\n"),
                 *val);
      return FALSE;
    }

  return TRUE;
}

static gboolean
parse_alpha (const char          *str,
             double              *val,
             GMarkupParseContext *context,
             GError             **error)
{
  if (!parse_double (str, val, context, error))
    return FALSE;

  if (*val < (0.0 - 1e6) || *val > (1.0 + 1e6))
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Alpha must be between 0.0 (invisible) and 1.0 (fully opaque), was %g\n"),
                 *val);
      return FALSE;
    }

  return TRUE;
}

static gboolean
parse_title_scale (const char          *str,
                   double              *val,
                   GMarkupParseContext *context,
                   GError             **error)
{
  double factor;
  
  if (strcmp (str, "xx-small") == 0)
    factor = PANGO_SCALE_XX_SMALL;
  else if (strcmp (str, "x-small") == 0)
    factor = PANGO_SCALE_X_SMALL;
  else if (strcmp (str, "small") == 0)
    factor = PANGO_SCALE_SMALL;
  else if (strcmp (str, "medium") == 0)
    factor = PANGO_SCALE_MEDIUM;
  else if (strcmp (str, "large") == 0)
    factor = PANGO_SCALE_LARGE;
  else if (strcmp (str, "x-large") == 0)
    factor = PANGO_SCALE_X_LARGE;
  else if (strcmp (str, "xx-large") == 0)
    factor = PANGO_SCALE_XX_LARGE;
  else
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Invalid title scale \"%s\" (must be one of xx-small,x-small,small,medium,large,x-large,xx-large)\n"),
                 str);
      return FALSE;
    }

  *val = factor;
  
  return TRUE;
}

static void
parse_toplevel_element (GMarkupParseContext  *context,
                        const gchar          *element_name,
                        const gchar         **attribute_names,
                        const gchar         **attribute_values,
                        ParseInfo            *info,
                        GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_THEME);

  if (ELEMENT_IS ("info"))
    {
      if (!check_no_attributes (context, element_name,
                                attribute_names, attribute_values,
                                error))
        return;

      push_state (info, STATE_INFO);
    }
  else if (ELEMENT_IS ("constant"))
    {
      const char *name;
      const char *value;
      int ival;
      double dval;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "name", &name, "value", &value,
                              NULL))
        return;

      if (name == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"%s\" attribute on element <%s>"),
                     "name", element_name);
          return;
        }
      
      if (value == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"%s\" attribute on element <%s>"),
                     "value", element_name);
          return;
        }

      if (strchr (value, '.'))
        {
          dval = 0.0;
          if (!parse_double (value, &dval, context, error))
            return;

          if (!meta_theme_define_float_constant (info->theme,
                                                 name,
                                                 dval,
                                                 error))
            {
              add_context_to_error (error, context);
              return;
            }
        }
      else
        {
          ival = 0;
          if (!parse_positive_integer (value, &ival, context, error))
            return;

          if (!meta_theme_define_int_constant (info->theme,
                                               name,
                                               ival,
                                               error))
            {
              add_context_to_error (error, context);
              return;
            }
        }

      push_state (info, STATE_CONSTANT);
    }
  else if (ELEMENT_IS ("frame_geometry"))
    {
      const char *name = NULL;
      const char *parent = NULL;
      const char *has_title = NULL;
      const char *title_scale = NULL;
      gboolean has_title_val;
      double title_scale_val;
      MetaFrameLayout *parent_layout;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "name", &name, "parent", &parent,
                              "has_title", &has_title, "title_scale", &title_scale,
                              NULL))
        return;

      if (name == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"%s\" attribute on <%s> element"),
                     "name", element_name);
          return;
        }

      has_title_val = TRUE;
      if (has_title && !parse_boolean (has_title, &has_title_val, context, error))
        return;

      title_scale_val = 1.0;
      if (title_scale && !parse_title_scale (title_scale, &title_scale_val, context, error))
        return;
      
      if (meta_theme_lookup_layout (info->theme, name))
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("<%s> name \"%s\" used a second time"),
                     element_name, name);
          return;
        }

      parent_layout = NULL;
      if (parent)
        {
          parent_layout = meta_theme_lookup_layout (info->theme, parent);
          if (parent_layout == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("<%s> parent \"%s\" has not been defined"),
                         element_name, parent);
              return;
            }
        }

      g_assert (info->layout == NULL);

      if (parent_layout)
        info->layout = meta_frame_layout_copy (parent_layout);
      else
        info->layout = meta_frame_layout_new ();

      if (has_title) /* only if explicit, otherwise inherit */
        info->layout->has_title = has_title_val;

      if (title_scale)
	info->layout->title_scale = title_scale_val;
      
      meta_theme_insert_layout (info->theme, name, info->layout);

      push_state (info, STATE_FRAME_GEOMETRY);
    }
  else if (ELEMENT_IS ("draw_ops"))
    {
      const char *name = NULL;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "name", &name,
                              NULL))
        return;

      if (name == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"%s\" attribute on <%s> element"),
                     "name", element_name);
          return;
        }

      if (meta_theme_lookup_draw_op_list (info->theme, name))
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("<%s> name \"%s\" used a second time"),
                     element_name, name);
          return;
        }

      g_assert (info->op_list == NULL);
      info->op_list = meta_draw_op_list_new (2);

      meta_theme_insert_draw_op_list (info->theme, name, info->op_list);

      push_state (info, STATE_DRAW_OPS);
    }
  else if (ELEMENT_IS ("frame_style"))
    {
      const char *name = NULL;
      const char *parent = NULL;
      const char *geometry = NULL;
      MetaFrameStyle *parent_style;
      MetaFrameLayout *layout;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "name", &name, "parent", &parent,
                              "geometry", &geometry,
                              NULL))
        return;

      if (name == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"%s\" attribute on <%s> element"),
                     "name", element_name);
          return;
        }

      if (meta_theme_lookup_style (info->theme, name))
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("<%s> name \"%s\" used a second time"),
                     element_name, name);
          return;
        }

      parent_style = NULL;
      if (parent)
        {
          parent_style = meta_theme_lookup_style (info->theme, parent);
          if (parent_style == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("<%s> parent \"%s\" has not been defined"),
                         element_name, parent);
              return;
            }
        }

      layout = NULL;
      if (geometry)
        {
          layout = meta_theme_lookup_layout (info->theme, geometry);
          if (layout == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("<%s> geometry \"%s\" has not been defined"),
                         element_name, geometry);
              return;
            }
        }
      else if (parent_style)
        {
          layout = parent_style->layout;
        }

      if (layout == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("<%s> must specify either a geometry or a parent that has a geometry"),
                     element_name);
          return;
        }

      g_assert (info->style == NULL);

      info->style = meta_frame_style_new (parent_style);
      g_assert (info->style->layout == NULL);
      meta_frame_layout_ref (layout);
      info->style->layout = layout;

      meta_theme_insert_style (info->theme, name, info->style);

      push_state (info, STATE_FRAME_STYLE);
    }
  else if (ELEMENT_IS ("frame_style_set"))
    {
      const char *name = NULL;
      const char *parent = NULL;
      MetaFrameStyleSet *parent_set;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "name", &name, "parent", &parent,
                              NULL))
        return;

      if (name == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"%s\" attribute on <%s> element"),
                     "name", element_name);
          return;
        }

      if (meta_theme_lookup_style_set (info->theme, name))
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("<%s> name \"%s\" used a second time"),
                     element_name, name);
          return;
        }

      parent_set = NULL;
      if (parent)
        {
          parent_set = meta_theme_lookup_style_set (info->theme, parent);
          if (parent_set == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("<%s> parent \"%s\" has not been defined"),
                         element_name, parent);
              return;
            }
        }

      g_assert (info->style_set == NULL);

      info->style_set = meta_frame_style_set_new (parent_set);

      meta_theme_insert_style_set (info->theme, name, info->style_set);

      push_state (info, STATE_FRAME_STYLE_SET);
    }
  else if (ELEMENT_IS ("window"))
    {
      const char *type_name = NULL;
      const char *style_set_name = NULL;
      MetaFrameStyleSet *style_set;
      MetaFrameType type;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "type", &type_name, "style_set", &style_set_name,
                              NULL))
        return;

      if (type_name == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"%s\" attribute on <%s> element"),
                     "type", element_name);
          return;
        }

      if (style_set_name == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"%s\" attribute on <%s> element"),
                     "style_set", element_name);
          return;
        }

      type = meta_frame_type_from_string (type_name);

      if (type == META_FRAME_TYPE_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Unknown type \"%s\" on <%s> element"),
                     type_name, element_name);
          return;
        }

      style_set = meta_theme_lookup_style_set (info->theme,
                                               style_set_name);

      if (style_set == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Unknown style_set \"%s\" on <%s> element"),
                     style_set_name, element_name);
          return;
        }

      if (info->theme->style_sets_by_type[type] != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Window type \"%s\" has already been assigned a style set"),
                     type_name);
          return;
        }

      meta_frame_style_set_ref (style_set);
      info->theme->style_sets_by_type[type] = style_set;

      push_state (info, STATE_WINDOW);
    }
  else if (ELEMENT_IS ("menu_icon"))
    {
      const char *function = NULL;
      const char *state = NULL;
      const char *draw_ops = NULL;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "function", &function,
                              "state", &state,
                              "draw_ops", &draw_ops,
                              NULL))
        return;

      if (function == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"%s\" attribute on <%s> element"),
                     "function", element_name);
          return;
        }

      if (state == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"%s\" attribute on <%s> element"),
                     "state", element_name);
          return;
        }
      
      info->menu_icon_type = meta_menu_icon_type_from_string (function);
      if (info->menu_icon_type == META_BUTTON_TYPE_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Unknown function \"%s\" for menu icon"),
                     function);
          return;
        }

      info->menu_icon_state = meta_gtk_state_from_string (state);
      if (((int) info->menu_icon_state) == -1)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Unknown state \"%s\" for menu icon"),
                     state);
          return;
        }
      
      if (info->theme->menu_icons[info->menu_icon_type][info->menu_icon_state] != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Theme already has a menu icon for function %s state %s"),
                     function, state);
          return;
        }

      g_assert (info->op_list == NULL);
      
      if (draw_ops)
        {
          MetaDrawOpList *op_list;

          op_list = meta_theme_lookup_draw_op_list (info->theme,
                                                    draw_ops);

          if (op_list == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("No <draw_ops> with the name \"%s\" has been defined"),
                         draw_ops);
              return;
            }

          meta_draw_op_list_ref (op_list);
          info->op_list = op_list;
        }

      push_state (info, STATE_MENU_ICON);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "metacity_theme");
    }
}

static void
parse_info_element (GMarkupParseContext  *context,
                    const gchar          *element_name,
                    const gchar         **attribute_names,
                    const gchar         **attribute_values,
                    ParseInfo            *info,
                    GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_INFO);

  if (ELEMENT_IS ("name"))
    {
      if (!check_no_attributes (context, element_name,
                                attribute_names, attribute_values,
                                error))
        return;

      push_state (info, STATE_NAME);
    }
  else if (ELEMENT_IS ("author"))
    {
      if (!check_no_attributes (context, element_name,
                                attribute_names, attribute_values,
                                error))
        return;

      push_state (info, STATE_AUTHOR);
    }
  else if (ELEMENT_IS ("copyright"))
    {
      if (!check_no_attributes (context, element_name,
                                attribute_names, attribute_values,
                                error))
        return;

      push_state (info, STATE_COPYRIGHT);
    }
  else if (ELEMENT_IS ("description"))
    {
      if (!check_no_attributes (context, element_name,
                                attribute_names, attribute_values,
                                error))
        return;

      push_state (info, STATE_DESCRIPTION);
    }
  else if (ELEMENT_IS ("date"))
    {
      if (!check_no_attributes (context, element_name,
                                attribute_names, attribute_values,
                                error))
        return;

      push_state (info, STATE_DATE);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "info");
    }
}

static void
parse_distance (GMarkupParseContext  *context,
                const gchar          *element_name,
                const gchar         **attribute_names,
                const gchar         **attribute_values,
                ParseInfo            *info,
                GError              **error)
{
  const char *name;
  const char *value;
  int val;
  
  if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                          error,
                          "name", &name, "value", &value,
                          NULL))
    return;

  if (name == NULL)
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("No \"name\" attribute on element <%s>"), element_name);
      return;
    }

  if (value == NULL)
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("No \"value\" attribute on element <%s>"), element_name);
      return;
    }

  val = 0;
  if (!parse_positive_integer (value, &val, context, error))
    return;

  g_assert (val >= 0); /* yeah, "non-negative" not "positive" get over it */
  g_assert (info->layout);

  if (strcmp (name, "left_width") == 0)
    info->layout->left_width = val;
  else if (strcmp (name, "right_width") == 0)
    info->layout->right_width = val;
  else if (strcmp (name, "bottom_height") == 0)
    info->layout->bottom_height = val;
  else if (strcmp (name, "title_vertical_pad") == 0)
    info->layout->title_vertical_pad = val;
  else if (strcmp (name, "right_titlebar_edge") == 0)
    info->layout->right_titlebar_edge = val;
  else if (strcmp (name, "left_titlebar_edge") == 0)
    info->layout->left_titlebar_edge = val;
  else if (strcmp (name, "button_width") == 0)
    info->layout->button_width = val;
  else if (strcmp (name, "button_height") == 0)
    info->layout->button_height = val;
  else
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Distance \"%s\" is unknown"), name);
      return;
    }
}

static void
parse_border (GMarkupParseContext  *context,
              const gchar          *element_name,
              const gchar         **attribute_names,
              const gchar         **attribute_values,
              ParseInfo            *info,
              GError              **error)
{
  const char *name;
  const char *top;
  const char *bottom;
  const char *left;
  const char *right;
  int top_val;
  int bottom_val;
  int left_val;
  int right_val;
  GtkBorder *border;
  
  if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                          error,
                          "name", &name,
                          "top", &top,
                          "bottom", &bottom,
                          "left", &left,
                          "right", &right,
                          NULL))
    return;
  
  if (name == NULL)
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("No \"name\" attribute on element <%s>"), element_name);
      return;
    }

  if (top == NULL)
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("No \"top\" attribute on element <%s>"), element_name);
      return;
    }

  if (bottom == NULL)
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("No \"bottom\" attribute on element <%s>"), element_name);
      return;
    }

  if (left == NULL)
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("No \"left\" attribute on element <%s>"), element_name);
      return;
    }

  if (right == NULL)
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("No \"right\" attribute on element <%s>"), element_name);
      return;
    }

  top_val = 0;
  if (!parse_positive_integer (top, &top_val, context, error))
    return;

  bottom_val = 0;
  if (!parse_positive_integer (bottom, &bottom_val, context, error))
    return;

  left_val = 0;
  if (!parse_positive_integer (left, &left_val, context, error))
    return;

  right_val = 0;
  if (!parse_positive_integer (right, &right_val, context, error))
    return;
  
  g_assert (info->layout);

  border = NULL;
  
  if (strcmp (name, "title_border") == 0)
    border = &info->layout->title_border;
  else if (strcmp (name, "button_border") == 0)
    border = &info->layout->button_border;

  if (border == NULL)
    {
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Border \"%s\" is unknown"), name);
      return;
    }

  border->top = top_val;
  border->bottom = bottom_val;
  border->left = left_val;
  border->right = right_val;
}

static void
parse_geometry_element (GMarkupParseContext  *context,
                        const gchar          *element_name,
                        const gchar         **attribute_names,
                        const gchar         **attribute_values,
                        ParseInfo            *info,
                        GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_FRAME_GEOMETRY);

  if (ELEMENT_IS ("distance"))
    {
      parse_distance (context, element_name,
                      attribute_names, attribute_values,
                      info, error);
      push_state (info, STATE_DISTANCE);
    }
  else if (ELEMENT_IS ("border"))
    {
      parse_border (context, element_name,
                    attribute_names, attribute_values,
                    info, error);
      push_state (info, STATE_BORDER);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "frame_geometry");
    }
}

static gboolean
check_expression (const char          *expr,
                  gboolean             has_object,
                  MetaTheme           *theme,
                  GMarkupParseContext *context,
                  GError             **error)
{
  MetaPositionExprEnv env;
  int x, y;

  /* We set it all to 0 to try and catch divide-by-zero screwups.
   * it's possible we should instead guarantee that widths and heights
   * are at least 1.
   */
  
  env.x = 0;
  env.y = 0;
  env.width = 0;
  env.height = 0;
  if (has_object)
    {
      env.object_width = 0;
      env.object_height = 0;
    }
  else
    {
      env.object_width = -1;
      env.object_height = -1;
    }

  env.left_width = 0;
  env.right_width = 0;
  env.top_height = 0;
  env.bottom_height = 0;
  env.title_width = 0;
  env.title_height = 0;
  
  env.icon_width = 0;
  env.icon_height = 0;
  env.mini_icon_width = 0;
  env.mini_icon_height = 0;
  env.theme = theme;
  
  if (!meta_parse_position_expression (expr,
                                       &env,
                                       &x, &y,
                                       error))
    {
      add_context_to_error (error, context);
      return FALSE;
    }

  return TRUE;
}

static char*
optimize_expression (MetaTheme  *theme,
                     const char *expr)
{
  /* We aren't expecting an error here, since we already
   * did check_expression
   */
  return meta_theme_replace_constants (theme, expr, NULL);
}

static void
parse_draw_op_element (GMarkupParseContext  *context,
                       const gchar          *element_name,
                       const gchar         **attribute_names,
                       const gchar         **attribute_values,
                       ParseInfo            *info,
                       GError              **error)
{  
  g_return_if_fail (peek_state (info) == STATE_DRAW_OPS);

  if (ELEMENT_IS ("line"))
    {
      MetaDrawOp *op;
      const char *color;
      const char *x1;
      const char *y1;
      const char *x2;
      const char *y2;
      const char *dash_on_length;
      const char *dash_off_length;
      const char *width;
      MetaColorSpec *color_spec;
      int dash_on_val;
      int dash_off_val;
      int width_val;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "color", &color,
                              "x1", &x1, "y1", &y1,
                              "x2", &x2, "y2", &y2,
                              "dash_on_length", &dash_on_length,
                              "dash_off_length", &dash_off_length,
                              "width", &width,
                              NULL))
        return;

      if (color == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"color\" attribute on element <%s>"), element_name);
          return;
        }
      
      if (x1 == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"x1\" attribute on element <%s>"), element_name);
          return;
        }

      if (y1 == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"y1\" attribute on element <%s>"), element_name);
          return;
        }

      if (x2 == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"x2\" attribute on element <%s>"), element_name);
          return;
        }

      if (y2 == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"y2\" attribute on element <%s>"), element_name);
          return;
        }

      if (!check_expression (x1, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y1, FALSE, info->theme, context, error))
        return;

      if (!check_expression (x2, FALSE, info->theme, context, error))
        return;
      
      if (!check_expression (y2, FALSE, info->theme, context, error))
        return;
      
      dash_on_val = 0;
      if (dash_on_length &&
          !parse_positive_integer (dash_on_length, &dash_on_val, context, error))
        return;

      dash_off_val = 0;
      if (dash_off_length &&
          !parse_positive_integer (dash_off_length, &dash_off_val, context, error))
        return;

      width_val = 0;
      if (width &&
          !parse_positive_integer (width, &width_val, context, error))
        return;

      /* Check last so we don't have to free it when other
       * stuff fails
       */
      color_spec = meta_color_spec_new_from_string (color, error);
      if (color_spec == NULL)
        {
          add_context_to_error (error, context);
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_LINE);

      op->data.line.color_spec = color_spec;
      op->data.line.x1 = optimize_expression (info->theme, x1);
      op->data.line.y1 = optimize_expression (info->theme, y1);
      op->data.line.x2 = optimize_expression (info->theme, x2);
      op->data.line.y2 = optimize_expression (info->theme, y2);
      op->data.line.width = width_val;
      op->data.line.dash_on_length = dash_on_val;
      op->data.line.dash_off_length = dash_off_val;

      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_LINE);
    }
  else if (ELEMENT_IS ("rectangle"))
    {
      MetaDrawOp *op;
      const char *color;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *filled;
      gboolean filled_val;
      MetaColorSpec *color_spec;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "color", &color,
                              "x", &x, "y", &y,
                              "width", &width, "height", &height,
                              "filled", &filled,
                              NULL))
        return;

      if (color == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"color\" attribute on element <%s>"), element_name);
          return;
        }
      
      if (x == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"x\" attribute on element <%s>"), element_name);
          return;
        }

      if (y == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"y\" attribute on element <%s>"), element_name);
          return;
        }

      if (width == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"width\" attribute on element <%s>"), element_name);
          return;
        }

      if (height == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"height\" attribute on element <%s>"), element_name);
          return;
        }

      if (!check_expression (x, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y, FALSE, info->theme, context, error))
        return;

      if (!check_expression (width, FALSE, info->theme, context, error))
        return;
      
      if (!check_expression (height, FALSE, info->theme, context, error))
        return;

      filled_val = FALSE;
      if (filled && !parse_boolean (filled, &filled_val, context, error))
        return;
      
      /* Check last so we don't have to free it when other
       * stuff fails
       */
      color_spec = meta_color_spec_new_from_string (color, error);
      if (color_spec == NULL)
        {
          add_context_to_error (error, context);
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_RECTANGLE);

      op->data.rectangle.color_spec = color_spec;
      op->data.rectangle.x = optimize_expression (info->theme, x);
      op->data.rectangle.y = optimize_expression (info->theme, y);
      op->data.rectangle.width = optimize_expression (info->theme, width);
      op->data.rectangle.height = optimize_expression (info->theme, height);
      op->data.rectangle.filled = filled_val;

      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_RECTANGLE);
    }
  else if (ELEMENT_IS ("arc"))
    {
      MetaDrawOp *op;
      const char *color;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *filled;
      const char *start_angle;
      const char *extent_angle;
      gboolean filled_val;
      double start_angle_val;
      double extent_angle_val;
      MetaColorSpec *color_spec;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "color", &color,
                              "x", &x, "y", &y,
                              "width", &width, "height", &height,
                              "filled", &filled,
                              "start_angle", &start_angle,
                              "extent_angle", &extent_angle,
                              NULL))
        return;

      if (color == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"color\" attribute on element <%s>"), element_name);
          return;
        }
      
      if (x == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"x\" attribute on element <%s>"), element_name);
          return;
        }

      if (y == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"y\" attribute on element <%s>"), element_name);
          return;
        }

      if (width == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"width\" attribute on element <%s>"), element_name);
          return;
        }

      if (height == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"height\" attribute on element <%s>"), element_name);
          return;
        }

      if (start_angle == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"start_angle\" attribute on element <%s>"), element_name);
          return;
        }

      if (extent_angle == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"extent_angle\" attribute on element <%s>"), element_name);
          return;
        }
      
      if (!check_expression (x, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y, FALSE, info->theme, context, error))
        return;

      if (!check_expression (width, FALSE, info->theme, context, error))
        return;
      
      if (!check_expression (height, FALSE, info->theme, context, error))
        return;

      if (!parse_angle (start_angle, &start_angle_val, context, error))
        return;

      if (!parse_angle (extent_angle, &extent_angle_val, context, error))
        return;
      
      filled_val = FALSE;
      if (filled && !parse_boolean (filled, &filled_val, context, error))
        return;
      
      /* Check last so we don't have to free it when other
       * stuff fails
       */
      color_spec = meta_color_spec_new_from_string (color, error);
      if (color_spec == NULL)
        {
          add_context_to_error (error, context);
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_ARC);

      op->data.arc.color_spec = color_spec;
      op->data.arc.x = optimize_expression (info->theme, x);
      op->data.arc.y = optimize_expression (info->theme, y);
      op->data.arc.width = optimize_expression (info->theme, width);
      op->data.arc.height = optimize_expression (info->theme, height);
      op->data.arc.filled = filled_val;
      op->data.arc.start_angle = start_angle_val;
      op->data.arc.extent_angle = extent_angle_val;
      
      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_ARC);
    }
  else if (ELEMENT_IS ("clip"))
    {
      MetaDrawOp *op;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "x", &x, "y", &y,
                              "width", &width, "height", &height,
                              NULL))
        return;
      
      if (x == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"x\" attribute on element <%s>"), element_name);
          return;
        }

      if (y == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"y\" attribute on element <%s>"), element_name);
          return;
        }

      if (width == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"width\" attribute on element <%s>"), element_name);
          return;
        }

      if (height == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"height\" attribute on element <%s>"), element_name);
          return;
        }

      if (!check_expression (x, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y, FALSE, info->theme, context, error))
        return;

      if (!check_expression (width, FALSE, info->theme, context, error))
        return;
      
      if (!check_expression (height, FALSE, info->theme, context, error))
        return;
      
      op = meta_draw_op_new (META_DRAW_CLIP);

      op->data.clip.x = optimize_expression (info->theme, x);
      op->data.clip.y = optimize_expression (info->theme, y);
      op->data.clip.width = optimize_expression (info->theme, width);
      op->data.clip.height = optimize_expression (info->theme, height);

      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_CLIP);
    }
  else if (ELEMENT_IS ("tint"))
    {
      MetaDrawOp *op;
      const char *color;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *alpha;
      double alpha_val;
      MetaColorSpec *color_spec;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "color", &color,
                              "x", &x, "y", &y,
                              "width", &width, "height", &height,
                              "alpha", &alpha,
                              NULL))
        return;

      if (color == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"color\" attribute on element <%s>"), element_name);
          return;
        }
      
      if (x == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"x\" attribute on element <%s>"), element_name);
          return;
        }

      if (y == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"y\" attribute on element <%s>"), element_name);
          return;
        }

      if (width == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"width\" attribute on element <%s>"), element_name);
          return;
        }

      if (height == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"height\" attribute on element <%s>"), element_name);
          return;
        }

      if (alpha == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"alpha\" attribute on element <%s>"), element_name);
          return;
        }
      
      if (!check_expression (x, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y, FALSE, info->theme, context, error))
        return;

      if (!check_expression (width, FALSE, info->theme, context, error))
        return;
      
      if (!check_expression (height, FALSE, info->theme, context, error))
        return;

      if (!parse_alpha (alpha, &alpha_val, context, error))
        return;
      
      /* Check last so we don't have to free it when other
       * stuff fails
       */
      color_spec = meta_color_spec_new_from_string (color, error);
      if (color_spec == NULL)
        {
          add_context_to_error (error, context);
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_TINT);

      op->data.tint.color_spec = color_spec;
      op->data.tint.x = optimize_expression (info->theme, x);
      op->data.tint.y = optimize_expression (info->theme, y);
      op->data.tint.width = optimize_expression (info->theme, width);
      op->data.tint.height = optimize_expression (info->theme, height);
      op->data.tint.alpha = alpha_val;

      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_TINT);
    }
  else if (ELEMENT_IS ("gradient"))
    {
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *type;
      const char *alpha;
      double alpha_val;
      MetaGradientType type_val;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "type", &type,
                              "x", &x, "y", &y,
                              "width", &width, "height", &height,
                              "alpha", &alpha,
                              NULL))
        return;

      if (type == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"type\" attribute on element <%s>"), element_name);
          return;
        }
      
      if (x == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"x\" attribute on element <%s>"), element_name);
          return;
        }

      if (y == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"y\" attribute on element <%s>"), element_name);
          return;
        }

      if (width == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"width\" attribute on element <%s>"), element_name);
          return;
        }

      if (height == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"height\" attribute on element <%s>"), element_name);
          return;
        }

      if (!check_expression (x, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y, FALSE, info->theme, context, error))
        return;

      if (!check_expression (width, FALSE, info->theme, context, error))
        return;
      
      if (!check_expression (height, FALSE, info->theme, context, error))
        return;

      alpha_val = 1.0;
      if (alpha && !parse_alpha (alpha, &alpha_val, context, error))
        return;
      
      type_val = meta_gradient_type_from_string (type);
      if (type_val == META_GRADIENT_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Did not understand value \"%s\" for type of gradient"),
                     type);
          return;
        }

      g_assert (info->op == NULL);
      info->op = meta_draw_op_new (META_DRAW_GRADIENT);

      info->op->data.gradient.x = optimize_expression (info->theme, x);
      info->op->data.gradient.y = optimize_expression (info->theme, y);
      info->op->data.gradient.width = optimize_expression (info->theme, width);
      info->op->data.gradient.height = optimize_expression (info->theme, height);

      info->op->data.gradient.gradient_spec =
        meta_gradient_spec_new (type_val);

      info->op->data.gradient.alpha = alpha_val;
      
      push_state (info, STATE_GRADIENT);

      /* op gets appended on close tag */
    }
  else if (ELEMENT_IS ("image"))
    {
      MetaDrawOp *op;
      const char *filename;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *alpha;
      const char *colorize;
      double alpha_val;
      GdkPixbuf *pixbuf;
      MetaColorSpec *colorize_spec = NULL;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "x", &x, "y", &y,
                              "width", &width, "height", &height,
                              "alpha", &alpha, "filename", &filename,
			      "colorize", &colorize,
                              NULL))
        return;
      
      if (x == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"x\" attribute on element <%s>"), element_name);
          return;
        }

      if (y == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"y\" attribute on element <%s>"), element_name);
          return;
        }

      if (width == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"width\" attribute on element <%s>"), element_name);
          return;
        }

      if (height == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"height\" attribute on element <%s>"), element_name);
          return;
        }

      if (filename == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"filename\" attribute on element <%s>"), element_name);
          return;
        }
      
      if (!check_expression (x, TRUE, info->theme, context, error))
        return;

      if (!check_expression (y, TRUE, info->theme, context, error))
        return;

      if (!check_expression (width, TRUE, info->theme, context, error))
        return;
      
      if (!check_expression (height, TRUE, info->theme, context, error))
        return;

      alpha_val = 1.0;
      if (alpha && !parse_alpha (alpha, &alpha_val, context, error))
        return;
      
      /* Check last so we don't have to free it when other
       * stuff fails
       */
      pixbuf = meta_theme_load_image (info->theme, filename, error);

      if (pixbuf == NULL)
        {
          add_context_to_error (error, context);
          return;
        }

      if (colorize)
	{
	  colorize_spec = meta_color_spec_new_from_string (colorize, error);

	  if (colorize_spec == NULL)
	    {
	      add_context_to_error (error, context);
              g_object_unref (G_OBJECT (pixbuf));
	      return;
	    }
	}
      
      op = meta_draw_op_new (META_DRAW_IMAGE);

      op->data.image.pixbuf = pixbuf;
      op->data.image.colorize_spec = colorize_spec;
      op->data.image.x = optimize_expression (info->theme, x);
      op->data.image.y = optimize_expression (info->theme, y);
      op->data.image.width = optimize_expression (info->theme, width);
      op->data.image.height = optimize_expression (info->theme, height);
      op->data.image.alpha = alpha_val;

      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_IMAGE);
    }
  else if (ELEMENT_IS ("gtk_arrow"))
    {
      MetaDrawOp *op;
      const char *state;
      const char *shadow;
      const char *arrow;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *filled;
      gboolean filled_val;
      GtkStateType state_val;
      GtkShadowType shadow_val;
      GtkArrowType arrow_val;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "state", &state,
                              "shadow", &shadow,
                              "arrow", &arrow,
                              "x", &x, "y", &y,
                              "width", &width, "height", &height,
                              "filled", &filled,
                              NULL))
        return;

      if (state == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"state\" attribute on element <%s>"), element_name);
          return;
        }

      if (shadow == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"shadow\" attribute on element <%s>"), element_name);
          return;
        }

      if (arrow == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"arrow\" attribute on element <%s>"), element_name);
          return;
        }
      
      if (x == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"x\" attribute on element <%s>"), element_name);
          return;
        }

      if (y == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"y\" attribute on element <%s>"), element_name);
          return;
        }

      if (width == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"width\" attribute on element <%s>"), element_name);
          return;
        }

      if (height == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"height\" attribute on element <%s>"), element_name);
          return;
        }

      if (!check_expression (x, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y, FALSE, info->theme, context, error))
        return;

      if (!check_expression (width, FALSE, info->theme, context, error))
        return;
      
      if (!check_expression (height, FALSE, info->theme, context, error))
        return;

      filled_val = TRUE;
      if (filled && !parse_boolean (filled, &filled_val, context, error))
        return;

      state_val = meta_gtk_state_from_string (state);
      if (((int) state_val) == -1)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Did not understand state \"%s\" for <%s> element"),
                     state, element_name);
          return;
        }

      shadow_val = meta_gtk_shadow_from_string (shadow);
      if (((int) shadow_val) == -1)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Did not understand shadow \"%s\" for <%s> element"),
                     shadow, element_name);
          return;
        }

      arrow_val = meta_gtk_arrow_from_string (arrow);
      if (((int) arrow_val) == -1)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Did not understand arrow \"%s\" for <%s> element"),
                     arrow, element_name);
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_GTK_ARROW);

      op->data.gtk_arrow.x = optimize_expression (info->theme, x);
      op->data.gtk_arrow.y = optimize_expression (info->theme, y);
      op->data.gtk_arrow.width = optimize_expression (info->theme, width);
      op->data.gtk_arrow.height = optimize_expression (info->theme, height);
      op->data.gtk_arrow.filled = filled_val;
      op->data.gtk_arrow.state = state_val;
      op->data.gtk_arrow.shadow = shadow_val;
      op->data.gtk_arrow.arrow = arrow_val;
      
      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_GTK_ARROW);
    }
  else if (ELEMENT_IS ("gtk_box"))
    {
      MetaDrawOp *op;
      const char *state;
      const char *shadow;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      GtkStateType state_val;
      GtkShadowType shadow_val;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "state", &state,
                              "shadow", &shadow,
                              "x", &x, "y", &y,
                              "width", &width, "height", &height,
                              NULL))
        return;

      if (state == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"state\" attribute on element <%s>"), element_name);
          return;
        }

      if (shadow == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"shadow\" attribute on element <%s>"), element_name);
          return;
        }
      
      if (x == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"x\" attribute on element <%s>"), element_name);
          return;
        }

      if (y == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"y\" attribute on element <%s>"), element_name);
          return;
        }

      if (width == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"width\" attribute on element <%s>"), element_name);
          return;
        }

      if (height == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"height\" attribute on element <%s>"), element_name);
          return;
        }

      if (!check_expression (x, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y, FALSE, info->theme, context, error))
        return;

      if (!check_expression (width, FALSE, info->theme, context, error))
        return;
      
      if (!check_expression (height, FALSE, info->theme, context, error))
        return;

      state_val = meta_gtk_state_from_string (state);
      if (((int) state_val) == -1)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Did not understand state \"%s\" for <%s> element"),
                     state, element_name);
          return;
        }

      shadow_val = meta_gtk_shadow_from_string (shadow);
      if (((int) shadow_val) == -1)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Did not understand shadow \"%s\" for <%s> element"),
                     shadow, element_name);
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_GTK_BOX);

      op->data.gtk_box.x = optimize_expression (info->theme, x);
      op->data.gtk_box.y = optimize_expression (info->theme, y);
      op->data.gtk_box.width = optimize_expression (info->theme, width);
      op->data.gtk_box.height = optimize_expression (info->theme, height);
      op->data.gtk_box.state = state_val;
      op->data.gtk_box.shadow = shadow_val;
      
      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_GTK_BOX);
    }
  else if (ELEMENT_IS ("gtk_vline"))
    {
      MetaDrawOp *op;
      const char *state;
      const char *x;
      const char *y1;
      const char *y2;
      GtkStateType state_val;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "state", &state,
                              "x", &x, "y1", &y1, "y2", &y2,
                              NULL))
        return;

      if (state == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"state\" attribute on element <%s>"), element_name);
          return;
        }
      
      if (x == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"x\" attribute on element <%s>"), element_name);
          return;
        }

      if (y1 == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"y1\" attribute on element <%s>"), element_name);
          return;
        }

      if (y2 == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"y2\" attribute on element <%s>"), element_name);
          return;
        }
      
      if (!check_expression (x, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y1, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y2, FALSE, info->theme, context, error))
        return;

      state_val = meta_gtk_state_from_string (state);
      if (((int) state_val) == -1)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Did not understand state \"%s\" for <%s> element"),
                     state, element_name);
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_GTK_VLINE);

      op->data.gtk_vline.x = optimize_expression (info->theme, x);
      op->data.gtk_vline.y1 = optimize_expression (info->theme, y1);
      op->data.gtk_vline.y2 = optimize_expression (info->theme, y2);
      op->data.gtk_vline.state = state_val;
      
      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_GTK_VLINE);
    }
  else if (ELEMENT_IS ("icon"))
    {
      MetaDrawOp *op;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *alpha;
      double alpha_val;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "x", &x, "y", &y,
                              "width", &width, "height", &height,
                              "alpha", &alpha,
                              NULL))
        return;
      
      if (x == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"x\" attribute on element <%s>"), element_name);
          return;
        }

      if (y == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"y\" attribute on element <%s>"), element_name);
          return;
        }

      if (width == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"width\" attribute on element <%s>"), element_name);
          return;
        }

      if (height == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"height\" attribute on element <%s>"), element_name);
          return;
        }
      
      if (!check_expression (x, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y, FALSE, info->theme, context, error))
        return;

      if (!check_expression (width, FALSE, info->theme, context, error))
        return;
      
      if (!check_expression (height, FALSE, info->theme, context, error))
        return;

      alpha_val = 1.0;
      if (alpha && !parse_alpha (alpha, &alpha_val, context, error))
        return;
      
      op = meta_draw_op_new (META_DRAW_ICON);

      op->data.icon.x = optimize_expression (info->theme, x);
      op->data.icon.y = optimize_expression (info->theme, y);
      op->data.icon.width = optimize_expression (info->theme, width);
      op->data.icon.height = optimize_expression (info->theme, height);
      op->data.icon.alpha = alpha_val;

      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_ICON);
    }
  else if (ELEMENT_IS ("title"))
    {
      MetaDrawOp *op;
      const char *color;
      const char *x;
      const char *y;
      MetaColorSpec *color_spec;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "color", &color,
                              "x", &x, "y", &y,
                              NULL))
        return;

      if (color == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"color\" attribute on element <%s>"), element_name);
          return;
        }
      
      if (x == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"x\" attribute on element <%s>"), element_name);
          return;
        }

      if (y == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"y\" attribute on element <%s>"), element_name);
          return;
        }
      
      if (!check_expression (x, FALSE, info->theme, context, error))
        return;

      if (!check_expression (y, FALSE, info->theme, context, error))
        return;
      
      /* Check last so we don't have to free it when other
       * stuff fails
       */
      color_spec = meta_color_spec_new_from_string (color, error);
      if (color_spec == NULL)
        {
          add_context_to_error (error, context);
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_TITLE);

      op->data.title.color_spec = color_spec;
      op->data.title.x = optimize_expression (info->theme, x);
      op->data.title.y = optimize_expression (info->theme, y);

      g_assert (info->op_list);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_TITLE);
    }
  else if (ELEMENT_IS ("include"))
    {
      MetaDrawOp *op;
      const char *name;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      MetaDrawOpList *op_list;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "x", &x, "y", &y,
                              "width", &width, "height", &height,
                              "name", &name,
                              NULL))
        return;

      if (name == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"%s\" attribute on element <%s>"), "name", element_name);
          return;
        }

      /* x/y/width/height default to 0,0,width,height - should
       * probably do this for all the draw ops
       */
      
      if (x && !check_expression (x, FALSE, info->theme, context, error))
        return;

      if (y && !check_expression (y, FALSE, info->theme, context, error))
        return;

      if (width && !check_expression (width, FALSE, info->theme, context, error))
        return;
      
      if (height && !check_expression (height, FALSE, info->theme, context, error))
        return;

      op_list = meta_theme_lookup_draw_op_list (info->theme,
                                                name);
      if (op_list == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("No <draw_ops> called \"%s\" has been defined"),
                     name);
          return;
        }

      g_assert (info->op_list);
      
      if (op_list == info->op_list ||
          meta_draw_op_list_contains (op_list, info->op_list))
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Including draw_ops \"%s\" here would create a circular reference"),
                     name);
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_OP_LIST);

      meta_draw_op_list_ref (op_list);
      op->data.op_list.op_list = op_list;      
      op->data.op_list.x = x ? optimize_expression (info->theme, x) :
        g_strdup ("0");
      op->data.op_list.y = y ? optimize_expression (info->theme, y) :
        g_strdup ("0");
      op->data.op_list.width = width ? optimize_expression (info->theme, width) :
        g_strdup ("width");
      op->data.op_list.height = height ?  optimize_expression (info->theme, height) :
        g_strdup ("height");
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_INCLUDE);
    }
  else if (ELEMENT_IS ("tile"))
    {
      MetaDrawOp *op;
      const char *name;
      const char *x;
      const char *y;
      const char *width;
      const char *height;
      const char *tile_xoffset;
      const char *tile_yoffset;
      const char *tile_width;
      const char *tile_height;
      MetaDrawOpList *op_list;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "x", &x, "y", &y,
                              "width", &width, "height", &height,
                              "name", &name,
                              "tile_xoffset", &tile_xoffset,
                              "tile_yoffset", &tile_yoffset,
                              "tile_width", &tile_width,
                              "tile_height", &tile_height,
                              NULL))
        return;

      if (name == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"%s\" attribute on element <%s>"), "name", element_name);
          return;
        }

      if (tile_width == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"%s\" attribute on element <%s>"), "tile_width", element_name);
          return;
        }

      if (tile_height == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"%s\" attribute on element <%s>"), "tile_height", element_name);
          return;
        }

      /* These default to 0 */
      if (tile_xoffset && !check_expression (tile_xoffset, FALSE, info->theme, context, error))
        return;

      if (tile_yoffset && !check_expression (tile_xoffset, FALSE, info->theme, context, error))
        return;
      
      /* x/y/width/height default to 0,0,width,height - should
       * probably do this for all the draw ops
       */
      
      if (x && !check_expression (x, FALSE, info->theme, context, error))
        return;

      if (y && !check_expression (y, FALSE, info->theme, context, error))
        return;

      if (width && !check_expression (width, FALSE, info->theme, context, error))
        return;
      
      if (height && !check_expression (height, FALSE, info->theme, context, error))
        return;

      if (!check_expression (tile_width, FALSE, info->theme, context, error))
        return;

      if (!check_expression (tile_height, FALSE, info->theme, context, error))
        return;
      
      op_list = meta_theme_lookup_draw_op_list (info->theme,
                                                name);
      if (op_list == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("No <draw_ops> called \"%s\" has been defined"),
                     name);
          return;
        }

      g_assert (info->op_list);
      
      if (op_list == info->op_list ||
          meta_draw_op_list_contains (op_list, info->op_list))
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("Including draw_ops \"%s\" here would create a circular reference"),
                     name);
          return;
        }
      
      op = meta_draw_op_new (META_DRAW_TILE);

      meta_draw_op_list_ref (op_list);
      op->data.tile.op_list = op_list;      
      op->data.tile.x = x ? optimize_expression (info->theme, x) :
        g_strdup ("0");
      op->data.tile.y = y ? optimize_expression (info->theme, y) :
        g_strdup ("0");
      op->data.tile.width = width ? optimize_expression (info->theme, width) :
        g_strdup ("width");
      op->data.tile.height = height ?  optimize_expression (info->theme, height) :
        g_strdup ("height");
      op->data.tile.tile_xoffset = tile_xoffset ?
        optimize_expression (info->theme, tile_xoffset) :
        g_strdup ("0");
      op->data.tile.tile_yoffset = tile_yoffset ?
        optimize_expression (info->theme, tile_yoffset) :
        g_strdup ("0");
      op->data.tile.tile_width = optimize_expression (info->theme, tile_width);
      op->data.tile.tile_height = optimize_expression (info->theme, tile_height);
      
      meta_draw_op_list_append (info->op_list, op);

      push_state (info, STATE_TILE);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "draw_ops");
    }
}

static void
parse_gradient_element (GMarkupParseContext  *context,
                        const gchar          *element_name,
                        const gchar         **attribute_names,
                        const gchar         **attribute_values,
                        ParseInfo            *info,
                        GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_GRADIENT);

  if (ELEMENT_IS ("color"))
    {
      const char *value = NULL;
      MetaColorSpec *color_spec;

      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "value", &value,
                              NULL))
        return;

      if (value == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"value\" attribute on <%s> element"),
                     element_name);
          return;
        }

      color_spec = meta_color_spec_new_from_string (value, error);
      if (color_spec == NULL)
        {
          add_context_to_error (error, context);
          return;
        }

      g_assert (info->op);
      g_assert (info->op->type == META_DRAW_GRADIENT);
      g_assert (info->op->data.gradient.gradient_spec != NULL);
      info->op->data.gradient.gradient_spec->color_specs =
        g_slist_append (info->op->data.gradient.gradient_spec->color_specs,
                        color_spec);
      
      push_state (info, STATE_COLOR);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "gradient");
    }
}

static void
parse_style_element (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     const gchar         **attribute_names,
                     const gchar         **attribute_values,
                     ParseInfo            *info,
                     GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_FRAME_STYLE);

  g_assert (info->style);
  
  if (ELEMENT_IS ("piece"))
    {
      const char *position = NULL;
      const char *draw_ops = NULL;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "position", &position,
                              "draw_ops", &draw_ops,
                              NULL))
        return;

      if (position == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"position\" attribute on <%s> element"),
                     element_name);
          return;
        }

      info->piece = meta_frame_piece_from_string (position);
      if (info->piece == META_FRAME_PIECE_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Unknown position \"%s\" for frame piece"),
                     position);
          return;
        }
      
      if (info->style->pieces[info->piece] != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Frame style already has a piece at position %s"),
                     position);
          return;
        }

      g_assert (info->op_list == NULL);
      
      if (draw_ops)
        {
          MetaDrawOpList *op_list;

          op_list = meta_theme_lookup_draw_op_list (info->theme,
                                                    draw_ops);

          if (op_list == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("No <draw_ops> with the name \"%s\" has been defined"),
                         draw_ops);
              return;
            }

          meta_draw_op_list_ref (op_list);
          info->op_list = op_list;
        }
      
      push_state (info, STATE_PIECE);
    }
  else if (ELEMENT_IS ("button"))
    {
      const char *function = NULL;
      const char *state = NULL;
      const char *draw_ops = NULL;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "function", &function,
                              "state", &state,
                              "draw_ops", &draw_ops,
                              NULL))
        return;

      if (function == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"function\" attribute on <%s> element"),
                     element_name);
          return;
        }

      if (state == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"state\" attribute on <%s> element"),
                     element_name);
          return;
        }
      
      info->button_type = meta_button_type_from_string (function);
      if (info->button_type == META_BUTTON_TYPE_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Unknown function \"%s\" for button"),
                     function);
          return;
        }

      info->button_state = meta_button_state_from_string (state);
      if (info->button_state == META_BUTTON_STATE_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Unknown state \"%s\" for button"),
                     state);
          return;
        }
      
      if (info->style->buttons[info->button_type][info->button_state] != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Frame style already has a button for function %s state %s"),
                     function, state);
          return;
        }

      g_assert (info->op_list == NULL);
      
      if (draw_ops)
        {
          MetaDrawOpList *op_list;

          op_list = meta_theme_lookup_draw_op_list (info->theme,
                                                    draw_ops);

          if (op_list == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("No <draw_ops> with the name \"%s\" has been defined"),
                         draw_ops);
              return;
            }

          meta_draw_op_list_ref (op_list);
          info->op_list = op_list;
        }
      
      push_state (info, STATE_BUTTON);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "frame_style");
    }
}

static void
parse_style_set_element (GMarkupParseContext  *context,
                         const gchar          *element_name,
                         const gchar         **attribute_names,
                         const gchar         **attribute_values,
                         ParseInfo            *info,
                         GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_FRAME_STYLE_SET);

  if (ELEMENT_IS ("frame"))
    {
      const char *focus = NULL;
      const char *state = NULL;
      const char *resize = NULL;
      const char *style = NULL;
      MetaFrameFocus frame_focus;
      MetaFrameState frame_state;
      MetaFrameResize frame_resize;
      MetaFrameStyle *frame_style;
      
      if (!locate_attributes (context, element_name, attribute_names, attribute_values,
                              error,
                              "focus", &focus,
                              "state", &state,
                              "resize", &resize,
                              "style", &style,
                              NULL))
        return;

      if (focus == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"focus\" attribute on <%s> element"),
                     element_name);
          return;
        }

      if (state == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"state\" attribute on <%s> element"),
                     element_name);
          return;
        }
      
      if (style == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No \"style\" attribute on <%s> element"),
                     element_name);
          return;
        }

      frame_focus = meta_frame_focus_from_string (focus);
      if (frame_focus == META_FRAME_FOCUS_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("\"%s\" is not a valid value for focus attribute"),
                     focus);
          return;
        }
      
      frame_state = meta_frame_state_from_string (state);
      if (frame_state == META_FRAME_STATE_LAST)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("\"%s\" is not a valid value for state attribute"),
                     focus);
          return;
        }

      frame_style = meta_theme_lookup_style (info->theme, style);

      if (frame_style == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("A style called \"%s\" has not been defined"),
                     style);
          return;
        }

      if (frame_state == META_FRAME_STATE_NORMAL)
        {
          if (resize == NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("No \"resize\" attribute on <%s> element"),
                         element_name);
              return;
            }

          
          frame_resize = meta_frame_resize_from_string (resize);
          if (frame_resize == META_FRAME_RESIZE_LAST)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("\"%s\" is not a valid value for resize attribute"),
                         focus);
              return;
            }
        }
      else
        {
          if (resize != NULL)
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("Should not have \"resize\" attribute on <%s> element for maximized/shaded states"),
                         element_name);
              return;
            }

          frame_resize = META_FRAME_RESIZE_LAST;
        }
      
      switch (frame_state)
        {
        case META_FRAME_STATE_NORMAL:
          if (info->style_set->normal_styles[frame_resize][frame_focus])
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("Style has already been specified for state %s resize %s focus %s"),
                         state, resize, focus);
              return;
            }
          meta_frame_style_ref (frame_style);
          info->style_set->normal_styles[frame_resize][frame_focus] = frame_style;
          break;
        case META_FRAME_STATE_MAXIMIZED:
          if (info->style_set->maximized_styles[frame_focus])
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("Style has already been specified for state %s focus %s"),
                         state, focus);
              return;
            }
          meta_frame_style_ref (frame_style);
          info->style_set->maximized_styles[frame_focus] = frame_style;
          break;
        case META_FRAME_STATE_SHADED:
          if (info->style_set->shaded_styles[frame_focus])
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("Style has already been specified for state %s focus %s"),
                         state, focus);
              return;
            }
          meta_frame_style_ref (frame_style);
          info->style_set->shaded_styles[frame_focus] = frame_style;
          break;
        case META_FRAME_STATE_MAXIMIZED_AND_SHADED:
          if (info->style_set->maximized_and_shaded_styles[frame_focus])
            {
              set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                         _("Style has already been specified for state %s focus %s"),
                         state, focus);
              return;
            }
          meta_frame_style_ref (frame_style);
          info->style_set->maximized_and_shaded_styles[frame_focus] = frame_style;
          break;
        case META_FRAME_STATE_LAST:
          g_assert_not_reached ();
          break;
        }

      push_state (info, STATE_FRAME);      
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "frame_style_set");
    }
}

static void
parse_piece_element (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     const gchar         **attribute_names,
                     const gchar         **attribute_values,
                     ParseInfo            *info,
                     GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_PIECE);

  if (ELEMENT_IS ("draw_ops"))
    {
      if (info->op_list)
        {
          set_error (error, context,
                     G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Can't have a two draw_ops for a <piece> element (theme specified a draw_ops attribute and also a <draw_ops> element, or specified two elements)"));
          return;
        }
            
      if (!check_no_attributes (context, element_name, attribute_names, attribute_values,
                                error))
        return;

      g_assert (info->op_list == NULL);
      info->op_list = meta_draw_op_list_new (2);

      push_state (info, STATE_DRAW_OPS);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "piece");
    }
}

static void
parse_button_element (GMarkupParseContext  *context,
                      const gchar          *element_name,
                      const gchar         **attribute_names,
                      const gchar         **attribute_values,
                      ParseInfo            *info,
                      GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_BUTTON);
  
  if (ELEMENT_IS ("draw_ops"))
    {
      if (info->op_list)
        {
          set_error (error, context,
                     G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Can't have a two draw_ops for a <button> element (theme specified a draw_ops attribute and also a <draw_ops> element, or specified two elements)"));
          return;
        }
            
      if (!check_no_attributes (context, element_name, attribute_names, attribute_values,
                                error))
        return;

      g_assert (info->op_list == NULL);
      info->op_list = meta_draw_op_list_new (2);

      push_state (info, STATE_DRAW_OPS);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "button");
    }
}

static void
parse_menu_icon_element (GMarkupParseContext  *context,
                         const gchar          *element_name,
                         const gchar         **attribute_names,
                         const gchar         **attribute_values,
                         ParseInfo            *info,
                         GError              **error)
{
  g_return_if_fail (peek_state (info) == STATE_MENU_ICON);

  if (ELEMENT_IS ("draw_ops"))
    {
      if (info->op_list)
        {
          set_error (error, context,
                     G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("Can't have a two draw_ops for a <menu_icon> element (theme specified a draw_ops attribute and also a <draw_ops> element, or specified two elements)"));
          return;
        }
            
      if (!check_no_attributes (context, element_name, attribute_names, attribute_values,
                                error))
        return;

      g_assert (info->op_list == NULL);
      info->op_list = meta_draw_op_list_new (2);

      push_state (info, STATE_DRAW_OPS);
    }
  else
    {
      set_error (error, context,
                 G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed below <%s>"),
                 element_name, "menu_icon");
    }
}


static void
start_element_handler (GMarkupParseContext *context,
                       const gchar         *element_name,
                       const gchar        **attribute_names,
                       const gchar        **attribute_values,
                       gpointer             user_data,
                       GError             **error)
{
  ParseInfo *info = user_data;

  switch (peek_state (info))
    {
    case STATE_START:
      if (strcmp (element_name, "metacity_theme") == 0)
        {
          info->theme = meta_theme_new ();
          info->theme->name = g_strdup (info->theme_name);
          info->theme->filename = g_strdup (info->theme_file);
          info->theme->dirname = g_strdup (info->theme_dir);
          
          push_state (info, STATE_THEME);
        }
      else
        set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                   _("Outermost element in theme must be <metacity_theme> not <%s>"),
                   element_name);
      break;

    case STATE_THEME:
      parse_toplevel_element (context, element_name,
                              attribute_names, attribute_values,
                              info, error);
      break;
    case STATE_INFO:
      parse_info_element (context, element_name,
                          attribute_names, attribute_values,
                          info, error);
      break;
    case STATE_NAME:
    case STATE_AUTHOR:
    case STATE_COPYRIGHT:
    case STATE_DATE:
    case STATE_DESCRIPTION:
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed inside a name/author/date/description element"),
                 element_name);
      break;
    case STATE_CONSTANT:
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed inside a <constant> element"),
                 element_name);
      break;
    case STATE_FRAME_GEOMETRY:
      parse_geometry_element (context, element_name,
                              attribute_names, attribute_values,
                              info, error);
      break;
    case STATE_DISTANCE:
    case STATE_BORDER:
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed inside a distance/border element"),
                 element_name);
      break;
    case STATE_DRAW_OPS:
      parse_draw_op_element (context, element_name,
                             attribute_names, attribute_values,
                             info, error);
      break;
    case STATE_LINE:
    case STATE_RECTANGLE:
    case STATE_ARC:
    case STATE_CLIP:
    case STATE_TINT:
    case STATE_IMAGE:
    case STATE_GTK_ARROW:
    case STATE_GTK_BOX:
    case STATE_GTK_VLINE:
    case STATE_ICON:
    case STATE_TITLE:
    case STATE_INCLUDE:
    case STATE_TILE:
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed inside a draw operation element"),
                 element_name);
      break;
    case STATE_GRADIENT:
      parse_gradient_element (context, element_name,
                              attribute_names, attribute_values,
                              info, error);
      break;
    case STATE_COLOR:
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed inside a <%s> element"),
                 element_name, "color");
      break;
    case STATE_FRAME_STYLE:
      parse_style_element (context, element_name,
                           attribute_names, attribute_values,
                           info, error);
      break;
    case STATE_PIECE:
      parse_piece_element (context, element_name,
                           attribute_names, attribute_values,
                           info, error);
      break;
    case STATE_BUTTON:
      parse_button_element (context, element_name,
                            attribute_names, attribute_values,
                            info, error);
      break;
    case STATE_MENU_ICON:
      parse_menu_icon_element (context, element_name,
                               attribute_names, attribute_values,
                               info, error);
      break;
    case STATE_FRAME_STYLE_SET:
      parse_style_set_element (context, element_name,
                               attribute_names, attribute_values,
                               info, error);
      break;
    case STATE_FRAME:
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed inside a <%s> element"),
                 element_name, "frame");
      break;
    case STATE_WINDOW:
      set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                 _("Element <%s> is not allowed inside a <%s> element"),
                 element_name, "window");
      break;
    }
}

static void
end_element_handler (GMarkupParseContext *context,
                     const gchar         *element_name,
                     gpointer             user_data,
                     GError             **error)
{
  ParseInfo *info = user_data;

  switch (peek_state (info))
    {
    case STATE_START:
      break;
    case STATE_THEME:
      g_assert (info->theme);

      if (!meta_theme_validate (info->theme, error))
        {
          add_context_to_error (error, context);
          meta_theme_free (info->theme);
          info->theme = NULL;
        }
      
      pop_state (info);
      g_assert (peek_state (info) == STATE_START);
      break;
    case STATE_INFO:
      pop_state (info);
      g_assert (peek_state (info) == STATE_THEME);
      break;
    case STATE_NAME:
      pop_state (info);
      g_assert (peek_state (info) == STATE_INFO);
      break;
    case STATE_AUTHOR:
      pop_state (info);
      g_assert (peek_state (info) == STATE_INFO);
      break;
    case STATE_COPYRIGHT:
      pop_state (info);
      g_assert (peek_state (info) == STATE_INFO);
      break;
    case STATE_DATE:
      pop_state (info);
      g_assert (peek_state (info) == STATE_INFO);
      break;
    case STATE_DESCRIPTION:
      pop_state (info);
      g_assert (peek_state (info) == STATE_INFO);
      break;
    case STATE_CONSTANT:
      pop_state (info);
      g_assert (peek_state (info) == STATE_THEME);
      break;
    case STATE_FRAME_GEOMETRY:
      g_assert (info->layout);

      if (!meta_frame_layout_validate (info->layout,
                                       error))
        {
          add_context_to_error (error, context);
        }

      /* layout will already be stored in the theme under
       * its name
       */
      meta_frame_layout_unref (info->layout);
      info->layout = NULL;
      pop_state (info);
      g_assert (peek_state (info) == STATE_THEME);
      break;
    case STATE_DISTANCE:
      pop_state (info);
      g_assert (peek_state (info) == STATE_FRAME_GEOMETRY);
      break;
    case STATE_BORDER:
      pop_state (info);
      g_assert (peek_state (info) == STATE_FRAME_GEOMETRY);
      break;
    case STATE_DRAW_OPS:
      {
        g_assert (info->op_list);
        
        if (!meta_draw_op_list_validate (info->op_list,
                                         error))
          {
            add_context_to_error (error, context);
            meta_draw_op_list_unref (info->op_list);
            info->op_list = NULL;
          }

        pop_state (info);

        switch (peek_state (info))
          {
          case STATE_BUTTON:
          case STATE_PIECE:
          case STATE_MENU_ICON:
            /* Leave info->op_list to be picked up
             * when these elements are closed
             */
            g_assert (info->op_list);
            break;
          case STATE_THEME:
            g_assert (info->op_list);
            meta_draw_op_list_unref (info->op_list);
            info->op_list = NULL;
            break;
          default:
            /* Op list can't occur in other contexts */
            g_assert_not_reached ();
            break;
          }
      }
      break;
    case STATE_LINE:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_RECTANGLE:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_ARC:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_CLIP:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_TINT:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_GRADIENT:
      g_assert (info->op);
      g_assert (info->op->type == META_DRAW_GRADIENT);
      if (!meta_gradient_spec_validate (info->op->data.gradient.gradient_spec,
                                        error))
        {
          add_context_to_error (error, context);
          meta_draw_op_free (info->op);
          info->op = NULL;
        }
      else
        {
          g_assert (info->op_list);
          meta_draw_op_list_append (info->op_list, info->op);
          info->op = NULL;
        }
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_IMAGE:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_GTK_ARROW:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_GTK_BOX:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_GTK_VLINE:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_ICON:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_TITLE:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_INCLUDE:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_TILE:
      pop_state (info);
      g_assert (peek_state (info) == STATE_DRAW_OPS);
      break;
    case STATE_COLOR:
      pop_state (info);
      g_assert (peek_state (info) == STATE_GRADIENT);
      break;
    case STATE_FRAME_STYLE:
      g_assert (info->style);

      if (!meta_frame_style_validate (info->style,
                                      error))
        {
          add_context_to_error (error, context);
        }

      /* Frame style is in the theme hash table and a ref
       * is held there
       */
      meta_frame_style_unref (info->style);
      info->style = NULL;
      pop_state (info);
      g_assert (peek_state (info) == STATE_THEME);
      break;
    case STATE_PIECE:
      g_assert (info->style);
      if (info->op_list == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No draw_ops provided for frame piece"));
        }
      else
        {
          info->style->pieces[info->piece] = info->op_list;
          info->op_list = NULL;
        }
      pop_state (info);
      g_assert (peek_state (info) == STATE_FRAME_STYLE);
      break;
    case STATE_BUTTON:
      g_assert (info->style);
      if (info->op_list == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No draw_ops provided for button"));
        }
      else
        {
          info->style->buttons[info->button_type][info->button_state] =
            info->op_list;
          info->op_list = NULL;
        }
      pop_state (info);
      break;
    case STATE_MENU_ICON:
      g_assert (info->theme);
      if (info->op_list == NULL)
        {
          set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                     _("No draw_ops provided for menu icon"));
        }
      else
        {
          g_assert (info->theme->menu_icons[info->menu_icon_type][info->menu_icon_state] == NULL);
          info->theme->menu_icons[info->menu_icon_type][info->menu_icon_state] =
            info->op_list;
          info->op_list = NULL;
        }
      pop_state (info);
      g_assert (peek_state (info) == STATE_THEME);
      break;
    case STATE_FRAME_STYLE_SET:
      g_assert (info->style_set);

      if (!meta_frame_style_set_validate (info->style_set,
                                          error))
        {
          add_context_to_error (error, context);
        }

      /* Style set is in the theme hash table and a reference
       * is held there.
       */
      meta_frame_style_set_unref (info->style_set);
      info->style_set = NULL;
      pop_state (info);
      g_assert (peek_state (info) == STATE_THEME);
      break;
    case STATE_FRAME:
      pop_state (info);
      g_assert (peek_state (info) == STATE_FRAME_STYLE_SET);
      break;
    case STATE_WINDOW:
      pop_state (info);
      g_assert (peek_state (info) == STATE_THEME);
      break;
    }
}

#define NO_TEXT(element_name) set_error (error, context, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE, _("No text is allowed inside element <%s>"), element_name)

static gboolean
all_whitespace (const char *text,
                int         text_len)
{
  const char *p;
  const char *end;
  
  p = text;
  end = text + text_len;
  
  while (p != end)
    {
      if (!g_ascii_isspace (*p))
        return FALSE;

      p = g_utf8_next_char (p);
    }

  return TRUE;
}

static void
text_handler (GMarkupParseContext *context,
              const gchar         *text,
              gsize                text_len,
              gpointer             user_data,
              GError             **error)
{
  ParseInfo *info = user_data;

  if (all_whitespace (text, text_len))
    return;
  
  /* FIXME http://bugzilla.gnome.org/show_bug.cgi?id=70448 would
   * allow a nice cleanup here.
   */

  switch (peek_state (info))
    {
    case STATE_START:
      g_assert_not_reached (); /* gmarkup shouldn't do this */
      break;
    case STATE_THEME:
      NO_TEXT ("metacity_theme");
      break;
    case STATE_INFO:
      NO_TEXT ("info");
      break;
    case STATE_NAME:
      if (info->theme->readable_name != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("<name> specified twice for this theme"));
          return;
        }

      info->theme->readable_name = g_strndup (text, text_len);
      break;
    case STATE_AUTHOR:
      if (info->theme->author != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("<author> specified twice for this theme"));
          return;
        }

      info->theme->author = g_strndup (text, text_len);
      break;
    case STATE_COPYRIGHT:
      if (info->theme->copyright != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("<copyright> specified twice for this theme"));
          return;
        }

      info->theme->copyright = g_strndup (text, text_len);
      break;
    case STATE_DATE:
      if (info->theme->date != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("<date> specified twice for this theme"));
          return;
        }

      info->theme->date = g_strndup (text, text_len);
      break;
    case STATE_DESCRIPTION:
      if (info->theme->description != NULL)
        {
          set_error (error, context, G_MARKUP_ERROR,
                     G_MARKUP_ERROR_PARSE,
                     _("<description> specified twice for this theme"));
          return;
        }

      info->theme->description = g_strndup (text, text_len);
      break;
    case STATE_CONSTANT:
      NO_TEXT ("constant");
      break;
    case STATE_FRAME_GEOMETRY:
      NO_TEXT ("frame_geometry");
      break;
    case STATE_DISTANCE:
      NO_TEXT ("distance");
      break;
    case STATE_BORDER:
      NO_TEXT ("border");
      break;
    case STATE_DRAW_OPS:
      NO_TEXT ("draw_ops");
      break;
    case STATE_LINE:
      NO_TEXT ("line");
      break;
    case STATE_RECTANGLE:
      NO_TEXT ("rectangle");
      break;
    case STATE_ARC:
      NO_TEXT ("arc");
      break;
    case STATE_CLIP:
      NO_TEXT ("clip");
      break;
    case STATE_TINT:
      NO_TEXT ("tint");
      break;
    case STATE_GRADIENT:
      NO_TEXT ("gradient");
      break;
    case STATE_IMAGE:
      NO_TEXT ("image");
      break;
    case STATE_GTK_ARROW:
      NO_TEXT ("gtk_arrow");
      break;
    case STATE_GTK_BOX:
      NO_TEXT ("gtk_box");
      break;
    case STATE_GTK_VLINE:
      NO_TEXT ("gtk_vline");
      break;
    case STATE_ICON:
      NO_TEXT ("icon");
      break;
    case STATE_TITLE:
      NO_TEXT ("title");
      break;
    case STATE_INCLUDE:
      NO_TEXT ("include");
      break;
    case STATE_TILE:
      NO_TEXT ("tile");
      break;
    case STATE_COLOR:
      NO_TEXT ("color");
      break;
    case STATE_FRAME_STYLE:
      NO_TEXT ("frame_style");
      break;
    case STATE_PIECE:
      NO_TEXT ("piece");
      break;
    case STATE_BUTTON:
      NO_TEXT ("button");
      break;
    case STATE_MENU_ICON:
      NO_TEXT ("menu_icon");
      break;
    case STATE_FRAME_STYLE_SET:
      NO_TEXT ("frame_style_set");
      break;
    case STATE_FRAME:
      NO_TEXT ("frame");
      break;
    case STATE_WINDOW:
      NO_TEXT ("window");
      break;
    }
}

/* We change the filename when we break the format,
 * so themes can work with various metacity versions
 */
#define THEME_FILENAME "metacity-theme-1.xml"

MetaTheme*
meta_theme_load (const char *theme_name,
                 GError    **err)
{
  GMarkupParseContext *context;
  GError *error;
  ParseInfo info;
  char *text;
  int length;
  char *theme_file;
  char *theme_dir;
  MetaTheme *retval;

  text = NULL;
  length = 0;
  retval = NULL;

  theme_dir = NULL;
  theme_file = NULL;
  
  if (meta_is_debugging ())
    {
      theme_dir = g_build_filename ("./themes", theme_name, NULL);
      
      theme_file = g_build_filename (theme_dir,
                                     THEME_FILENAME,
                                     NULL);
      
      error = NULL;
      if (!g_file_get_contents (theme_file,
                                &text,
                                &length,
                                &error))
        {
          meta_topic (META_DEBUG_THEMES, "Failed to read theme from file %s: %s\n",
                      theme_file, error->message);
          g_error_free (error);
          g_free (theme_dir);
          g_free (theme_file);
          theme_file = NULL;
        }
    }
  
  /* We try in current dir, then home dir, then system dir for themes */
  if (text == NULL)
    {
      theme_dir = g_build_filename ("./", theme_name, NULL);
      
      theme_file = g_build_filename (theme_dir,
                                     THEME_FILENAME,
                                     NULL);
      
      error = NULL;
      if (!g_file_get_contents (theme_file,
                                &text,
                                &length,
                                &error))
        {
          meta_topic (META_DEBUG_THEMES, "Failed to read theme from file %s: %s\n",
                      theme_file, error->message);
          g_error_free (error);
          g_free (theme_dir);
          g_free (theme_file);
          theme_file = NULL;
        }
    }
  
  if (text == NULL)
    {
      theme_dir = g_build_filename (g_get_home_dir (),
                                    ".metacity/themes/", theme_name, NULL);
      
      theme_file = g_build_filename (theme_dir,
                                     THEME_FILENAME,
                                     NULL);

      error = NULL;
      if (!g_file_get_contents (theme_file,
                                &text,
                                &length,
                                &error))
        {
          meta_topic (META_DEBUG_THEMES, "Failed to read theme from file %s: %s\n",
                      theme_file, error->message);
          g_error_free (error);
          g_free (theme_dir);
          g_free (theme_file);
          theme_file = NULL;
        }
    }

  if (text == NULL)
    {
      theme_dir = g_build_filename (METACITY_PKGDATADIR,
                                    "themes",
                                    theme_name, NULL);
      
      theme_file = g_build_filename (theme_dir,
                                     THEME_FILENAME,
                                     NULL);

      error = NULL;
      if (!g_file_get_contents (theme_file,
                                &text,
                                &length,
                                &error))
        {
          meta_warning (_("Failed to read theme from file %s: %s\n"),
                        theme_file, error->message);
          g_propagate_error (err, error);
          g_free (theme_file);
          g_free (theme_dir);
          return FALSE; /* all fallbacks failed */
        }
    }

  g_assert (text);

  meta_topic (META_DEBUG_THEMES, "Parsing theme file %s\n", theme_file);

  parse_info_init (&info);
  info.theme_name = theme_name;
  
  /* pass ownership to info so we free it with the info */
  info.theme_file = theme_file;
  info.theme_dir = theme_dir;
  
  context = g_markup_parse_context_new (&metacity_theme_parser,
                                        0, &info, NULL);

  error = NULL;
  if (!g_markup_parse_context_parse (context,
                                     text,
                                     length,
                                     &error))
    goto out;

  error = NULL;
  if (!g_markup_parse_context_end_parse (context, &error))
    goto out;

  g_markup_parse_context_free (context);

  goto out;

 out:

  g_free (text);
  
  if (error)
    {
      g_propagate_error (err, error);
    }
  else if (info.theme)
    {
      /* Steal theme from info */
      retval = info.theme;
      info.theme = NULL;
    }
  else
    {
      g_set_error (err, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE,
                   _("Theme file %s did not contain a root <metacity_theme> element"),
                   info.theme_file);
    }

  parse_info_free (&info);

  return retval;
}
