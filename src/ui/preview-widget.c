/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity theme preview widget */

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

#include "preview-widget.h"

static void     meta_preview_class_init    (MetaPreviewClass *klass);
static void     meta_preview_init          (MetaPreview      *preview);
static void     meta_preview_size_request  (GtkWidget        *widget,
                                            GtkRequisition   *req);
static void     meta_preview_size_allocate (GtkWidget        *widget,
                                            GtkAllocation    *allocation);
static gboolean meta_preview_expose        (GtkWidget        *widget,
                                            GdkEventExpose   *event);
static void     meta_preview_finalize      (GObject          *object);

static GtkWidgetClass *parent_class;

GtkType
meta_preview_get_type (void)
{
  static GtkType preview_type = 0;

  if (!preview_type)
    {
      static const GtkTypeInfo preview_info =
      {
	"MetaPreview",
	sizeof (MetaPreview),
	sizeof (MetaPreviewClass),
	(GtkClassInitFunc) meta_preview_class_init,
	(GtkObjectInitFunc) meta_preview_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      preview_type = gtk_type_unique (GTK_TYPE_BIN, &preview_info);
    }

  return preview_type;
}

static void
meta_preview_class_init (MetaPreviewClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) class;
  parent_class = gtk_type_class (GTK_TYPE_BIN);

  gobject_class->finalize = meta_preview_finalize;

  widget_class->expose_event = meta_preview_expose;
  widget_class->size_request = meta_preview_size_request;
  widget_class->size_allocate = meta_preview_size_allocate;
}

static void
meta_preview_init (MetaPreview *preview)
{
  int i;
  
  GTK_WIDGET_SET_FLAGS (preview, GTK_NO_WINDOW);

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
  
  preview->left_width = -1;
  preview->right_width = -1;
  preview->top_height = -1;
  preview->bottom_height = -1;
}

GtkWidget*
meta_preview_new (void)
{
  MetaPreview *preview;
  
  preview = gtk_type_new (META_TYPE_PREVIEW);
  
  return GTK_WIDGET (preview);
}

static void
meta_preview_finalize (GObject *object)
{
  MetaPreview *preview;

  preview = META_PREVIEW (object);

  g_free (preview->title);
  preview->title = NULL;
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
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

  if (preview->top_height < 0)
    {
      if (preview->theme)
        {
          meta_theme_get_frame_borders (preview->theme,
                                        preview->type,
                                        preview->text_height,
                                        preview->flags,
                                        &preview->top_height,
                                        &preview->bottom_height,
                                        &preview->left_width,
                                        &preview->right_width);
        }
      else
        {
          preview->top_height = 0;
          preview->bottom_height = 0;
          preview->left_width = 0;
          preview->right_width = 0;
        }
    }
}

static gboolean
meta_preview_expose (GtkWidget      *widget,
                     GdkEventExpose *event)
{
  MetaPreview *preview;
  int border_width;
  int client_width;
  int client_height;
  MetaButtonState button_states[META_BUTTON_TYPE_LAST] =
  {
    META_BUTTON_STATE_NORMAL,
    META_BUTTON_STATE_NORMAL,
    META_BUTTON_STATE_NORMAL,
    META_BUTTON_STATE_NORMAL
  };
  
  g_return_val_if_fail (META_IS_PREVIEW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  preview = META_PREVIEW (widget);

  ensure_info (preview);

  border_width = GTK_CONTAINER (widget)->border_width;
  
  client_width = widget->allocation.width - preview->left_width - preview->right_width - border_width * 2;
  client_height = widget->allocation.height - preview->top_height - preview->bottom_height - border_width * 2;

  if (client_width < 0)
    client_width = 1;
  if (client_height < 0)
    client_height = 1;  
  
  if (preview->theme)
    {
      border_width = GTK_CONTAINER (widget)->border_width;
      
      meta_theme_draw_frame (preview->theme,
                             widget,
                             widget->window,
                             &event->area,
                             widget->allocation.x + border_width,
                             widget->allocation.y + border_width,
                             preview->type,
                             preview->flags,
                             client_width, client_height,
                             preview->layout,
                             preview->text_height,
                             &preview->button_layout,
                             button_states,
                             meta_preview_get_mini_icon (),
                             meta_preview_get_icon ());
    }

  /* draw child */
  return GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);
}

