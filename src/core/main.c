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

/**
 * \file 
 * Program startup.
 * Functions which parse the command-line arguments, create the display,
 * kick everything off and then close down Metacity when it's time to go.
 */

/**
 * \mainpage
 * Metacity - a boring window manager for the adult in you
 *
 * Many window managers are like Marshmallow Froot Loops; Metacity
 * is like Cheerios.
 *
 * The best way to get a handle on how the whole system fits together
 * is discussed in doc/code-overview.txt; if you're looking for functions
 * to investigate, read main(), meta_display_open(), and event_callback().
 */

#define _GNU_SOURCE
#define _SVID_SOURCE /* for putenv() and some signal-related functions */

#include <config.h>
#include "main.h"
#include "util.h"
#include "display-private.h"
#include "errors.h"
#include "ui.h"
#include "session.h"
#include "prefs.h"
#include "compositor.h"

#include <glib-object.h>
#include <gdk/gdkx.h>

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

#ifdef WITH_CLUTTER
#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>
#endif

/**
 * The exit code we'll return to our parent process when we eventually die.
 */
static MetaExitCode meta_exit_code = META_EXIT_SUCCESS;

/**
 * Handle on the main loop, so that we have an easy way of shutting Metacity
 * down.
 */
static GMainLoop *meta_main_loop = NULL;

/**
 * If set, Metacity will spawn an identical copy of itself immediately
 * before quitting.
 */
static gboolean meta_restart_after_quit = FALSE;

static void prefs_changed_callback (MetaPreference pref,
                                    gpointer       data);

/**
 * Prints log messages. If Metacity was compiled with backtrace support,
 * also prints a backtrace (see meta_print_backtrace()).
 *
 * \param log_domain  the domain the error occurred in (we ignore this)
 * \param log_level   the log level so that we can filter out less
 *                    important messages
 * \param message     the message to log
 * \param user_data   arbitrary data (we ignore this)
 */
static void
log_handler (const gchar   *log_domain,
             GLogLevelFlags log_level,
             const gchar   *message,
             gpointer       user_data)
{
  meta_warning ("Log level %d: %s\n", log_level, message);
  meta_print_backtrace ();
}

/**
 * Prints the version notice. This is shown when Metacity is called
 * with the --version switch.
 */
static void
version (void)
{
  g_print (_("metacity %s\n"
             "Copyright (C) 2001-2008 Havoc Pennington, Red Hat, Inc., and others\n"
             "This is free software; see the source for copying conditions.\n"
             "There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"),
           VERSION);
  exit (0);
}

/**
 * Prints a list of which configure script options were used to
 * build this copy of Metacity. This is actually always called
 * on startup, but it's all no-op unless we're in verbose mode
 * (see meta_set_verbose).
 */
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

/**
 * Prints the version number, the current timestamp (not the
 * build date), the locale, the character encoding, and a list
 * of configure script options that were used to build this
 * copy of Metacity. This is actually always called
 * on startup, but it's all no-op unless we're in verbose mode
 * (see meta_set_verbose).
 */
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

/**
 * The set of possible options that can be set on Metacity's
 * command line. This type exists so that meta_parse_options() can
 * write to an instance of it.
 */
typedef struct
{
  gchar *save_file;
  gchar *display_name;
  gchar *client_id;
  gchar *mutter_plugins;
  gboolean replace_wm;
  gboolean disable_sm;
  gboolean print_version;
  gboolean sync;
  gboolean composite;
  gboolean no_composite;
} MetaArguments;

#ifdef HAVE_COMPOSITE_EXTENSIONS
#define COMPOSITE_OPTS_FLAGS 0
#else /* HAVE_COMPOSITE_EXTENSIONS */
/* No compositor, so don't show the arguments in --help */
#define COMPOSITE_OPTS_FLAGS G_OPTION_FLAG_HIDDEN
#endif /* HAVE_COMPOSITE_EXTENSIONS */

/**
 * Parses argc and argv and returns the
 * arguments that Metacity understands in meta_args.
 *
 * The strange call signature has to be written like it is so
 * that g_option_context_parse() gets a chance to modify argc and
 * argv.
 *
 * \param argc  Pointer to the number of arguments Metacity was given
 * \param argv  Pointer to the array of arguments Metacity was given
 * \param meta_args  The result of parsing the arguments.
 **/
static GOptionContext *
meta_parse_options (int *argc, char ***argv,
                    MetaArguments *meta_args)
{
  MetaArguments my_args = {NULL, NULL, NULL, NULL,
                           FALSE, FALSE, FALSE, FALSE, FALSE};
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
      "display", 'd', 0, G_OPTION_ARG_STRING,
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
    {
      "sync", 0, 0, G_OPTION_ARG_NONE,
      &my_args.sync,
      N_("Make X calls synchronous"),
      NULL
    },
    {
      "composite", 'c', COMPOSITE_OPTS_FLAGS, G_OPTION_ARG_NONE,
      &my_args.composite,
      N_("Turn compositing on"),
      NULL
    },
    {
      "no-composite", 0, COMPOSITE_OPTS_FLAGS, G_OPTION_ARG_NONE,
      &my_args.no_composite,
      N_("Turn compositing off"),
      NULL
    },
#ifdef WITH_CLUTTER
    {
      "mutter-plugins", 0, 0, G_OPTION_ARG_STRING,
      &my_args.mutter_plugins,
      N_("Comma-separated list of compositor plugins"),
      "PLUGINS"
    },
#endif
    {NULL}
  };
  GOptionContext *ctx;
  GError *error = NULL;

  ctx = g_option_context_new (NULL);
  g_option_context_add_main_entries (ctx, options, "metacity");
  
#ifdef WITH_CLUTTER
  /*
   * This function is only available in clutter >= 0.8.2
   */
