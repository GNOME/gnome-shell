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
#include <unistd.h>

#include <libintl.h>
#define _(x) dgettext (GETTEXT_PACKAGE, x)
#define N_(x) x

#include <gdk/gdkx.h>
#include <X11/Xatom.h>

/* FIXME: When we switch to gtk+-2.6, use of this function should be
 * replaced by using the real gdk_x11_window_set_user_time.
 */
static void
copy_of_gdk_x11_window_set_user_time (GdkWindow *window,
                                      Time       timestamp)
{
  GdkDisplay *display;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  display = gdk_drawable_get_display (window);

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display), GDK_WINDOW_XID (window),
                   gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_USER_TIME"),
                   XA_CARDINAL, 32, PropModeReplace,
                   (guchar *)&timestamp, 1);
}

static Window
window_from_string (const char *str)
{
  char *end;
  unsigned long l;

  end = NULL;
  
  l = strtoul (str, &end, 16);

  if (end == NULL || end == str)
    {
      g_printerr (_("Could not parse \"%s\" as an integer"),
                  str);
      return None;
    }

  if (*end != '\0')
    {
      g_printerr (_("Did not understand trailing characters \"%s\" in string \"%s\""),
                  end, str);
      return None;
    }

  return l;
}

static void
on_realize (GtkWidget *dialog,
            void      *data)
{
  const char *parent_str = data;
  Window xwindow;

  xwindow = window_from_string (parent_str);

  gdk_error_trap_push ();
  XSetTransientForHint (gdk_display, GDK_WINDOW_XID (dialog->window),
                        xwindow);
  XSync (gdk_display, False);
  gdk_error_trap_pop ();
}

static int
kill_window_question (const char *window_name,
                      const char *parent_str,
                      Time        timestamp)
{
  GtkWidget *dialog;
  char *str, *tmp;

  tmp = g_markup_escape_text (window_name, -1);
  str = g_strdup_printf (_("The window \"%s\" is not responding."), tmp);
  g_free (tmp);

  dialog = gtk_message_dialog_new (NULL, 0,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_NONE,
				   "<b>%s</b>\n\n%s",
				   str,
				   _("Forcing this application to quit will "
				     "cause you to lose any unsaved changes."));
  g_free (str);
  
  gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);
  gtk_label_set_line_wrap (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);
  
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_CANCEL,
                          GTK_RESPONSE_REJECT,
                          _("_Force Quit"),
                          GTK_RESPONSE_ACCEPT,
                          NULL);
  
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_REJECT);

  g_signal_connect (G_OBJECT (dialog), "realize",
                    G_CALLBACK (on_realize), (char*) parent_str);
  
  gtk_widget_realize (dialog);
  copy_of_gdk_x11_window_set_user_time (dialog->window, timestamp);

  /* return our PID, then window ID that should be killed */
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    g_print ("%d\n%s\n", (int) getpid (), parent_str);
  else
    g_print ("%d\n0x0\n", (int) getpid ());

  return 0;
}

static char*
latin1_to_utf8 (const char *text)
{
  GString *str;
  const char *p;
  
  str = g_string_new ("");

  p = text;
  while (*p)
    {
      g_string_append_unichar (str, *p);
      ++p;
    }

  return g_string_free (str, FALSE);
}

enum
{
  COLUMN_TITLE,
  COLUMN_CLASS,
  COLUMN_LAST
};

static GtkWidget*
create_lame_apps_list (char **lame_apps)
{
  GtkTreeSelection *selection;
  GtkCellRenderer *cell;
  GtkWidget *tree_view;
  GtkTreeViewColumn *column;
  GtkListStore *model;
  GtkTreeIter iter;
  int i;
  
  model = gtk_list_store_new (COLUMN_LAST,
                              G_TYPE_STRING,
                              G_TYPE_STRING);
  
  tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));

  g_object_unref (G_OBJECT (model));
  
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

  gtk_tree_selection_set_mode (GTK_TREE_SELECTION (selection),
			       GTK_SELECTION_NONE);

  i = 0;
  while (lame_apps[i])
    {
      char *s;
      
      gtk_list_store_append (model, &iter);

      /* window class is latin-1 */
      s = latin1_to_utf8 (lame_apps[i+1]);
      
      gtk_list_store_set (model,
			  &iter,
                          COLUMN_TITLE, lame_apps[i],
                          COLUMN_CLASS, s,
                          -1);

      g_free (s);
      
      i += 2;
    }
  
  cell = gtk_cell_renderer_text_new ();
  
  g_object_set (G_OBJECT (cell),
                "xpad", 2,
                NULL);
  
  column = gtk_tree_view_column_new_with_attributes (_("Title"),
						     cell,
						     "text", COLUMN_TITLE,
						     NULL);

  gtk_tree_view_column_set_sort_column_id (column, COLUMN_TITLE);
  
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),
			       GTK_TREE_VIEW_COLUMN (column));

  cell = gtk_cell_renderer_text_new ();
  
  column = gtk_tree_view_column_new_with_attributes (_("Class"),
						     cell,
						     "text", COLUMN_CLASS,
						     NULL);

  gtk_tree_view_column_set_sort_column_id (column, COLUMN_CLASS);
  
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),
			       GTK_TREE_VIEW_COLUMN (column));

  return tree_view;
}

