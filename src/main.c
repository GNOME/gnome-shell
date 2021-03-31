/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#if defined (HAVE_MALLINFO) || defined (HAVE_MALLINFO2)
#include <malloc.h>
#endif
#include <stdlib.h>
#include <string.h>

#include <cogl-pango/cogl-pango.h>
#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <girepository.h>
#include <meta/main.h>
#include <meta/meta-plugin.h>
#include <meta/prefs.h>
#include <atk-bridge.h>

#include "shell-global.h"
#include "shell-global-private.h"
#include "shell-perf-log.h"
#include "st.h"

extern GType gnome_shell_plugin_get_type (void);

#define SHELL_DBUS_SERVICE "org.gnome.Shell"
#define MAGNIFIER_DBUS_SERVICE "org.gnome.Magnifier"

#define WM_NAME "GNOME Shell"
#define GNOME_WM_KEYBINDINGS "Mutter,GNOME Shell"

static gboolean is_gdm_mode = FALSE;
static char *session_mode = NULL;
static int caught_signal = 0;

#define DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER 1
#define DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER 4

enum {
  SHELL_DEBUG_BACKTRACE_WARNINGS = 1,
  SHELL_DEBUG_BACKTRACE_SEGFAULTS = 2,
};
static int _shell_debug;
static gboolean _tracked_signals[NSIG] = { 0 };

static void
shell_dbus_acquire_name (GDBusProxy  *bus,
                         guint32      request_name_flags,
                         guint32     *request_name_result,
                         const gchar *name,
                         gboolean     fatal)
{
  GError *error = NULL;
  GVariant *request_name_variant;

  if (!(request_name_variant = g_dbus_proxy_call_sync (bus,
                                                       "RequestName",
                                                       g_variant_new ("(su)", name, request_name_flags),
                                                       0, /* call flags */
                                                       -1, /* timeout */
                                                       NULL, /* cancellable */
                                                       &error)))
    {
      g_printerr ("failed to acquire %s: %s\n", name, error->message);
      g_clear_error (&error);
      if (!fatal)
        return;
      exit (1);
    }
  g_variant_get (request_name_variant, "(u)", request_name_result);
  g_variant_unref (request_name_variant);
}

static void
shell_dbus_acquire_names (GDBusProxy  *bus,
                          guint32      request_name_flags,
                          const gchar *name,
                          gboolean     fatal, ...) G_GNUC_NULL_TERMINATED;

static void
shell_dbus_acquire_names (GDBusProxy  *bus,
                          guint32      request_name_flags,
                          const gchar *name,
                          gboolean     fatal, ...)
{
  va_list al;
  guint32 request_name_result;
  va_start (al, fatal);
  for (;;)
  {
    shell_dbus_acquire_name (bus,
                             request_name_flags,
                             &request_name_result,
                             name, fatal);
    name = va_arg (al, gchar *);
    if (!name)
      break;
    fatal = va_arg (al, gboolean);
  }
  va_end (al);
}

static void
shell_dbus_init (gboolean replace)
{
  GDBusConnection *session;
  GDBusProxy *bus;
  GError *error = NULL;
  guint32 request_name_flags;
  guint32 request_name_result;

  session = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

  if (error) {
    g_printerr ("Failed to connect to session bus: %s", error->message);
    exit (1);
  }

  bus = g_dbus_proxy_new_sync (session,
                               G_DBUS_PROXY_FLAGS_NONE,
                               NULL, /* interface info */
                               "org.freedesktop.DBus",
                               "/org/freedesktop/DBus",
                               "org.freedesktop.DBus",
                               NULL, /* cancellable */
                               &error);

  if (!bus)
    {
      g_printerr ("Failed to get a session bus proxy: %s", error->message);
      exit (1);
    }

  request_name_flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT;
  if (replace)
    request_name_flags |= G_BUS_NAME_OWNER_FLAGS_REPLACE;

  shell_dbus_acquire_name (bus,
                           request_name_flags,
                           &request_name_result,
                           SHELL_DBUS_SERVICE, TRUE);
  if (!(request_name_result == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER
        || request_name_result == DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER))
    {
      g_printerr (SHELL_DBUS_SERVICE " already exists on bus and --replace not specified\n");
      exit (1);
    }

  /*
   * We always specify REPLACE_EXISTING to ensure we kill off
   * the existing service if it was running.
   */
  request_name_flags |= G_BUS_NAME_OWNER_FLAGS_REPLACE;

  shell_dbus_acquire_names (bus,
                            request_name_flags,
  /* Also grab org.gnome.Panel to replace any existing panel process */
                            "org.gnome.Panel", TRUE,
  /* ...and the org.gnome.Magnifier service. */
                            MAGNIFIER_DBUS_SERVICE, FALSE,
                            NULL);
  g_object_unref (bus);
  g_object_unref (session);
}

