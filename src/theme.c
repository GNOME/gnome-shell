/* Metacity Theme Rendering */

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
#include "theme.h"
#include "util.h"
#include "gradient.h"
#include <gtk/gtkwidget.h>
#include <string.h>
#include <stdlib.h>

#define GDK_COLOR_RGBA(color)                                   \
                         (0xff                         |        \
                         (((color).red / 256) << 24)   |        \
                         (((color).green / 256) << 16) |        \
                         (((color).blue / 256) << 8))

#define GDK_COLOR_RGB(color)                                    \
                         ((((color).red / 256) << 16)   |        \
                          (((color).green / 256) << 8)  |        \
                          (((color).blue / 256)))

#define ALPHA_TO_UCHAR(d) ((unsigned char) ((d) * 255))

static void
color_composite (const GdkColor *bg,
                 const GdkColor *fg,
                 double          alpha_d,
                 GdkColor       *color)
{
  guint16 alpha;
  
  *color = *bg;
  alpha = alpha_d * 0xffff;
  color->red = color->red + (((fg->red - color->red) * alpha + 0x8000) >> 16);
  color->green = color->green + (((fg->green - color->green) * alpha + 0x8000) >> 16);
  color->blue = color->blue + (((fg->blue - color->blue) * alpha + 0x8000) >> 16);
}

MetaFrameLayout*
meta_frame_layout_new  (void)
{
  MetaFrameLayout *layout;

  layout = g_new0 (MetaFrameLayout, 1);

  return layout;
}

void
meta_frame_layout_free (MetaFrameLayout *layout)
{
  g_return_if_fail (layout != NULL);
  
  g_free (layout);
}

void
meta_frame_layout_get_borders (const MetaFrameLayout *layout,
                               GtkWidget             *widget,
                               int                    text_height,
                               MetaFrameFlags         flags,
                               int                   *top_height,
                               int                   *bottom_height,
                               int                   *left_width,
                               int                   *right_width)
{
  int buttons_height, title_height, spacer_height;

  g_return_if_fail (top_height != NULL);
  g_return_if_fail (bottom_height != NULL);
  g_return_if_fail (left_width != NULL);
  g_return_if_fail (right_width != NULL);
  
  buttons_height = layout->button_height +
    layout->button_border.top + layout->button_border.bottom;
  title_height = text_height +
    layout->text_border.top + layout->text_border.bottom +
    layout->title_border.top + layout->title_border.bottom;
  spacer_height = layout->spacer_height;

  if (top_height)
    {
      *top_height = MAX (buttons_height, title_height);
      *top_height = MAX (*top_height, spacer_height);
    }

  if (left_width)
    *left_width = layout->left_width;
  if (right_width)
    *right_width = layout->right_width;

  if (bottom_height)
    {
      if (flags & META_FRAME_SHADED)
        *bottom_height = 0;
      else
        *bottom_height = layout->bottom_height;
    }
}

void
meta_frame_layout_calc_geometry (const MetaFrameLayout *layout,
                                 GtkWidget             *widget,
                                 int                    text_height,
                                 MetaFrameFlags         flags,
                                 int                    client_width,
                                 int                    client_height,
                                 MetaFrameGeometry     *fgeom)
{
  int x;
  int button_y;
  int title_right_edge;
  int width, height;

  meta_frame_layout_get_borders (layout, widget, text_height,
                                 flags,
                                 &fgeom->top_height,
                                 &fgeom->bottom_height,
                                 &fgeom->left_width,
                                 &fgeom->right_width);
  
  width = client_width + fgeom->left_width + fgeom->right_width;
  height = client_height + fgeom->top_height + fgeom->bottom_height;
  
  fgeom->width = width;
  fgeom->height = height;

  fgeom->top_titlebar_edge = layout->title_border.top;
  fgeom->bottom_titlebar_edge = layout->title_border.bottom;
  fgeom->left_titlebar_edge = layout->left_inset;
  fgeom->right_titlebar_edge = layout->right_inset;
  
  x = width - layout->right_inset;

  /* center buttons */
  button_y = (fgeom->top_height -
              (layout->button_height + layout->button_border.top + layout->button_border.bottom)) / 2 + layout->button_border.top;

  if ((flags & META_FRAME_ALLOWS_DELETE) &&
      x >= 0)
    {
      fgeom->close_rect.x = x - layout->button_border.right - layout->button_width;
      fgeom->close_rect.y = button_y;
      fgeom->close_rect.width = layout->button_width;
      fgeom->close_rect.height = layout->button_height;

      x = fgeom->close_rect.x - layout->button_border.left;
    }
  else
    {
      fgeom->close_rect.x = 0;
      fgeom->close_rect.y = 0;
      fgeom->close_rect.width = 0;
      fgeom->close_rect.height = 0;
    }

  if ((flags & META_FRAME_ALLOWS_MAXIMIZE) &&
      x >= 0)
    {
      fgeom->max_rect.x = x - layout->button_border.right - layout->button_width;
      fgeom->max_rect.y = button_y;
      fgeom->max_rect.width = layout->button_width;
      fgeom->max_rect.height = layout->button_height;

      x = fgeom->max_rect.x - layout->button_border.left;
    }
  else
    {
      fgeom->max_rect.x = 0;
      fgeom->max_rect.y = 0;
      fgeom->max_rect.width = 0;
      fgeom->max_rect.height = 0;
    }
  
  if ((flags & META_FRAME_ALLOWS_MINIMIZE) &&
      x >= 0)
    {
      fgeom->min_rect.x = x - layout->button_border.right - layout->button_width;
      fgeom->min_rect.y = button_y;
      fgeom->min_rect.width = layout->button_width;
      fgeom->min_rect.height = layout->button_height;

      x = fgeom->min_rect.x - layout->button_border.left;
    }
  else
    {
      fgeom->min_rect.x = 0;
      fgeom->min_rect.y = 0;
      fgeom->min_rect.width = 0;
      fgeom->min_rect.height = 0;
    }

  if ((fgeom->close_rect.width > 0 ||
       fgeom->max_rect.width > 0 ||
       fgeom->min_rect.width > 0) &&
      x >= 0)
    {
      fgeom->spacer_rect.x = x - layout->spacer_padding - layout->spacer_width;
      fgeom->spacer_rect.y = (fgeom->top_height - layout->spacer_height) / 2;
      fgeom->spacer_rect.width = layout->spacer_width;
      fgeom->spacer_rect.height = layout->spacer_height;

      x = fgeom->spacer_rect.x - layout->spacer_padding;
    }
  else
    {
      fgeom->spacer_rect.x = 0;
      fgeom->spacer_rect.y = 0;
      fgeom->spacer_rect.width = 0;
      fgeom->spacer_rect.height = 0;
    }

  title_right_edge = x - layout->title_border.right;
  
  /* Now x changes to be position from the left */
  x = layout->left_inset;
  
  if (flags & META_FRAME_ALLOWS_MENU)
    {
      fgeom->menu_rect.x = x + layout->button_border.left;
      fgeom->menu_rect.y = button_y;
      fgeom->menu_rect.width = layout->button_width;
      fgeom->menu_rect.height = layout->button_height;

      x = fgeom->menu_rect.x + fgeom->menu_rect.width + layout->button_border.right;
    }
  else
    {
      fgeom->menu_rect.x = 0;
      fgeom->menu_rect.y = 0;
      fgeom->menu_rect.width = 0;
      fgeom->menu_rect.height = 0;
    }

  /* If menu overlaps close button, then the menu wins since it
   * lets you perform any operation including close
   */
  if (fgeom->close_rect.width > 0 &&
      fgeom->close_rect.x < (fgeom->menu_rect.x + fgeom->menu_rect.height))
    {
      fgeom->close_rect.width = 0;
      fgeom->close_rect.height = 0;
    }

  /* Check for maximize overlap */
  if (fgeom->max_rect.width > 0 &&
      fgeom->max_rect.x < (fgeom->menu_rect.x + fgeom->menu_rect.height))
    {
      fgeom->max_rect.width = 0;
      fgeom->max_rect.height = 0;
    }
  
  /* Check for minimize overlap */
  if (fgeom->min_rect.width > 0 &&
      fgeom->min_rect.x < (fgeom->menu_rect.x + fgeom->menu_rect.height))
    {
      fgeom->min_rect.width = 0;
      fgeom->min_rect.height = 0;
    }

  /* Check for spacer overlap */
  if (fgeom->spacer_rect.width > 0 &&
      fgeom->spacer_rect.x < (fgeom->menu_rect.x + fgeom->menu_rect.height))
    {
      fgeom->spacer_rect.width = 0;
      fgeom->spacer_rect.height = 0;
    }
  
  /* We always fill as much vertical space as possible with title rect,
   * rather than centering it like the buttons and spacer
   */
  fgeom->title_rect.x = x + layout->title_border.left;
  fgeom->title_rect.y = layout->title_border.top;
  fgeom->title_rect.width = title_right_edge - fgeom->title_rect.x;
  fgeom->title_rect.height = fgeom->top_height - layout->title_border.top - layout->title_border.bottom;

  /* Nuke title if it won't fit */
  if (fgeom->title_rect.width < 0 ||
      fgeom->title_rect.height < 0)
    {
      fgeom->title_rect.width = 0;
      fgeom->title_rect.height = 0;
    }
}


MetaGradientSpec*
meta_gradient_spec_new (MetaGradientType type)
{
  MetaGradientSpec *spec;

  spec = g_new (MetaGradientSpec, 1);

  spec->type = type;
  spec->color_specs = NULL;
  
  return spec;
}

void
meta_gradient_spec_free (MetaGradientSpec *spec)
{
  g_return_if_fail (spec != NULL);
  
  g_slist_foreach (spec->color_specs, (GFunc) meta_color_spec_free, NULL);
  g_slist_free (spec->color_specs);
  g_free (spec);
}

GdkPixbuf*
meta_gradient_spec_render (const MetaGradientSpec *spec,
                           GtkWidget              *widget,
                           int                     width,
                           int                     height)
{
  int n_colors;
  GdkColor *colors;
  GSList *tmp;
  int i;
  GdkPixbuf *pixbuf;
  
  n_colors = g_slist_length (spec->color_specs);

  if (n_colors == 0)
    return NULL;
  
  colors = g_new (GdkColor, n_colors);

  i = 0;
  tmp = spec->color_specs;
  while (tmp != NULL)
    {
      meta_color_spec_render (tmp->data, widget, &colors[i]);
      
      tmp = tmp->next;
      ++i;
    }
  
  pixbuf = meta_gradient_create_multi (width, height,
                                       colors, n_colors,
                                       spec->type);

  g_free (colors);

  return pixbuf;
}

