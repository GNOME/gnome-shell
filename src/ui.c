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
    meta_fatal ("Unable to open X display %s\n", gdk_display_name);
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
    gdk_x11_window_map (window);
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


