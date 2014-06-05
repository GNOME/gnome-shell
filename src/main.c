/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#ifdef HAVE_MALLINFO
#include <malloc.h>
#endif
#include <stdlib.h>
#include <string.h>

#include <cogl-pango/cogl-pango.h>
#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <girepository.h>
#include <meta/main.h>
#include <meta/meta-plugin.h>
#include <meta/prefs.h>
#include <atk-bridge.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/debug-sender.h>

#include "shell-global.h"
#include "shell-global-private.h"
#include "shell-js.h"
#include "shell-perf-log.h"
#include "st.h"

extern GType gnome_shell_plugin_get_type (void);

#define SHELL_DBUS_SERVICE "org.gnome.Shell"
#define MAGNIFIER_DBUS_SERVICE "org.gnome.Magnifier"

#define OVERRIDES_SCHEMA "org.gnome.shell.overrides"

#define WM_NAME "GNOME Shell"
#define GNOME_WM_KEYBINDINGS "Mutter,GNOME Shell"

static gboolean is_gdm_mode = FALSE;
static char *session_mode = NULL;

#define DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER 1
#define DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER 4

enum {
  SHELL_DEBUG_BACKTRACE_WARNINGS = 1,
};
static int _shell_debug;

static void
shell_dbus_acquire_name (GDBusProxy *bus,
                         guint32     request_name_flags,
                         guint32    *request_name_result,
                         gchar      *name,
                         gboolean    fatal)
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
      if (!fatal)
        return;
      exit (1);
    }
  g_variant_get (request_name_variant, "(u)", request_name_result);
}

static void
shell_dbus_acquire_names (GDBusProxy *bus,
                          guint32     request_name_flags,
                          gchar      *name,
                          gboolean    fatal, ...) G_GNUC_NULL_TERMINATED;

static void
shell_dbus_acquire_names (GDBusProxy *bus,
                          guint32     request_name_flags,
                          gchar      *name,
                          gboolean    fatal, ...)
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

  request_name_flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT;
  if (replace)
    request_name_flags |= DBUS_NAME_FLAG_REPLACE_EXISTING;

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
  /* ...and the org.freedesktop.Notifications service. */
                            "org.freedesktop.Notifications", FALSE,
                            NULL);
  /* ...and the on-screen keyboard service */
  shell_dbus_acquire_name (bus,
                           DBUS_NAME_FLAG_REPLACE_EXISTING,
                           &request_name_result,
                           "org.gnome.Caribou.Keyboard", FALSE);
  g_object_unref (bus);
  g_object_unref (session);
}

static void
shell_prefs_init (void)
{
  ShellGlobal *global = shell_global_get ();
  GSettings *settings = shell_global_get_overrides_settings (global);
  char **keys, **k;

  if (!settings)
    return;

  keys = g_settings_list_keys (settings);
  for (keys = k = g_settings_list_keys (settings); *k; k++)
    meta_prefs_override_preference_schema (*k, OVERRIDES_SCHEMA);

  g_strfreev (keys);
}

static void
shell_introspection_init (void)
{

  g_irepository_prepend_search_path (MUTTER_TYPELIB_DIR);
  g_irepository_prepend_search_path (GNOME_SHELL_PKGLIBDIR);
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
malloc_statistics_callback (ShellPerfLog *perf_log,
                            gpointer      data)
{
#ifdef HAVE_MALLINFO
  struct mallinfo info = mallinfo ();

  shell_perf_log_update_statistic_i (perf_log,
                                     "malloc.arenaSize",
                                     info.arena);
  shell_perf_log_update_statistic_i (perf_log,
                                     "malloc.mmapSize",
                                     info.hblkhd);
  shell_perf_log_update_statistic_i (perf_log,
                                     "malloc.usedSize",
                                     info.uordblks);
#endif
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
    { "backtrace-warnings", SHELL_DEBUG_BACKTRACE_WARNINGS }
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
  TpDebugSender *sender = data;
  GTimeVal now;

  g_get_current_time (&now);

  /* Send telepathy debug through DBus */
  if (log_domain != NULL && g_str_has_prefix (log_domain, "tp-glib"))
    tp_debug_sender_add_message (sender, &now, log_domain, log_level, message);

  /* Filter out telepathy-glib logs, we don't want to flood Shell's output
   * with those. */
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
    N_("Use a specific mode, e.g. \"gdm\" for login screen"),
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
  TpDebugSender *sender;

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

  /* Prevent meta_init() from causing gtk to load gail and at-bridge */
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

  /* Turn on telepathy-glib debugging but filter it out in
   * default_log_handler. This handler also exposes all the logs over D-Bus
   * using TpDebugSender. */
  tp_debug_set_flags ("all");

  sender = tp_debug_sender_dup ();

  g_log_set_default_handler (default_log_handler, sender);

  /* Initialize the global object */
  if (session_mode == NULL)
    session_mode = is_gdm_mode ? "gdm" : "user";

  _shell_global_init ("session-mode", session_mode, NULL);

  shell_prefs_init ();

  ecode = meta_run ();

  if (g_getenv ("GNOME_SHELL_ENABLE_CLEANUP"))
    {
      g_printerr ("Doing final cleanup...\n");
      g_object_unref (shell_global_get ());
    }

  g_object_unref (sender);

  return ecode;
}

/* HACK:
   Add a dummy function that calls into libgnome-shell-js.so to ensure it's
   linked to /usr/bin/gnome-shell even when linking with --as-needed.
   This function is never actually called.
   https://bugzilla.gnome.org/show_bug.cgi?id=670477
*/
void _shell_link_to_shell_js (void);

void
_shell_link_to_shell_js (void)
{
  shell_js_add_extension_importer (NULL, NULL, NULL, NULL);
}
