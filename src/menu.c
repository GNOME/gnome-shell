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
#include "util.h"
#include "core.h"

typedef struct _MenuItem MenuItem;
typedef struct _MenuData MenuData;

typedef enum
{
  META_MENU_OP_DELETE      = 1 << 0,
  META_MENU_OP_MINIMIZE    = 1 << 1,
  META_MENU_OP_UNMAXIMIZE  = 1 << 2,
  META_MENU_OP_MAXIMIZE    = 1 << 3,
  META_MENU_OP_UNSHADE     = 1 << 4,
  META_MENU_OP_SHADE       = 1 << 5,
  META_MENU_OP_UNSTICK     = 1 << 6,
  META_MENU_OP_STICK       = 1 << 7,
  META_MENU_OP_WORKSPACES  = 1 << 8
} MetaMenuOp;

struct _MenuItem
{
  MetaMenuOp op;
  const char *stock_id;
  const char *label;
};


struct _MenuData
{
  MetaFrames *frames;
  MetaUIFrame *frame;
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
menu_closed (GtkMenu *menu,
             gpointer data)
{
  MetaFrames *frames;

  frames = META_FRAMES (data);

  meta_frames_notify_menu_hide (frames);

  gtk_widget_destroy (frames->menu);
  frames->menu = NULL;
}

void
meta_window_menu_show (MetaFrames              *frames,
                       MetaUIFrame             *frame,
                       int                      root_x,
                       int                      root_y,
                       int                      button,
                       guint32                  timestamp)
{
  int i;
  GdkPoint *pt;
  int n_workspaces;
  int current_workspace;
  MetaMenuOp ops;
  MetaMenuOp insensitive;
  MetaFrameFlags flags;
  
  flags = meta_core_get_frame_flags (gdk_display, frame->xwindow);
  
  ops = 0;
  insensitive = 0;
  
  if (flags & META_FRAME_ALLOWS_MAXIMIZE)
    {
      if (flags & META_FRAME_MAXIMIZED)
        ops |= META_MENU_OP_UNMAXIMIZE;
      else
        ops |= META_MENU_OP_MAXIMIZE;
    }

  if (flags & META_FRAME_SHADED)
    ops |= META_MENU_OP_UNSHADE;
  else
    ops |= META_MENU_OP_SHADE;

  if (flags & META_FRAME_STUCK)
    ops |= META_MENU_OP_UNSTICK;
  else
    ops |= META_MENU_OP_STICK;
  
  ops |= (META_MENU_OP_DELETE | META_MENU_OP_WORKSPACES | META_MENU_OP_MINIMIZE);

  if (!(flags & META_FRAME_ALLOWS_MINIMIZE))
    insensitive |= META_MENU_OP_MINIMIZE;
  
  if (!(flags & META_FRAME_ALLOWS_DELETE))
    insensitive |= META_MENU_OP_DELETE;
  
  if (frames->menu)
    gtk_widget_destroy (frames->menu);
  
  frames->menu = gtk_menu_new ();

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
              GtkWidget *image;
              GdkPixmap *pix;
              GdkBitmap *mask;

              image = NULL;
              pix = NULL;
              mask = NULL;
              
              switch (menuitems[i].op)
                {
                case META_MENU_OP_MAXIMIZE:
                  meta_frames_get_pixmap_for_control (frames,
                                                      META_FRAME_CONTROL_MAXIMIZE,
                                                      &pix, &mask);
                  break;
                  
                case META_MENU_OP_MINIMIZE:
                  meta_frames_get_pixmap_for_control (frames,
                                                      META_FRAME_CONTROL_MINIMIZE,
                                                      &pix, &mask);
                  break;

                case META_MENU_OP_DELETE:
                  meta_frames_get_pixmap_for_control (frames,
                                                      META_FRAME_CONTROL_DELETE,
                                                      &pix, &mask);
                  break;
                default:
                  break;
                }

              if (pix)
                {
                  image = gtk_image_new_from_pixmap (pix, mask);
                  g_object_unref (G_OBJECT (pix));
                  g_object_unref (G_OBJECT (mask));
                }
              
              if (image == NULL && 
                  menuitems[i].stock_id)
                {
                  image = gtk_image_new_from_stock (menuitems[i].stock_id,
                                                    GTK_ICON_SIZE_MENU);

                }
              
              if (image)
                {
                  mi = gtk_image_menu_item_new_with_mnemonic (menuitems[i].label);
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
              
              md->frames = frames;
              md->frame = frame;
              md->op = menuitems[i].op;
              
              gtk_signal_connect_full (GTK_OBJECT (mi),
                                       "activate",
                                       GTK_SIGNAL_FUNC (activate_cb),
                                       NULL,
                                       md,
                                       g_free, FALSE, FALSE);
            }
          
          gtk_menu_shell_append (GTK_MENU_SHELL (frames->menu),
                                 mi);
          
          gtk_widget_show (mi);
        }
      ++i;
    }

  if (ops & META_MENU_OP_WORKSPACES)
    {
      n_workspaces = meta_core_get_num_workspaces (DefaultScreenOfDisplay (gdk_display));
      current_workspace = meta_core_get_frame_workspace (gdk_display,
                                                         frame->xwindow);

      meta_warning ("Creating %d-workspace menu current %d\n",
                    n_workspaces, current_workspace);
      
      if (n_workspaces > 0)
        {
          GtkWidget *mi;
          
          i = 0;
          while (i < n_workspaces)
            {
              char *label;
              MenuData *md;

              if (flags & META_FRAME_STUCK)
                label = g_strdup_printf (_("Only on workspace _%d\n"),
                                         i + 1);
              else
                label = g_strdup_printf (_("Move to workspace _%d\n"),
                                         i + 1);
          
              mi = gtk_menu_item_new_with_mnemonic (label);

              g_free (label);

              if (!(flags & META_FRAME_STUCK) &&
                  (current_workspace == i ||
                   insensitive & META_MENU_OP_WORKSPACES))
                gtk_widget_set_sensitive (mi, FALSE);

              md = g_new (MenuData, 1);

              md->frames = frames;
              md->frame = frame;
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

              gtk_menu_shell_append (GTK_MENU_SHELL (frames->menu),
                                     mi);
          
              gtk_widget_show (mi);
          
              ++i;
            }
        }
    }
  else
    meta_verbose ("not creating workspace menu\n");
  
  gtk_signal_connect (GTK_OBJECT (frames->menu),
                      "selection_done",
                      GTK_SIGNAL_FUNC (menu_closed),
                      frames);
  
  pt = g_new (GdkPoint, 1);

  g_object_set_data_full (G_OBJECT (frames->menu),
                          "destroy-point",
                          pt,
                          g_free);

  pt->x = root_x;
  pt->y = root_y;
  
  gtk_menu_popup (GTK_MENU (frames->menu),
                  NULL, NULL,
                  popup_position_func, pt,
                  button,
                  timestamp);

  if (!GTK_MENU_SHELL (frames->menu)->have_xgrab)
    meta_warning ("GtkMenu failed to grab the pointer\n");
}

