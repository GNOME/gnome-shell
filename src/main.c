/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity main() */

/* 
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2006 Elijah Newren
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
#include <locale.h>
#include <time.h>

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
  meta_print_backtrace ();
}

static void
version (void)
{
  g_print (_("metacity %s\n"
             "Copyright (C) 2001-2007 Havoc Pennington, Red Hat, Inc., and others\n"
             "This is free software; see the source for copying conditions.\n"
             "There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"),
           VERSION);
  exit (0);
}

static void
meta_print_compilation_info (void)
{
#ifdef HAVE_SHAPE
  meta_verbose ("Compiled with shape extension\n");
#else
  meta_verbose ("Compiled without shape extension\n");
#endif
#ifdef HAVE_XINERAMA
  meta_topic (META_DEBUG_XINERAMA, "Compiled with Xinerama extension\n");
#else
  meta_topic (META_DEBUG_XINERAMA, "Compiled without Xinerama extension\n");
#endif
#ifdef HAVE_XFREE_XINERAMA
  meta_topic (META_DEBUG_XINERAMA, " (using XFree86 Xinerama)\n");
#else
  meta_topic (META_DEBUG_XINERAMA, " (not using XFree86 Xinerama)\n");
#endif
#ifdef HAVE_SOLARIS_XINERAMA
  meta_topic (META_DEBUG_XINERAMA, " (using Solaris Xinerama)\n");
#else
  meta_topic (META_DEBUG_XINERAMA, " (not using Solaris Xinerama)\n");
#endif
#ifdef HAVE_XSYNC
  meta_verbose ("Compiled with sync extension\n");
#else
  meta_verbose ("Compiled without sync extension\n");
#endif
#ifdef HAVE_RANDR
  meta_verbose ("Compiled with randr extension\n");
#else
  meta_verbose ("Compiled without randr extension\n");
#endif
#ifdef HAVE_STARTUP_NOTIFICATION
  meta_verbose ("Compiled with startup notification\n");
#else
  meta_verbose ("Compiled without startup notification\n");
#endif
#ifdef HAVE_COMPOSITE_EXTENSIONS
  meta_verbose ("Compiled with composite extensions\n");
#else
  meta_verbose ("Compiled without composite extensions\n");
#endif
}

static void
meta_print_self_identity (void)
{
  char buf[256];
  GDate d;
  const char *charset;

  /* Version and current date. */
  g_date_clear (&d, 1);
  g_date_set_time_t (&d, time (NULL));
  g_date_strftime (buf, sizeof (buf), "%x", &d);
  meta_verbose ("Metacity version %s running on %s\n",
    VERSION, buf);
  
  /* Locale and encoding. */
  g_get_charset (&charset);
  meta_verbose ("Running in locale \"%s\" with encoding \"%s\"\n",
    setlocale (LC_ALL, NULL), charset);

  /* Compilation settings. */
  meta_print_compilation_info ();
}

typedef struct
{
  gchar *save_file;
  gchar *display_name;
  gchar *client_id;
  gboolean replace_wm;
  gboolean disable_sm;
  gboolean print_version;
} MetaArguments;

/**
 * meta_parse_options() parses argc and argv and returns the
 * arguments that Metacity understands in struct
 * MetaArguments. In meta_args.
 *
 * The strange call signature has to be written like it is so
 * that g_option_context_parse() gets a chance to modify argc and
 * argv.
 **/
static void
meta_parse_options (int *argc, char ***argv,
                    MetaArguments *meta_args)
{
  MetaArguments my_args = {NULL, NULL, NULL, FALSE, FALSE, FALSE};
  GOptionEntry options[] = {
    {
      "sm-disable", 0, 0, G_OPTION_ARG_NONE,
      &my_args.disable_sm,
      N_("Disable connection to session manager"),
      NULL
    },
    {
      "replace", 0, 0, G_OPTION_ARG_NONE,
      &my_args.replace_wm,
      N_("Replace the running window manager with Metacity"),
      NULL
    },
    {
      "sm-client-id", 0, 0, G_OPTION_ARG_STRING,
      &my_args.client_id,
      N_("Specify session management ID"),
      "ID"
    },
    {
      "display", 0, 0, G_OPTION_ARG_STRING,
      &my_args.display_name, N_("X Display to use"),
      "DISPLAY"
    },
    {
      "sm-save-file", 0, 0, G_OPTION_ARG_FILENAME,
      &my_args.save_file,
      N_("Initialize session from savefile"),
      "FILE"
    },
    {
      "version", 0, 0, G_OPTION_ARG_NONE,
      &my_args.print_version,
      N_("Print version"),
      NULL
    },
    {NULL}
  };
  GOptionContext *ctx;
  GError *error = NULL;

  ctx = g_option_context_new (NULL);
  g_option_context_add_main_entries (ctx, options, "metacity");
  if (!g_option_context_parse (ctx, argc, argv, &error))
    {
      g_print ("metacity: %s\n", error->message);
      exit(1);
    }
  g_option_context_free (ctx);
  /* Return the parsed options through the meta_args param. */
  *meta_args = my_args;
}

/**
 * meta_select_display() is a helper function that selects
 * which display Metacity should use. It first tries to use
 * display_name as the display. If display_name is NULL then
 * try to use the environment variable METACITY_DISPLAY. If that
 * also is NULL, use the default - :0.0
 */
