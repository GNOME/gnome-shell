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

#include <config.h>

#include "util.h"
#include "core.h"
#include "tabpopup.h"
#include <gtk/gtk.h>
#include <math.h>

#define OUTSIDE_SELECT_RECT 2
#define INSIDE_SELECT_RECT 2

typedef struct _TabEntry TabEntry;

struct _TabEntry
{
  MetaTabEntryKey  key;
  char            *title;
  GdkPixbuf       *icon;
  GtkWidget       *widget;
  GdkRectangle     rect;
  GdkRectangle     inner_rect;
};

struct _MetaTabPopup
{
  GtkWidget *window;
  GtkWidget *label;
  GList *current;
  GList *entries;
  TabEntry *current_selected_entry;
  GtkWidget *outline_window;
  gboolean outline;
};

static GtkWidget* selectable_image_new (GdkPixbuf *pixbuf);
static void       select_image         (GtkWidget *widget);
static void       unselect_image       (GtkWidget *widget);

static gboolean
outline_window_expose (GtkWidget      *widget,
                       GdkEventExpose *event,
                       gpointer        data)
{
  MetaTabPopup *popup;
  int w, h;
  TabEntry *te;  
  
  popup = data;

  if (!popup->outline || popup->current_selected_entry == NULL)
    return FALSE;

  te = popup->current_selected_entry;
  
  gdk_window_get_size (widget->window, &w, &h);

  gdk_draw_rectangle (widget->window,
                      widget->style->white_gc,
                      FALSE,
                      0, 0,
                      te->rect.width - 1,
                      te->rect.height - 1);

  gdk_draw_rectangle (widget->window,
                      widget->style->white_gc,
                      FALSE,
                      te->inner_rect.x - 1, te->inner_rect.y - 1,
                      te->inner_rect.width + 1,
                      te->inner_rect.height + 1);

  return FALSE;
}

MetaTabPopup*
meta_ui_tab_popup_new (const MetaTabEntry *entries,
                       int                 screen_number,
                       int                 entry_count,
                       int                 width,
                       gboolean            outline)
{
  MetaTabPopup *popup;
  int i, left, right, top, bottom;
  GList *tab_entries;
  int height;
  GtkWidget *table;
  GtkWidget *vbox;
  GtkWidget *align;
  GList *tmp;
  GtkWidget *frame;
  int max_label_width;

  popup = g_new (MetaTabPopup, 1);

  popup->outline_window = gtk_window_new (GTK_WINDOW_POPUP);
#ifdef HAVE_GTK_MULTIHEAD
  gtk_window_set_screen (GTK_WINDOW (popup->outline_window),
			 gdk_display_get_screen (gdk_get_default_display (),
						 screen_number));
#endif

  gtk_widget_set_app_paintable (popup->outline_window, TRUE);
  gtk_widget_realize (popup->outline_window);

  g_signal_connect (G_OBJECT (popup->outline_window), "expose_event",
                    G_CALLBACK (outline_window_expose), popup);
  
  popup->window = gtk_window_new (GTK_WINDOW_POPUP);
#ifdef HAVE_GTK_MULTIHEAD
  gtk_window_set_screen (GTK_WINDOW (popup->window),
			 gdk_display_get_screen (gdk_get_default_display (),
						 screen_number));
#endif

  gtk_window_set_position (GTK_WINDOW (popup->window),
                           GTK_WIN_POS_CENTER_ALWAYS);
  /* enable resizing, to get never-shrink behavior */
  gtk_window_set_resizable (GTK_WINDOW (popup->window),
                            TRUE);
  popup->current = NULL;
  popup->entries = NULL;
  popup->current_selected_entry = NULL;
  popup->outline = outline;
  
  tab_entries = NULL;
  for (i = 0; i < entry_count; ++i)
    {
      TabEntry *te;

      te = g_new (TabEntry, 1);
      te->key = entries[i].key;
      te->title = g_strdup (entries[i].title);
      te->icon = entries[i].icon;
      g_object_ref (G_OBJECT (te->icon));
      te->widget = NULL;

      if (outline)
        {
          te->rect.x = entries[i].x;
          te->rect.y = entries[i].y;
          te->rect.width = entries[i].width;
          te->rect.height = entries[i].height;

          te->inner_rect.x = entries[i].inner_x;
          te->inner_rect.y = entries[i].inner_y;
          te->inner_rect.width = entries[i].inner_width;
          te->inner_rect.height = entries[i].inner_height;
        }

      tab_entries = g_list_prepend (tab_entries, te);
    }

  popup->entries = g_list_reverse (tab_entries);

  g_assert (width > 0);
  height = i / width;
  if (i % width)
    height += 1;

  table = gtk_table_new (height, width, FALSE);
  vbox = gtk_vbox_new (FALSE, 0);
  
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_container_set_border_width (GTK_CONTAINER (table), 1);
  gtk_container_add (GTK_CONTAINER (popup->window),
                     frame);
  gtk_container_add (GTK_CONTAINER (frame),
                     vbox);

  align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  
  gtk_box_pack_start (GTK_BOX (vbox), align, TRUE, TRUE, 0);

  gtk_container_add (GTK_CONTAINER (align),
                     table);
  
  popup->label = gtk_label_new ("");
  gtk_misc_set_padding (GTK_MISC (popup->label), 3, 3);

  gtk_box_pack_end (GTK_BOX (vbox), popup->label, FALSE, FALSE, 0);

  max_label_width = 0;
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
          GtkRequisition req;
          
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
                            0,                     0,
                            0,                     0);

          /* Efficiency rules! */
          gtk_label_set_text (GTK_LABEL (popup->label),
                              te->title);
          gtk_widget_size_request (popup->label, &req);
          max_label_width = MAX (max_label_width, req.width);
          
          tmp = tmp->next;
          
          ++left;
          ++right;
        }
      
      ++top;
      ++bottom;
    }

  /* remove all the temporary text */
  gtk_label_set_text (GTK_LABEL (popup->label), "");

  max_label_width += 20; /* add random padding */
  
  gtk_window_set_default_size (GTK_WINDOW (popup->window),
                               max_label_width,
                               -1);
  
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
  meta_verbose ("Destroying tab popup window\n");
  
  gtk_widget_destroy (popup->outline_window);
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
    {
      gtk_widget_show_all (popup->window);
    }
  else
    {
      meta_verbose ("Hiding tab popup window\n");
      gtk_widget_hide (popup->window);
      meta_core_increment_event_serial (gdk_display);
    }
}