static void
shell_introspection_init (void)
{

  g_irepository_prepend_search_path (MUTTER_TYPELIB_DIR);
  g_irepository_prepend_search_path (GNOME_SHELL_PKGLIBDIR);

  /* We need to explicitly add the directories where the private libraries are
   * installed to the GIR's library path, so that they can be found at runtime
   * when linking using DT_RUNPATH (instead of DT_RPATH), which is the default
   * for some linkers (e.g. gold) and in some distros (e.g. Debian).
   */
  g_irepository_prepend_library_path (MUTTER_TYPELIB_DIR);
  g_irepository_prepend_library_path (GNOME_SHELL_PKGLIBDIR);
}

static void
shell_fonts_init (void)
{
  CoglPangoFontMap *fontmap;

  /* Disable text mipmapping; it causes problems on pre-GEM Intel
   * drivers and we should just be rendering text at the right
   * size rather than scaling it. If we do effects where we dynamically
   * zoom labels, then we might want to reconsider.
   */
  fontmap = COGL_PANGO_FONT_MAP (clutter_get_font_map ());
  cogl_pango_font_map_set_use_mipmapping (fontmap, FALSE);
}

static void
shell_profiler_init (void)
{
  ShellGlobal *global;
  GjsProfiler *profiler;
  GjsContext *context;
  const char *enabled;
  const char *fd_str;
  int fd = -1;

  /* Sysprof uses the "GJS_TRACE_FD=N" environment variable to connect GJS
   * profiler data to the combined Sysprof capture. Since we are in control of
   * the GjsContext, we need to proxy this FD across to the GJS profiler.
   */

  fd_str = g_getenv ("GJS_TRACE_FD");
  enabled = g_getenv ("GJS_ENABLE_PROFILER");
  if (fd_str == NULL || enabled == NULL)
    return;

  global = shell_global_get ();
  g_return_if_fail (global);

  context = _shell_global_get_gjs_context (global);
  g_return_if_fail (context);

  profiler = gjs_context_get_profiler (context);
  g_return_if_fail (profiler);

  if (fd_str)
    {
      fd = atoi (fd_str);

      if (fd > 2)
        {
          gjs_profiler_set_fd (profiler, fd);
          gjs_profiler_start (profiler);
        }
    }
}

static void
shell_profiler_shutdown (void)
{
  ShellGlobal *global;
  GjsProfiler *profiler;
  GjsContext *context;

  global = shell_global_get ();
  context = _shell_global_get_gjs_context (global);
  profiler = gjs_context_get_profiler (context);

  if (profiler)
    gjs_profiler_stop (profiler);
}

static void
malloc_statistics_callback (ShellPerfLog *perf_log,
                            gpointer      data)
{
#if defined (HAVE_MALLINFO) || defined (HAVE_MALLINFO2)
#ifdef HAVE_MALLINFO2
  struct mallinfo2 info = mallinfo2 ();
#else
  struct mallinfo info = mallinfo ();
#endif

  shell_perf_log_update_statistic_i (perf_log,
                                     "malloc.arenaSize",
                                     info.arena);
  shell_perf_log_update_statistic_i (perf_log,
                                     "malloc.mmapSize",
                                     info.hblkhd);
  shell_perf_log_update_statistic_i (perf_log,
                                     "malloc.usedSize",
                                     info.uordblks);
#endif /* defined (HAVE_MALLINFO) || defined (HAVE_MALLINFO2) */
}

