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
#include <X11/Xatom.h>

#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

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
  { META_MESSAGE_MENU_DELETE, GTK_STOCK_CLOSE, N_("_Close") },
  { META_MESSAGE_MENU_MINIMIZE, NULL, N_("_Minimize") },
  { META_MESSAGE_MENU_MAXIMIZE, NULL, N_("Ma_ximize") },
  { META_MESSAGE_MENU_UNMAXIMIZE, NULL, N_("_Unmaximize") },
  { META_MESSAGE_MENU_SHADE, NULL, N_("_Shade") },
  { META_MESSAGE_MENU_UNSHADE, NULL, N_("U_nshade") },
  { 0, NULL, NULL }, /* separator */
  { META_MESSAGE_MENU_STICK, NULL, N_("Put on _All Workspaces") },
  { META_MESSAGE_MENU_UNSTICK, NULL, N_("Only on _This Workspace") }
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

static gint
get_num_desktops (void)
{  
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  gulong *num;
  int result;
  
  XGetWindowProperty (gdk_display, gdk_root_window,
                      gdk_atom_intern ("_NET_NUMBER_OF_DESKTOPS", FALSE),
                      0, G_MAXLONG,
		      False, XA_CARDINAL, &type, &format, &nitems,
		      &bytes_after, (guchar **)&num);  

  if (type != XA_CARDINAL)
    return 0; 

  result = *num;
  
  XFree (num);

  return result;
}

static gint
get_active_desktop (void)
{  
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  gulong *num;
  int result;
  
  XGetWindowProperty (gdk_display, gdk_root_window,
                      gdk_atom_intern ("_NET_CURRENT_DESKTOP", FALSE),
                      0, G_MAXLONG,
		      False, XA_CARDINAL, &type, &format, &nitems,
		      &bytes_after, (guchar **)&num);  

  if (type != XA_CARDINAL)
    return 0; 

  result = *num;
  
  XFree (num);

  return result;
}