static int
warn_about_no_sm_support (char **lame_apps,
                          Time   timestamp)
{
  GtkWidget *dialog;
  GtkWidget *list;
  GtkWidget *sw;
      
  dialog = gtk_message_dialog_new (NULL,
                                   0,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_CLOSE,
                                   _("These windows do not support \"save current setup\" and will have to be restarted manually next time you log in."));
  
  g_signal_connect (G_OBJECT (dialog),
                    "response",
                    G_CALLBACK (gtk_main_quit),
                    NULL);

  /* Wait 4 minutes then force quit, so we don't wait around all night */
  g_timeout_add (4 * 60 * 1000, (GSourceFunc) gtk_main_quit, NULL);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);
  list = create_lame_apps_list (lame_apps);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_container_set_border_width (GTK_CONTAINER (sw), 3);
      
  gtk_container_add (GTK_CONTAINER (sw), list);

  /* sw as geometry widget */
  gtk_window_set_geometry_hints (GTK_WINDOW (dialog),
                                 sw, NULL, 0);

  gtk_window_set_resizable (GTK_WINDOW(dialog), TRUE);

  /* applies to geometry widget; try to avoid scrollbars,
   * but don't make the window huge
   */
  gtk_window_set_default_size (GTK_WINDOW (dialog),
                               400, 225);

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                      sw,
                      TRUE, TRUE, 0);
  
  gtk_window_stick (GTK_WINDOW (dialog));

  gtk_widget_realize (dialog);
  copy_of_gdk_x11_window_set_user_time (dialog->window, timestamp);

  gtk_widget_show_all (dialog);

  gtk_main ();
  
  return 0;
}

static int
error_about_command (const char *gconf_key,
                     const char *command,
                     const char *error,
                     Time        timestamp)
{
  GtkWidget *dialog;

  /* FIXME offer to change the value of the command's gconf key */

  if (*command != '\0')
    dialog = gtk_message_dialog_new (NULL, 0,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     _("There was an error running \"%s\":\n"
                                       "%s."),
                                     command, error);
  else
    dialog = gtk_message_dialog_new (NULL, 0,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     "%s", error);
  
  gtk_widget_realize (dialog);
  copy_of_gdk_x11_window_set_user_time (dialog->window, timestamp);

  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);
  
  return 0;
}

int
main (int argc, char **argv)
{
  Time timestamp;

  bindtextdomain (GETTEXT_PACKAGE, METACITY_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gtk_init (&argc, &argv);
    
  if (argc < 4)
    {
      g_printerr ("bad args to metacity-dialog\n");
      return 1;
    }

  if (strcmp (argv[1], "--timestamp") != 0)
    {
      g_printerr ("bad args to metacity-dialog\n");
      return 1;
    }
  timestamp = strtoul (argv[2], NULL, 10);
  
  if (strcmp (argv[3], "--kill-window-question") == 0)
    {
      if (argc < 6)
        {
          g_printerr ("bad args to metacity-dialog\n");
          return 1;
        }

      return kill_window_question (argv[4], argv[5], timestamp);
    }
  else if (strcmp (argv[3], "--warn-about-no-sm-support") == 0)
    {
      /* argc must be even because we want title-class pairs */
      if (argc < 5 || (argc % 2) != 0)
        {
          g_printerr ("bad args to metacity-dialog\n");
          return 1;
        }

      return warn_about_no_sm_support (&argv[4], timestamp);
    }
  else if (strcmp (argv[3], "--command-failed-error") == 0)
    {

      /* the args are the gconf key of the failed command, the text of
       * the command, and the error message
       */
      if (argc != 7)
        {
          g_printerr ("bad args to metacity-dialog\n");
          return 1;
        }

      return error_about_command (argv[4], argv[5], argv[6], timestamp);
    }
  
  g_printerr ("bad args to metacity-dialog\n");
  return 1;
} 

