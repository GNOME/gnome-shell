/* Metacity popup window thing showing windows you can tab to */

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

#include "ui.h"
#include "util.h"
#include <math.h>
#include <gtk/gtk.h>

#define OUTSIDE_SELECT_RECT 2
#define INSIDE_SELECT_RECT 2

typedef struct _TabEntry TabEntry;

struct _TabEntry
{
  Window      xwindow;
  char       *title;
  GdkPixbuf  *icon;
  GtkWidget  *widget;
};

struct _MetaTabPopup
{
  GtkWidget *window;
  GtkWidget *label;
  GList *current;
  GList *entries;
  GtkWidget *current_selected_widget;
};

static GtkWidget* selectable_image_new (GdkPixbuf *pixbuf);
static void       select_image         (GtkWidget *widget);
static void       unselect_image       (GtkWidget *widget);

MetaTabPopup*
meta_ui_tab_popup_new (const MetaTabEntry *entries)
{
  MetaTabPopup *popup;
  int i, left, right, top, bottom;
  GList *tab_entries;
  int width;
  int height;
  GtkWidget *table;
  GList *tmp;
  GtkWidget *frame;
  
  popup = g_new (MetaTabPopup, 1);
  popup->window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_window_set_position (GTK_WINDOW (popup->window),
                           GTK_WIN_POS_CENTER_ALWAYS);
  /* enable resizing, to get never-shrink behavior */
  gtk_window_set_resizable (GTK_WINDOW (popup->window),
                            TRUE);
  popup->current = NULL;
  popup->entries = NULL;
  popup->current_selected_widget = NULL;
  
  tab_entries = NULL;
  i = 0;
  while (entries[i].xwindow != None)
    {
      TabEntry *te;

      te = g_new (TabEntry, 1);
      te->xwindow = entries[i].xwindow;
      te->title = g_strdup (entries[i].title);
      te->icon = entries[i].icon;
      g_object_ref (G_OBJECT (te->icon));
      te->widget = NULL;

      tab_entries = g_list_prepend (tab_entries, te);
      
      ++i;
    }

  popup->entries = g_list_reverse (tab_entries);

  width = 5; /* FIXME */
  height = i / width;
  if (i % width)
    height += 1;

  table = gtk_table_new (height + 1, width, FALSE);
  
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_container_set_border_width (GTK_CONTAINER (table), 1);
  gtk_container_add (GTK_CONTAINER (popup->window),
                     frame);
  gtk_container_add (GTK_CONTAINER (frame),
                     table);

  
  popup->label = gtk_label_new ("");
  
  gtk_table_attach (GTK_TABLE (table),
                    popup->label,
                    0, width,              height, height + 1,
                    GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
                    3,                     3);
  

  top = 0;
  bottom = 1;
  tmp = popup->entries;
  
  while (tmp && top < height)
    {      
      left = 0;
      right = 1;

      while (tmp && left < width)
        {
          GtkWidget *image;
          
          TabEntry *te;

          te = tmp->data;
          
          image = selectable_image_new (te->icon);
          gtk_misc_set_padding (GTK_MISC (image),
                                INSIDE_SELECT_RECT + OUTSIDE_SELECT_RECT + 1,
                                INSIDE_SELECT_RECT + OUTSIDE_SELECT_RECT + 1);
          gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.5);
          
          te->widget = image;

          gtk_table_attach (GTK_TABLE (table),
                            te->widget,
                            left, right,           top, bottom,
                            GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL,
                            0,                     0);
          
          tmp = tmp->next;
          
          ++left;
          ++right;
        }
      
      ++top;
      ++bottom;
    }
      
  return popup;
}

static void
free_entry (gpointer data, gpointer user_data)
{
  TabEntry *te;

  te = data;
  
  g_free (te->title);
  g_object_unref (G_OBJECT (te->icon));

  g_free (te);
}

void
meta_ui_tab_popup_free (MetaTabPopup *popup)
{

  gtk_widget_destroy (popup->window);
  
  g_list_foreach (popup->entries, free_entry, NULL);

  g_list_free (popup->entries);
  
  g_free (popup);
}

void
meta_ui_tab_popup_set_showing (MetaTabPopup *popup,
                               gboolean      showing)
{
  if (showing)
    gtk_widget_show_all (popup->window);
  else
    gtk_widget_hide (popup->window);
}

static void
display_entry (MetaTabPopup *popup,
               TabEntry     *te)
{
  if (popup->current_selected_widget)
    unselect_image (popup->current_selected_widget);
  
  gtk_label_set_text (GTK_LABEL (popup->label), te->title);
  select_image (te->widget);

  popup->current_selected_widget = te->widget;
}

void
meta_ui_tab_popup_forward (MetaTabPopup *popup)
{
  if (popup->current != NULL)
    popup->current = popup->current->next;

  if (popup->current == NULL)
    popup->current = popup->entries;
  
  if (popup->current != NULL)
    {
      TabEntry *te;

      te = popup->current->data;

      display_entry (popup, te);
    }
}

