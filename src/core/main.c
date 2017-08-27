/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter main() */

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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:main
 * @title: Main
 * @short_description: Program startup.
 *
 * Functions which parse the command-line arguments, create the display,
 * kick everything off and then close down Mutter when it's time to go.
 *
 *
 *
 * Mutter - a boring window manager for the adult in you
 *
 * Many window managers are like Marshmallow Froot Loops; Mutter
 * is like Frosted Flakes: it's still plain old corn, but dusted
 * with some sugar.
 *
 * The best way to get a handle on how the whole system fits together
 * is discussed in doc/code-overview.txt; if you're looking for functions
 * to investigate, read main(), meta_display_open(), and event_callback().
 */

#define _XOPEN_SOURCE /* for putenv() and some signal-related functions */

#include <config.h>
#include <meta/main.h>
#include "util-private.h"
#include "display-private.h"
#include <meta/meta-x11-errors.h>
#include "ui.h"
#include <meta/prefs.h>
#include <meta/compositor.h>
#include <meta/meta-backend.h>
#include "core/main-private.h"

#include <glib-object.h>
#include <glib-unix.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <time.h>
#include <unistd.h>

#include <clutter/clutter.h>

#ifdef HAVE_INTROSPECTION
#include <girepository.h>
#endif

#include "x11/session.h"

#ifdef HAVE_WAYLAND
#include "wayland/meta-wayland.h"
#include "backends/x11/nested/meta-backend-x11-nested.h"
# endif

#include "backends/meta-backend-private.h"
#include "backends/x11/meta-backend-x11.h"
#include "backends/x11/cm/meta-backend-x11-cm.h"

#ifdef HAVE_NATIVE_BACKEND
#include "backends/native/meta-backend-native.h"
#ifdef HAVE_WAYLAND
#include <systemd/sd-login.h>
#endif /* HAVE_WAYLAND */
#endif /* HAVE_NATIVE_BACKEND */

/*
 * The exit code we'll return to our parent process when we eventually die.
 */
static MetaExitCode meta_exit_code = META_EXIT_SUCCESS;

/*
 * Handle on the main loop, so that we have an easy way of shutting Mutter
 * down.
 */
static GMainLoop *meta_main_loop = NULL;

static void prefs_changed_callback (MetaPreference pref,
                                    gpointer       data);

/**
 * meta_print_compilation_info:
 *
 * Prints a list of which configure script options were used to
 * build this copy of Mutter. This is actually always called
 * on startup, but it's all no-op unless we're in verbose mode
 * (see meta_set_verbose()).
 */
static void
meta_print_compilation_info (void)
{
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
}

/**
 * meta_print_self_identity:
 *
 * Prints the version number, the current timestamp (not the
 * build date), the locale, the character encoding, and a list
 * of configure script options that were used to build this
 * copy of Mutter. This is actually always called
 * on startup, but it's all no-op unless we're in verbose mode
 * (see meta_set_verbose()).
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
  meta_verbose ("Mutter version %s running on %s\n",
    VERSION, buf);

  /* Locale and encoding. */
  g_get_charset (&charset);
  meta_verbose ("Running in locale \"%s\" with encoding \"%s\"\n",
    setlocale (LC_ALL, NULL), charset);

  /* Compilation settings. */
  meta_print_compilation_info ();
}

/*
 * The set of possible options that can be set on Mutter's
 * command line.
 */
static gchar    *opt_save_file;
static gchar    *opt_display_name;
static gchar    *opt_client_id;
static gboolean  opt_replace_wm;
static gboolean  opt_disable_sm;
static gboolean  opt_sync;
#ifdef HAVE_WAYLAND
static gboolean  opt_wayland;
static gboolean  opt_nested;
static gboolean  opt_no_x11;
#endif
#ifdef HAVE_NATIVE_BACKEND
static gboolean  opt_display_server;
#endif
static gboolean  opt_x11;

