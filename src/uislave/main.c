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
#include "messagequeue.h"
#include "fixedtip.h"
#include "main.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <gtk/gtk.h>

static void message_callback (MetaMessageQueue *mq,
                              MetaMessage      *message,
                              gpointer          data);

int
main (int argc, char **argv)
{
  MetaMessageQueue *mq;
  
  /* report our nature to the window manager */
  meta_message_send_check ();
  
  gtk_init (&argc, &argv);

  mq = meta_message_queue_new (0, message_callback, NULL);
  
  gtk_main ();
  
  return 0;
}

static void
message_callback (MetaMessageQueue *mq,
                  MetaMessage      *message,
                  gpointer          data)
{
  switch (message->header.message_code)
    {
    case MetaMessageShowTipCode:
      meta_fixed_tip_show (message->show_tip.root_x,
                           message->show_tip.root_y,
                           message->show_tip.markup);
      break;

    case MetaMessageHideTipCode:
      meta_fixed_tip_hide ();
      break;
      
    default:
      meta_ui_warning ("Unhandled message code %d\n",
                       message->header.message_code);
      break;
    }
}

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

  
#if 0
{
  int i;
  /* Try breaking message queue system. */
  i = 0;
  while (i < 1500)
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
}
#endif