void
meta_ui_tab_popup_backward (MetaTabPopup *popup)
{
  if (popup->current != NULL)
    popup->current = popup->current->prev;

  if (popup->current == NULL)
    popup->current = g_list_last (popup->entries);
  
  if (popup->current != NULL)
    {
      TabEntry *te;

      te = popup->current->data;

      display_entry (popup, te);
    }
}

Window
meta_ui_tab_popup_get_selected (MetaTabPopup *popup)
{
  if (popup->current)
    {
      TabEntry *te;

      te = popup->current->data;

      return te->xwindow;
    }
  else
    return None;
}

void
meta_ui_tab_popup_select (MetaTabPopup *popup,
                          Window        xwindow)
{
  GList *tmp;

  tmp = popup->entries;
  while (tmp != NULL)
    {
      TabEntry *te;

      te = tmp->data;

      if (te->xwindow == xwindow)
        {
          popup->current = tmp;
          
          display_entry (popup, te);

          return;
        }
      
      tmp = tmp->next;
    }
}

#define META_TYPE_SELECT_IMAGE            (meta_select_image_get_type ())
#define META_SELECT_IMAGE(obj)            (GTK_CHECK_CAST ((obj), META_TYPE_SELECT_IMAGE, MetaSelectImage))

typedef struct _MetaSelectImage       MetaSelectImage;
typedef struct _MetaSelectImageClass  MetaSelectImageClass;

struct _MetaSelectImage
{
  GtkImage parent_instance;
  guint selected : 1;
};

struct _MetaSelectImageClass
{
  GtkImageClass parent_class;
};


static GType meta_select_image_get_type (void) G_GNUC_CONST;

static GtkWidget*
selectable_image_new (GdkPixbuf *pixbuf)
{
  GtkWidget *w;

  w = g_object_new (meta_select_image_get_type (), NULL);
  gtk_image_set_from_pixbuf (GTK_IMAGE (w), pixbuf); 

  return w;
}

static void
select_image (GtkWidget *widget)
{
  META_SELECT_IMAGE (widget)->selected = TRUE;
  gtk_widget_queue_draw (widget);
}

static void
unselect_image (GtkWidget *widget)
{
  META_SELECT_IMAGE (widget)->selected = FALSE;
  gtk_widget_queue_draw (widget);
}

static void     meta_select_image_class_init   (MetaSelectImageClass *klass);
static gboolean meta_select_image_expose_event (GtkWidget            *widget,
                                                GdkEventExpose       *event);

static GtkImageClass *parent_class;

GType
meta_select_image_get_type (void)
{
  static GtkType image_type = 0;

  if (!image_type)
    {
      static const GTypeInfo image_info =
      {
	sizeof (MetaSelectImageClass),
	NULL,           /* base_init */
	NULL,           /* base_finalize */
	(GClassInitFunc) meta_select_image_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data */
	sizeof (MetaSelectImage),
	16,             /* n_preallocs */
	(GInstanceInitFunc) NULL,
      };

      image_type = g_type_register_static (GTK_TYPE_IMAGE, "MetaSelectImage", &image_info, 0);
    }

  return image_type;
}

static void
meta_select_image_class_init (MetaSelectImageClass *klass)
{
  GtkWidgetClass *widget_class;
  
  parent_class = gtk_type_class (gtk_image_get_type ());

  widget_class = GTK_WIDGET_CLASS (klass);
  
  widget_class->expose_event = meta_select_image_expose_event;
}

static gboolean
meta_select_image_expose_event (GtkWidget      *widget,
                                GdkEventExpose *event)
{
  if (META_SELECT_IMAGE (widget)->selected)
    {
      int x, y, w, h;
      GtkMisc *misc;

      misc = GTK_MISC (widget);
      
      x = (widget->allocation.x * (1.0 - misc->xalign) +
	   (widget->allocation.x + widget->allocation.width
	    - (widget->requisition.width - misc->xpad * 2)) *
	   misc->xalign) + 0.5;
      y = (widget->allocation.y * (1.0 - misc->yalign) +
	   (widget->allocation.y + widget->allocation.height
	    - (widget->requisition.height - misc->ypad * 2)) *
	   misc->yalign) + 0.5;

      x -= INSIDE_SELECT_RECT;
      y -= INSIDE_SELECT_RECT;
      
      
      w = widget->requisition.width - OUTSIDE_SELECT_RECT * 2 - 1;
      h = widget->requisition.height - OUTSIDE_SELECT_RECT * 2 - 1;
      
      gdk_draw_rectangle (widget->window,
                          widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
                          FALSE,
                          x, y, w, h);
      gdk_draw_rectangle (widget->window,
                          widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
                          FALSE,
                          x - 1, y - 1, w + 2, h + 2);
      
#if 0
      gdk_draw_rectangle (widget->window,
                          widget->style->bg_gc[GTK_STATE_SELECTED],
                          TRUE,
                          x, y, w, h);
#endif
#if 0      
      gtk_paint_focus (widget->style, widget->window,
                       &event->area, widget, "meta-tab-image",
                       x, y, w, h);
#endif
    }

  return GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);
}





