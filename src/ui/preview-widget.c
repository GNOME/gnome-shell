/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* 
 * Copyright (C) 2002 Havoc Pennington
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

/*
 * SECTION:preview-widget
 * @title: MetaPreview
 * @short_description: Mutter theme preview widget
 */

#define _GNU_SOURCE
#define _XOPEN_SOURCE 600 /* for the maths routines over floats */

#include <math.h>
#include <gtk/gtk.h>
#include <meta/preview-widget.h>
#include "theme-private.h"

static void     meta_preview_get_preferred_width  (GtkWidget *widget,
                                                   gint      *minimum,
                                                   gint      *natural);
static void     meta_preview_get_preferred_height (GtkWidget *widget,
                                                   gint      *minimum,
                                                   gint      *natural);
static void     meta_preview_size_allocate (GtkWidget        *widget,
                                            GtkAllocation    *allocation);
static gboolean meta_preview_draw          (GtkWidget        *widget,
                                            cairo_t          *cr);
static void     meta_preview_realize       (GtkWidget        *widget);
static void     meta_preview_dispose       (GObject          *object);
static void     meta_preview_finalize      (GObject          *object);

G_DEFINE_TYPE (MetaPreview, meta_preview, GTK_TYPE_BIN);

static void
meta_preview_class_init (MetaPreviewClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;

  gobject_class->dispose = meta_preview_dispose;
  gobject_class->finalize = meta_preview_finalize;

  widget_class->realize = meta_preview_realize;
  widget_class->draw = meta_preview_draw;
  widget_class->get_preferred_width = meta_preview_get_preferred_width;
  widget_class->get_preferred_height = meta_preview_get_preferred_height;
  widget_class->size_allocate = meta_preview_size_allocate;

  gtk_container_class_handle_border_width (GTK_CONTAINER_CLASS (class));
}

static void
meta_preview_init (MetaPreview *preview)
{
  int i;

  gtk_widget_set_has_window (GTK_WIDGET (preview), FALSE);

  i = 0;
  while (i < MAX_BUTTONS_PER_CORNER)
    {
      preview->button_layout.left_buttons[i] = META_BUTTON_FUNCTION_LAST;
      preview->button_layout.right_buttons[i] = META_BUTTON_FUNCTION_LAST;
      ++i;
    }
  
  preview->button_layout.left_buttons[0] = META_BUTTON_FUNCTION_MENU;

  preview->button_layout.right_buttons[0] = META_BUTTON_FUNCTION_MINIMIZE;
  preview->button_layout.right_buttons[1] = META_BUTTON_FUNCTION_MAXIMIZE;
  preview->button_layout.right_buttons[2] = META_BUTTON_FUNCTION_CLOSE;
  
  preview->type = META_FRAME_TYPE_NORMAL;
  preview->flags =
    META_FRAME_ALLOWS_DELETE |
    META_FRAME_ALLOWS_MENU |
    META_FRAME_ALLOWS_MINIMIZE |
    META_FRAME_ALLOWS_MAXIMIZE |
    META_FRAME_ALLOWS_VERTICAL_RESIZE |
    META_FRAME_ALLOWS_HORIZONTAL_RESIZE |
    META_FRAME_HAS_FOCUS |
    META_FRAME_ALLOWS_SHADE |
    META_FRAME_ALLOWS_MOVE;

  preview->borders_cached = FALSE;
}

GtkWidget*
meta_preview_new (void)
{
  MetaPreview *preview;
  
  preview = g_object_new (META_TYPE_PREVIEW, NULL);
  
  return GTK_WIDGET (preview);
}

static void
meta_preview_dispose (GObject *object)
{
  MetaPreview *preview = META_PREVIEW (object);

  g_clear_object (&preview->style_context);

  G_OBJECT_CLASS (meta_preview_parent_class)->dispose (object);
}

static void
meta_preview_finalize (GObject *object)
{
  MetaPreview *preview;

  preview = META_PREVIEW (object);

  g_free (preview->title);
  preview->title = NULL;
  
  G_OBJECT_CLASS (meta_preview_parent_class)->finalize (object);
}