MetaColorSpec*
meta_color_spec_new (MetaColorSpecType type)
{
  MetaColorSpec *spec;
  MetaColorSpec dummy;
  int size;

  size = G_STRUCT_OFFSET (MetaColorSpec, data);
  
  switch (type)
    {
    case META_COLOR_SPEC_BASIC:
      size += sizeof (dummy.data.basic);
      break;

    case META_COLOR_SPEC_GTK:
      size += sizeof (dummy.data.gtk);
      break;

    case META_COLOR_SPEC_BLEND:
      size += sizeof (dummy.data.blend);
      break;
    }
  
  spec = g_malloc0 (size);

  spec->type = type;  
  
  return spec;
}

void
meta_color_spec_free (MetaColorSpec *spec)
{
  g_return_if_fail (spec != NULL);
  
  switch (spec->type)
    {
    case META_COLOR_SPEC_BASIC:      
      break;

    case META_COLOR_SPEC_GTK:
      break;

    case META_COLOR_SPEC_BLEND:
      if (spec->data.blend.foreground)
        meta_color_spec_free (spec->data.blend.foreground);      
      if (spec->data.blend.background)
        meta_color_spec_free (spec->data.blend.background);
      break;
    }
  
  g_free (spec);
}

MetaColorSpec*
meta_color_spec_new_from_string (const char *str,
                                 GError    **err)
{
  /* FIXME handle GTK colors, etc. */
  MetaColorSpec *spec;

  spec = meta_color_spec_new (META_COLOR_SPEC_BASIC);
  
  gdk_color_parse (str, &spec->data.basic.color);

  return spec;
}

MetaColorSpec*
meta_color_spec_new_gtk (MetaGtkColorComponent component,
                         GtkStateType          state)
{
  /* FIXME handle GTK colors, etc. */
  MetaColorSpec *spec;

  spec = meta_color_spec_new (META_COLOR_SPEC_GTK);
  
  spec->data.gtk.component = component;
  spec->data.gtk.state = state;

  return spec;
}

