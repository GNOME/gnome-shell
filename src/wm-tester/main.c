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

static void
usage (void)
{
  g_print ("wm-tester [--evil]\n");
  exit (0);
}

int
main (int argc, char **argv)
{
  int i;
  gboolean do_evil;

  gtk_init (&argc, &argv);
  
  do_evil = FALSE;
  
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
      else
        usage ();
      
      ++i;
    }

  /* Be sure some option was provided */
  if (! (do_evil))
    return 1;
  
  if (do_evil)
    set_up_the_evil ();
    
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
      
      w = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      c = gtk_button_new_with_label ("Evil Window!");
      gtk_container_add (GTK_CONTAINER (w), c);

      gtk_widget_set_uposition (w,
                                g_random_int_range (0,
                                                    gdk_screen_width ()),
                                g_random_int_range (0,
                                                    gdk_screen_height ()));
      
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
  g_timeout_add (40, evil_timeout, NULL);
}