static
void meta_select_display (gchar *display_name)
{
  gchar *envVar = "";
  if (display_name)
    envVar = g_strconcat ("DISPLAY=", display_name, NULL);
  else if (g_getenv ("METACITY_DISPLAY"))
    envVar = g_strconcat ("DISPLAY=",
      g_getenv ("METACITY_DISPLAY"), NULL);
  /* DO NOT FREE envVar, putenv() sucks */
  putenv (envVar);
}
    
int
main (int argc, char **argv)
{
  struct sigaction act;
  sigset_t empty_mask;
  MetaArguments meta_args;

  if (setlocale (LC_ALL, "") == NULL)
    meta_warning ("Locale not understood by C library, internationalization will not work\n");
  
  sigemptyset (&empty_mask);
  act.sa_handler = SIG_IGN;
  act.sa_mask    = empty_mask;
  act.sa_flags   = 0;
  if (sigaction (SIGPIPE,  &act, NULL) < 0)
    g_printerr ("Failed to register SIGPIPE handler: %s\n",
                g_strerror (errno));
#ifdef SIGXFSZ
  if (sigaction (SIGXFSZ,  &act, NULL) < 0)
    g_printerr ("Failed to register SIGXFSZ handler: %s\n",
                g_strerror (errno));
#endif

  if (g_getenv ("METACITY_VERBOSE"))
    meta_set_verbose (TRUE);
  if (g_getenv ("METACITY_DEBUG"))
    meta_set_debugging (TRUE);
  meta_set_syncing (g_getenv ("METACITY_SYNC") != NULL);

  if (g_get_home_dir ())
    chdir (g_get_home_dir ());

  meta_print_self_identity ();
  
  bindtextdomain (GETTEXT_PACKAGE, METACITY_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  /* Parse command line arguments.*/
  meta_parse_options (&argc, &argv, &meta_args);

  if (meta_args.print_version)
    version ();

  meta_select_display (meta_args.display_name);
  
  if (meta_args.replace_wm)
    meta_set_replace_current_wm (TRUE);

  if (meta_args.save_file && meta_args.client_id)
    meta_fatal ("Can't specify both SM save file and SM client id\n");
  
  meta_main_loop = g_main_loop_new (NULL, FALSE);
  
  g_type_init ();

  meta_ui_init (&argc, &argv);  

  /* must be after UI init so we can override GDK handlers */
  meta_errors_init ();

  /* Load prefs */
  meta_prefs_init ();
  meta_prefs_add_listener (prefs_changed_callback, NULL);


#if 1
  g_log_set_handler (NULL,
                     G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
                     log_handler, NULL);
  g_log_set_handler (G_LOG_DOMAIN,
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
#endif

  if (g_getenv ("METACITY_G_FATAL_WARNINGS") != NULL)
    g_log_set_always_fatal (G_LOG_LEVEL_MASK);
  
  meta_ui_set_current_theme (meta_prefs_get_theme (), FALSE);

  /* Try to find some theme that'll work if the theme preference
   * doesn't exist.  First try Simple (the default theme) then just
   * try anything in the themes directory.
   */
  if (!meta_ui_have_a_theme ())
    meta_ui_set_current_theme ("Simple", FALSE);
  
  if (!meta_ui_have_a_theme ())
    {
      const char *dir_entry = NULL;
      GError *err = NULL;
      GDir   *themes_dir = NULL;
      
      if (!(themes_dir = g_dir_open (METACITY_DATADIR"/themes", 0, &err)))
        {
          meta_fatal (_("Failed to scan themes directory: %s\n"), err->message);
          g_error_free (err);
        } 
      else 
        {
          while (((dir_entry = g_dir_read_name (themes_dir)) != NULL) && 
                 (!meta_ui_have_a_theme ()))
            {
              meta_ui_set_current_theme (dir_entry, FALSE);
            }
          
          g_dir_close (themes_dir);
        }
    }
  
  if (!meta_ui_have_a_theme ())
    meta_fatal (_("Could not find a theme! Be sure %s exists and contains the usual themes.\n"),
                METACITY_DATADIR"/themes");
  
  /* Connect to SM as late as possible - but before managing display,
   * or we might try to manage a window before we have the session
   * info
   */
  if (!meta_args.disable_sm)
    meta_session_init (meta_args.client_id, meta_args.save_file);

  /* Free memory possibly allocated by the argument parsing which are
   * no longer needed.
   */
  if (meta_args.save_file)
    g_free (meta_args.save_file);
  if (meta_args.display_name)
    g_free (meta_args.display_name);
  if (meta_args.client_id)
    g_free (meta_args.client_id);
  
  if (!meta_display_open ())
    meta_exit (META_EXIT_ERROR);
  
  g_main_loop_run (meta_main_loop);

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
        guint32 timestamp;
        timestamp = CurrentTime; /* I doubt correct timestamps matter here */
        meta_display_close (tmp->data, timestamp);
        tmp = tmp->next;
      }
    g_slist_free (displays);
  }

  meta_session_shutdown ();
  
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

  if (g_main_loop_is_running (meta_main_loop))
    g_main_loop_quit (meta_main_loop);
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

    case META_PREF_CURSOR_THEME:
    case META_PREF_CURSOR_SIZE:
      meta_display_set_cursor_theme (meta_prefs_get_cursor_theme (),
				     meta_prefs_get_cursor_size ());
      break;
    default:
      /* handled elsewhere or otherwise */
      break;
    }
}
