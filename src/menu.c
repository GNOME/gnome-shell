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

#include <config.h>
#include <stdio.h>
#include <string.h>
#include "menu.h"
#include "main.h"
#include "util.h"
#include "core.h"
#include "themewidget.h"
#include "metaaccellabel.h"

typedef struct _MenuItem MenuItem;
typedef struct _MenuData MenuData;

struct _MenuItem
{
  MetaMenuOp op;
  const char *stock_id;
  const char *label;
};


struct _MenuData
{
  MetaWindowMenu *menu;
  MetaMenuOp op;
};

static void activate_cb (GtkWidget *menuitem, gpointer data);

static MenuItem menuitems[] = {
  { META_MENU_OP_MINIMIZE, METACITY_STOCK_MINIMIZE, N_("Mi_nimize") },
  { META_MENU_OP_MAXIMIZE, METACITY_STOCK_MAXIMIZE, N_("Ma_ximize") },
  { META_MENU_OP_UNMAXIMIZE, NULL, N_("Unma_ximize") },
  { META_MENU_OP_SHADE, NULL, N_("Roll _Up") },
  { META_MENU_OP_UNSHADE, NULL, N_("_Unroll") },
  { META_MENU_OP_MOVE, NULL, N_("_Move") },
  { META_MENU_OP_RESIZE, NULL, N_("_Resize") },
  { 0, NULL, NULL }, /* separator */
  { META_MENU_OP_DELETE, METACITY_STOCK_DELETE, N_("_Close") },
  { 0, NULL, NULL }, /* separator */
  { META_MENU_OP_STICK, NULL, N_("Put on _All Workspaces") },
  { META_MENU_OP_UNSTICK, NULL, N_("Only on _This Workspace") }
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

static void
menu_closed (GtkMenu *widget,
             gpointer data)
{
  MetaWindowMenu *menu;
  
  menu = data;

  meta_frames_notify_menu_hide (menu->frames);
  (* menu->func) (menu, gdk_display,
                  menu->client_xwindow,
                  0, 0,
                  menu->data);
  
  /* menu may now be freed */
}

static void
activate_cb (GtkWidget *menuitem, gpointer data)
{
  MenuData *md;
  
  g_return_if_fail (GTK_IS_WIDGET (menuitem));
  
  md = data;

  meta_frames_notify_menu_hide (md->menu->frames);
  (* md->menu->func) (md->menu, gdk_display,
                      md->menu->client_xwindow,
                      md->op,
                      GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menuitem),
                                                          "workspace")),
                      md->menu->data);

  /* menu may now be freed */
}

/*
 * Given a Display and an index, get the workspace name and add any
 * accelerators. At the moment this means adding a _ if the name is of
 * the form "Workspace n" where n is less than 10, and escaping any
 * other '_'s so they do not create inadvertant accelerators.
 * 
 * The calling code owns the string, and is reponsible to free the
 * memory after use.
 */
static char*
get_workspace_name_with_accel (Display *display,
                               Window   xroot,
                               int      index)
{
  const char *name;
  int number;

  name = meta_core_get_workspace_name_with_index (display, xroot, index);

  g_assert (name != NULL);
  
  /*
   * If the name is of the form "Workspace x" where x is an unsigned
   * integer, insert a '_' before the number if it is less than 10 and
   * return it
   */
  number = 0;
  if (sscanf (name, _("Workspace %d"), &number) == 1)
    {
      char *new_name;
      
      /*
       * Above name is a pointer into the Workspace struct. Here we make
       * a copy copy so we can have our wicked way with it.
       */
      new_name = g_strdup_printf (_("Workspace %s%d"),
                                  number < 10 ? "_" : "",
                                  number);
      return new_name;
    }
  else
    {
      /*
       * Otherwise this is just a normal name to which we cannot really
       * add accelerators. Escape any _ characters so that the user's
       * workspace names do not get mangled.
       */
      char *new_name;
      const char *source;
      char *dest;

      /*
       * Assume the worst case, that every character is a _
       */
      new_name = g_malloc0 (strlen (name) * 2 + 1);

      /*
       * Now iterate down the strings, adding '_' to escape as we go
       */
      dest = new_name;
      source = name;
      while (*source != '\0')
        {
          if (*source == '_')
            *dest++ = '_';
          *dest++ = *source++;
        }

      return new_name;
    }
}

static GtkWidget*
menu_item_new (const char         *label,
               gboolean            with_image,
               unsigned int        key,
               MetaVirtualModifier mods)
{
  GtkWidget *menu_item;
  GtkWidget *accel_label;

  if (with_image)
    menu_item = gtk_image_menu_item_new ();
  else
    menu_item = gtk_menu_item_new ();
  accel_label = meta_accel_label_new_with_mnemonic (label);
  gtk_misc_set_alignment (GTK_MISC (accel_label), 0.0, 0.5);

  gtk_container_add (GTK_CONTAINER (menu_item), accel_label);
  gtk_widget_show (accel_label);

  meta_accel_label_set_accelerator (META_ACCEL_LABEL (accel_label),
                                    key, mods);
  
  return menu_item;
}