static void
display_entry (MetaTabPopup *popup,
               TabEntry     *te)
{
  GdkRectangle rect;
  GdkRegion *region;
  GdkRegion *inner_region;

  
  if (popup->current_selected_entry)
    unselect_image (popup->current_selected_entry->widget);
  
  gtk_label_set_text (GTK_LABEL (popup->label), te->title);
  select_image (te->widget);
  
  if (popup->outline)
    {
      /* Do stuff behind gtk's back */
      gdk_window_hide (popup->outline_window->window);
      meta_core_increment_event_serial (gdk_display);
  
      rect = te->rect;
      rect.x = 0;
      rect.y = 0;

      gdk_window_move_resize (popup->outline_window->window,
                              te->rect.x, te->rect.y,
                              te->rect.width, te->rect.height);
  
      gdk_window_set_background (popup->outline_window->window,
                                 &popup->outline_window->style->black);
  
      region = gdk_region_rectangle (&rect);
      inner_region = gdk_region_rectangle (&te->inner_rect);
      gdk_region_subtract (region, inner_region);
      gdk_region_destroy (inner_region);
  
      gdk_window_shape_combine_region (popup->outline_window->window,
                                       region,
                                       0, 0);

      gdk_region_destroy (region);
  
      /* This should piss off gtk a bit, but we don't want to raise
       * above the tab popup
       */
      gdk_window_show_unraised (popup->outline_window->window);
    }

  /* Must be before we handle an expose for the outline window */
  popup->current_selected_entry = te;
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

MetaTabEntryKey
meta_ui_tab_popup_get_selected (MetaTabPopup *popup)
{
  if (popup->current)
    {
      TabEntry *te;

      te = popup->current->data;

      return te->key;
    }
  else
    return None;
}

void
meta_ui_tab_popup_select (MetaTabPopup *popup,
                          MetaTabEntryKey key)
{
  GList *tmp;

  tmp = popup->entries;
  while (tmp != NULL)
    {
      TabEntry *te;

      te = tmp->data;

      if (te->key == key)
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

      x -= INSIDE_SELECT_RECT + 1;
      y -= INSIDE_SELECT_RECT + 1;      
      
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