void
meta_color_spec_render (MetaColorSpec *spec,
                        GtkWidget     *widget,
                        GdkColor      *color)
{
  g_return_if_fail (spec != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (widget->style != NULL);
  
  switch (spec->type)
    {
    case META_COLOR_SPEC_BASIC:
      *color = spec->data.basic.color;
      break;

    case META_COLOR_SPEC_GTK:
      switch (spec->data.gtk.component)
        {
        case META_GTK_COLOR_BG:
          *color = widget->style->bg[spec->data.gtk.state];
          break;
        case META_GTK_COLOR_FG:
          *color = widget->style->fg[spec->data.gtk.state];
          break;
        case META_GTK_COLOR_BASE:
          *color = widget->style->base[spec->data.gtk.state];
          break;
        case META_GTK_COLOR_TEXT:
          *color = widget->style->text[spec->data.gtk.state];
          break;
        case META_GTK_COLOR_LIGHT:
          *color = widget->style->light[spec->data.gtk.state];
          break;
        case META_GTK_COLOR_DARK:
          *color = widget->style->dark[spec->data.gtk.state];
          break;
        case META_GTK_COLOR_MID:
          *color = widget->style->mid[spec->data.gtk.state];
          break;
        case META_GTK_COLOR_TEXT_AA:
          *color = widget->style->text_aa[spec->data.gtk.state];
          break;
        }
      break;

    case META_COLOR_SPEC_BLEND:
      {
        GdkColor bg, fg;

        meta_color_spec_render (spec->data.blend.background, widget, &bg);
        meta_color_spec_render (spec->data.blend.foreground, widget, &fg);

        color_composite (&bg, &fg, spec->data.blend.alpha, color);
      }
      break;
    }
}

typedef enum
{
  POS_TOKEN_INT,
  POS_TOKEN_DOUBLE,
  POS_TOKEN_OPERATOR,
  POS_TOKEN_VARIABLE,
  POS_TOKEN_OPEN_PAREN,
  POS_TOKEN_CLOSE_PAREN
} PosTokenType;

typedef struct
{
  PosTokenType type;

  union
  {
    struct {
      int val;
    } i;

    struct {
      double val;
    } d;

    struct {
      char op;
    } o;

    struct {
      char *name;
    } v;
    
  } d;

} PosToken;


static void
free_tokens (PosToken *tokens,
             int       n_tokens)
{
  int i;

  /* n_tokens can be 0 since tokens may have been allocated more than
   * it was initialized
   */
  
  i = 0;
  while (i < n_tokens)
    {
      if (tokens[i].type == POS_TOKEN_VARIABLE)
        g_free (tokens[i].d.v.name);
      ++i;
    }
  g_free (tokens);
}

static gboolean
parse_number (const char  *p,
              const char **end_return,
              PosToken    *next,
              GError     **err)
{
  const char *start = p;
  char *end;
  gboolean is_float;
  char *num_str;
              
  while (*p && (*p == '.' || g_ascii_isdigit (*p)))
    ++p;
              
  if (p == start)
    {
      g_set_error (err, META_POSITION_EXPR_ERROR,
                   META_POSITION_EXPR_ERROR_BAD_CHARACTER,
                   _("Coordinate expression contains character '%c' which is not allowed"),
                   *p);
      return FALSE;
    }

  *end_return = p;
  
  /* we need this to exclude floats like "1e6" */
  num_str = g_strndup (start, p - start);
  start = num_str;
  is_float = FALSE;
  while (*start)
    {
      if (*start == '.')
        is_float = TRUE;
      ++start;
    }
              
  if (is_float)
    {
      next->type = POS_TOKEN_DOUBLE;
      next->d.d.val = g_ascii_strtod (num_str, &end);

      if (end == num_str)
        {
          g_set_error (err, META_POSITION_EXPR_ERROR,
                       META_POSITION_EXPR_ERROR_FAILED,
                       _("Coordinate expression contains floating point number '%s' which could not be parsed"),
                       num_str);
          g_free (num_str);
          return FALSE;
        }
    }
  else
    {
      next->type = POS_TOKEN_INT;
      next->d.i.val = strtol (num_str, &end, 10);
      if (end == num_str)
        {
          g_set_error (err, META_POSITION_EXPR_ERROR,
                       META_POSITION_EXPR_ERROR_FAILED,
                       _("Coordinate expression contains integer '%s' which could not be parsed"),
                       num_str);
          g_free (num_str);
          return FALSE;
        }
    }

  g_free (num_str);

  g_assert (next->type == POS_TOKEN_INT || next->type == POS_TOKEN_DOUBLE);
  
  return TRUE;
}

static gboolean
pos_tokenize (const char  *expr,
              PosToken   **tokens_p,
              int         *n_tokens_p,
              GError     **err)
{
  PosToken *tokens;
  int n_tokens;
  int allocated;
  const char *p;
  
  *tokens_p = NULL;
  *n_tokens_p = 0;

  allocated = 3;
  n_tokens = 0;
  tokens = g_new (PosToken, allocated);

  p = expr;
  while (*p)
    {
      PosToken *next;
      
      if (n_tokens == allocated)
        {
          allocated *= 2;
          tokens = g_renew (PosToken, tokens, allocated);
        }

      next = &tokens[n_tokens];
      
      switch (*p)
        {
        case '*':
        case '/':
        case '+':
        case '-': /* negative numbers aren't allowed so this is easy */
        case '%':
          next->type = POS_TOKEN_OPERATOR;
          next->d.o.op = *p;
          ++n_tokens;
          break;
          
        case '(':
          next->type = POS_TOKEN_OPEN_PAREN;
          ++n_tokens;
          break;

        case ')':
          next->type = POS_TOKEN_CLOSE_PAREN;
          ++n_tokens;
          break;

        case ' ':
        case '\t':
          break;

        default:
          if (g_ascii_isalpha (*p))
            {
              /* Assume variable */
              const char *start = p;
              while (*p && g_ascii_isalpha (*p))
                ++p;
              g_assert (p != start);
              next->type = POS_TOKEN_VARIABLE;
              next->d.v.name = g_strndup (start, p - start);
              ++n_tokens;
              --p; /* since we ++p again at the end of while loop */
            }
          else
            {
              /* Assume number */
              const char *end;

              if (!parse_number (p, &end, next, err))
                goto error;
              
              ++n_tokens;
              p = end - 1; /* -1 since we ++p again at the end of while loop */
            }
          
          break;
        }

      ++p;
    }

  if (n_tokens == 0)
    {
      g_set_error (err, META_POSITION_EXPR_ERROR,
                   META_POSITION_EXPR_ERROR_FAILED,
                   _("Coordinate expression was empty or not understood"));
                   
      goto error;
    }

  *tokens_p = tokens;
  *n_tokens_p = n_tokens;
  
  return TRUE;

 error:
  g_assert (err == NULL || *err != NULL);

  free_tokens (tokens, n_tokens);
  return FALSE;
}

static void
debug_print_tokens (PosToken *tokens,
                    int       n_tokens)
{
  int i;

  i = 0;
  while (i < n_tokens)
    {
      PosToken *t = &tokens[i];

      g_print (" ");
      
      switch (t->type)
        {
        case POS_TOKEN_INT:
          g_print ("\"%d\"", t->d.i.val);
          break;
        case POS_TOKEN_DOUBLE:
          g_print ("\"%g\"", t->d.d.val);
          break;
        case POS_TOKEN_OPEN_PAREN:
          g_print ("\"(\"");
          break;
        case POS_TOKEN_CLOSE_PAREN:
          g_print ("\")\"");
          break;
        case POS_TOKEN_VARIABLE:
          g_print ("\"%s\"", t->d.v.name);
          break;
        case POS_TOKEN_OPERATOR:
          g_print ("\"%c\"", t->d.o.op);
          break;
        }

      ++i;
    }

  g_print ("\n");
}

typedef enum
{ 
  POS_EXPR_INT,
  POS_EXPR_DOUBLE,
  POS_EXPR_OPERATOR
} PosExprType;

typedef struct
{
  PosExprType type;
  union
  {
    double double_val;
    int int_val;
    char operator;
  } d;
} PosExpr;

static void
debug_print_exprs (PosExpr *exprs,
                   int      n_exprs)
{
  int i;

  i = 0;
  while (i < n_exprs)
    {
      switch (exprs[i].type)
        {
        case POS_EXPR_INT:
          g_print (" %d", exprs[i].d.int_val);
          break;
        case POS_EXPR_DOUBLE:
          g_print (" %g", exprs[i].d.double_val);
          break;
        case POS_EXPR_OPERATOR:
          g_print (" %c", exprs[i].d.operator);
          break;
        }
      
      ++i;
    }
  g_print ("\n");
}

static gboolean
do_operation (PosExpr *a,
              PosExpr *b,
              char     op,
              GError **err)
{
  /* Promote types to double if required */
  if (a->type == POS_EXPR_DOUBLE ||
      b->type == POS_EXPR_DOUBLE)
    {
      if (a->type != POS_EXPR_DOUBLE)
        {
          a->type = POS_EXPR_DOUBLE;
          a->d.double_val = a->d.int_val;
        }
      if (b->type != POS_EXPR_DOUBLE)
        {
          b->type = POS_EXPR_DOUBLE;
          b->d.double_val = b->d.int_val;
        }
    }

  g_assert (a->type == b->type);
  
  if (a->type == POS_EXPR_INT)
    {
      switch (op)
        {
        case '*':
          a->d.int_val = a->d.int_val * b->d.int_val;
          break;
        case '/':
          if (b->d.int_val == 0)
            {
              g_set_error (err, META_POSITION_EXPR_ERROR,
                           META_POSITION_EXPR_ERROR_DIVIDE_BY_ZERO,
                           _("Coordinate expression results in division by zero"));
              return FALSE;
            }
          a->d.int_val = a->d.int_val / b->d.int_val;
          break;
        case '%':
          if (b->d.int_val == 0)
            {
              g_set_error (err, META_POSITION_EXPR_ERROR,
                           META_POSITION_EXPR_ERROR_DIVIDE_BY_ZERO,
                           _("Coordinate expression results in division by zero"));
              return FALSE;
            }
          a->d.int_val = a->d.int_val % b->d.int_val;
          break;
        case '+':
          a->d.int_val = a->d.int_val + b->d.int_val;
          break;
        case '-':
          a->d.int_val = a->d.int_val - b->d.int_val;
          break;
        }
    }
  else if (a->type == POS_EXPR_DOUBLE)
    {
      switch (op)
        {
        case '*':
          a->d.double_val = a->d.double_val * b->d.double_val;
          break;
        case '/':
          if (b->d.double_val == 0.0)
            {
              g_set_error (err, META_POSITION_EXPR_ERROR,
                           META_POSITION_EXPR_ERROR_DIVIDE_BY_ZERO,
                           _("Coordinate expression results in division by zero"));
              return FALSE;
            }
          a->d.double_val = a->d.double_val / b->d.double_val;
          break;
        case '%':
          g_set_error (err, META_POSITION_EXPR_ERROR,
                       META_POSITION_EXPR_ERROR_MOD_ON_FLOAT,
                       _("Coordinate expression tries to use mod operator on a floating-point number"));
          return FALSE;
          break;
        case '+':
          a->d.double_val = a->d.double_val + b->d.double_val;
          break;
        case '-':
          a->d.double_val = a->d.double_val - b->d.double_val;
          break;
        }
    }
  else
    g_assert_not_reached ();

  return TRUE;
}

static gboolean
do_operations (PosExpr *exprs,
               int     *n_exprs,
               int      precedence,
               GError **err)
{
  int i;

#if 0
  g_print ("Doing prec %d ops on %d exprs\n", precedence, *n_exprs);
  debug_print_exprs (exprs, *n_exprs);
#endif
  
  i = 1;
  while (i < *n_exprs)
    {
      gboolean compress;

      /* exprs[i-1] first operand 
       * exprs[i]   operator
       * exprs[i+1] second operand
       *
       * we replace first operand with result of mul/div/mod,
       * or skip over operator and second operand if we have
       * an add/subtract
       */

      if (exprs[i-1].type == POS_EXPR_OPERATOR)
        {
          g_set_error (err, META_POSITION_EXPR_ERROR,
                       META_POSITION_EXPR_ERROR_FAILED,
                       _("Coordinate expression has an operator \"%c\" where an operand was expected"),
                       exprs[i-1].d.operator);
          return FALSE;
        }
      
      if (exprs[i].type != POS_EXPR_OPERATOR)
        {
          g_set_error (err, META_POSITION_EXPR_ERROR,
                       META_POSITION_EXPR_ERROR_FAILED,
                       _("Coordinate expression had an operand where an operator was expected"));
          return FALSE;
        }

      if (i == (*n_exprs - 1))
        {
          g_set_error (err, META_POSITION_EXPR_ERROR,
                       META_POSITION_EXPR_ERROR_FAILED,
                       _("Coordinate expression ended with an operator instead of an operand"));
          return FALSE;
        }

      g_assert ((i+1) < *n_exprs);
      
      if (exprs[i+1].type == POS_EXPR_OPERATOR)
        {
          g_set_error (err, META_POSITION_EXPR_ERROR,
                       META_POSITION_EXPR_ERROR_FAILED,
                       _("Coordinate expression has operator \"%c\" following operator \"%c\" with no operand in between"),
                       exprs[i+1].d.operator,
                       exprs[i].d.operator);
          return FALSE;
        }

      compress = FALSE;
      
      if (precedence == 1)
        {
          switch (exprs[i].d.operator)
            {
            case '*':
            case '/':
            case '%':
              compress = TRUE;
              if (!do_operation (&exprs[i-1], &exprs[i+1],
                                 exprs[i].d.operator,
                                 err))
                return FALSE;
              break;
            }
        }
      else if (precedence == 0)
        {
          switch (exprs[i].d.operator)
            {
            case '-':
            case '+':
              compress = TRUE;
              if (!do_operation (&exprs[i-1], &exprs[i+1],
                                 exprs[i].d.operator,
                                 err))
                return FALSE;
              break;
            }
        }

      if (compress)
        {
          /* exprs[i-1] first operand (now result)
           * exprs[i]   operator
           * exprs[i+1] second operand
           * exprs[i+2] new operator
           *
           * we move new operator just after first operand
           */
          if ((i+2) < *n_exprs)
            {
              g_memmove (&exprs[i], &exprs[i+2],
                         sizeof (PosExpr) * (*n_exprs - i - 2));
            }
          
          *n_exprs -= 2;
        }
      else
        {
          /* Skip operator and next operand */
          i += 2;
        }
    }

  return TRUE;
}

static gboolean
pos_eval_helper (PosToken                   *tokens,
                 int                         n_tokens,
                 const MetaPositionExprEnv  *env,
                 PosExpr                    *result,
                 GError                    **err)
{
  /* lazy-ass hardcoded limit on expression size */
#define MAX_EXPRS 32
  int paren_level;
  int first_paren;
  int i;
  PosExpr exprs[MAX_EXPRS];
  int n_exprs;

#if 0
  g_print ("Pos eval helper on %d tokens:\n", n_tokens);
  debug_print_tokens (tokens, n_tokens);
#endif
  
  /* Our first goal is to get a list of PosExpr, essentially
   * substituting variables and handling parentheses.
   */
  
  first_paren = 0;
  paren_level = 0;
  n_exprs = 0;
  i = 0;
  while (i < n_tokens)
    {
      PosToken *t = &tokens[i];

      if (n_exprs >= MAX_EXPRS)
        {
          g_set_error (err, META_POSITION_EXPR_ERROR,
                       META_POSITION_EXPR_ERROR_FAILED,
                       _("Coordinate expression parser overflowed its buffer, this is really a Metacity bug, but are you sure you need a huge expression like that?"));
          return FALSE;
        }

      if (paren_level == 0)
        {
          switch (t->type)
            {
            case POS_TOKEN_INT:
              exprs[n_exprs].type = POS_EXPR_INT;
              exprs[n_exprs].d.int_val = t->d.i.val;
              ++n_exprs;
              break;

            case POS_TOKEN_DOUBLE:
              exprs[n_exprs].type = POS_EXPR_DOUBLE;
              exprs[n_exprs].d.double_val = t->d.d.val;
              ++n_exprs;
              break;

            case POS_TOKEN_OPEN_PAREN:
              ++paren_level;
              if (paren_level == 1)
                first_paren = i;
              break;

            case POS_TOKEN_CLOSE_PAREN:
              g_set_error (err, META_POSITION_EXPR_ERROR,
                           META_POSITION_EXPR_ERROR_BAD_PARENS,
                           _("Coordinate expression had a close parenthesis with no open parenthesis"));
              return FALSE;
              break;
          
            case POS_TOKEN_VARIABLE:
              exprs[n_exprs].type = POS_EXPR_INT;
              
              if (strcmp (t->d.v.name, "width") == 0)
                exprs[n_exprs].d.int_val = env->width;
              else if (strcmp (t->d.v.name, "height") == 0)
                exprs[n_exprs].d.int_val = env->height;
              else if (env->object_width >= 0 &&
                       strcmp (t->d.v.name, "object_width") == 0)
                exprs[n_exprs].d.int_val = env->object_width;
              else if (env->object_height >= 0 &&
                       strcmp (t->d.v.name, "object_height") == 0)
                exprs[n_exprs].d.int_val = env->object_height;
              else
                {
                  g_set_error (err, META_POSITION_EXPR_ERROR,
                               META_POSITION_EXPR_ERROR_UNKNOWN_VARIABLE,
                               _("Coordinate expression had unknown variable \"%s\""),
                               t->d.v.name);
                  return FALSE;
                }
              ++n_exprs;
              break;

            case POS_TOKEN_OPERATOR:
              exprs[n_exprs].type = POS_EXPR_OPERATOR;
              exprs[n_exprs].d.operator = t->d.o.op;
              ++n_exprs;
              break;  
            }
        }
      else
        {
          g_assert (paren_level > 0);
          
          switch (t->type)
            {
            case POS_TOKEN_INT:
            case POS_TOKEN_DOUBLE:
            case POS_TOKEN_VARIABLE:
            case POS_TOKEN_OPERATOR:
              break;
              
            case POS_TOKEN_OPEN_PAREN:
              ++paren_level;
              break;

            case POS_TOKEN_CLOSE_PAREN:
              if (paren_level == 1)
                {
                  /* We closed a toplevel paren group, so recurse */
                  if (!pos_eval_helper (&tokens[first_paren+1],
                                        i - first_paren - 1,
                                        env,
                                        &exprs[n_exprs],
                                        err))
                    return FALSE;
                  
                  ++n_exprs;
                }
          
              --paren_level;
              break;
          
            }
        }

      ++i;
    }

  if (paren_level > 0)
    {
      g_set_error (err, META_POSITION_EXPR_ERROR,
                   META_POSITION_EXPR_ERROR_BAD_PARENS,
                   _("Coordinate expression had an open parenthesis with no close parenthesis"));
      return FALSE;      
    }

  /* Now we have no parens and no vars; so we just do all the multiplies
   * and divides, then all the add and subtract.
   */
  if (n_exprs == 0)
    {
      g_set_error (err, META_POSITION_EXPR_ERROR,
                   META_POSITION_EXPR_ERROR_FAILED,
                   _("Coordinate expression doesn't seem to have any operators or operands"));
      return FALSE;
    }

  /* precedence 1 ops */
  if (!do_operations (exprs, &n_exprs, 1, err))
    return FALSE;

  /* precedence 0 ops */
  if (!do_operations (exprs, &n_exprs, 0, err))
    return FALSE;

  g_assert (n_exprs == 1);

  *result = *exprs;

  return TRUE;
}

/*
 *   expr = int | double | expr * expr | expr / expr |
 *          expr + expr | expr - expr | (expr)
 *
 *   so very not worth fooling with bison, yet so very painful by hand.
 */
static gboolean
pos_eval (PosToken                  *tokens,
          int                        n_tokens,
          const MetaPositionExprEnv *env,
          int                       *val_p,
          GError                   **err)
{
  PosExpr expr;
  
  *val_p = 0;

  if (pos_eval_helper (tokens, n_tokens, env, &expr, err))
    {
      switch (expr.type)
        {
        case POS_EXPR_INT:
          *val_p = expr.d.int_val;
          break;
        case POS_EXPR_DOUBLE:
          *val_p = expr.d.double_val;
          break;
        case POS_EXPR_OPERATOR:
          g_assert_not_reached ();
          break;
        }
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

/* We always return both X and Y, but only one will be meaningful in
 * most contexts.
 */
            
gboolean
meta_parse_position_expression (const char                *expr,
                                const MetaPositionExprEnv *env,
                                int                       *x_return,
                                int                       *y_return,
                                GError                   **err)
{
  /* All positions are in a coordinate system with x, y at the origin.
   * The expression can have -, +, *, / as operators, floating point
   * or integer constants, and the variables "width" and "height" and
   * optionally "object_width" and object_height". Negative numbers
   * aren't allowed.
   */
  PosToken *tokens;
  int n_tokens;
  int val;
  
  if (!pos_tokenize (expr, &tokens, &n_tokens, err))
    {
      g_assert (err == NULL || *err != NULL);
      return FALSE;
    }

#if 0
  g_print ("Tokenized \"%s\" to --->\n", expr);
  debug_print_tokens (tokens, n_tokens);
#endif

  if (pos_eval (tokens, n_tokens, env, &val, err))
    {
      if (x_return)    
        *x_return = env->x + val;
      if (y_return)    
        *y_return = env->y + val;
      free_tokens (tokens, n_tokens);
      return TRUE;
    }
  else
    {
      g_assert (err == NULL || *err != NULL);
      free_tokens (tokens, n_tokens);
      return FALSE;
    }
}


gboolean
meta_parse_size_expression (const char                *expr,
                            const MetaPositionExprEnv *env,
                            int                       *val_return,
                            GError                   **err)
{
  PosToken *tokens;
  int n_tokens;
  int val;
  
  if (!pos_tokenize (expr, &tokens, &n_tokens, err))
    {
      g_assert (err == NULL || *err != NULL);
      return FALSE;
    }

#if 0
  g_print ("Tokenized \"%s\" to --->\n", expr);
  debug_print_tokens (tokens, n_tokens);
#endif

  if (pos_eval (tokens, n_tokens, env, &val, err))
    {
      if (val_return)    
        *val_return = val;
      free_tokens (tokens, n_tokens);
      return TRUE;
    }
  else
    {
      g_assert (err == NULL || *err != NULL);
      free_tokens (tokens, n_tokens);
      return FALSE;
    }
}

static int
parse_x_position_unchecked (const char                *expr,
                            const MetaPositionExprEnv *env)
{
  int retval;
  GError *error;  
  
  retval = 0;
  error = NULL;
  if (!meta_parse_position_expression (expr, env,
                                       &retval, NULL,
                                       &error))
    {
      meta_warning (_("Theme contained an expression \"%s\" that resulted in an error: %s\n"),
                    expr, error->message);

      g_error_free (error);
    }
  
  return retval;
}

static int
parse_y_position_unchecked (const char                *expr,
                            const MetaPositionExprEnv *env)
{
  int retval;
  GError *error;  
  
  retval = 0;
  error = NULL;
  if (!meta_parse_position_expression (expr, env,
                                       NULL, &retval,
                                       &error))
    {
      meta_warning (_("Theme contained an expression \"%s\" that resulted in an error: %s\n"),
                    expr, error->message);

      g_error_free (error);
    }
  
  return retval;
}

static int
parse_size_unchecked (const char          *expr,
                      MetaPositionExprEnv *env)
{
  int retval;
  GError *error;  
  
  retval = 0;
  error = NULL;
  if (!meta_parse_size_expression (expr, env,
                                   &retval, &error))
    {
      meta_warning (_("Theme contained an expression \"%s\" that resulted in an error: %s\n"),
                    expr, error->message);

      g_error_free (error);
    }
  
  return retval;
}


MetaDrawOp*
meta_draw_op_new (MetaDrawType type)
{
  MetaDrawOp *op;
  MetaDrawOp dummy;
  int size;

  size = G_STRUCT_OFFSET (MetaDrawOp, data);
  
  switch (type)
    {
    case META_DRAW_LINE:
      size += sizeof (dummy.data.line);
      break;

    case META_DRAW_RECTANGLE:
      size += sizeof (dummy.data.rectangle);
      break;

    case META_DRAW_ARC:
      size += sizeof (dummy.data.arc);
      break;

    case META_DRAW_TINT:
      size += sizeof (dummy.data.tint);
      break;

    case META_DRAW_GRADIENT:
      size += sizeof (dummy.data.gradient);
      break;

    case META_DRAW_IMAGE:
      size += sizeof (dummy.data.image);
      break;
      
    case META_DRAW_GTK_ARROW:
      size += sizeof (dummy.data.gtk_arrow);
      break;

    case META_DRAW_GTK_BOX:
      size += sizeof (dummy.data.gtk_box);
      break;

    case META_DRAW_GTK_VLINE:
      size += sizeof (dummy.data.gtk_vline);
      break;
    }
  
  op = g_malloc0 (size);

  op->type = type;  
  
  return op;
}

void
meta_draw_op_free (MetaDrawOp *op)
{
  g_return_if_fail (op != NULL);
  
  switch (op->type)
    {
    case META_DRAW_LINE:
      g_free (op->data.line.x1);
      g_free (op->data.line.y1);
      g_free (op->data.line.x2);
      g_free (op->data.line.y2);
      break;

    case META_DRAW_RECTANGLE:
      if (op->data.rectangle.color_spec)
        g_free (op->data.rectangle.color_spec);
      g_free (op->data.rectangle.x);
      g_free (op->data.rectangle.y);
      g_free (op->data.rectangle.width);
      g_free (op->data.rectangle.height);
      break;

    case META_DRAW_ARC:
      if (op->data.arc.color_spec)
        g_free (op->data.arc.color_spec);
      g_free (op->data.arc.x);
      g_free (op->data.arc.y);
      g_free (op->data.arc.width);
      g_free (op->data.arc.height);
      break;

    case META_DRAW_TINT:
      if (op->data.tint.color_spec)
        meta_color_spec_free (op->data.tint.color_spec);
      g_free (op->data.tint.x);
      g_free (op->data.tint.y);
      g_free (op->data.tint.width);
      g_free (op->data.tint.height);
      break;

    case META_DRAW_GRADIENT:
      if (op->data.gradient.gradient_spec)
        meta_gradient_spec_free (op->data.gradient.gradient_spec);
      g_free (op->data.gradient.x);
      g_free (op->data.gradient.y);
      g_free (op->data.gradient.width);
      g_free (op->data.gradient.height);
      break;

    case META_DRAW_IMAGE:
      if (op->data.image.pixbuf)
        g_object_unref (G_OBJECT (op->data.image.pixbuf));
      g_free (op->data.image.x);
      g_free (op->data.image.y);
      g_free (op->data.image.width);
      g_free (op->data.image.height);
      break;

      
    case META_DRAW_GTK_ARROW:
      g_free (op->data.gtk_arrow.x);
      g_free (op->data.gtk_arrow.y);
      g_free (op->data.gtk_arrow.width);
      g_free (op->data.gtk_arrow.height);
      break;

    case META_DRAW_GTK_BOX:
      g_free (op->data.gtk_box.x);
      g_free (op->data.gtk_box.y);
      g_free (op->data.gtk_box.width);
      g_free (op->data.gtk_box.height);
      break;

    case META_DRAW_GTK_VLINE:
      g_free (op->data.gtk_vline.x);
      g_free (op->data.gtk_vline.y1);
      g_free (op->data.gtk_vline.y2);
      break;
    }

  g_free (op);
}

static GdkGC*
get_gc_for_primitive (GtkWidget          *widget,
                      GdkDrawable        *drawable,
                      MetaColorSpec      *color_spec,
                      const GdkRectangle *clip,
                      int                 line_width)
{
  GdkGC *gc;
  GdkGCValues values;  
  GdkColor color;
  
  meta_color_spec_render (color_spec, widget, &color);
  
  values.foreground = color;
  gdk_rgb_find_color (widget->style->colormap, &values.foreground);
  values.line_width = line_width;
  
  gc = gdk_gc_new_with_values (drawable, &values,
                               GDK_GC_FOREGROUND | GDK_GC_LINE_WIDTH);

  if (clip)
    gdk_gc_set_clip_rectangle (gc,
                               (GdkRectangle*) clip); /* const cast */

  return gc;
}

static GdkPixbuf*
multiply_alpha (GdkPixbuf *pixbuf,
                guchar     alpha)
{
  GdkPixbuf *new_pixbuf;
  guchar *pixels;
  int rowstride;
  int height;
  int row;

  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);
  
  if (alpha == 255)
    return pixbuf;

  if (!gdk_pixbuf_get_has_alpha (pixbuf))
    {
      new_pixbuf = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);
      g_object_unref (G_OBJECT (pixbuf));
      pixbuf = new_pixbuf;
    }
  
  pixels = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  row = 0;
  while (row < height)
    {
      guchar *p;
      guchar *end;

      p = pixels + row * rowstride;
      end = p + rowstride;

      while (p != end)
        {
          p += 3; /* skip RGB */

          /* multiply the two alpha channels. not sure this is right.
           * but some end cases are that if the pixbuf contains 255,
           * then it should be modified to contain "alpha"; if the
           * pixbuf contains 0, it should remain 0.
           */
          *p = (*p * alpha) / 65025; /* (*p / 255) * (alpha / 255); */
          
          ++p; /* skip A */
        }
      
      ++row;
    }

  return pixbuf;
}

static void
render_pixbuf (GdkDrawable        *drawable,
               const GdkRectangle *clip,
               GdkPixbuf          *pixbuf,
               int                 x,
               int                 y)
{
  /* grumble, render_to_drawable_alpha does not accept a clip
   * mask, so we have to go through some BS
   */
  /* FIXME once GTK 1.3.13 has been out a while we can use
   * render_to_drawable() which now does alpha with clip
   */
  GdkRectangle pixbuf_rect;
  GdkRectangle draw_rect;
  
  pixbuf_rect.x = x;
  pixbuf_rect.y = y;
  pixbuf_rect.width = gdk_pixbuf_get_width (pixbuf);
  pixbuf_rect.height = gdk_pixbuf_get_height (pixbuf);

  if (clip)
    {
      if (!gdk_rectangle_intersect ((GdkRectangle*)clip,
                                    &pixbuf_rect, &draw_rect))
        return;
    }
  else
    {
      draw_rect = pixbuf_rect;
    }
  
  gdk_pixbuf_render_to_drawable_alpha (pixbuf,
                                       drawable,
                                       draw_rect.x - pixbuf_rect.x,
                                       draw_rect.y - pixbuf_rect.y,
                                       draw_rect.x, draw_rect.y,
                                       draw_rect.width,
                                       draw_rect.height,
                                       GDK_PIXBUF_ALPHA_FULL, /* ignored */
                                       128,                   /* ignored */
                                       GDK_RGB_DITHER_NORMAL,
                                       draw_rect.x - pixbuf_rect.x,
                                       draw_rect.y - pixbuf_rect.y);
}

static GdkPixbuf*
draw_op_as_pixbuf (const MetaDrawOp    *op,
                   GtkWidget           *widget,
                   int                  width,
                   int                  height)
{
  /* Try to get the op as a pixbuf, assuming w/h in the op
   * matches the width/height passed in. return NULL
   * if the op can't be converted to an equivalent pixbuf.
   */
  GdkPixbuf *pixbuf;

  pixbuf = NULL;
  
  switch (op->type)
    {
    case META_DRAW_LINE:
      break;

    case META_DRAW_RECTANGLE:
      if (op->data.rectangle.filled)
        {
          GdkColor color;

          meta_color_spec_render (op->data.rectangle.color_spec,
                                  widget,
                                  &color);
          
          pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                   FALSE,
                                   8, width, height);
          
          gdk_pixbuf_fill (pixbuf, GDK_COLOR_RGBA (color));
        }
      break;

    case META_DRAW_ARC:
      break;

    case META_DRAW_TINT:
      {
        GdkColor color;
        guint32 rgba;
        
        meta_color_spec_render (op->data.rectangle.color_spec,
                                widget,
                                &color);
        
        pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                 ALPHA_TO_UCHAR (op->data.tint.alpha) < 255,
                                 8, width, height);

        rgba = GDK_COLOR_RGBA (color);
        rgba &= ~0xff;
        rgba |= ALPHA_TO_UCHAR (op->data.tint.alpha);
        
        gdk_pixbuf_fill (pixbuf, rgba);
      }
      break;

    case META_DRAW_GRADIENT:
      {
        pixbuf = meta_gradient_spec_render (op->data.gradient.gradient_spec,
                                            widget, width, height);

        pixbuf = multiply_alpha (pixbuf,
                                 ALPHA_TO_UCHAR (op->data.gradient.alpha));
      }
      break;

    case META_DRAW_IMAGE:
      {                
        switch (op->data.image.scale_mode)
          {
          case META_SCALE_NONE:
            pixbuf = op->data.image.pixbuf;
            g_object_ref (G_OBJECT (pixbuf));
            break;
          case META_SCALE_VERTICALLY:
            pixbuf = op->data.image.pixbuf;
            if (gdk_pixbuf_get_height (pixbuf) == height)
              {
                g_object_ref (G_OBJECT (pixbuf));
              }
            else
              {
                pixbuf = gdk_pixbuf_scale_simple (pixbuf,
                                                  gdk_pixbuf_get_width (pixbuf),
                                                  height,
                                                  GDK_INTERP_BILINEAR);
              }
            break;
          case META_SCALE_HORIZONTALLY:
            pixbuf = op->data.image.pixbuf;
            if (gdk_pixbuf_get_width (pixbuf) == width)
              {
                g_object_ref (G_OBJECT (pixbuf));
              }
            else
              {
                pixbuf = gdk_pixbuf_scale_simple (pixbuf,
                                                  width,
                                                  gdk_pixbuf_get_height (pixbuf),
                                                  GDK_INTERP_BILINEAR);
              }
            break;
          case META_SCALE_BOTH:
            pixbuf = op->data.image.pixbuf;
            if (gdk_pixbuf_get_width (pixbuf) == width &&
                gdk_pixbuf_get_height (pixbuf) == height)
              {
                g_object_ref (G_OBJECT (pixbuf));
              }
            else
              {
                pixbuf = gdk_pixbuf_scale_simple (pixbuf,
                                                  width, height,
                                                  GDK_INTERP_BILINEAR);
              }
            break;
          }

        pixbuf = multiply_alpha (pixbuf,
                                 ALPHA_TO_UCHAR (op->data.image.alpha));
      }
      break;

      
    case META_DRAW_GTK_ARROW:
    case META_DRAW_GTK_BOX:
    case META_DRAW_GTK_VLINE:
      break;  
    }

  return pixbuf;
}

