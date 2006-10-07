/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity dialog process */

/* 
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2004 Elijah Newren
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
                      guint32     timestamp)
{
  GtkWidget *dialog;
  char *str, *tmp;

  tmp = g_markup_escape_text (window_name, -1);
  str = g_strdup_printf (_("\"%s\" is not responding."), tmp);
  g_free (tmp);
  dialog = gtk_message_dialog_new (NULL, 0,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_NONE,
                                   "<big><b>%s</b></big>\n\n<i>%s</i>",
                                   str,
                                   _("You may choose to wait a short while"
                                   "for it to continue or force the application"
                                   "to quit entirely."));
  g_free (str);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "panel-force-quit");

  gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);
  gtk_label_set_line_wrap (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);
  
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("_Wait"),
                          GTK_RESPONSE_REJECT,
                          _("_Force Quit"),
                          GTK_RESPONSE_ACCEPT,
                          NULL);
  
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_REJECT);

  g_signal_connect (G_OBJECT (dialog), "realize",
                    G_CALLBACK (on_realize), (char*) parent_str);
  
  gtk_widget_realize (dialog);
  gdk_x11_window_set_user_time (dialog->window, timestamp);

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
warn_about_no_sm_support (char    **lame_apps,
                          guint32   timestamp)
{
  GtkWidget *dialog;
  GtkWidget *list;
  GtkWidget *sw;
  GtkWidget *button;
      
  dialog = gtk_message_dialog_new (NULL,
                                   0,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_NONE,
                                   _("These windows do not support \"save current setup\" and will have to be restarted manually next time you log in."));
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "stock_dialog-warning");
  
  g_signal_connect (G_OBJECT (dialog),
                    "response",
                    G_CALLBACK (gtk_main_quit),
                    NULL);

  /* Wait 4 minutes then force quit, so we don't wait around all night */
  g_timeout_add (4 * 60 * 1000, (GSourceFunc) gtk_main_quit, NULL);

  button = gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
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
  gdk_x11_window_set_user_time (dialog->window, timestamp);

  gtk_widget_grab_focus (button);
  gtk_widget_show_all (dialog);

  gtk_main ();
  
  return 0;
}

static int
error_about_command (const char *gconf_key,
                     const char *command,
                     const char *error,
                     guint32     timestamp)
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
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "stock_dialog-error");
  
  gtk_widget_realize (dialog);
  gdk_x11_window_set_user_time (dialog->window, timestamp);

  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);
  
  return 0;
}

static gchar *screen = NULL;
static gchar *timestamp_string = NULL;
static gboolean isset_kill_window_question = FALSE;
static gboolean isset_warn_about_no_sm_support = FALSE;
static gboolean isset_command_failed_error = FALSE;
static gchar **remaining_args;

static const GOptionEntry options[] = {
  { "screen", 0, 0, G_OPTION_ARG_STRING, &screen, NULL, NULL},
  { "timestamp", 0, 0, G_OPTION_ARG_STRING, &timestamp_string, NULL, NULL},
  { "kill-window-question", 'k', 0, G_OPTION_ARG_NONE, 
    &isset_kill_window_question, NULL, NULL},
  { "warn-about-no-sm-support", 'w', 0, G_OPTION_ARG_NONE, 
    &isset_warn_about_no_sm_support, NULL, NULL},
  { "command-failed-error", 'c', 0, G_OPTION_ARG_NONE, 
    &isset_command_failed_error, NULL, NULL},
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, 
    &remaining_args, NULL, NULL},
  { NULL}
};

int
main (int argc, char **argv)
{
  GOptionContext *ctx;
  guint32 timestamp = 0;
  gint num_args = 0;

  bindtextdomain (GETTEXT_PACKAGE, METACITY_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gtk_init (&argc, &argv);

  ctx = g_option_context_new ("- Dialogs for metacity. "
                          "This program is intented for use by metacity only.");
  g_option_context_add_main_entries (ctx, options, GETTEXT_PACKAGE);
  g_option_context_parse (ctx, &argc, &argv, NULL);
  g_option_context_free (ctx);
    
  if (timestamp_string != NULL)
    {
      timestamp = strtoul (timestamp_string, NULL, 10);
    }

  if (remaining_args != NULL)
    {
      num_args = g_strv_length (remaining_args);
    }

  if ((isset_kill_window_question && isset_warn_about_no_sm_support) ||
      (isset_kill_window_question && isset_command_failed_error) ||
      (isset_warn_about_no_sm_support && isset_command_failed_error) ||
      timestamp == 0) 
    {
      g_printerr ("bad args to metacity-dialog\n");
      return 1;
    }

  else if (isset_kill_window_question)
    {
      if (num_args < 2)
        {
          g_printerr ("bad args to metacity-dialog\n");
          return 1;
        } 
      else 
        {
          return kill_window_question (remaining_args[0], 
                          remaining_args[1], timestamp);
        }
    }

  else if (isset_warn_about_no_sm_support)
    {
      /* argc must be even because we want title-class pairs */
      if (num_args == 0 || (num_args % 2) != 0)
        {
          g_printerr ("bad args to metacity-dialog\n");
          return 1;
        } 
      else 
        {
          return warn_about_no_sm_support (&remaining_args[0], timestamp);
        }
    }

  else if (isset_command_failed_error)
    {
      /* the args are the gconf key of the failed command, the text of
       * the command, and the error message
       */
      if (num_args != 3)
        {
          g_printerr ("bad args to metacity-dialog\n");
          return 1;
        } 
      else 
        {
          return error_about_command (remaining_args[0], 
                          remaining_args[1], remaining_args[2], timestamp);
        }
    }
  else
    {
      g_printerr ("bad args to metacity-dialog\n");
      return 1;
    }
}