static void
ensure_info (MetaPreview *preview)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (preview);
  
  if (preview->layout == NULL)
    {
      PangoFontDescription *font_desc;
      double scale;
      PangoAttrList *attrs;
      PangoAttribute *attr;

      if (preview->theme)        
        scale = meta_theme_get_title_scale (preview->theme,
                                            preview->type,
                                            preview->flags);
      else
        scale = 1.0;
      
      preview->layout = gtk_widget_create_pango_layout (widget,
                                                        preview->title);
      
      font_desc = meta_gtk_widget_get_font_desc (widget, scale, NULL);
      
      preview->text_height =
        meta_pango_font_desc_get_text_height (font_desc,
                                              gtk_widget_get_pango_context (widget));
          
      attrs = pango_attr_list_new ();
      
      attr = pango_attr_size_new (pango_font_description_get_size (font_desc));
      attr->start_index = 0;
      attr->end_index = G_MAXINT;
      
      pango_attr_list_insert (attrs, attr);
      
      pango_layout_set_attributes (preview->layout, attrs);
      
      pango_attr_list_unref (attrs);      
  
      pango_font_description_free (font_desc);
    }

  if (!preview->borders_cached)
    {
      if (preview->theme)
        meta_theme_get_frame_borders (preview->theme,
                                      preview->type,
                                      preview->text_height,
                                      preview->flags,
                                      &preview->borders);
      else
        meta_frame_borders_clear (&preview->borders);
      preview->borders_cached = TRUE;
    }
}

static gboolean
meta_preview_draw (GtkWidget *widget,
                   cairo_t   *cr)
{
  MetaPreview *preview = META_PREVIEW (widget);
  GtkAllocation allocation;

  gtk_widget_get_allocation (widget, &allocation);

  if (preview->theme)
    {
      int client_width;
      int client_height;
      MetaButtonState button_states[META_BUTTON_TYPE_LAST] =
      {
        META_BUTTON_STATE_NORMAL,
        META_BUTTON_STATE_NORMAL,
        META_BUTTON_STATE_NORMAL,
        META_BUTTON_STATE_NORMAL
      };
  
      ensure_info (preview);
      cairo_save (cr);

      client_width = allocation.width - preview->borders.total.left - preview->borders.total.right;
      client_height = allocation.height - preview->borders.total.top - preview->borders.total.bottom;

      if (client_width < 0)
        client_width = 1;
      if (client_height < 0)
        client_height = 1;  
      
      meta_theme_draw_frame (preview->theme,
                             preview->style_context,
                             cr,
                             preview->type,
                             preview->flags,
                             client_width, client_height,
                             preview->layout,
                             preview->text_height,
                             &preview->button_layout,
                             button_states,
                             meta_preview_get_mini_icon (),
                             meta_preview_get_icon ());

      cairo_restore (cr);
    }

  /* draw child */
  return GTK_WIDGET_CLASS (meta_preview_parent_class)->draw (widget, cr);
}

static void
meta_preview_realize (GtkWidget *widget)
{
  MetaPreview *preview = META_PREVIEW (widget);

  GTK_WIDGET_CLASS (meta_preview_parent_class)->realize (widget);

  preview->style_context = meta_theme_create_style_context (gtk_widget_get_screen (widget),
                                                            NULL);
}

#define NO_CHILD_WIDTH 80
#define NO_CHILD_HEIGHT 20

static void
meta_preview_get_preferred_width (GtkWidget *widget,
                                  gint      *minimum,
                                  gint      *natural)
{
  MetaPreview *preview;
  GtkWidget *child;

  preview = META_PREVIEW (widget);

  ensure_info (preview);

  *minimum = *natural = preview->borders.total.left + preview->borders.total.right;

  child = gtk_bin_get_child (GTK_BIN (preview));
  if (child && gtk_widget_get_visible (child))
    {
      gint child_min, child_nat;

      gtk_widget_get_preferred_width (child, &child_min, &child_nat);

      *minimum += child_min;
      *natural += child_nat;
    }
  else
    {
      *minimum += NO_CHILD_WIDTH;
      *natural += NO_CHILD_WIDTH;
    }
}

static void
meta_preview_get_preferred_height (GtkWidget *widget,
                                   gint      *minimum,
                                   gint      *natural)
{
  MetaPreview *preview;
  GtkWidget *child;

  preview = META_PREVIEW (widget);

  ensure_info (preview);

  *minimum = *natural = preview->borders.total.top + preview->borders.total.bottom;

  child = gtk_bin_get_child (GTK_BIN (preview));
  if (child && gtk_widget_get_visible (child))
    {
      gint child_min, child_nat;

      gtk_widget_get_preferred_height (child, &child_min, &child_nat);

      *minimum += child_min;
      *natural += child_nat;
    }
  else
    {
      *minimum += NO_CHILD_HEIGHT;
      *natural += NO_CHILD_HEIGHT;
    }
}