void
meta_draw_op_draw (const MetaDrawOp    *op,
                   GtkWidget           *widget,
                   GdkDrawable         *drawable,
                   const GdkRectangle  *clip,
                   int                  x,
                   int                  y,
                   int                  width,
                   int                  height)
{
  GdkGC *gc;
  MetaPositionExprEnv env;

  env.x = x;
  env.y = y;
  env.width = width;
  env.height = height;
  env.object_width = -1;
  env.object_height = -1;
  
  switch (op->type)
    {
    case META_DRAW_LINE:
      {
        int x1, x2, y1, y2;
        
        gc = get_gc_for_primitive (widget, drawable,
                                   op->data.line.color_spec,
                                   clip,
                                   op->data.line.width);
        
        if (op->data.line.dash_on_length > 0 &&
            op->data.line.dash_off_length > 0)
          {
            gint8 dash_list[2];
            dash_list[0] = op->data.line.dash_on_length;
            dash_list[1] = op->data.line.dash_off_length;
            gdk_gc_set_dashes (gc, 0, dash_list, 2);
          }

        x1 = parse_x_position_unchecked (op->data.line.x1, &env);
        y1 = parse_y_position_unchecked (op->data.line.y1, &env);
        x2 = parse_x_position_unchecked (op->data.line.x2, &env);
        y2 = parse_y_position_unchecked (op->data.line.y2, &env);

        gdk_draw_line (drawable, gc, x1, y1, x2, y2);
        
        g_object_unref (G_OBJECT (gc));
      }
      break;

    case META_DRAW_RECTANGLE:
      {
        int rx, ry, rwidth, rheight;
        
        gc = get_gc_for_primitive (widget, drawable, 
                                   op->data.rectangle.color_spec,
                                   clip, 0);
        
        rx = parse_x_position_unchecked (op->data.rectangle.x, &env);
        ry = parse_y_position_unchecked (op->data.rectangle.y, &env);
        rwidth = parse_size_unchecked (op->data.rectangle.width, &env);
        rheight = parse_size_unchecked (op->data.rectangle.height, &env);

        gdk_draw_rectangle (drawable, gc,
                            op->data.rectangle.filled,
                            rx, ry, rwidth, rheight);
        
        g_object_unref (G_OBJECT (gc));
      }
      break;

    case META_DRAW_ARC:
      {
        int rx, ry, rwidth, rheight;
        
        gc = get_gc_for_primitive (widget, drawable,
                                   op->data.arc.color_spec,
                                   clip, 0);
        
        rx = parse_x_position_unchecked (op->data.arc.x, &env);
        ry = parse_y_position_unchecked (op->data.arc.y, &env);
        rwidth = parse_size_unchecked (op->data.arc.width, &env);
        rheight = parse_size_unchecked (op->data.arc.height, &env);

        gdk_draw_arc (drawable,
                      gc,
                      op->data.arc.filled,
                      rx, ry, rwidth, rheight,
                      op->data.arc.start_angle * (360.0 * 64.0) -
                      (90.0 * 64.0), /* start at 12 instead of 3 oclock */
                      op->data.arc.extent_angle * (360.0 * 64.0));
        
        g_object_unref (G_OBJECT (gc));
      }
      break;

    case META_DRAW_TINT:
      {
        int rx, ry, rwidth, rheight;

        rx = parse_x_position_unchecked (op->data.tint.x, &env);
        ry = parse_y_position_unchecked (op->data.tint.y, &env);
        rwidth = parse_size_unchecked (op->data.tint.width, &env);
        rheight = parse_size_unchecked (op->data.tint.height, &env);

        if (ALPHA_TO_UCHAR (op->data.tint.alpha) == 255)
          {
            gc = get_gc_for_primitive (widget, drawable,
                                       op->data.tint.color_spec,
                                       clip, 0);

            gdk_draw_rectangle (drawable, gc,
                                TRUE,
                                rx, ry, rwidth, rheight);
            
            g_object_unref (G_OBJECT (gc));
          }
        else
          {
            GdkPixbuf *pixbuf;

            pixbuf = draw_op_as_pixbuf (op, widget, rwidth, rheight);

            render_pixbuf (drawable, clip, pixbuf, rx, ry);

            g_object_unref (G_OBJECT (pixbuf));
          }
      }
      break;

    case META_DRAW_GRADIENT:
      {
        int rx, ry, rwidth, rheight;
        GdkPixbuf *pixbuf;
        
        rx = parse_x_position_unchecked (op->data.gradient.x, &env);
        ry = parse_y_position_unchecked (op->data.gradient.y, &env);
        rwidth = parse_size_unchecked (op->data.gradient.width, &env);
        rheight = parse_size_unchecked (op->data.gradient.height, &env);

        pixbuf = draw_op_as_pixbuf (op, widget, rwidth, rheight);
        
        render_pixbuf (drawable, clip, pixbuf, rx, ry);
        
        g_object_unref (G_OBJECT (pixbuf));        
      }
      break;
      
    case META_DRAW_IMAGE:
      {
        int rx, ry, rwidth, rheight;
        GdkPixbuf *pixbuf;
        
        rx = parse_x_position_unchecked (op->data.image.x, &env);
        ry = parse_y_position_unchecked (op->data.image.y, &env);
        rwidth = parse_size_unchecked (op->data.image.width, &env);
        rheight = parse_size_unchecked (op->data.image.height, &env);

        pixbuf = draw_op_as_pixbuf (op, widget, rwidth, rheight);

        env.object_width = gdk_pixbuf_get_width (pixbuf);
        env.object_height = gdk_pixbuf_get_height (pixbuf);
        
        render_pixbuf (drawable, clip, pixbuf, rx, ry);
        
        g_object_unref (G_OBJECT (pixbuf));        
      }
      break;
      
    case META_DRAW_GTK_ARROW:
      {
        int rx, ry, rwidth, rheight;

        rx = parse_x_position_unchecked (op->data.gtk_arrow.x, &env);
        ry = parse_y_position_unchecked (op->data.gtk_arrow.y, &env);
        rwidth = parse_size_unchecked (op->data.gtk_arrow.width, &env);
        rheight = parse_size_unchecked (op->data.gtk_arrow.height, &env);

        gtk_paint_arrow (widget->style,
                         drawable,
                         op->data.gtk_arrow.state,
                         op->data.gtk_arrow.shadow,
                         (GdkRectangle*) clip,
                         widget,
                         "metacity",
                         op->data.gtk_arrow.arrow,
                         op->data.gtk_arrow.filled,
                         rx, ry, rwidth, rheight);
      }
      break;

    case META_DRAW_GTK_BOX:
      {
        int rx, ry, rwidth, rheight;
        
        rx = parse_x_position_unchecked (op->data.gtk_box.x, &env);
        ry = parse_y_position_unchecked (op->data.gtk_box.y, &env);
        rwidth = parse_size_unchecked (op->data.gtk_box.width, &env);
        rheight = parse_size_unchecked (op->data.gtk_box.height, &env);

        gtk_paint_box (widget->style,
                       drawable,
                       op->data.gtk_box.state,
                       op->data.gtk_box.shadow,
                       (GdkRectangle*) clip,
                       widget,
                       "metacity",
                       rx, ry, rwidth, rheight);
      }
      break;

    case META_DRAW_GTK_VLINE:
      {
        int rx, ry1, ry2;
        
        rx = parse_x_position_unchecked (op->data.gtk_vline.x, &env);
        ry1 = parse_y_position_unchecked (op->data.gtk_vline.y1, &env);
        ry2 = parse_y_position_unchecked (op->data.gtk_vline.y2, &env);

        gtk_paint_vline (widget->style,
                         drawable,
                         op->data.gtk_vline.state,
                         (GdkRectangle*) clip,
                         widget,
                         "metacity",
                         rx, ry1, ry2);
      }
      break;
    }
}