#if CLUTTER_CHECK_VERSION(0,8,2)
  g_option_context_add_group (ctx, clutter_get_option_group_without_init ());
#endif
#endif

  if (!g_option_context_parse (ctx, argc, argv, &error))
    {
      g_print ("metacity: %s\n", error->message);
      exit(1);
    }

  /* Return the parsed options through the meta_args param. */
  *meta_args = my_args;
  return ctx;
}


#ifdef WITH_CLUTTER
static void
meta_clutter_init (GOptionContext *ctx, int *argc, char ***argv)
{
  clutter_x11_set_display (gdk_display);
  clutter_x11_disable_event_retrieval ();
  
  if (CLUTTER_INIT_SUCCESS == clutter_init (argc, argv))
    {
      meta_compositor_can_use_clutter__ = 1;
    }
  else
    {
      g_message ("Unable to initialize Clutter.\n");
      meta_compositor_can_use_clutter__ = 0;
    }
}
#endif

/**
 * Selects which display Metacity should use. It first tries to use
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
    
/**
 * This is where the story begins. It parses commandline options and
 * environment variables, sets up the screen, hands control off to
 * GTK, and cleans up afterwards.
 *
 * \param argc Number of arguments (as usual)
 * \param argv Array of arguments (as usual)
 *
 * \bug It's a bit long. It would be good to split it out into separate
 * functions.
 */
int
main (int argc, char **argv)
{
  struct sigaction act;
  sigset_t empty_mask;
  MetaArguments meta_args;
  const gchar *log_domains[] = {
    NULL, G_LOG_DOMAIN, "Gtk", "Gdk", "GLib",
    "Pango", "GLib-GObject", "GThread"
  };
  guint i;
  GOptionContext *ctx;
  
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

  if (g_get_home_dir ())
    chdir (g_get_home_dir ());

  meta_print_self_identity ();
  
  bindtextdomain (GETTEXT_PACKAGE, METACITY_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  /* Parse command line arguments.*/
  ctx = meta_parse_options (&argc, &argv, &meta_args);

  meta_set_syncing (meta_args.sync || (g_getenv ("METACITY_SYNC") != NULL));

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

#ifdef WITH_CLUTTER
  /*
   * Clutter can only be initialized after the UI.
   */
  meta_clutter_init (ctx, &argc, &argv);
#endif

  g_option_context_free (ctx);

  /* must be after UI init so we can override GDK handlers */
  meta_errors_init ();

  /* Load prefs */
  meta_prefs_init ();
  meta_prefs_add_listener (prefs_changed_callback, NULL);


#if 1

  for (i=0; i<G_N_ELEMENTS(log_domains); i++)
    g_log_set_handler (log_domains[i],
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
    {
      if (meta_args.client_id == NULL)
        {
          const gchar *desktop_autostart_id;
  
          desktop_autostart_id = g_getenv ("DESKTOP_AUTOSTART_ID");
 
          if (desktop_autostart_id != NULL)
            meta_args.client_id = g_strdup (desktop_autostart_id);
        }

      /* Unset DESKTOP_AUTOSTART_ID in order to avoid child processes to
       * use the same client id. */
      g_unsetenv ("DESKTOP_AUTOSTART_ID");

      meta_session_init (meta_args.client_id, meta_args.save_file);
    }
  /* Free memory possibly allocated by the argument parsing which are
   * no longer needed.
   */
  g_free (meta_args.save_file);
  g_free (meta_args.display_name);
  g_free (meta_args.client_id);

  if (meta_args.composite || meta_args.no_composite)
    meta_prefs_set_compositing_manager (meta_args.composite);

#ifdef WITH_CLUTTER
  if (meta_args.mutter_plugins)
    {
      char **plugins = g_strsplit (meta_args.mutter_plugins, ",", -1);
      char **plugin;
      GSList *plugins_list = NULL;

      for (plugin = plugins; *plugin; plugin++)
        {
          g_strstrip (*plugin);
          plugins_list = g_slist_prepend (plugins_list, *plugin);
        }

      plugins_list = g_slist_reverse (plugins_list);
      meta_prefs_override_clutter_plugins (plugins_list);

      g_slist_free(plugins_list);
      g_strfreev (plugins);
    }
#endif

  if (!meta_display_open ())
    meta_exit (META_EXIT_ERROR);
  
  g_main_loop_run (meta_main_loop);

  meta_display_close (meta_get_display (),
                      CurrentTime); /* I doubt correct timestamps matter here */

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

/**
 * Stops Metacity. This tells the event loop to stop processing; it is rather
 * dangerous to use this rather than meta_restart() because this will leave
 * the user with no window manager. We generally do this only if, for example,
 * the session manager asks us to; we assume the session manager knows what
 * it's talking about.
 *
 * \param code The success or failure code to return to the calling process.
 */
void
meta_quit (MetaExitCode code)
{
  meta_exit_code = code;

  if (g_main_loop_is_running (meta_main_loop))
    g_main_loop_quit (meta_main_loop);
}

/**
 * Restarts Metacity. In practice, this tells the event loop to stop
 * processing, having first set the meta_restart_after_quit flag which
 * tells Metacity to spawn an identical copy of itself before quitting.
 * This happens on receipt of a _METACITY_RESTART_MESSAGE client event.
 */
void
meta_restart (void)
{
  meta_restart_after_quit = TRUE;
  meta_quit (META_EXIT_SUCCESS);
}

/**
 * Called on pref changes. (One of several functions of its kind and purpose.)
 *
 * \bug Why are these particular prefs handled in main.c and not others?
 * Should they be?
 *
 * \param pref  Which preference has changed
 * \param data  Arbitrary data (which we ignore)
 */
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