static void
shell_perf_log_init (void)
{
  ShellPerfLog *perf_log = shell_perf_log_get_default ();

  /* For probably historical reasons, mallinfo() defines the returned values,
   * even those in bytes as int, not size_t. We're determined not to use
   * more than 2G of malloc'ed memory, so are OK with that.
   */
  shell_perf_log_define_statistic (perf_log,
                                   "malloc.arenaSize",
                                   "Amount of memory allocated by malloc() with brk(), in bytes",
                                   "i");
  shell_perf_log_define_statistic (perf_log,
                                   "malloc.mmapSize",
                                   "Amount of memory allocated by malloc() with mmap(), in bytes",
                                   "i");
  shell_perf_log_define_statistic (perf_log,
                                   "malloc.usedSize",
                                   "Amount of malloc'ed memory currently in use",
                                   "i");

  shell_perf_log_add_statistics_callback (perf_log,
                                          malloc_statistics_callback,
                                          NULL, NULL);
}

static void
shell_a11y_init (void)
{
  cally_accessibility_init ();

  if (clutter_get_accessibility_enabled () == FALSE)
    {
      g_warning ("Accessibility: clutter has no accessibility enabled"
                 " skipping the atk-bridge load");
    }
  else
    {
      atk_bridge_adaptor_init (NULL, NULL);
    }
}

static void
shell_init_debug (const char *debug_env)
{
  static const GDebugKey keys[] = {
    { "backtrace-warnings", SHELL_DEBUG_BACKTRACE_WARNINGS },
    { "backtrace-segfaults", SHELL_DEBUG_BACKTRACE_SEGFAULTS },
  };

  _shell_debug = g_parse_debug_string (debug_env, keys,
                                       G_N_ELEMENTS (keys));
}

static void
default_log_handler (const char     *log_domain,
                     GLogLevelFlags  log_level,
                     const char     *message,
                     gpointer        data)
{
  if (!log_domain || !g_str_has_prefix (log_domain, "tp-glib"))
    g_log_default_handler (log_domain, log_level, message, data);

  /* Filter out Gjs logs, those already have the stack */
  if (log_domain && strcmp (log_domain, "Gjs") == 0)
    return;

  if ((_shell_debug & SHELL_DEBUG_BACKTRACE_WARNINGS) &&
      ((log_level & G_LOG_LEVEL_CRITICAL) ||
       (log_level & G_LOG_LEVEL_WARNING)))
    gjs_dumpstack ();
}

static void
shut_up (const char     *domain,
         GLogLevelFlags  level,
         const char     *message,
         gpointer        user_data)
{
}

static void
dump_gjs_stack_alarm_sigaction (int signo)
{
  g_log_set_default_handler (g_log_default_handler, NULL);
  g_warning ("Failed to dump Javascript stack, got stuck");
  g_log_set_default_handler (default_log_handler, NULL);

  raise (caught_signal);
}

static void
dump_gjs_stack_on_signal_handler (int signo)
{
  struct sigaction sa = { .sa_handler = dump_gjs_stack_alarm_sigaction };
  gsize i;

  /* Ignore all the signals starting this point, a part the one we'll raise
   * (which is implicitly ignored here through SA_RESETHAND), this is needed
   * not to get this handler being called by other signals that we were
   * tracking and that might be emitted by code called starting from now.
   */
  for (i = 0; i < G_N_ELEMENTS (_tracked_signals); ++i)
    {
      if (_tracked_signals[i] && i != signo)
        signal (i, SIG_IGN);
    }

  /* Waiting at least 5 seconds for the dumpstack, if it fails, we raise the error */
  caught_signal = signo;
  sigemptyset (&sa.sa_mask);
  sigaction (SIGALRM, &sa, NULL);

  alarm (5);
  gjs_dumpstack ();
  alarm (0);

  raise (signo);
}

static void
dump_gjs_stack_on_signal (int signo)
{
  struct sigaction sa = {
    .sa_flags   = SA_RESETHAND | SA_NODEFER,
    .sa_handler = dump_gjs_stack_on_signal_handler,
  };

  sigemptyset (&sa.sa_mask);

  sigaction (signo, &sa, NULL);
  _tracked_signals[signo] = TRUE;
}