MetaDrawOpList*
meta_draw_op_list_new (int n_preallocs)
{
  MetaDrawOpList *op_list;

  g_return_val_if_fail (n_preallocs >= 0, NULL);
  
  op_list = g_new (MetaDrawOpList, 1);

  op_list->refcount = 1;
  op_list->n_allocated = n_preallocs;
  op_list->ops = g_new (MetaDrawOp*, op_list->n_allocated);
  op_list->n_ops = 0;
  
  return op_list;
}

void
meta_draw_op_list_ref (MetaDrawOpList *op_list)
{
  g_return_if_fail (op_list != NULL);

  op_list->refcount += 1;
}

void
meta_draw_op_list_unref (MetaDrawOpList *op_list)
{
  g_return_if_fail (op_list != NULL);
  g_return_if_fail (op_list->refcount > 0);

  op_list->refcount -= 1;

  if (op_list->refcount == 0)
    {
      int i;

      i = 0;
      while (i < op_list->n_ops)
        {
          meta_draw_op_free (op_list->ops[i]);
          ++i;
        }

      g_free (op_list->ops);
      
      g_free (op_list);
    }
}

void
meta_draw_op_list_draw  (const MetaDrawOpList *op_list,
                         GtkWidget            *widget,
                         GdkDrawable          *drawable,
                         const GdkRectangle   *clip,
                         int                   x,
                         int                   y,
                         int                   width,
                         int                   height)
{
  int i;

  /* FIXME this can be optimized, potentially a lot, by
   * compressing multiple ops when possible. For example,
   * anything convertible to a pixbuf can be composited
   * client-side, and putting a color tint over a pixbuf
   * can be done without creating the solid-color pixbuf.
   */
  
  i = 0;
  while (i < op_list->n_ops)
    {
      meta_draw_op_draw (op_list->ops[i],
                         widget, drawable, clip,
                         x, y, width, height);
      ++i;
    }
}