static GOptionEntry meta_options[] = {
  {
    "sm-disable", 0, 0, G_OPTION_ARG_NONE,
    &opt_disable_sm,
    N_("Disable connection to session manager"),
    NULL
  },
  {
    "replace", 'r', 0, G_OPTION_ARG_NONE,
    &opt_replace_wm,
    N_("Replace the running window manager"),
    NULL
  },
  {
    "sm-client-id", 0, 0, G_OPTION_ARG_STRING,
    &opt_client_id,
    N_("Specify session management ID"),
    "ID"
  },
  {
    "display", 'd', 0, G_OPTION_ARG_STRING,
    &opt_display_name, N_("X Display to use"),
    "DISPLAY"
  },
  {
    "sm-save-file", 0, 0, G_OPTION_ARG_FILENAME,
    &opt_save_file,
    N_("Initialize session from savefile"),
    "FILE"
  },
  {
    "sync", 0, 0, G_OPTION_ARG_NONE,
    &opt_sync,
    N_("Make X calls synchronous"),
    NULL
  },
#ifdef HAVE_WAYLAND
  {
    "wayland", 0, 0, G_OPTION_ARG_NONE,
    &opt_wayland,
    N_("Run as a wayland compositor"),
    NULL
  },
  {
    "nested", 0, 0, G_OPTION_ARG_NONE,
    &opt_nested,
    N_("Run as a nested compositor"),
    NULL
  },
  {
    "no-x11", 0, 0, G_OPTION_ARG_NONE,
    &opt_no_x11,
    N_("Run wayland compositor without starting Xwayland"),
    NULL
  },
#endif
#ifdef HAVE_NATIVE_BACKEND
  {
    "display-server", 0, 0, G_OPTION_ARG_NONE,
    &opt_display_server,
    N_("Run as a full display server, rather than nested")
  },
#endif
  {
    "x11", 0, 0, G_OPTION_ARG_NONE,
    &opt_x11,
    N_("Run with X11 backend")
  },
  {NULL}
};

/**
 * meta_get_option_context: (skip)
 *
 * Returns a #GOptionContext initialized with mutter-related options.
 * Parse the command-line args with this before calling meta_init().
 *
 * Return value: the #GOptionContext
 */