static void
meta_preview_size_request (GtkWidget      *widget,
                           GtkRequisition *req)
{
  MetaPreview *preview;

  preview = META_PREVIEW (widget);

  ensure_info (preview);

  req->width = preview->left_width + preview->right_width;
  req->height = preview->top_height + preview->bottom_height;
  
  if (GTK_BIN (preview)->child &&
      GTK_WIDGET_VISIBLE (GTK_BIN (preview)->child))
    {
      GtkRequisition child_requisition;

      gtk_widget_size_request (GTK_BIN (preview)->child, &child_requisition);

      req->width += child_requisition.width;
      req->height += child_requisition.height;
    }
  else
    {
#define NO_CHILD_WIDTH 80
#define NO_CHILD_HEIGHT 20
      req->width += NO_CHILD_WIDTH;
      req->height += NO_CHILD_HEIGHT;
    }

  req->width += GTK_CONTAINER (widget)->border_width * 2;
  req->height += GTK_CONTAINER (widget)->border_width * 2;
}

static void
meta_preview_size_allocate (GtkWidget         *widget,
                            GtkAllocation     *allocation)
{
  MetaPreview *preview;
  int border_width;
  GtkAllocation child_allocation;
  
  preview = META_PREVIEW (widget);

  ensure_info (preview);
  
  widget->allocation = *allocation;

  border_width = GTK_CONTAINER (widget)->border_width;
  
  if (GTK_BIN (widget)->child &&
      GTK_WIDGET_VISIBLE (GTK_BIN (widget)->child))
    {
      child_allocation.x = widget->allocation.x + border_width + preview->left_width;
      child_allocation.y = widget->allocation.y + border_width + preview->top_height;
      
      child_allocation.width = MAX (1, widget->allocation.width - border_width * 2 - preview->left_width - preview->right_width);
      child_allocation.height = MAX (1, widget->allocation.height - border_width * 2 - preview->top_height - preview->bottom_height);

      gtk_widget_size_allocate (GTK_BIN (widget)->child, &child_allocation);
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

  preview->left_width = -1;
  preview->right_width = -1;
  preview->top_height = -1;
  preview->bottom_height = -1;
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

#include "inlinepixbufs.h"

GdkPixbuf*
meta_preview_get_icon (void)
{
  static GdkPixbuf *default_icon = NULL;

  if (default_icon == NULL)
    {
      GdkPixbuf *base;

      base = gdk_pixbuf_new_from_inline (-1, default_icon_data,
                                         FALSE,
                                         NULL);

      g_assert (base);

      default_icon = gdk_pixbuf_scale_simple (base,
                                              META_ICON_WIDTH,
                                              META_ICON_HEIGHT,
                                              GDK_INTERP_BILINEAR);

      g_object_unref (G_OBJECT (base));
    }
  
  return default_icon;
}

GdkPixbuf*
meta_preview_get_mini_icon (void)
{
  static GdkPixbuf *default_icon = NULL;

  if (default_icon == NULL)
    {
      GdkPixbuf *base;

      base = gdk_pixbuf_new_from_inline (-1, default_icon_data,
                                         FALSE,
                                         NULL);

      g_assert (base);

      default_icon = gdk_pixbuf_scale_simple (base,
                                              META_MINI_ICON_WIDTH,
                                              META_MINI_ICON_HEIGHT,
                                              GDK_INTERP_BILINEAR);

      g_object_unref (G_OBJECT (base));
    }
  
  return default_icon;
}
