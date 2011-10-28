/* Hack for use instead of xmag */

/* 
 * Copyright (C) 2002 Red Hat Inc.
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

#define _GNU_SOURCE
#define _XOPEN_SOURCE 600 /* C99 -- for rint() */

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

static GtkWidget *grab_widget = NULL;
static GtkWidget *display_window = NULL;
static int last_grab_x = 0;
static int last_grab_y = 0;
static int last_grab_width = 150;
static int last_grab_height = 150;
static GtkAllocation last_grab_allocation;
static double width_factor = 4.0;
static double height_factor = 4.0;
static GdkInterpType interp_mode = GDK_INTERP_NEAREST;
static guint regrab_idle_id = 0;

static GdkPixbuf*
get_pixbuf (void)
{
  GdkPixbuf *screenshot;
  GdkPixbuf *magnified;

#if 0
  g_print ("Size %d x %d\n",
           last_grab_width, last_grab_height);
#endif
  
  screenshot = gdk_pixbuf_get_from_window (gdk_get_default_root_window (),
                                           last_grab_x, last_grab_y,
                                           last_grab_width, last_grab_height);

  if (screenshot == NULL)
    {
      g_printerr ("Screenshot failed\n");
      exit (1);
    }

  magnified = gdk_pixbuf_scale_simple (screenshot, last_grab_width * width_factor,
                                       last_grab_height * height_factor,
                                       interp_mode);


  g_object_unref (G_OBJECT (screenshot));

  return magnified;
}

static gboolean
regrab_idle (GtkWidget *image)
{
  GtkAllocation allocation;
  GdkPixbuf *magnified;

  gtk_widget_get_allocation (image, &allocation);

  if (allocation.width != last_grab_allocation.width ||
      allocation.height != last_grab_allocation.height)
    {
      last_grab_width = rint (allocation.width / width_factor);
      last_grab_height = rint (allocation.height / height_factor);
      last_grab_allocation = allocation;
      
      magnified = get_pixbuf ();

      gtk_image_set_from_pixbuf (GTK_IMAGE (image), magnified);

      g_object_unref (G_OBJECT (magnified));
    }

  regrab_idle_id = 0;
  
  return FALSE;
}

static void
image_resized (GtkWidget *image)
{
  if (regrab_idle_id == 0)
    regrab_idle_id = g_idle_add_full (G_PRIORITY_LOW + 100, (GSourceFunc) regrab_idle,
                                      image, NULL);
}

static void
grab_area_at_mouse (GtkWidget *invisible,
                    int        x_root,
                    int        y_root)
{
  GdkPixbuf *magnified;
  int width, height;
  GtkWidget *widget;
  
  width = last_grab_width;
  height = last_grab_height;

  last_grab_x = x_root;
  last_grab_y = y_root;
  last_grab_width = width;
  last_grab_height = height;
  
  magnified = get_pixbuf ();
  
  display_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (display_window),
                               last_grab_width, last_grab_height);
  widget = gtk_image_new_from_pixbuf (magnified);
  gtk_widget_set_size_request (widget, 40, 40);
  gtk_container_add (GTK_CONTAINER (display_window), widget);
  g_object_unref (G_OBJECT (magnified));

  g_object_add_weak_pointer (G_OBJECT (display_window),
                             (gpointer) &display_window);

  g_signal_connect (G_OBJECT (display_window), "destroy",
                    G_CALLBACK (gtk_main_quit), NULL);

  g_signal_connect_after (G_OBJECT (widget), "size_allocate", G_CALLBACK (image_resized), NULL);
  
  gtk_widget_show_all (display_window);
}

static void
shutdown_grab (void)
{
  GdkDeviceManager *manager;
  GdkDevice *device;

  manager = gdk_display_get_device_manager (gdk_display_get_default ());
  device = gdk_device_manager_get_client_pointer (manager);

  gdk_device_ungrab (device, gtk_get_current_event_time ());
  gdk_device_ungrab (gdk_device_get_associated_device (device),
                     gtk_get_current_event_time ());
  gtk_grab_remove (grab_widget);
}

static void
mouse_motion (GtkWidget      *invisible,
	      GdkEventMotion *event,
	      gpointer        data)
{
  
}

static gboolean
mouse_release (GtkWidget      *invisible,
	       GdkEventButton *event,
	       gpointer        data)
{
  if (event->button != 1)
    return FALSE;

  grab_area_at_mouse (invisible, event->x_root, event->y_root);

  shutdown_grab ();
  
  g_signal_handlers_disconnect_by_func (invisible, mouse_motion, NULL);
  g_signal_handlers_disconnect_by_func (invisible, mouse_release, NULL);

  return TRUE;
}

/* Helper Functions */

static gboolean mouse_press (GtkWidget      *invisible,
                             GdkEventButton *event,
                             gpointer        data);

static gboolean
key_press (GtkWidget   *invisible,
           GdkEventKey *event,
           gpointer     data)
{  
  if (event->keyval == GDK_KEY_Escape)
    {
      shutdown_grab ();

      g_signal_handlers_disconnect_by_func (invisible, mouse_press, NULL);
      g_signal_handlers_disconnect_by_func (invisible, key_press, NULL);
      
      gtk_main_quit ();

      return TRUE;
    }

  return FALSE;
}

static gboolean
mouse_press (GtkWidget      *invisible,
	     GdkEventButton *event,
	     gpointer        data)
{  
  if (event->type == GDK_BUTTON_PRESS &&
      event->button == 1)
    {
      g_signal_connect (invisible, "motion_notify_event",
                        G_CALLBACK (mouse_motion), NULL);
      g_signal_connect (invisible, "button_release_event",
                        G_CALLBACK (mouse_release), NULL);
      g_signal_handlers_disconnect_by_func (invisible, mouse_press, NULL);
      g_signal_handlers_disconnect_by_func (invisible, key_press, NULL);
      return TRUE;
    }

  return FALSE;
}

static void
begin_area_grab (void)
{
  GdkWindow *window;
  GdkDeviceManager *manager;
  GdkDevice *device;

  if (grab_widget == NULL)
    {
      grab_widget = gtk_invisible_new ();

      gtk_widget_add_events (grab_widget,
                             GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK);
      
      gtk_widget_show (grab_widget);
    }

  window = gtk_widget_get_window (grab_widget);
  manager = gdk_display_get_device_manager (gdk_display_get_default ());
  device = gdk_device_manager_get_client_pointer (manager);

  if (gdk_device_grab (device,
                       window,
                       GDK_OWNERSHIP_NONE,
                       FALSE,
                       GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK,
                       NULL,
                       gtk_get_current_event_time ()) != GDK_GRAB_SUCCESS)
    {
      g_warning ("Failed to grab pointer to do eyedropper");
      return;
    }

  if (gdk_device_grab (gdk_device_get_associated_device (device),
                       window,
                       GDK_OWNERSHIP_NONE,
                       FALSE,
                       GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK,
                       NULL,
                       gtk_get_current_event_time ()) != GDK_GRAB_SUCCESS)
    {
      gdk_device_ungrab (device, gtk_get_current_event_time ());
      g_warning ("Failed to grab keyboard to do eyedropper");
      return;
    }

  gtk_grab_add (grab_widget);
  
  g_signal_connect (grab_widget, "button_press_event",
                    G_CALLBACK (mouse_press), NULL);
  g_signal_connect (grab_widget, "key_press_event",
                    G_CALLBACK (key_press), NULL);
}

int
main (int argc, char **argv)
{
  gtk_init (&argc, &argv);

  begin_area_grab ();
  
  gtk_main ();
  
  return 0;
}
