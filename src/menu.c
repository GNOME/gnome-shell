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
#include "menu.h"
#include "main.h"
#include "util.h"
#include "core.h"
#include "themewidget.h"

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
  { META_MENU_OP_DELETE, NULL, N_("_Close") },
  { META_MENU_OP_MINIMIZE, NULL, N_("_Minimize") },
  { META_MENU_OP_MAXIMIZE, NULL, N_("Ma_ximize") },
  { META_MENU_OP_UNMAXIMIZE, NULL, N_("_Unmaximize") },
  { META_MENU_OP_SHADE, NULL, N_("_Shade") },
  { META_MENU_OP_UNSHADE, NULL, N_("U_nshade") },
  { META_MENU_OP_MOVE, NULL, N_("Mo_ve") },
  { META_MENU_OP_RESIZE, NULL, N_("_Resize") },
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

static void
menu_icon_size_func (MetaArea *area,
                     int      *width,
                     int      *height,
                     void     *user_data)
{
  gtk_icon_size_lookup (GTK_ICON_SIZE_MENU,
                        width, height);
}

static void
menu_icon_expose_func (MetaArea       *area,
                       GdkEventExpose *event,
                       int             x_offset,
                       int             y_offset,
                       void           *user_data)
{
  int width, height;
  MetaMenuIconType type;
  
  type = GPOINTER_TO_INT (user_data);
  
  gtk_icon_size_lookup (GTK_ICON_SIZE_MENU,
                        &width, &height);
  
  meta_theme_draw_menu_icon (meta_theme_get_current (),
                             GTK_WIDGET (area),
                             GTK_WIDGET (area)->window,
                             &event->area,
                             x_offset, y_offset,
                             width, height,
                             type);
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

  i = 0;
  while (i < (int) G_N_ELEMENTS (menuitems))
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
              GtkWidget *image;

              image = NULL;
              
              switch (menuitems[i].op)
                {
                case META_MENU_OP_MAXIMIZE:
                  image = meta_area_new ();
                  meta_area_setup (META_AREA (image),
                                   menu_icon_size_func,
                                   menu_icon_expose_func,
                                   GINT_TO_POINTER (META_MENU_ICON_TYPE_MAXIMIZE),
                                   NULL);
                  break;

                case META_MENU_OP_UNMAXIMIZE:
                  image = meta_area_new ();
                  meta_area_setup (META_AREA (image),
                                   menu_icon_size_func,
                                   menu_icon_expose_func,
                                   GINT_TO_POINTER (META_MENU_ICON_TYPE_UNMAXIMIZE),
                                   NULL);
                  break;
                  
                case META_MENU_OP_MINIMIZE:
                  image = meta_area_new ();
                  meta_area_setup (META_AREA (image),
                                   menu_icon_size_func,
                                   menu_icon_expose_func,
                                   GINT_TO_POINTER (META_MENU_ICON_TYPE_MINIMIZE),
                                   NULL);
                  break;

                case META_MENU_OP_DELETE:
                  image = meta_area_new ();
                  meta_area_setup (META_AREA (image),
                                   menu_icon_size_func,
                                   menu_icon_expose_func,
                                   GINT_TO_POINTER (META_MENU_ICON_TYPE_CLOSE),
                                   NULL);
                  break;
                default:
                  break;
                }
              
              if (image == NULL && 
                  menuitems[i].stock_id)
                {
                  image = gtk_image_new_from_stock (menuitems[i].stock_id,
                                                    GTK_ICON_SIZE_MENU);

                }
              
              if (image)
                {
                  mi = gtk_image_menu_item_new_with_mnemonic (_(menuitems[i].label));
                  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mi),
                                                 image);
                  gtk_widget_show (image);
                }
              else
                {
                  mi = gtk_menu_item_new_with_mnemonic (_(menuitems[i].label));
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
      
      if (n_workspaces > 0)
        {
          GtkWidget *mi;
          
          i = 0;
          while (i < n_workspaces)
            {
              char *label;
              MenuData *md;

              if (ops & META_MENU_OP_UNSTICK)
                label = g_strdup_printf (_("Only on workspace %s%d"),
                                         i < 9 ? "_" : "", i + 1);
              else
                label = g_strdup_printf (_("Move to workspace %s%d"),
                                         i < 9 ? "_" : "", i + 1);
              
              mi = gtk_menu_item_new_with_mnemonic (label);

              g_free (label);

              if (!(ops & META_MENU_OP_UNSTICK) &&
                  (active_workspace == i ||
                   insensitive & META_MENU_OP_WORKSPACES))
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