static gboolean
list_modes (const char  *option_name,
            const char  *value,
            gpointer     data,
            GError     **error)
{
  ShellGlobal *global;
  GjsContext *context;
  const char *script;
  int status;

  /* Many of our imports require global to be set, so rather than
   * tayloring our imports carefully here to avoid that dependency,
   * we just set it.
   * ShellGlobal has some GTK+ dependencies, so initialize GTK+; we
   * don't really care if it fails though (e.g. when running from a tty),
   * so we mute all warnings */
  g_log_set_default_handler (shut_up, NULL);
  gtk_init_check (NULL, NULL);

  _shell_global_init (NULL);
  global = shell_global_get ();
  context = _shell_global_get_gjs_context (global);

  shell_introspection_init ();

  script = "imports.ui.environment.init();"
           "imports.ui.sessionMode.listModes();";
  if (!gjs_context_eval (context, script, -1, "<main>", &status, NULL))
      g_message ("Retrieving list of available modes failed.");

  g_object_unref (context);
  exit (status);
}

static gboolean
print_version (const gchar    *option_name,
               const gchar    *value,
               gpointer        data,
               GError        **error)
{
  g_print ("GNOME Shell %s\n", VERSION);
  exit (0);
}

GOptionEntry gnome_shell_options[] = {
  {
    "version", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
    print_version,
    N_("Print version"),
    NULL
  },
  {
    "gdm-mode", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,
    &is_gdm_mode,
    N_("Mode used by GDM for login screen"),
    NULL
  },
  {
    "mode", 0, 0, G_OPTION_ARG_STRING,
    &session_mode,
    N_("Use a specific mode, e.g. “gdm” for login screen"),
    "MODE"
  },
  {
    "list-modes", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
    list_modes,
    N_("List possible modes"),
    NULL
  },
  { NULL }
};

int
main (int argc, char **argv)
{
  GOptionContext *ctx;
  GError *error = NULL;
  int ecode;

  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  session_mode = (char *) g_getenv ("GNOME_SHELL_SESSION_MODE");

  ctx = meta_get_option_context ();
  g_option_context_add_main_entries (ctx, gnome_shell_options, GETTEXT_PACKAGE);
  g_option_context_add_group (ctx, g_irepository_get_option_group ());
  if (!g_option_context_parse (ctx, &argc, &argv, &error))
    {
      g_printerr ("%s: %s\n", argv[0], error->message);
      exit (1);
    }

  g_option_context_free (ctx);

  meta_plugin_manager_set_plugin_type (gnome_shell_plugin_get_type ());

  meta_set_wm_name (WM_NAME);
  meta_set_gnome_wm_keybindings (GNOME_WM_KEYBINDINGS);

  /* Prevent meta_init() from causing gtk to load the atk-bridge*/
  g_setenv ("NO_AT_BRIDGE", "1", TRUE);
  meta_init ();
  g_unsetenv ("NO_AT_BRIDGE");

  /* FIXME: Add gjs API to set this stuff and don't depend on the
   * environment.  These propagate to child processes.
   */
  g_setenv ("GJS_DEBUG_OUTPUT", "stderr", TRUE);
  g_setenv ("GJS_DEBUG_TOPICS", "JS ERROR;JS LOG", TRUE);

  shell_init_debug (g_getenv ("SHELL_DEBUG"));

  shell_dbus_init (meta_get_replace_current_wm ());
  shell_a11y_init ();
  shell_perf_log_init ();
  shell_introspection_init ();
  shell_fonts_init ();

  g_log_set_default_handler (default_log_handler, NULL);

  /* Initialize the global object */
  if (session_mode == NULL)
    session_mode = is_gdm_mode ? (char *)"gdm" : (char *)"user";

  _shell_global_init ("session-mode", session_mode, NULL);

  dump_gjs_stack_on_signal (SIGABRT);
  dump_gjs_stack_on_signal (SIGFPE);
  dump_gjs_stack_on_signal (SIGIOT);
  dump_gjs_stack_on_signal (SIGTRAP);

  if ((_shell_debug & SHELL_DEBUG_BACKTRACE_SEGFAULTS))
    {
      dump_gjs_stack_on_signal (SIGBUS);
      dump_gjs_stack_on_signal (SIGSEGV);
    }

  shell_profiler_init ();
  ecode = meta_run ();
  shell_profiler_shutdown ();

  g_debug ("Doing final cleanup");
  _shell_global_destroy_gjs_context (shell_global_get ());
  g_object_unref (shell_global_get ());

  return ecode;
}