void
meta_draw_op_list_append (MetaDrawOpList       *op_list,
                          MetaDrawOp           *op)
{
  if (op_list->n_ops == op_list->n_allocated)
    {
      op_list->n_allocated *= 2;
      op_list->ops = g_renew (MetaDrawOp*, op_list->ops, op_list->n_allocated);
    }

  op_list->ops[op_list->n_ops] = op;
  op_list->n_ops += 1;
}

MetaFrameStyle*
meta_frame_style_new (MetaFrameStyle *parent)
{
  MetaFrameStyle *style;

  style = g_new0 (MetaFrameStyle, 1);

  style->refcount = 1;

  style->parent = parent;
  if (parent)
    meta_frame_style_ref (parent);
  
  return style;
}

void
meta_frame_style_ref (MetaFrameStyle *style)
{
  g_return_if_fail (style != NULL);
  
  style->refcount += 1;
}

static void
free_button_ops (MetaDrawOpList *op_lists[META_BUTTON_TYPE_LAST][META_BUTTON_STATE_LAST])
{
  int i, j;
  
  i = 0;
  while (i < META_BUTTON_TYPE_LAST)
    {
      j = 0;
      while (j < META_BUTTON_STATE_LAST)
        {
          if (op_lists[i][j])
            meta_draw_op_list_unref (op_lists[i][j]);
          
          ++j;
        }
      
      ++i;
    }
}

void
meta_frame_style_unref (MetaFrameStyle *style)
{
  g_return_if_fail (style != NULL);
  g_return_if_fail (style->refcount > 0);
  
  style->refcount -= 1;

  if (style->refcount == 0)
    {
      int i;
      
      free_button_ops (style->button_icons);
      free_button_ops (style->button_backgrounds);

      i = 0;
      while (i < META_FRAME_PIECE_LAST)
        {
          if (style->pieces[i])
            meta_draw_op_list_unref (style->pieces[i]);
          
          ++i;
        }

      if (style->layout)
        meta_frame_layout_free (style->layout);

      /* we hold a reference to any parent style */
      if (style->parent)
        meta_frame_style_unref (style->parent);
      
      g_free (style);
    }
}

static void
button_rect (MetaButtonType type,
             MetaFrameGeometry *fgeom,
             GdkRectangle *rect)
{
  switch (type)
    {
    case META_BUTTON_TYPE_CLOSE:
      *rect = fgeom->close_rect;
      break;
      
    case META_BUTTON_TYPE_MAXIMIZE:
      *rect = fgeom->max_rect;
      break;
      
    case META_BUTTON_TYPE_MINIMIZE:
      *rect = fgeom->min_rect;
      break;
      
    case META_BUTTON_TYPE_MENU:
      *rect = fgeom->menu_rect;
      break;

    case META_BUTTON_TYPE_LAST:
      g_assert_not_reached ();
      break;
    }
}