GOptionContext *
meta_get_option_context (void)
{
  GOptionContext *ctx;

  if (setlocale (LC_ALL, "") == NULL)
    meta_warning ("Locale not understood by C library, internationalization will not work\n");
  bindtextdomain (GETTEXT_PACKAGE, MUTTER_LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  ctx = g_option_context_new (NULL);
  g_option_context_add_main_entries (ctx, meta_options, GETTEXT_PACKAGE);
  return ctx;
}

/**
 * meta_select_display:
 *
 * Selects which display Mutter should use. It first tries to use
 * @display_name as the display. If @display_name is %NULL then
 * try to use the environment variable MUTTER_DISPLAY. If that
 * also is %NULL, use the default - :0.0
 */
static void
meta_select_display (char *display_arg)
{
  const char *display_name;

  if (display_arg)
    display_name = (const char *) display_arg;
  else
    display_name = g_getenv ("MUTTER_DISPLAY");

  if (display_name)
    g_setenv ("DISPLAY", display_name, TRUE);
}

static void
meta_finalize (void)
{
  MetaDisplay *display = meta_get_display ();

  if (display)
    meta_display_close (display,
                        META_CURRENT_TIME); /* I doubt correct timestamps matter here */

#ifdef HAVE_WAYLAND
  if (meta_is_wayland_compositor ())
    meta_wayland_finalize ();
#endif
}

static gboolean
on_sigterm (gpointer user_data)
{
  meta_quit (EXIT_SUCCESS);

  return G_SOURCE_REMOVE;
}

#if defined(HAVE_WAYLAND) && defined(HAVE_NATIVE_BACKEND)
static gboolean
session_type_is_supported (const char *session_type)
{
   return (g_strcmp0 (session_type, "x11") == 0) ||
          (g_strcmp0 (session_type, "wayland") == 0);
}

static char *
find_session_type (void)
{
  char **sessions = NULL;
  char *session_id;
  char *session_type;
  const char *session_type_env;
  gboolean is_tty = FALSE;
  int ret, i;

  ret = sd_pid_get_session (0, &session_id);
  if (ret == 0 && session_id != NULL)
    {
      ret = sd_session_get_type (session_id, &session_type);
      free (session_id);

      if (ret == 0)
        {
          if (session_type_is_supported (session_type))
            goto out;
          else
            is_tty = g_strcmp0 (session_type, "tty") == 0;
          free (session_type);
        }
    }
  else if (sd_uid_get_sessions (getuid (), 1, &sessions) > 0)
    {
      for (i = 0; sessions[i] != NULL; i++)
        {
          ret = sd_session_get_type (sessions[i], &session_type);

          if (ret < 0)
            continue;

          if (session_type_is_supported (session_type))
            {
              g_strfreev (sessions);
              goto out;
            }

          free (session_type);
        }
    }
  g_strfreev (sessions);

  session_type_env = g_getenv ("XDG_SESSION_TYPE");
  if (session_type_is_supported (session_type_env))
    {
      /* The string should be freeable */
      session_type = strdup (session_type_env);
      goto out;
    }

  /* Legacy support for starting through xinit */
  if (is_tty && (g_getenv ("MUTTER_DISPLAY") || g_getenv ("DISPLAY")))
    {
      session_type = strdup ("x11");
      goto out;
    }

  meta_warning ("Unsupported session type\n");
  meta_exit (META_EXIT_ERROR);

out:
  return session_type;
}

static gboolean
check_for_wayland_session_type (void)
{
  char *session_type;
  gboolean is_wayland;

  session_type = find_session_type ();
  is_wayland = g_strcmp0 (session_type, "wayland") == 0;
  free (session_type);

  return is_wayland;
}
#endif

/*
 * Determine the compositor configuration, i.e. whether to run as a Wayland
 * compositor, as well as what backend to use.
 *
 * There are various different flags affecting this:
 *
 *    --nested always forces the use of the nested X11 backend
 *    --display-server always forces the use of the native backend
 *    --wayland always forces the compositor type to be a Wayland compositor
 *
 * If no flag is passed that forces the compositor type, the compositor type
 * is determined first from the logind session type, or if that fails, from the
 * XDG_SESSION_TYPE environment variable.
 *
 * If no flag is passed that forces the backend type, the backend type is
 * determined given the compositor type. If the compositor is a Wayland
 * compositor, then the native backend is used, or the nested backend, would
 * the native backend not be enabled at build time. If the compositor is not a
 * Wayland compositor, then the X11 Compositing Manager backend is used.
 */
static void
calculate_compositor_configuration (MetaCompositorType *compositor_type,
                                    GType              *backend_gtype)
{
#ifdef HAVE_WAYLAND
  gboolean run_as_wayland_compositor = opt_wayland && !opt_x11;

#ifdef HAVE_NATIVE_BACKEND
  if ((opt_wayland || opt_nested || opt_display_server) && opt_x11)
#else
  if ((opt_wayland || opt_nested) && opt_x11)
#endif
    {
      meta_warning ("Can't run both as Wayland compositor and X11 compositing manager\n");
      meta_exit (META_EXIT_ERROR);
    }

#ifdef HAVE_NATIVE_BACKEND
  if (opt_nested && opt_display_server)
    {
      meta_warning ("Can't run both as nested and as a display server\n");
      meta_exit (META_EXIT_ERROR);
    }

  if (!run_as_wayland_compositor && !opt_x11)
    run_as_wayland_compositor = check_for_wayland_session_type ();
#endif /* HAVE_NATIVE_BACKEND */

  if (!run_as_wayland_compositor && opt_no_x11)
    {
      meta_warning ("Can't disable X11 support on X11 compositor\n");
      meta_exit (META_EXIT_ERROR);
    }

  if (run_as_wayland_compositor)
    *compositor_type = META_COMPOSITOR_TYPE_WAYLAND;
  else
#endif /* HAVE_WAYLAND */
    *compositor_type = META_COMPOSITOR_TYPE_X11;

#ifdef HAVE_WAYLAND
  if (opt_nested)
    {
      *backend_gtype = META_TYPE_BACKEND_X11_NESTED;
      return;
    }
#endif /* HAVE_WAYLAND */

#ifdef HAVE_NATIVE_BACKEND
  if (opt_display_server)
    {
      *backend_gtype = META_TYPE_BACKEND_NATIVE;
      return;
    }

#ifdef HAVE_WAYLAND
  if (run_as_wayland_compositor)
    {
      *backend_gtype = META_TYPE_BACKEND_NATIVE;
      return;
    }
#endif /* HAVE_WAYLAND */
#endif /* HAVE_NATIVE_BACKEND */

#ifdef HAVE_WAYLAND
  if (run_as_wayland_compositor)
    {
      *backend_gtype = META_TYPE_BACKEND_X11_NESTED;
      return;
    }
  else
#endif /* HAVE_WAYLAND */
    {
      *backend_gtype = META_TYPE_BACKEND_X11_CM;
      return;
    }
}

static gboolean _compositor_configuration_overridden = FALSE;
static MetaCompositorType _compositor_type_override;
static GType _backend_gtype_override;

void
meta_override_compositor_configuration (MetaCompositorType compositor_type,
                                        GType              backend_gtype)
{
  _compositor_configuration_overridden = TRUE;
  _compositor_type_override = compositor_type;
  _backend_gtype_override = backend_gtype;
}

/**
 * meta_init: (skip)
 *
 * Initialize mutter. Call this after meta_get_option_context() and
 * meta_plugin_manager_set_plugin_type(), and before meta_run().
 */
void
meta_init (void)
{
  struct sigaction act;
  sigset_t empty_mask;
  MetaCompositorType compositor_type;
  GType backend_gtype;

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

  g_unix_signal_add (SIGTERM, on_sigterm, NULL);

  if (g_getenv ("MUTTER_VERBOSE"))
    meta_set_verbose (TRUE);
  if (g_getenv ("MUTTER_DEBUG"))
    meta_set_debugging (TRUE);

  if (_compositor_configuration_overridden)
    {
      compositor_type = _compositor_type_override;
      backend_gtype = _backend_gtype_override;
    }
  else
    {
      calculate_compositor_configuration (&compositor_type, &backend_gtype);
    }

#ifdef HAVE_WAYLAND
  if (compositor_type == META_COMPOSITOR_TYPE_WAYLAND)
    meta_set_is_wayland_compositor (TRUE);
#endif

  if (g_get_home_dir ())
    if (chdir (g_get_home_dir ()) < 0)
      meta_warning ("Could not change to home directory %s.\n",
                    g_get_home_dir ());

  meta_print_self_identity ();

#ifdef HAVE_INTROSPECTION
  g_irepository_prepend_search_path (MUTTER_PKGLIBDIR);
#endif

#ifdef HAVE_WAYLAND
  if (meta_is_wayland_compositor ())
    meta_wayland_pre_clutter_init ();
#endif

  /* NB: When running as a hybrid wayland compositor we run our own headless X
   * server so the user can't control the X display to connect too. */
  if (!meta_is_wayland_compositor ())
    meta_select_display (opt_display_name);

  meta_init_backend (backend_gtype);

  meta_clutter_init ();

#ifdef HAVE_WAYLAND
  /* Bring up Wayland. This also launches Xwayland and sets DISPLAY as well... */
  if (meta_is_wayland_compositor ())
    meta_wayland_init ();
#endif

  meta_set_syncing (opt_sync || (g_getenv ("MUTTER_SYNC") != NULL));

  if (opt_replace_wm)
    meta_set_replace_current_wm (TRUE);

  if (opt_save_file && opt_client_id)
    meta_fatal ("Can't specify both SM save file and SM client id\n");

  meta_main_loop = g_main_loop_new (NULL, FALSE);
}

/**
 * meta_register_with_session:
 *
 * Registers mutter with the session manager.  Call this after completing your own
 * initialization.
 *
 * This should be called when the session manager can safely continue to the
 * next phase of startup and potentially display windows.
 */
void
meta_register_with_session (void)
{
  if (!opt_disable_sm)
    {
      if (opt_client_id == NULL)
        {
          const gchar *desktop_autostart_id;

          desktop_autostart_id = g_getenv ("DESKTOP_AUTOSTART_ID");

          if (desktop_autostart_id != NULL)
            opt_client_id = g_strdup (desktop_autostart_id);
        }

      /* Unset DESKTOP_AUTOSTART_ID in order to avoid child processes to
       * use the same client id. */
      g_unsetenv ("DESKTOP_AUTOSTART_ID");

      meta_session_init (opt_client_id, opt_save_file);
    }
  /* Free memory possibly allocated by the argument parsing which are
   * no longer needed.
   */
  g_free (opt_save_file);
  g_free (opt_display_name);
  g_free (opt_client_id);
}

/**
 * meta_run: (skip)
 *
 * Runs mutter. Call this after completing initialization that doesn't require
 * an event loop.
 *
 * Return value: mutter's exit status
 */
int
meta_run (void)
{
  /* Load prefs */
  meta_prefs_init ();
  meta_prefs_add_listener (prefs_changed_callback, NULL);

  if (!meta_display_open ())
    meta_exit (META_EXIT_ERROR);

  g_main_loop_run (meta_main_loop);

  meta_finalize ();

  return meta_exit_code;
}

/**
 * meta_quit:
 * @code: The success or failure code to return to the calling process.
 *
 * Stops Mutter. This tells the event loop to stop processing; it is
 * rather dangerous to use this because this will leave the user with
 * no window manager. We generally do this only if, for example, the
 * session manager asks us to; we assume the session manager knows
 * what it's talking about.
 */
void
meta_quit (MetaExitCode code)
{
  if (g_main_loop_is_running (meta_main_loop))
    {
      meta_exit_code = code;
      g_main_loop_quit (meta_main_loop);
    }
}

/**
 * prefs_changed_callback:
 * @pref:  Which preference has changed
 * @data:  Arbitrary data (which we ignore)
 *
 * Called on pref changes. (One of several functions of its kind and purpose.)
 *
 * FIXME: Why are these particular prefs handled in main.c and not others?
 *        Should they be?
 */
static void
prefs_changed_callback (MetaPreference pref,
                        gpointer       data)
{
  switch (pref)
    {
    case META_PREF_DRAGGABLE_BORDER_WIDTH:
      meta_display_retheme_all ();
      break;

    default:
      /* handled elsewhere or otherwise */
      break;
    }
}

gboolean
meta_should_autostart_x11_display (void)
{
  MetaBackend *backend = meta_get_backend ();
  gboolean wants_x11 = TRUE;

#ifdef HAVE_WAYLAND
  wants_x11 = !opt_no_x11;
#endif

  return META_IS_BACKEND_X11_CM (backend) || wants_x11;
}
