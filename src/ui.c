/* Metacity interface for talking to GTK+ UI module */

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
#include "frames.h"
#include "util.h"
#include "menu.h"
#include "core.h"

struct _MetaUI
{
  Display *xdisplay;
  Screen *xscreen;
  MetaFrames *frames;
};

void
meta_ui_init (int *argc, char ***argv)
{
  if (!gtk_init_check (argc, argv))
    meta_fatal ("Unable to open X display %s\n", XDisplayName (NULL));
}

Display*
meta_ui_get_display (const char *name)
{
  if (name == NULL)
    return gdk_display;
  else
    return NULL;
}

typedef struct _EventFunc EventFunc;

struct _EventFunc
{
  MetaEventFunc func;
  gpointer data;
};

static GdkFilterReturn
filter_func (GdkXEvent *xevent,
             GdkEvent *event,
             gpointer data)
{
  EventFunc *ef;

  ef = data;

  if ((* ef->func) (xevent, ef->data))
    return GDK_FILTER_REMOVE;
  else
    return GDK_FILTER_CONTINUE;
}

static EventFunc *ef = NULL;

void
meta_ui_add_event_func (Display       *xdisplay,
                        MetaEventFunc  func,
                        gpointer       data)
{
  g_return_if_fail (ef == NULL);

  ef = g_new (EventFunc, 1);
  ef->func = func;
  ef->data = data;

  gdk_window_add_filter (NULL, filter_func, ef);
}

/* removal is by data due to proxy function */
void
meta_ui_remove_event_func (Display       *xdisplay,
                           MetaEventFunc  func,
                           gpointer       data)
{
  g_return_if_fail (ef != NULL);
  
  gdk_window_remove_filter (NULL, filter_func, ef);

  g_free (ef);
  ef = NULL;
}

MetaUI*
meta_ui_new (Display *xdisplay,
             Screen  *screen)
{
  MetaUI *ui;

  ui = g_new (MetaUI, 1);
  ui->xdisplay = xdisplay;
  ui->xscreen = screen;

  /* FIXME when gtk has multihead use it here */
  ui->frames = meta_frames_new ();
  gtk_widget_realize (GTK_WIDGET (ui->frames));
  
  return ui;
}

void
meta_ui_free (MetaUI *ui)
{
  gtk_widget_destroy (GTK_WIDGET (ui->frames));

  g_free (ui);
}

void
meta_ui_get_frame_geometry (MetaUI *ui,
                            Window frame_xwindow,
                            int *top_height, int *bottom_height,
                            int *left_width, int *right_width)
{
  meta_frames_get_geometry (ui->frames, frame_xwindow,
                            top_height, bottom_height,
                            left_width, right_width);
}


void
meta_ui_add_frame (MetaUI *ui,
                   Window  xwindow)
{
  meta_frames_manage_window (ui->frames, xwindow);
}

void
meta_ui_remove_frame (MetaUI *ui,
                      Window  xwindow)
{
  meta_frames_unmanage_window (ui->frames, xwindow);
}

void
meta_ui_map_frame   (MetaUI *ui,
                     Window  xwindow)
{
  GdkWindow *window;

  window = gdk_xid_table_lookup (xwindow);

  if (window)
    gdk_window_show_unraised (window);
}

void
meta_ui_unmap_frame (MetaUI *ui,
                     Window  xwindow)
{
  GdkWindow *window;

  window = gdk_xid_table_lookup (xwindow);

  if (window)
    gdk_window_hide (window);
}

void
meta_ui_reset_frame_bg (MetaUI *ui,
                        Window xwindow)
{
  meta_frames_reset_bg (ui->frames, xwindow);
}

void
meta_ui_queue_frame_draw (MetaUI *ui,
                          Window xwindow)
{
  meta_frames_queue_draw (ui->frames, xwindow);
}


void
meta_ui_set_frame_title (MetaUI     *ui,
                         Window      xwindow,
                         const char *title)
{
  meta_frames_set_title (ui->frames, xwindow, title);
}

MetaWindowMenu*
meta_ui_window_menu_new  (MetaUI             *ui,
                          Window              client_xwindow,
                          MetaMenuOp          ops,
                          MetaMenuOp          insensitive,
                          int                 active_workspace,
                          int                 n_workspaces,
                          MetaWindowMenuFunc  func,
                          gpointer            data)
{
  return meta_window_menu_new (ui->frames,
                               ops, insensitive,
                               client_xwindow,
                               active_workspace,
                               n_workspaces,
                               func, data);
}

void
meta_ui_window_menu_popup (MetaWindowMenu     *menu,
                           int                 root_x,
                           int                 root_y,
                           int                 button,
                           guint32             timestamp)
{
  meta_window_menu_popup (menu, root_x, root_y, button, timestamp);
}

void
meta_ui_window_menu_free (MetaWindowMenu *menu)
{
  meta_window_menu_free (menu);
}

struct _MetaImageWindow
{
  GtkWidget *window;
  GtkWidget *image;
};

MetaImageWindow*
meta_image_window_new (void)
{
  MetaImageWindow *iw;

  iw = g_new (MetaImageWindow, 1);
  iw->window = gtk_window_new (GTK_WINDOW_POPUP);
  iw->image = g_object_new (GTK_TYPE_IMAGE, NULL);

  gtk_container_add (GTK_CONTAINER (iw->window), iw->image);

  /* Ensure we auto-shrink to fit image */
  gtk_window_set_resizable (GTK_WINDOW (iw->window),
                            FALSE);
  
  return iw;
}

void
meta_image_window_free (MetaImageWindow *iw)
{
  gtk_widget_destroy (iw->window);
  g_free (iw);
}