void
meta_frame_style_draw (MetaFrameStyle     *style,
                       GtkWidget          *widget,
                       GdkDrawable        *drawable,
                       int                 x_offset,
                       int                 y_offset,
                       const GdkRectangle *clip,
                       MetaFrameFlags      flags,
                       int                 client_width,
                       int                 client_height,
                       PangoLayout        *title_layout,
                       int                 text_height,
                       MetaButtonState     button_states[META_BUTTON_TYPE_LAST])
{
  int i;
  MetaFrameGeometry fgeom;
  GdkRectangle titlebar_rect;
  GdkRectangle left_titlebar_edge;
  GdkRectangle right_titlebar_edge;
  GdkRectangle bottom_titlebar_edge;
  GdkRectangle top_titlebar_edge;
  GdkRectangle left_edge, right_edge, bottom_edge;
  
  meta_frame_layout_calc_geometry (style->layout,
                                   widget,
                                   text_height,
                                   flags,
                                   client_width, client_height,
                                   &fgeom);

  titlebar_rect.x = 0;
  titlebar_rect.y = 0;
  titlebar_rect.width = fgeom.width;
  titlebar_rect.height = fgeom.top_height;

  left_titlebar_edge.x = titlebar_rect.x;
  left_titlebar_edge.y = titlebar_rect.y + fgeom.top_titlebar_edge;
  left_titlebar_edge.width = fgeom.left_titlebar_edge;
  left_titlebar_edge.height = titlebar_rect.height - fgeom.top_titlebar_edge - fgeom.bottom_titlebar_edge;

  right_titlebar_edge.y = left_titlebar_edge.y;
  right_titlebar_edge.height = left_titlebar_edge.height;
  right_titlebar_edge.width = fgeom.right_titlebar_edge;
  right_titlebar_edge.x = titlebar_rect.x + titlebar_rect.width - right_titlebar_edge.width;
  
  top_titlebar_edge.x = titlebar_rect.x;
  top_titlebar_edge.y = titlebar_rect.y;
  top_titlebar_edge.width = titlebar_rect.width;
  top_titlebar_edge.height = fgeom.top_titlebar_edge;

  bottom_titlebar_edge.x = titlebar_rect.x;
  bottom_titlebar_edge.width = titlebar_rect.width;
  bottom_titlebar_edge.height = fgeom.bottom_titlebar_edge;
  bottom_titlebar_edge.y = titlebar_rect.y + titlebar_rect.height - bottom_titlebar_edge.height;  

  left_edge.x = 0;
  left_edge.y = fgeom.top_height;
  left_edge.width = fgeom.left_width;
  left_edge.height = fgeom.height - fgeom.top_height - fgeom.bottom_height;

  right_edge.x = fgeom.width - fgeom.right_width;
  right_edge.y = fgeom.top_height;
  right_edge.width = fgeom.right_width;
  right_edge.height = fgeom.height - fgeom.top_height - fgeom.bottom_height;

  bottom_edge.x = 0;
  bottom_edge.y = fgeom.height - fgeom.bottom_height;
  bottom_edge.width = fgeom.width;
  bottom_edge.height = fgeom.bottom_height;
  
  /* The enum is in the order the pieces should be rendered. */
  i = 0;
  while (i < META_FRAME_PIECE_LAST)
    {
      GdkRectangle rect;
      GdkRectangle combined_clip;
      gboolean draw_title_text = FALSE;
      
      switch ((MetaFramePiece) i)
        {
        case META_FRAME_PIECE_ENTIRE_BACKGROUND:
          rect.x = 0;
          rect.y = 0;
          rect.width = fgeom.width;
          rect.height = fgeom.height;
          break;

        case META_FRAME_PIECE_TITLEBAR_BACKGROUND:
          rect = titlebar_rect;
          break;

        case META_FRAME_PIECE_LEFT_TITLEBAR_EDGE:
          rect = left_titlebar_edge;
          break;

        case META_FRAME_PIECE_RIGHT_TITLEBAR_EDGE:
          rect = right_titlebar_edge;
          break;

        case META_FRAME_PIECE_TOP_TITLEBAR_EDGE:
          rect = top_titlebar_edge;
          break;

        case META_FRAME_PIECE_BOTTOM_TITLEBAR_EDGE:
          rect = bottom_titlebar_edge;
          break;

        case META_FRAME_PIECE_TITLEBAR_MIDDLE:
          rect.x = left_titlebar_edge.x + left_titlebar_edge.width;
          rect.y = top_titlebar_edge.y + top_titlebar_edge.height;
          rect.width = titlebar_rect.width - left_titlebar_edge.width -
            right_titlebar_edge.width;
          rect.height = titlebar_rect.height - top_titlebar_edge.height - bottom_titlebar_edge.height;
          break;
          
        case META_FRAME_PIECE_TITLE_BACKGROUND:
          rect = fgeom.title_rect;

          /* Trigger drawing the title itself */
          draw_title_text = TRUE;
          break;
          
        case META_FRAME_PIECE_LEFT_EDGE:
          rect = left_edge;
          break;

        case META_FRAME_PIECE_RIGHT_EDGE:
          rect = right_edge;
          break;

        case META_FRAME_PIECE_BOTTOM_EDGE:
          rect = bottom_edge;
          break;

        case META_FRAME_PIECE_OVERLAY:
          rect.x = 0;
          rect.y = 0;
          rect.width = fgeom.width;
          rect.height = fgeom.height;
          break;

        case META_FRAME_PIECE_LAST:
          g_assert_not_reached ();
          break;
        }

      rect.x += x_offset;
      rect.y += y_offset;
      
      if (clip == NULL)
        combined_clip = rect;
      else
        gdk_rectangle_intersect ((GdkRectangle*) clip, /* const cast */
                                 &rect,
                                 &combined_clip);

      if (combined_clip.width > 0 && combined_clip.height > 0)
        {
          MetaDrawOpList *op_list;
          MetaFrameStyle *parent;

          parent = style;
          op_list = NULL;
          while (parent && op_list == NULL)
            {
              op_list = style->pieces[i];
              parent = style->parent;
            }

          if (op_list)
            meta_draw_op_list_draw (op_list,
                                    widget,
                                    drawable,
                                    &combined_clip,
                                    rect.x, rect.y, rect.width, rect.height);
        }

      if (draw_title_text)
        {
          /* FIXME */
          
          /* Deal with whole icon-in-the-titlebar issue; there should
           * probably be "this window's icon" and "this window's mini-icon"
           * textures, and a way to stick textures on either end of
           * the title text as well as behind the text.
           * Most likely the text_rect should extend a configurable
           * distance from either end of the text.
           */
        }
      
      ++i;
    }

  /* Now we draw button backgrounds */
  i = 0;
  while (i < META_BUTTON_TYPE_LAST)
    {
      GdkRectangle rect;
      GdkRectangle combined_clip;

      button_rect (i, &fgeom, &rect);

      rect.x += x_offset;
      rect.y += y_offset;
      
      if (clip == NULL)
        combined_clip = rect;
      else
        gdk_rectangle_intersect ((GdkRectangle*) clip, /* const cast */
                                 &rect,
                                 &combined_clip);

      if (combined_clip.width > 0 && combined_clip.height > 0)
        {
          MetaDrawOpList *op_list;
          MetaFrameStyle *parent;
          
          parent = style;
          op_list = NULL;
          while (parent && op_list == NULL)
            {
              op_list = style->button_backgrounds[i][button_states[i]];
              parent = style->parent;
            }
          
          if (op_list)
            meta_draw_op_list_draw (op_list,
                                    widget,
                                    drawable,
                                    &combined_clip,
                                    rect.x, rect.y, rect.width, rect.height);
        }
      
      ++i;
    }

  /* And button icons */
  i = 0;
  while (i < META_BUTTON_TYPE_LAST)
    {
      GdkRectangle rect;
      GdkRectangle combined_clip;

      button_rect (i, &fgeom, &rect);

      rect.x += x_offset;
      rect.y += y_offset;
      
      if (clip == NULL)
        combined_clip = rect;
      else
        gdk_rectangle_intersect ((GdkRectangle*) clip, /* const cast */
                                 &rect,
                                 &combined_clip);

      if (combined_clip.width > 0 && combined_clip.height > 0)
        {
          MetaDrawOpList *op_list;
          MetaFrameStyle *parent;
          
          parent = style;
          op_list = NULL;
          while (parent && op_list == NULL)
            {
              op_list = style->button_icons[i][button_states[i]];
              parent = style->parent;
            }
          
          if (op_list)
            meta_draw_op_list_draw (op_list,
                                    widget,
                                    drawable,
                                    &combined_clip,
                                    rect.x, rect.y, rect.width, rect.height);
        }
      
      ++i;
    }
}

MetaFrameStyleSet*
meta_frame_style_set_new (MetaFrameStyleSet *parent)
{
  MetaFrameStyleSet *style_set;

  style_set = g_new0 (MetaFrameStyleSet, 1);

  style_set->parent = parent;
  if (parent)
    meta_frame_style_set_ref (parent);
  
  return style_set;
}

static void
free_focus_styles (MetaFrameStyle *focus_styles[META_FRAME_FOCUS_LAST])
{
  int i;

  i = 0;
  while (i < META_FRAME_FOCUS_LAST)
    {
      if (focus_styles[i])
        meta_frame_style_unref (focus_styles[i]);

      ++i;
    }
}

void
meta_frame_style_set_ref (MetaFrameStyleSet *style_set)
{
  g_return_if_fail (style_set != NULL);
  
  style_set->refcount += 1;
}

void
meta_frame_style_set_unref (MetaFrameStyleSet *style_set)
{
  g_return_if_fail (style_set != NULL);
  g_return_if_fail (style_set->refcount > 0);

  style_set->refcount -= 1;

  if (style_set->refcount == 0)
    {
      int i;

      i = 0;
      while (i < META_FRAME_RESIZE_LAST)
        {
          free_focus_styles (style_set->normal_styles[i]);
          
          ++i;
        }
      
      free_focus_styles (style_set->maximized_styles);
      free_focus_styles (style_set->shaded_styles);
      free_focus_styles (style_set->maximized_and_shaded_styles);

      if (style_set->parent)
        meta_frame_style_set_unref (style_set->parent);
      
      g_free (style_set);
    }
}

MetaTheme*
meta_theme_new (void)
{
  MetaTheme *theme;

  theme = g_new0 (MetaTheme, 1);

  theme->styles_by_name = g_hash_table_new_full (g_str_hash,
                                                 g_str_equal,
                                                 g_free,
                                                 (GDestroyNotify) meta_frame_style_unref);

  theme->style_sets_by_name = g_hash_table_new_full (g_str_hash,
                                                     g_str_equal,
                                                     g_free,
                                                     (GDestroyNotify) meta_frame_style_set_unref);

  
  return theme;
}

void
meta_theme_free (MetaTheme *theme)
{
  int i;
  
  g_return_if_fail (theme != NULL);

  g_free (theme->name);
  g_free (theme->filename);

  g_hash_table_destroy (theme->styles_by_name);
  g_hash_table_destroy (theme->style_sets_by_name);

  i = 0;
  while (i < META_FRAME_TYPE_LAST)
    {
      if (theme->style_sets_by_type[i])
        meta_frame_style_set_unref (theme->style_sets_by_type[i]);
      ++i;
    }
  
  g_free (theme);
}

static MetaDrawOp*
line_op (MetaGtkColorComponent component,
         const char *x1,
         const char *y1,
         const char *x2,
         const char *y2)
{
  MetaDrawOp *op;
  
  op = meta_draw_op_new (META_DRAW_LINE);
  op->data.line.color_spec =
    meta_color_spec_new_gtk (component, GTK_STATE_NORMAL);
  op->data.line.x1 = g_strdup (x1);
  op->data.line.x2 = g_strdup (x2);
  op->data.line.y1 = g_strdup (y1);
  op->data.line.y2 = g_strdup (y2);
  op->data.line.dash_on_length = 0;
  op->data.line.dash_off_length = 0;
  op->data.line.width = 0;

  return op;
}