static void
meta_preview_size_allocate (GtkWidget         *widget,
                            GtkAllocation     *allocation)
{
  MetaPreview *preview;
  GtkAllocation widget_allocation, child_allocation;
  GtkWidget *child;

  preview = META_PREVIEW (widget);

  ensure_info (preview);

  gtk_widget_set_allocation (widget, allocation);

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child && gtk_widget_get_visible (child))
    {
      gtk_widget_get_allocation (widget, &widget_allocation);
      child_allocation.x = widget_allocation.x + preview->borders.total.left;
      child_allocation.y = widget_allocation.y + preview->borders.total.top;

      child_allocation.width = MAX (1, widget_allocation.width - preview->borders.total.left - preview->borders.total.right);
      child_allocation.height = MAX (1, widget_allocation.height - preview->borders.total.top - preview->borders.total.bottom);

      gtk_widget_size_allocate (child, &child_allocation);
    }
}

static void
clear_cache (MetaPreview *preview)
{
  if (preview->layout)
    {
      g_object_unref (G_OBJECT (preview->layout));
      preview->layout = NULL;
    }

  preview->borders_cached = FALSE;
}

void
meta_preview_set_theme (MetaPreview    *preview,
                        MetaTheme      *theme)
{
  g_return_if_fail (META_IS_PREVIEW (preview));

  preview->theme = theme;
  
  clear_cache (preview);

  gtk_widget_queue_resize (GTK_WIDGET (preview));
}

void
meta_preview_set_title (MetaPreview    *preview,
                        const char     *title)
{
  g_return_if_fail (META_IS_PREVIEW (preview));

  g_free (preview->title);
  preview->title = g_strdup (title);
  
  clear_cache (preview);

  gtk_widget_queue_resize (GTK_WIDGET (preview));
}

void
meta_preview_set_frame_type (MetaPreview    *preview,
                             MetaFrameType   type)
{
  g_return_if_fail (META_IS_PREVIEW (preview));

  preview->type = type;

  clear_cache (preview);

  gtk_widget_queue_resize (GTK_WIDGET (preview));
}

void
meta_preview_set_frame_flags (MetaPreview    *preview,
                              MetaFrameFlags  flags)
{
  g_return_if_fail (META_IS_PREVIEW (preview));

  preview->flags = flags;

  clear_cache (preview);

  gtk_widget_queue_resize (GTK_WIDGET (preview));
}

void
meta_preview_set_button_layout (MetaPreview            *preview,
                                const MetaButtonLayout *button_layout)
{
  g_return_if_fail (META_IS_PREVIEW (preview));
  
  preview->button_layout = *button_layout;  
  
  gtk_widget_queue_draw (GTK_WIDGET (preview));
}

GdkPixbuf*
meta_preview_get_icon (void)
{
  static GdkPixbuf *default_icon = NULL;

  if (default_icon == NULL)
    {
      GtkIconTheme *theme;
      gboolean icon_exists;

      theme = gtk_icon_theme_get_default ();

      icon_exists = gtk_icon_theme_has_icon (theme, META_DEFAULT_ICON_NAME);

      if (icon_exists)
          default_icon = gtk_icon_theme_load_icon (theme,
                                                   META_DEFAULT_ICON_NAME,
                                                   META_ICON_WIDTH,
                                                   0,
                                                   NULL);
      else
          default_icon = gtk_icon_theme_load_icon (theme,
                                                   "gtk-missing-image",
                                                   META_ICON_WIDTH,
                                                   0,
                                                   NULL);

      g_assert (default_icon);
    }
  
  return default_icon;
}

GdkPixbuf*
meta_preview_get_mini_icon (void)
{
  static GdkPixbuf *default_icon = NULL;

  if (default_icon == NULL)
    {
      GtkIconTheme *theme;
      gboolean icon_exists;

      theme = gtk_icon_theme_get_default ();

      icon_exists = gtk_icon_theme_has_icon (theme, META_DEFAULT_ICON_NAME);

      if (icon_exists)
          default_icon = gtk_icon_theme_load_icon (theme,
                                                   META_DEFAULT_ICON_NAME,
                                                   META_MINI_ICON_WIDTH,
                                                   0,
                                                   NULL);
      else
          default_icon = gtk_icon_theme_load_icon (theme,
                                                   "gtk-missing-image",
                                                   META_MINI_ICON_WIDTH,
                                                   0,
                                                   NULL);

      g_assert (default_icon);
    }
  
  return default_icon;
}
