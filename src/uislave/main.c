/* Metacity UI slave main() */

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

#include "messages.h"

#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>

void
meta_ui_warning (const char *format, ...)
{
  va_list args;
  gchar *str;
  
  g_return_if_fail (format != NULL);
  
  va_start (args, format);
  str = g_strdup_vprintf (format, args);
  va_end (args);
  
  fputs (str, stderr);
  
  g_free (str);
}

int
main (int argc, char **argv)
{
  int i;

  /* report our nature to the window manager */
  meta_message_send_check ();

#if 1
  /* Try breaking message queue system. */
  i = 0;
  while (i < 100)
    {
      meta_message_send_check ();
      if (g_random_boolean ())
        {
          int j;
          if (g_random_boolean ())
            j = g_random_int_range (0, 15);
          else
            j = g_random_int_range (0, 1000);
          while (j > 0)
            {
              char b;
              b = g_random_int_range (0, 256);
              
              write (1, &b, 1);
              --j;
            }
        }
      
      ++i;
    }
#endif
  
  gtk_init (&argc, &argv);
  
  gtk_main ();
  
  return 0;
}