MetaWindowMenu*
meta_window_menu_new   (MetaFrames         *frames,
                        MetaMenuOp          ops,
                        MetaMenuOp          insensitive,
                        Window              client_xwindow,
                        int                 active_workspace,
                        int                 n_workspaces,
                        MetaWindowMenuFunc  func,
                        gpointer            data)
{
  int i;
  MetaWindowMenu *menu;

  menu = g_new (MetaWindowMenu, 1);
  menu->frames = frames;
  menu->client_xwindow = client_xwindow;
  menu->func = func;
  menu->data = data;
  menu->ops = ops;
  menu->insensitive = insensitive;
  
  menu->menu = gtk_menu_new ();
#ifdef HAVE_GTK_MULTIHEAD
  gtk_menu_set_screen (GTK_MENU (menu->menu),
		       gtk_widget_get_screen (GTK_WIDGET (frames)));
#endif
  i = 0;
  while (i < (int) G_N_ELEMENTS (menuitems))
    {
      if (ops & menuitems[i].op || menuitems[i].op == 0)
        {
          GtkWidget *mi;
          MenuData *md;
          unsigned int key;
          MetaVirtualModifier mods;
          
          if (menuitems[i].op == 0)
            {
              mi = gtk_separator_menu_item_new ();
            }
          else
            {
              GtkWidget *image;

              image = NULL;
              
              if (menuitems[i].stock_id)
                {
                  image = gtk_image_new_from_stock (menuitems[i].stock_id,
                                                    GTK_ICON_SIZE_MENU);

                }

              meta_core_get_menu_accelerator (menuitems[i].op, -1,
                                              &key, &mods);
              
              if (image)
                {
                  mi = menu_item_new (_(menuitems[i].label), TRUE, key, mods);
                  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi),
                                                 image);
                  gtk_widget_show (image);
                }
              else
                {
                  mi = menu_item_new (_(menuitems[i].label), FALSE, key, mods);
                }
              
              if (insensitive & menuitems[i].op)
                gtk_widget_set_sensitive (mi, FALSE);
              
              md = g_new (MenuData, 1);
              
              md->menu = menu;
              md->op = menuitems[i].op;
              
              gtk_signal_connect_full (GTK_OBJECT (mi),
                                       "activate",
                                       GTK_SIGNAL_FUNC (activate_cb),
                                       NULL,
                                       md,
                                       g_free, FALSE, FALSE);
            }
          
          gtk_menu_shell_append (GTK_MENU_SHELL (menu->menu),
                                 mi);
          
          gtk_widget_show (mi);
        }
      ++i;
    }

  if (ops & META_MENU_OP_WORKSPACES)
    {
      meta_verbose ("Creating %d-workspace menu current space %d\n",
                    n_workspaces, active_workspace);
      
      if (n_workspaces > 1)
        {
          GtkWidget *mi;
          Display *display;
          Window xroot;
          
          display = gdk_x11_drawable_get_xdisplay (GTK_WIDGET (frames)->window);

#ifdef HAVE_GTK_MULTIHEAD
          {
            GdkScreen *screen;
            screen = gdk_drawable_get_screen (GTK_WIDGET (frames)->window);
            xroot = GDK_DRAWABLE_XID (gdk_screen_get_root_window (screen));
          }
#else
          {
            xroot = gdk_x11_get_default_root_xwindow ();
          }
#endif
          
          i = 0;
          while (i < n_workspaces)
            {
              char *label, *name;
              MenuData *md;
              unsigned int key;
              MetaVirtualModifier mods;
              
              meta_core_get_menu_accelerator (META_MENU_OP_WORKSPACES,
                                              i + 1,
                                              &key, &mods);
              
              name = get_workspace_name_with_accel (display, xroot, i);
              if (ops & META_MENU_OP_UNSTICK)
                label = g_strdup_printf (_("Only on %s"), name);
              else
                label = g_strdup_printf(_("Move to %s"), name);
              mi = menu_item_new (label, FALSE, key, mods);

              g_free (name);
              g_free (label);

              if (!(ops & META_MENU_OP_UNSTICK) &&
                  (active_workspace == i ||
                   insensitive & META_MENU_OP_WORKSPACES))
                gtk_widget_set_sensitive (mi, FALSE);
	      else if (insensitive & META_MENU_OP_WORKSPACES)
		gtk_widget_set_sensitive (mi, FALSE);
              md = g_new (MenuData, 1);

              md->menu = menu;
              md->op = META_MENU_OP_WORKSPACES;

              g_object_set_data (G_OBJECT (mi),
                                 "workspace",
                                 GINT_TO_POINTER (i));
          
              gtk_signal_connect_full (GTK_OBJECT (mi),
                                       "activate",
                                       GTK_SIGNAL_FUNC (activate_cb),
                                       NULL,
                                       md,
                                       g_free, FALSE, FALSE);

              gtk_menu_shell_append (GTK_MENU_SHELL (menu->menu),
                                     mi);
          
              gtk_widget_show (mi);
          
              ++i;
            }
        }
    }
  else
    meta_verbose ("not creating workspace menu\n");
  
  gtk_signal_connect (GTK_OBJECT (menu->menu),
                      "selection_done",
                      GTK_SIGNAL_FUNC (menu_closed),
                      menu);  

  return menu;
}

void
meta_window_menu_popup (MetaWindowMenu     *menu,
                        int                 root_x,
                        int                 root_y,
                        int                 button,
                        guint32             timestamp)
{
  GdkPoint *pt;
  
  pt = g_new (GdkPoint, 1);

  g_object_set_data_full (G_OBJECT (menu->menu),
                          "destroy-point",
                          pt,
                          g_free);

  pt->x = root_x;
  pt->y = root_y;
  
  gtk_menu_popup (GTK_MENU (menu->menu),
                  NULL, NULL,
                  popup_position_func, pt,
                  button,
                  timestamp);

  if (!GTK_MENU_SHELL (menu->menu)->have_xgrab)
    meta_warning ("GtkMenu failed to grab the pointer\n");
}

void
meta_window_menu_free (MetaWindowMenu *menu)
{
  gtk_widget_destroy (menu->menu);
  g_free (menu);
}