static void
activate_cb (GtkWidget *menuitem, gpointer data)
{
  MenuData *md;
  
  g_return_if_fail (GTK_IS_WIDGET (menuitem));
  
  md = data;
  
  switch (md->op)
    {
    case META_MENU_OP_DELETE:
      meta_core_delete (gdk_display,
                        md->frame->xwindow,
                        gtk_get_current_event_time ());
      break;

    case META_MENU_OP_MINIMIZE:
      meta_core_minimize (gdk_display,
                          md->frame->xwindow);
      break;

    case META_MENU_OP_UNMAXIMIZE:
      meta_core_unmaximize (gdk_display,
                            md->frame->xwindow);
      break;
      
    case META_MENU_OP_MAXIMIZE:
      meta_core_maximize (gdk_display,
                          md->frame->xwindow);
      break;

    case META_MENU_OP_UNSHADE:
      meta_core_unshade (gdk_display,
                         md->frame->xwindow);
      break;
      
    case META_MENU_OP_SHADE:
      meta_core_shade (gdk_display,
                       md->frame->xwindow);
      break;
      
    case META_MENU_OP_WORKSPACES:
      {
        int workspace;

        workspace = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menuitem),
                                                        "workspace"));

        meta_core_change_workspace (gdk_display, md->frame->xwindow,
                                    workspace);
      }
      break;

    case META_MENU_OP_STICK:
      meta_core_stick (gdk_display,
                       md->frame->xwindow);
      break;

    case META_MENU_OP_UNSTICK:
      meta_core_unstick (gdk_display,
                         md->frame->xwindow);
      break;
      
    default:
      meta_warning (G_STRLOC": Unknown window op\n");
      break;
    }
}
