/* Metacity dialog process */

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
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

#include <libintl.h>
#define _(x) dgettext (GETTEXT_PACKAGE, x)
#define N_(x) x

static int
kill_window_question (const char *window_name)
{
  GtkWidget *dialog;
  
  dialog = gtk_message_dialog_new (NULL, 0,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("The window \"%s\" is not responding.\n"
                                     "Force this application to exit?"),
                                   window_name);
  
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_CANCEL,
                          GTK_RESPONSE_REJECT,
                          _("Kill application"),
                          GTK_RESPONSE_ACCEPT,
                          NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_REJECT);

  /* return 0 if we should kill the application */
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    g_print ("Y");
  else
    g_print ("N");

  return 0;
}

int
main (int argc, char **argv)
{
  bindtextdomain (GETTEXT_PACKAGE, METACITY_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gtk_init (&argc, &argv);
    
  if (argc < 2)
    {
      g_printerr ("bad args to metacity-dialog\n");
      return 1;
    }
  
  if (strcmp (argv[1], "--kill-window-question") == 0)
    {
      if (argc < 3)
        {
          g_printerr ("bad args to metacity-dialog\n");
          return 1;
        }

      return kill_window_question (argv[2]);
    }

  g_printerr ("bad args to metacity-dialog\n");
  return 1;
} 

