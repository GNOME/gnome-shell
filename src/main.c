/* Metacity main() */

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
#include "main.h"
#include "util.h"
#include "display.h"
#include "errors.h"
#include "ui.h"
#include "session.h"
#include "prefs.h"

#include <glib-object.h>

#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

static MetaExitCode meta_exit_code = META_EXIT_SUCCESS;
static GMainLoop *meta_main_loop = NULL;
static gboolean meta_restart_after_quit = FALSE;

static void prefs_changed_callback (MetaPreference pref,
                                    gpointer       data);

static void
log_handler (const gchar   *log_domain,
             GLogLevelFlags log_level,
             const gchar   *message,
             gpointer       user_data)
{
  meta_warning ("Log level %d: %s\n", log_level, message);
}

static void
usage (void)
{
  g_print ("metacity [--disable-sm] [--sm-client-id=ID] [--display=DISPLAY]\n");
  exit (1);
}

int
main (int argc, char **argv)
{
  struct sigaction act;
  sigset_t empty_mask;
  char *display_name;
  int i;
  const char *client_id;
  gboolean disable_sm;
  const char *prev_arg;
  
  sigemptyset (&empty_mask);
  act.sa_handler = SIG_IGN;
  act.sa_mask    = empty_mask;
  act.sa_flags   = 0;
  sigaction (SIGPIPE,  &act, 0);
  
  g_set_prgname (argv[0]);

  bindtextdomain (GETTEXT_PACKAGE, METACITY_LOCALEDIR);
  
  meta_set_verbose (TRUE);
  meta_set_debugging (TRUE);
  meta_set_syncing (g_getenv ("METACITY_SYNC") != NULL);

  /* Parse options lamely */

  display_name = NULL;
  client_id = NULL;
  disable_sm = FALSE;
  prev_arg = NULL;
  i = 1;
  while (i < argc)
    {
      const char *arg = argv[i];
      
      if (strcmp (arg, "--help") == 0 ||
          strcmp (arg, "-h") == 0 ||
          strcmp (arg, "-?") == 0)
        usage ();
      else if (strcmp (arg, "--sm-disable") == 0)
        disable_sm = TRUE; 
      else if (strstr (arg, "--display=") == arg)
        {
          const char *disp;

          if (display_name != NULL)
            meta_fatal ("Can't specify display twice\n");
          
          disp = strchr (arg, '=');
          ++disp;
          
          display_name =
            g_strconcat ("DISPLAY=", disp, NULL);
        }
      else if (prev_arg &&
               strcmp (prev_arg, "--display") == 0)
        {
          if (display_name != NULL)
            meta_fatal ("Can't specify display twice\n");

          display_name = g_strconcat ("DISPLAY=", arg, NULL);
        }
      else if (strcmp (arg, "--display") == 0)
        ; /* wait for next arg */
      else if (strstr (arg, "--sm-client-id=") == arg)
        {
          const char *id;

          if (client_id)
            meta_fatal ("Can't specify client ID twice\n");
          
          id = strchr (arg, '=');
          ++id;

          client_id = g_strdup (id);
        }
      else if (prev_arg &&
               strcmp (prev_arg, "--sm-client-id") == 0)
        {
          if (client_id)
            meta_fatal ("Can't specify client ID twice\n");

          client_id = g_strdup (arg);
        }
      else if (strcmp (arg, "--sm-client-id") == 0)
        ; /* wait for next arg */
      else
        usage ();
      
      prev_arg = arg;
      
      ++i;
    }
    
  meta_main_loop = g_main_loop_new (NULL, FALSE);
  
  if (display_name == NULL &&
      g_getenv ("METACITY_DISPLAY"))
    {
      meta_verbose ("Using METACITY_DISPLAY %s\n",
                    g_getenv ("METACITY_DISPLAY"));
      display_name =
        g_strconcat ("DISPLAY=", g_getenv ("METACITY_DISPLAY"), NULL);
    }

  if (display_name)
    {
      putenv (display_name);
      /* DO NOT FREE display_name, putenv() sucks */
    }  
  
  g_type_init ();

  /* Load prefs */
  meta_prefs_init ();
  meta_prefs_add_listener (prefs_changed_callback, NULL);
  
  meta_ui_init (&argc, &argv);  

  /* must be after UI init so we can override GDK handlers */
  meta_errors_init ();

#if 0
  g_log_set_handler (NULL,
                     G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
                     log_handler, NULL);
  g_log_set_handler ("Gtk",
                     G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
                     log_handler, NULL);
  g_log_set_handler ("Gdk",
                     G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
                     log_handler, NULL);
  g_log_set_handler ("GLib",
                     G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
                     log_handler, NULL);
  g_log_set_handler ("Pango",
                     G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
                     log_handler, NULL);
  g_log_set_handler ("GLib-GObject",
                     G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
                     log_handler, NULL);
  g_log_set_handler ("GThread",
                     G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
                     log_handler, NULL);

  if (meta_is_debugging ())
    g_log_set_always_fatal (G_LOG_LEVEL_MASK);
#endif
  
  meta_ui_set_current_theme (meta_prefs_get_theme (), FALSE);

  /* Try some panic stuff, this is lame but we really
   * don't want users to lose their WM :-/
   */
  if (!meta_ui_have_a_theme ())
    meta_ui_set_current_theme ("Atlanta", FALSE);

  if (!meta_ui_have_a_theme ())
    meta_ui_set_current_theme ("Crux", FALSE);

  if (!meta_ui_have_a_theme ())
    meta_fatal (_("Could not find a theme! Be sure %s exits and contains the usual themes."),
                METACITY_PKGDATADIR"/themes");
  
  /* Connect to SM as late as possible - but before managing display,
   * or we might try to manage a window before we have the session
   * info
   */
  if (!disable_sm)
    meta_session_init (client_id); /* client_id == NULL is fine */
  
  if (!meta_display_open (NULL))
    meta_exit (META_EXIT_ERROR);
  
  g_main_run (meta_main_loop);

  {
    GSList *displays;
    GSList *tmp;

    /* we need a copy since closing the display removes it
     * from the list
     */
    displays = g_slist_copy (meta_displays_list ());
    tmp = displays;
    while (tmp != NULL)
      {
        meta_display_close (tmp->data);
        tmp = tmp->next;
      }
    g_slist_free (displays);
  }

  if (meta_restart_after_quit)
    {
      GError *err;

      err = NULL;
      if (!g_spawn_async (NULL,
                          argv,
                          NULL,
                          G_SPAWN_SEARCH_PATH,
                          NULL,
                          NULL,
                          NULL,
                          &err))
        {
          meta_fatal (_("Failed to restart: %s\n"),
                      err->message);
          g_error_free (err); /* not reached anyhow */
          meta_exit_code = META_EXIT_ERROR;
        }
    }
  
  return meta_exit_code;
}

GMainLoop*
meta_get_main_loop (void)
{
  return meta_main_loop;
}

void
meta_quit (MetaExitCode code)
{
  meta_exit_code = code;

  if (g_main_is_running (meta_main_loop))
    g_main_quit (meta_main_loop);
}

void
meta_restart (void)
{
  meta_restart_after_quit = TRUE;
  meta_quit (META_EXIT_SUCCESS);
}

static void
prefs_changed_callback (MetaPreference pref,
                        gpointer       data)
{
  switch (pref)
    {
    case META_PREF_THEME:
      meta_ui_set_current_theme (meta_prefs_get_theme (), FALSE);
      meta_display_retheme_all ();
      break;

    default:
      /* handled elsewhere or otherwise */
      break;
    }
}