void
meta_image_window_set_showing  (MetaImageWindow *iw,
                                gboolean         showing)
{
  if (showing)
    gtk_widget_show_all (iw->window);
  else
    {
      gtk_widget_hide (iw->window);
      meta_core_increment_event_serial (gdk_display);
    }
}

void
meta_image_window_set_image (MetaImageWindow *iw,
                             GdkPixbuf       *pixbuf)
{
  gtk_image_set_from_pixbuf (GTK_IMAGE (iw->image), pixbuf);
}

void
meta_image_window_set_position (MetaImageWindow *iw,
                                int              x,
                                int              y)
{
  /* We want to do move/resize at the same time to avoid ugliness.
   * Lame hack.
   */
  GtkRequisition req;

  g_return_if_fail (GTK_WIDGET_REALIZED (iw->window));
  
  gtk_widget_size_request (iw->window, &req);
  
  gdk_window_move_resize (GTK_WIDGET (iw->window)->window,
                          x, y, req.width, req.height);
}

static GdkColormap*
get_cmap (GdkPixmap *pixmap)
{
  GdkColormap *cmap;

  cmap = gdk_drawable_get_colormap (pixmap);
  if (cmap)
    g_object_ref (G_OBJECT (cmap));

  if (cmap == NULL)
    {
      if (gdk_drawable_get_depth (pixmap) == 1)
        {
          meta_verbose ("Using NULL colormap for snapshotting bitmap\n");
          cmap = NULL;
        }
      else
        {
          meta_verbose ("Using system cmap to snapshot pixmap\n");
          cmap = gdk_colormap_get_system ();
          g_object_ref (G_OBJECT (cmap));
        }
    }

  return cmap;
}

GdkPixbuf*
meta_gdk_pixbuf_get_from_window (GdkPixbuf   *dest,
                                 Window       xwindow,
                                 int          src_x,
                                 int          src_y,
                                 int          dest_x,
                                 int          dest_y,
                                 int          width,
                                 int          height)
{
  GdkDrawable *drawable;
  GdkPixbuf *retval;
  GdkColormap *cmap;
  
  retval = NULL;
  
  drawable = gdk_xid_table_lookup (xwindow);

  if (drawable)
    g_object_ref (G_OBJECT (drawable));
  else
    drawable = gdk_window_foreign_new (xwindow);

  cmap = get_cmap (drawable);
  
  retval = gdk_pixbuf_get_from_drawable (dest,
                                         drawable,
                                         cmap,
                                         src_x, src_y,
                                         dest_x, dest_y,
                                         width, height);

  if (cmap)
    g_object_unref (G_OBJECT (cmap));
  g_object_unref (G_OBJECT (drawable));

  return retval;
}

GdkPixbuf*
meta_gdk_pixbuf_get_from_pixmap (GdkPixbuf   *dest,
                                 Pixmap       xpixmap,
                                 int          src_x,
                                 int          src_y,
                                 int          dest_x,
                                 int          dest_y,
                                 int          width,
                                 int          height)
{
  GdkDrawable *drawable;
  GdkPixbuf *retval;
  GdkColormap *cmap;
  
  retval = NULL;
  
  drawable = gdk_xid_table_lookup (xpixmap);

  if (drawable)
    g_object_ref (G_OBJECT (drawable));
  else
    drawable = gdk_pixmap_foreign_new (xpixmap);

  cmap = get_cmap (drawable);
  
  retval = gdk_pixbuf_get_from_drawable (dest,
                                         drawable,
                                         cmap,
                                         src_x, src_y,
                                         dest_x, dest_y,
                                         width, height);

  if (cmap)
    g_object_unref (G_OBJECT (cmap));
  g_object_unref (G_OBJECT (drawable));

  return retval;
}

void
meta_ui_push_delay_exposes (MetaUI *ui)
{
  meta_frames_push_delay_exposes (ui->frames);
}

void
meta_ui_pop_delay_exposes  (MetaUI *ui)
{
  meta_frames_pop_delay_exposes (ui->frames);
}

GdkPixbuf*
meta_ui_get_default_window_icon (MetaUI *ui)
{
  /* FIXME */
  return gtk_widget_render_icon (GTK_WIDGET (ui->frames),
                                 GTK_STOCK_NEW,
                                 GTK_ICON_SIZE_LARGE_TOOLBAR,
                                 NULL);
}

GdkPixbuf*
meta_ui_get_default_mini_icon (MetaUI *ui)
{
  /* FIXME */
  return gtk_widget_render_icon (GTK_WIDGET (ui->frames),
                                 GTK_STOCK_NEW,
                                 GTK_ICON_SIZE_MENU,
                                 NULL);
}

gboolean
meta_ui_window_should_not_cause_focus (Display *xdisplay,
                                       Window   xwindow)
{
  GdkWindow *window;

  window = gdk_xid_table_lookup (xwindow);

  /* we shouldn't cause focus if we're an override redirect
   * toplevel which is not foreign
   */
  if (window && gdk_window_get_type (window) == GDK_WINDOW_TEMP)
    return TRUE;
  else
    return FALSE;
}

char*
meta_text_property_to_utf8 (Display             *xdisplay,
                            const XTextProperty *prop)
{
  char **list;
  int count;
  char *retval;
  
  list = NULL;

  count = gdk_text_property_to_utf8_list (prop->encoding,
                                          prop->format,
                                          prop->value,
                                          prop->nitems,
                                          &list);

  if (count == 0)
    return NULL;

  retval = list[0];
  list[0] = g_strdup (""); /* something to free */
  
  g_strfreev (list);

  return retval;
}