#define DEFAULT_INNER_BUTTON_BORDER 3
MetaFrameStyle*
meta_frame_style_get_test (void)
{
  static MetaFrameStyle *style = NULL;
  static GtkBorder default_title_border = { 3, 4, 4, 3 };
  static GtkBorder default_text_border = { 2, 2, 2, 2 };
  static GtkBorder default_button_border = { 0, 0, 1, 1 };
  static GtkBorder default_inner_button_border = {
    DEFAULT_INNER_BUTTON_BORDER,
    DEFAULT_INNER_BUTTON_BORDER,
    DEFAULT_INNER_BUTTON_BORDER,
    DEFAULT_INNER_BUTTON_BORDER
  };
  MetaDrawOpList *op_list;
  MetaDrawOp *op;
  MetaGradientSpec *gradient;
  
  if (style)
    return style;
  
  style = meta_frame_style_new (NULL);
  
  style->layout = meta_frame_layout_new ();

  style->layout->title_border = default_title_border;
  style->layout->text_border = default_text_border;
  style->layout->button_border = default_button_border;
  style->layout->inner_button_border = default_inner_button_border;
  
  style->layout->left_width = 6;
  style->layout->right_width = 6;
  style->layout->bottom_height = 7;
  style->layout->spacer_padding = 3;
  style->layout->spacer_width = 2;
  style->layout->spacer_height = 11;
  style->layout->right_inset = 6;
  style->layout->left_inset = 6;
  style->layout->button_width = 17;
  style->layout->button_height = 17;

  op_list = meta_draw_op_list_new (3);
  
  /* Gradient background */
  op = meta_draw_op_new (META_DRAW_GRADIENT);
  gradient = meta_gradient_spec_new (META_GRADIENT_VERTICAL);
  gradient->color_specs =
    g_slist_prepend (gradient->color_specs,
                     meta_color_spec_new_gtk (META_GTK_COLOR_BG,
                                              GTK_STATE_NORMAL));
  gradient->color_specs =
    g_slist_prepend (gradient->color_specs,
                     meta_color_spec_new_gtk (META_GTK_COLOR_BG,
                                              GTK_STATE_SELECTED));

  op->data.gradient.gradient_spec = gradient;
  op->data.gradient.alpha = 1.0;
  
  op->data.gradient.x = g_strdup ("0");
  op->data.gradient.y = g_strdup ("0");
  op->data.gradient.width = g_strdup ("width");
  op->data.gradient.height = g_strdup ("height");

  meta_draw_op_list_append (op_list, op);

  /* Put orange tint on parts of window */
  op = meta_draw_op_new (META_DRAW_TINT);
  op->data.tint.color_spec = meta_color_spec_new_from_string ("orange", NULL);
  op->data.tint.alpha = 0.3;
  op->data.gradient.x = g_strdup ("0");
  op->data.gradient.y = g_strdup ("0");
  op->data.gradient.width = g_strdup ("width / 2");
  op->data.gradient.height = g_strdup ("height / 2");

  meta_draw_op_list_append (op_list, op);

  op = meta_draw_op_new (META_DRAW_TINT);
  op->data.tint.color_spec = meta_color_spec_new_from_string ("orange", NULL);
  op->data.tint.alpha = 0.3;
  op->data.gradient.x = g_strdup ("width / 2 + width % 2");
  op->data.gradient.y = g_strdup ("height / 2 + height % 2");
  op->data.gradient.width = g_strdup ("width / 2");
  op->data.gradient.height = g_strdup ("height / 2");

  meta_draw_op_list_append (op_list, op);
  
  style->pieces[META_FRAME_PIECE_ENTIRE_BACKGROUND] = op_list;
  
  op_list = meta_draw_op_list_new (5);
  style->pieces[META_FRAME_PIECE_OVERLAY] = op_list;

  op = meta_draw_op_new (META_DRAW_RECTANGLE);
  op->data.rectangle.color_spec =
    meta_color_spec_new_from_string ("#000000", NULL);
  op->data.rectangle.filled = FALSE;
  op->data.rectangle.x = g_strdup ("0");
  op->data.rectangle.y = g_strdup ("0");
  op->data.rectangle.width = g_strdup ("width - 1");
  op->data.rectangle.height = g_strdup ("height - 1");

  meta_draw_op_list_append (op_list, op);

  meta_draw_op_list_append (op_list, 
                            line_op (META_GTK_COLOR_LIGHT,
                                     "1", "1", "1", "height - 2"));

  meta_draw_op_list_append (op_list, 
                            line_op (META_GTK_COLOR_LIGHT,
                                     "1", "1", "width - 2", "1"));

  meta_draw_op_list_append (op_list, 
                            line_op (META_GTK_COLOR_DARK,
                                     "width - 2", "1", "width - 2", "height - 2"));

  meta_draw_op_list_append (op_list,
                            line_op (META_GTK_COLOR_DARK,
                                     "1", "height - 2", "width - 2", "height - 2"));

  
  
  return style;
}






#if 0
/* These are some functions I'm saving to use in optimizing
 * MetaDrawOpList, namely to pre-composite pixbufs on client side
 * prior to rendering to the server
 */
static void
draw_bg_solid_composite (const MetaTextureSpec *bg,
                         const MetaTextureSpec *fg,
                         double                 alpha,
                         GtkWidget             *widget,
                         GdkDrawable           *drawable,
                         const GdkRectangle    *clip,
                         MetaTextureDrawMode    mode,
                         double                 xalign,
                         double                 yalign,
                         int                    x,
                         int                    y,
                         int                    width,
                         int                    height)
{
  GdkColor bg_color;
  
  g_assert (bg->type == META_TEXTURE_SOLID);
  g_assert (fg->type != META_TEXTURE_COMPOSITE);
  g_assert (fg->type != META_TEXTURE_SHAPE_LIST);
  
  meta_color_spec_render (bg->data.solid.color_spec,
                          widget,
                          &bg_color);  
  
  switch (fg->type)
    {
    case META_TEXTURE_SOLID:
      {
        GdkColor fg_color;

        meta_color_spec_render (fg->data.solid.color_spec,
                                widget,
                                &fg_color);

        color_composite (&bg_color, &fg_color,
                         alpha, &fg_color);

        draw_color_rectangle (widget, drawable, &fg_color, clip,
                              x, y, width, height);
      }
      break;

    case META_TEXTURE_GRADIENT:
      /* FIXME I think we could just composite all the colors in
       * the gradient prior to generating the gradient?
       */
      /* FALL THRU */
    case META_TEXTURE_IMAGE:
      {
        GdkPixbuf *pixbuf;
        GdkPixbuf *composited;
        
        pixbuf = meta_texture_spec_render (fg, widget, mode, 255,
                                           width, height);

        if (pixbuf == NULL)
          return;
        
        composited = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                     gdk_pixbuf_get_has_alpha (pixbuf), 8,
                                     gdk_pixbuf_get_width (pixbuf),
                                     gdk_pixbuf_get_height (pixbuf));

        if (composited == NULL)
          {
            g_object_unref (G_OBJECT (pixbuf));
            return;
          }
        
        gdk_pixbuf_composite_color (pixbuf,
                                    composited,
                                    0, 0,
                                    gdk_pixbuf_get_width (pixbuf),
                                    gdk_pixbuf_get_height (pixbuf),
                                    0.0, 0.0, /* offsets */
                                    1.0, 1.0, /* scale */
                                    GDK_INTERP_BILINEAR,
                                    255 * alpha,
                                    0, 0,     /* check offsets */
                                    0,        /* check size */
                                    GDK_COLOR_RGB (bg_color),
                                    GDK_COLOR_RGB (bg_color));

        /* Need to draw background since pixbuf is not
         * necessarily covering the whole thing
         */
        draw_color_rectangle (widget, drawable, &bg_color, clip,
                              x, y, width, height);
        
        render_pixbuf_aligned (drawable, clip, composited,
                               xalign, yalign,
                               x, y, width, height);
        
        g_object_unref (G_OBJECT (pixbuf));
        g_object_unref (G_OBJECT (composited));
      }
      break;

    case META_TEXTURE_BLANK:      
    case META_TEXTURE_COMPOSITE:
    case META_TEXTURE_SHAPE_LIST:
      g_assert_not_reached ();
      break;
    }
}

static void
draw_bg_gradient_composite (const MetaTextureSpec *bg,
                            const MetaTextureSpec *fg,
                            double                 alpha,
                            GtkWidget             *widget,
                            GdkDrawable           *drawable,
                            const GdkRectangle    *clip,
                            MetaTextureDrawMode    mode,
                            double                 xalign,
                            double                 yalign,
                            int                    x,
                            int                    y,
                            int                    width,
                            int                    height)
{
  g_assert (bg->type == META_TEXTURE_GRADIENT);
  g_assert (fg->type != META_TEXTURE_COMPOSITE);
  g_assert (fg->type != META_TEXTURE_SHAPE_LIST);
  
  switch (fg->type)
    {
    case META_TEXTURE_SOLID:
    case META_TEXTURE_GRADIENT:
    case META_TEXTURE_IMAGE:
      {
        GdkPixbuf *bg_pixbuf;
        GdkPixbuf *fg_pixbuf;
        GdkPixbuf *composited;
        int fg_width, fg_height;
        
        bg_pixbuf = meta_texture_spec_render (bg, widget, mode, 255,
                                              width, height);

        if (bg_pixbuf == NULL)
          return;

        fg_pixbuf = meta_texture_spec_render (fg, widget, mode, 255,
                                              width, height);

        if (fg_pixbuf == NULL)
          {
            g_object_unref (G_OBJECT (bg_pixbuf));            
            return;
          }

        /* gradients always fill the entire target area */
        g_assert (gdk_pixbuf_get_width (bg_pixbuf) == width);
        g_assert (gdk_pixbuf_get_height (bg_pixbuf) == height);
        
        composited = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                     gdk_pixbuf_get_has_alpha (bg_pixbuf), 8,
                                     gdk_pixbuf_get_width (bg_pixbuf),
                                     gdk_pixbuf_get_height (bg_pixbuf));

        if (composited == NULL)
          {
            g_object_unref (G_OBJECT (bg_pixbuf));
            g_object_unref (G_OBJECT (fg_pixbuf));
            return;
          }

        fg_width = gdk_pixbuf_get_width (fg_pixbuf);
        fg_height = gdk_pixbuf_get_height (fg_pixbuf);

        /* If we wanted to be all cool we could deal with the
         * offsets and try to composite only in the clip rectangle,
         * but I just don't care enough to figure it out.
         */
        
        gdk_pixbuf_composite (fg_pixbuf,
                              composited,
                              x + (width - fg_width) * xalign,
                              y + (height - fg_height) * yalign,
                              gdk_pixbuf_get_width (fg_pixbuf),
                              gdk_pixbuf_get_height (fg_pixbuf),
                              0.0, 0.0, /* offsets */
                              1.0, 1.0, /* scale */
                              GDK_INTERP_BILINEAR,
                              255 * alpha);
        
        render_pixbuf (drawable, clip, composited, x, y);
        
        g_object_unref (G_OBJECT (bg_pixbuf));
        g_object_unref (G_OBJECT (fg_pixbuf));
        g_object_unref (G_OBJECT (composited));
      }
      break;

    case META_TEXTURE_BLANK:
    case META_TEXTURE_SHAPE_LIST:
    case META_TEXTURE_COMPOSITE:
      g_assert_not_reached ();
      break;
    }
}
#endif
