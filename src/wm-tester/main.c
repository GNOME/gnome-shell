/* WM tester main() */

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

#include <gtk/gtk.h>

#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void set_up_the_evil (void);
static void set_up_icon_windows (void);

static void
usage (void)
{
  g_print ("wm-tester [--evil] [--icon-windows]\n");
  exit (0);
}

int
main (int argc, char **argv)
{
  int i;
  gboolean do_evil;
  gboolean do_icon_windows;
  
  gtk_init (&argc, &argv);  
  
  do_evil = FALSE;
  do_icon_windows = FALSE;
  
  i = 1;
  while (i < argc)
    {
      const char *arg = argv[i];
      
      if (strcmp (arg, "--help") == 0 ||
          strcmp (arg, "-h") == 0 ||
          strcmp (arg, "-?") == 0)
        usage ();
      else if (strcmp (arg, "--evil") == 0)
        do_evil = TRUE;
      else if (strcmp (arg, "--icon-windows") == 0)
        do_icon_windows = TRUE;
      else
        usage ();
      
      ++i;
    }

  /* Be sure some option was provided */
  if (! (do_evil || do_icon_windows))
    return 1;
  
  if (do_evil)
    set_up_the_evil ();

  if (do_icon_windows)
    set_up_icon_windows ();
  
  gtk_main ();

  return 0;
}

static GSList *evil_windows = NULL;

static gint
evil_timeout (gpointer data)
{
  int i;
  int n_windows;
  int len;
  int create_count;
  int destroy_count;
  
  len = g_slist_length (evil_windows);  
  
  if (len > 35)
    {
      create_count = 2;
      destroy_count = 5;
    }
  else
    {
      create_count = 5;
      destroy_count = 5;
    }

  /* Create some windows */
  n_windows = g_random_int_range (0, create_count);
  
  i = 0;
  while (i < n_windows)
    {
      GtkWidget *w;
      GtkWidget *c;
      int t;
      GtkWidget *parent;
      
      w = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_window_move (GTK_WINDOW (w),
                       g_random_int_range (0,
                                           gdk_screen_width ()),
                       g_random_int_range (0,
                                           gdk_screen_height ()));

      parent = NULL;
      
      /* set transient for random window (may create all kinds of weird cycles) */
      if (len > 0)
        {
          t = g_random_int_range (- (len / 3), len);
          if (t >= 0)
            {
              parent = g_slist_nth_data (evil_windows, t);
              
              if (parent != NULL)
                gtk_window_set_transient_for (GTK_WINDOW (w), GTK_WINDOW (parent));
            }
        }
      
      if (parent != NULL)
        c = gtk_button_new_with_label ("Evil Transient!");
      else
        c = gtk_button_new_with_label ("Evil Window!");
      gtk_container_add (GTK_CONTAINER (w), c);
      
      gtk_widget_show_all (w);
      
      evil_windows = g_slist_prepend (evil_windows, w);
      
      ++i;
    }

  /* Destroy some windows */
  if (len > destroy_count)
    {
      n_windows = g_random_int_range (0, destroy_count);
      i = 0;
      while (i < n_windows)
        {
          GtkWidget *w;
          
          w = g_slist_nth_data (evil_windows,
                                g_random_int_range (0, len));
          if (w)
            {
              --len;
              evil_windows = g_slist_remove (evil_windows, w);
              gtk_widget_destroy (w);
            }
          
          ++i;
        }
    }
  
  return TRUE;
}

static void
set_up_the_evil (void)
{
  g_timeout_add (400, evil_timeout, NULL);
}

static void
set_up_icon_windows (void)
{
  int i;
  int n_windows;

  /* Create some windows */
  n_windows = 9;
  
  i = 0;
  while (i < n_windows)
    {
      GtkWidget *w;
      GtkWidget *c;
      GList *icons;
      GdkPixbuf *pix;
      
      w = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      c = gtk_button_new_with_label ("Icon window");
      gtk_container_add (GTK_CONTAINER (w), c);

      icons = NULL;

      pix = gtk_widget_render_icon (w,
                                    GTK_STOCK_SAVE,
                                    GTK_ICON_SIZE_LARGE_TOOLBAR,
                                    NULL);
      
      icons = g_list_append (icons, pix);

      if (i % 2)
        {
          pix = gtk_widget_render_icon (w,
                                        GTK_STOCK_SAVE,
                                        GTK_ICON_SIZE_DIALOG,
                                        NULL);
          icons = g_list_append (icons, pix);
        }

      if (i % 3)
        {
          pix = gtk_widget_render_icon (w,
                                        GTK_STOCK_SAVE,
                                        GTK_ICON_SIZE_MENU,
                                        NULL);
          icons = g_list_append (icons, pix);
        }

      gtk_window_set_icon_list (GTK_WINDOW (w), icons);

      g_list_foreach (icons, (GFunc) g_object_unref, NULL);
      g_list_free (icons);
      
      gtk_widget_show_all (w);
      
      ++i;
    }
}
