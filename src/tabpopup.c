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
};

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
  
  popup = g_new (MetaTabPopup, 1);
  popup->window = gtk_window_new (GTK_WINDOW_POPUP);
  popup->current = NULL;
  popup->entries = NULL;

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

  popup->label = gtk_label_new ("");
  
  gtk_table_attach (GTK_TABLE (table),
                    popup->label,
                    0, width,   height - 1, height,
                    0,          0,
                    0,          0);
  
  left = 0;
  right = 1;
  top = 0;
  bottom = 1;
  tmp = popup->entries;
  
  while (tmp && top < height)
    {
      while (tmp && left < width)
        {
          GtkWidget *image;
          TabEntry *te;

          te = tmp->data;
          
          image = gtk_image_new_from_pixbuf (te->icon);

          te->widget = image;

          gtk_table_attach (GTK_TABLE (table),
                            te->widget,
                            left, right,   top, bottom,
                            0,             0,
                            0,             0);
          
          tmp = tmp->next;
          
          ++left;
        }
      
      ++top;
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
  gtk_label_set_text (GTK_LABEL (popup->label), te->title);
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

  meta_bug ("Selected nonexistent entry 0x%lx in tab popup\n", xwindow);
}
