/* Metacity window menu */

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

#include "menu.h"
#include "main.h"
#include <gdk/gdkx.h>

typedef struct _MenuItem MenuItem;
typedef struct _MenuData MenuData;

struct _MenuItem
{
  MetaMessageWindowMenuOps op;
  const char *stock_id;
  const char *label;
};


struct _MenuData
{
  GdkWindow *window;
  MetaMessageWindowMenuOps op;
};

static void activate_cb (GtkWidget *menuitem, gpointer data);

static GtkWidget *menu = NULL;
static MenuItem menuitems[] = {
  { META_MESSAGE_MENU_DELETE, GTK_STOCK_CLOSE, N_("Close") },
  { META_MESSAGE_MENU_MINIMIZE, NULL, N_("Minimize") },
  { META_MESSAGE_MENU_MAXIMIZE, NULL, N_("Maximize") }
};

static void
popup_position_func (GtkMenu   *menu,
                     gint      *x,
                     gint      *y,
                     gboolean  *push_in,
                     gpointer	user_data)
{
  GtkRequisition req;      
  GdkPoint *pos;

  pos = user_data;
  
  gtk_widget_size_request (GTK_WIDGET (menu), &req);

  *x = pos->x;
  *y = pos->y;
  
  /* Ensure onscreen */
  *x = CLAMP (*x, 0, MAX (0, gdk_screen_width () - req.width));
  *y = CLAMP (*y, 0, MAX (0, gdk_screen_height () - req.height));
}

void
meta_window_menu_show (gulong xwindow,
                       int root_x, int root_y,
                       int button,
                       MetaMessageWindowMenuOps ops,
                       MetaMessageWindowMenuOps insensitive,
                       guint32 timestamp)
{
  int i;
  GdkWindow *window;
  GdkPoint *pt;
  
  if (menu)
    gtk_widget_destroy (menu);

  window = gdk_xid_table_lookup (xwindow);
  if (window)
    g_object_ref (G_OBJECT (window));
  else
    window = gdk_window_foreign_new (xwindow);

  /* X error creating the foreign window means NULL here */
  if (window == NULL)
    return;
  
  menu = gtk_menu_new ();

  i = 0;
  while (i < G_N_ELEMENTS (menuitems))
    {
      if (ops & menuitems[i].op)
        {
          GtkWidget *mi;
          MenuData *md;
          
          if (menuitems[i].stock_id)
            {
              GtkWidget *image;
              
              mi = gtk_image_menu_item_new_with_label (menuitems[i].label);
              image = gtk_image_new_from_stock (menuitems[i].stock_id,
                                                GTK_ICON_SIZE_MENU);
              gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi),
                                             image);
              gtk_widget_show (image);
            }
          else
            {
              mi = gtk_menu_item_new_with_label (menuitems[i].label);
            }

          if (insensitive & menuitems[i].op)
            gtk_widget_set_sensitive (mi, FALSE);

          md = g_new (MenuData, 1);

          md->window = window;
          md->op = menuitems[i].op;
          
          gtk_signal_connect (GTK_OBJECT (mi),
                              "activate",
                              GTK_SIGNAL_FUNC (activate_cb),
                              md);

          gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                                 mi);
          
          gtk_widget_show (mi);
        }
      ++i;
    }
  
  gtk_signal_connect (GTK_OBJECT (menu),
                      "destroy",
                      GTK_SIGNAL_FUNC (gtk_widget_destroyed),
                      &menu);

  pt = g_new (GdkPoint, 1);

  g_object_set_data_full (G_OBJECT (menu),
                          "destroy-point",
                          pt,
                          g_free);

  pt->x = root_x;
  pt->y = root_y;
  
  gtk_menu_popup (GTK_MENU (menu),
                  NULL, NULL,
                  popup_position_func, pt,
                  button,
                  timestamp);

  if (!GTK_MENU_SHELL (menu)->have_xgrab)
    meta_ui_warning ("GtkMenu failed to grab the pointer\n");
}

void
meta_window_menu_hide (void)
{
  if (menu)
    gtk_widget_destroy (menu);
}

static void
close_window (GdkWindow  *window)
{  
  XClientMessageEvent ev;
  
  ev.type = ClientMessage;
  ev.window = GDK_WINDOW_XID (window);
  ev.message_type = gdk_atom_intern ("_NET_CLOSE_WINDOW", FALSE);
  ev.format = 32;
  ev.data.l[0] = 0;
  ev.data.l[1] = 0;

  gdk_error_trap_push ();
  XSendEvent (gdk_display,
              gdk_root_window, False,
              SubstructureNotifyMask | SubstructureRedirectMask,
              (XEvent*) &ev);
  gdk_flush ();
  gdk_error_trap_pop ();
}

static void
activate_cb (GtkWidget *menuitem, gpointer data)
{
  MenuData *md;

  md = data;
  
  switch (md->op)
    {
    case META_MESSAGE_MENU_DELETE:
      close_window (md->window);
      break;

    case META_MESSAGE_MENU_MINIMIZE:
      break;

    case META_MESSAGE_MENU_MAXIMIZE:
      gdk_error_trap_push ();
      gdk_window_maximize (md->window);
      gdk_flush ();
      gdk_error_trap_pop ();
      break;

    default:
      meta_ui_warning (G_STRLOC": Unknown window op\n");
      break;
    }

  if (menu)
    gtk_widget_destroy (menu);
  g_object_unref (G_OBJECT (md->window));
  g_free (md);
}