static gulong
get_current_desktop (GdkWindow *window)
{  
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  gulong *num;
  gulong result;
  int err;
  
  gdk_error_trap_push ();
  type = None;
  XGetWindowProperty (gdk_display, GDK_WINDOW_XID (window),
                      gdk_atom_intern ("_NET_WM_DESKTOP", FALSE),
                      0, G_MAXLONG,
		      False, XA_CARDINAL, &type, &format, &nitems,
		      &bytes_after, (guchar **)&num);  
  err = gdk_error_trap_pop ();
  if (err != Success)
    meta_ui_warning ("Error %d getting _NET_WM_DESKTOP\n", err);
  
  if (type != XA_CARDINAL)
    {
      meta_ui_warning ("_NET_WM_DESKTOP has wrong type %s\n", gdk_atom_name (type));
      return 0xFFFFFFFF; /* sticky */
    }

  result = *num;
  
  XFree (num);

  return result;
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
  int n_workspaces;
  int current_workspace;
  
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
      if (ops & menuitems[i].op || menuitems[i].op == 0)
        {
          GtkWidget *mi;
          MenuData *md;

          if (menuitems[i].op == 0)
            {
              mi = gtk_separator_menu_item_new ();
            }
          else
            {
              if (menuitems[i].stock_id)
                {
                  GtkWidget *image;
                  
                  mi = gtk_image_menu_item_new_with_mnemonic (menuitems[i].label);
                  image = gtk_image_new_from_stock (menuitems[i].stock_id,
                                                    GTK_ICON_SIZE_MENU);
                  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi),
                                                 image);
                  gtk_widget_show (image);
                }
              else
                {
                  mi = gtk_menu_item_new_with_mnemonic (menuitems[i].label);
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
            }
          
          gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                                 mi);
          
          gtk_widget_show (mi);
        }
      ++i;
    }

  if (ops & META_MESSAGE_MENU_WORKSPACES)
    {
      n_workspaces = get_num_desktops ();
      current_workspace = get_current_desktop (window);

      meta_ui_warning ("Creating %d workspace menu current %d\n",
                       n_workspaces, current_workspace);
      
      if (n_workspaces > 0)
        {
          GtkWidget *mi;
          
          i = 0;
          while (i < n_workspaces)
            {
              char *label;
              MenuData *md;

              if (current_workspace == 0xFFFFFFFF)
                label = g_strdup_printf (_("Only on workspace _%d\n"),
                                         i + 1);
              else
                label = g_strdup_printf (_("Move to workspace _%d\n"),
                                         i + 1);
          
              mi = gtk_menu_item_new_with_mnemonic (label);

              g_free (label);

              if (current_workspace == i ||
                  insensitive & META_MESSAGE_MENU_WORKSPACES)
                gtk_widget_set_sensitive (mi, FALSE);

              md = g_new (MenuData, 1);

              md->window = window;
              md->op = META_MESSAGE_MENU_WORKSPACES;

              g_object_set_data (G_OBJECT (mi),
                                 "workspace",
                                 GINT_TO_POINTER (i));
          
              gtk_signal_connect (GTK_OBJECT (mi),
                                  "activate",
                                  GTK_SIGNAL_FUNC (activate_cb),
                                  md);

              gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                                     mi);
          
              gtk_widget_show (mi);
          
              ++i;
            }
        }
    }
  else
    meta_ui_warning ("not creating workspace menu\n");
  
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
wmspec_change_state (gboolean   add,
                     GdkWindow *window,
                     GdkAtom    state1,
                     GdkAtom    state2)
{
  XEvent xev;
  gulong op;

  if (add)
    op = _NET_WM_STATE_ADD;
  else
    op = _NET_WM_STATE_REMOVE;
  
  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = gdk_display;
  xev.xclient.window = GDK_WINDOW_XID (window);
  xev.xclient.message_type = gdk_atom_intern ("_NET_WM_STATE", FALSE);
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = op;
  xev.xclient.data.l[1] = state1;
  xev.xclient.data.l[2] = state2;
  
  XSendEvent (gdk_display, gdk_root_window, False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
}

static void
wmspec_change_desktop (GdkWindow *window,
                       gint       desktop)
{
  XEvent xev;
  
  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = gdk_display;
  xev.xclient.window = GDK_WINDOW_XID (window);
  xev.xclient.message_type = gdk_atom_intern ("_NET_WM_DESKTOP", FALSE);
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = desktop;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;
  
  XSendEvent (gdk_display, gdk_root_window, False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);
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
      gdk_window_iconify (md->window);
      break;

    case META_MESSAGE_MENU_UNMAXIMIZE:
      wmspec_change_state (FALSE, md->window,
                           gdk_atom_intern ("_NET_WM_STATE_MAXIMIZED_HORZ", FALSE),
                           gdk_atom_intern ("_NET_WM_STATE_MAXIMIZED_VERT", FALSE));
      break;
      
    case META_MESSAGE_MENU_MAXIMIZE:
      wmspec_change_state (TRUE, md->window,
                           gdk_atom_intern ("_NET_WM_STATE_MAXIMIZED_HORZ", FALSE),
                           gdk_atom_intern ("_NET_WM_STATE_MAXIMIZED_VERT", FALSE));
      break;

    case META_MESSAGE_MENU_UNSHADE:
      wmspec_change_state (FALSE, md->window,
                           gdk_atom_intern ("_NET_WM_STATE_SHADED", FALSE),
                           0);
      break;
      
    case META_MESSAGE_MENU_SHADE:
      wmspec_change_state (TRUE, md->window,
                           gdk_atom_intern ("_NET_WM_STATE_SHADED", FALSE),
                           0);
      break;
      
    case META_MESSAGE_MENU_WORKSPACES:
      {
        int workspace;

        workspace = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menuitem),
                                                        "workspace"));

        wmspec_change_desktop (md->window, workspace);
      }
      break;

    case META_MESSAGE_MENU_STICK:
      wmspec_change_desktop (md->window, 0xFFFFFFFF);
      break;

    case META_MESSAGE_MENU_UNSTICK:
      wmspec_change_desktop (md->window, get_active_desktop ());
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
