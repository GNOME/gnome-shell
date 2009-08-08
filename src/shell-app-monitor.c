/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#include <string.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <dbus/dbus-glib.h>

#include "shell-app-monitor.h"
#include "shell-app-system.h"
#include "shell-global.h"

#include "display.h"
#include "window.h"

/* This file includes modified code from
 * desktop-data-engine/engine-dbus/hippo-application-monitor.c
 * in the functions collecting application usage data.
 * Written by Owen Taylor, originally licensed under LGPL 2.1.
 * Copyright Red Hat, Inc. 2006-2008
 */

/**
 * SECTION:shell-app-monitor
 * @short_description: Associate windows with application data and track usage/state data
 *
 * The application monitor has two primary purposes.  First, it
 * maintains a mapping from windows to applications (.desktop file ids).
 * It currently implements this with some heuristics on the WM_CLASS X11
 * property (and some static override regexps); in the future, we want to
 * have it also track through startup-notification.
 *
 * Second, the monitor also maintains some usage and state statistics for
 * windows by keeping track of the approximate time an application's
 * windows are focus, as well as the last workspace it was seen on.
 * This time tracking is implemented by watching for restack notifications,
 * and computing a time delta between them.  Also we monitor the
 * GNOME Session "StatusChanged" signal which by default is emitted after 5
 * minutes to signify idle.
 */

#define APP_MONITOR_GCONF_DIR SHELL_GCONF_DIR"/app_monitor"
#define ENABLE_MONITORING_KEY APP_MONITOR_GCONF_DIR"/enable_monitoring"

#define FOCUS_TIME_MIN_SECONDS 7 /* Need 7 continuous seconds of focus */

#define USAGE_CLEAN_DAYS 7 /* If after 7 days we haven't seen an app, purge it */

#define SNAPSHOTS_PER_CLEAN_SECONDS ((USAGE_CLEAN_DAYS * 24 * 60 * 60) / MIN_SNAPSHOT_APP_SECONDS) /* One week */

/* Data is saved to file SHELL_CONFIG_DIR/DATA_FILENAME */
#define DATA_FILENAME "application_state"

#define IDLE_TIME_TRANSITION_SECONDS 30 /* If we transition to idle, only count
                                         * this many seconds of usage */

/* The ranking algorithm we use is: every time an app score reaches SCORE_MAX,
 * divide all scores by 2. Scores are raised by 1 unit every SAVE_APPS_TIMEOUT
 * seconds. This mechanism allows the list to update relatively fast when
 * a new app is used intensively.
 * To keep the list clean, and avoid being Big Brother, apps that have not been
 * seen for a week and whose score is below SCORE_MIN are removed.
 */

/* How often we save internally app data, in seconds */
#define SAVE_APPS_TIMEOUT_SECONDS 5    /* leave this low for testing, we can bump later if need be */

/* With this value, an app goes from bottom to top of the
 * usage list in 50 hours of use */
#define SCORE_MAX (3600 * 50 / FOCUS_TIME_MIN_SECONDS)

/* If an app's score in lower than this and the app has not been used in a week,
 * remove it */
#define SCORE_MIN (SCORE_MAX >> 3)

/* Title patterns to detect apps that don't set WM class as needed.
 * Format: pseudo/wanted WM class, title regex pattern, NULL (for GRegex) */
static struct
{
  const char *app_id;
  const char *pattern;
  GRegex *regex;
} title_patterns[] =  {
    {"mozilla-firefox", ".* - Mozilla Firefox", NULL}, \
    {"openoffice.org-writer", ".* - OpenOffice.org Writer$", NULL}, \
    {"openoffice.org-calc", ".* - OpenOffice.org Calc$", NULL}, \
    {"openoffice.org-impress", ".* - OpenOffice.org Impress$", NULL}, \
    {"openoffice.org-draw", ".* - OpenOffice.org Draw$", NULL}, \
    {"openoffice.org-base", ".* - OpenOffice.org Base$", NULL}, \
    {"openoffice.org-math", ".* - OpenOffice.org Math$", NULL}, \
    {NULL, NULL, NULL}
};


typedef struct AppUsage AppUsage;
typedef struct ActiveAppsData ActiveAppsData;

struct _ShellAppMonitor
{
  GObject parent;

  GFile *configfile;
  DBusGProxy *session_proxy;
  GdkDisplay *display;
  GConfClient *gconf_client;
  gulong last_idle;
  guint idle_focus_change_id;
  guint save_id;
  guint gconf_notify;
  gboolean currently_idle;
  gboolean enable_monitoring;

  /* See comment in AppUsage below */
  guint initially_seen_sequence;

  GSList *previously_running;

  long watch_start_time;
  MetaWindow *watched_window;

  /* <MetaWindow * window, ShellAppInfo *app> */
  GHashTable *window_to_app;

  /* <char *context, GHashTable<char *appid, AppUsage *usage>> */
  GHashTable *app_usages_for_context;
};

G_DEFINE_TYPE (ShellAppMonitor, shell_app_monitor, G_TYPE_OBJECT);

/* Represents an application record for a given context */
struct AppUsage
{
  gdouble score; /* Based on the number of times we'e seen the app and normalized */
  long last_seen; /* Used to clear old apps we've only seen a few times */

  /* how many windows are currently open; in terms of persistence we only save
   * whether the app had any windows or not. */
  guint window_count;

  /* Transient data */
  guint initially_seen_sequence; /* Arbitrary ordered integer for when we first saw
                                  * this application in this session.  Used to order
                                  * the open applications.
                                  */
};

enum {
  CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void shell_app_monitor_finalize (GObject *object);

static void on_session_status_changed (DBusGProxy *proxy, guint status, ShellAppMonitor *monitor);
static void on_focus_window_changed (MetaDisplay *display, GParamSpec *spec, ShellAppMonitor *monitor);
static void ensure_queued_save (ShellAppMonitor *monitor);
static AppUsage * get_app_usage_for_context_and_id (ShellAppMonitor *monitor,
                                                    const char      *context,
                                                    const char      *appid);

static gboolean idle_save_application_usage (gpointer data);

static void restore_from_file (ShellAppMonitor *monitor);

static void update_enable_monitoring (ShellAppMonitor *monitor);

static void on_enable_monitoring_key_changed (GConfClient *client,
                                              guint        connexion_id,
                                              GConfEntry  *entry,
                                              gpointer     monitor);

static long
get_time (void)
{
  GTimeVal tv;
  g_get_current_time (&tv);
  return tv.tv_sec;
}

static void shell_app_monitor_class_init(ShellAppMonitorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = shell_app_monitor_finalize;

  signals[CHANGED] = g_signal_new ("changed",
                                   SHELL_TYPE_APP_MONITOR,
                                   G_SIGNAL_RUN_LAST,
                                   0,
                                   NULL, NULL,
                                   g_cclosure_marshal_VOID__VOID,
                                   G_TYPE_NONE, 0);
}

static void
destroy_usage (AppUsage *usage)
{
  g_free (usage);
}

static char *
get_wmclass_for_window (MetaWindow   *window)
{
  static gboolean patterns_initialized = FALSE;
  const char *wm_class;
  char *title;
  int i;

  wm_class = meta_window_get_wm_class (window);
  g_object_get (window, "title", &title, NULL);

  if (!patterns_initialized) /* Generate match patterns once for all */
    {
      patterns_initialized = TRUE;
      for (i = 0; title_patterns[i].app_id; i++)
        {
          title_patterns[i].regex = g_regex_new (title_patterns[i].pattern,
                                                 0, 0, NULL);
        }
    }

  /* Match window title patterns to identifiers for non-standard apps */
  if (title)
    {
      for (i = 0; title_patterns[i].app_id; i++)
        {
          if (g_regex_match (title_patterns[i].regex, title, 0, NULL))
            {
              /* Set a pseudo WM class, handled like true ones */
              wm_class = title_patterns[i].app_id;
              break;
            }
        }
    }

  g_free (title);
  return g_strdup (wm_class);
}

/**
 * get_cleaned_wmclass_for_window:
 *
 * A "cleaned" wmclass is the WM_CLASS property of a window,
 * after some transformations to turn it into a form
 * somewhat more resilient to changes, such as lowercasing.
 */
static char *
get_cleaned_wmclass_for_window (MetaWindow  *window)
{
  char *wmclass;
  char *cleaned_wmclass;

  if (meta_window_get_window_type (window) != META_WINDOW_NORMAL)
    return NULL;

  wmclass = get_wmclass_for_window (window);
  if (!wmclass)
    return NULL;

  cleaned_wmclass = g_utf8_strdown (wmclass, -1);
  g_free (wmclass);
  /* This handles "Fedora Eclipse", probably others.
   * Note g_strdelimit is modify-in-place. */
  g_strdelimit (cleaned_wmclass, " ", '-');
  wmclass = g_strdup (cleaned_wmclass);
  g_free (cleaned_wmclass);
  return wmclass;
}

/**
 * get_app_for_window:
 *
 * Returns the application associated with a window, or %NULL if
 * we're unable to determine one.
 */
static ShellAppInfo *
get_app_for_window (MetaWindow     *window)
{
  char *wmclass;
  char *with_desktop;
  ShellAppInfo *result;
  ShellAppSystem *appsys;

  wmclass = get_cleaned_wmclass_for_window (window);

  if (!wmclass)
    return NULL;

  with_desktop = g_strjoin (NULL, wmclass, ".desktop", NULL);
  g_free (wmclass);

  appsys = shell_app_system_get_default ();
  result = shell_app_system_lookup_heuristic_basename (appsys, with_desktop);
  g_free (with_desktop);

  return result;
}

static const char *
get_window_context (MetaWindow *window)
{
  return "";
}

static GHashTable *
get_usages_for_context (ShellAppMonitor *monitor,
                        const char      *context)
{
  GHashTable *context_usages;

  context_usages = g_hash_table_lookup (monitor->app_usages_for_context, context);
  if (context_usages == NULL)
    {
      context_usages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)destroy_usage);
      g_hash_table_insert (monitor->app_usages_for_context, g_strdup (context),
                           context_usages);
    }
  return context_usages;
}

static AppUsage *
get_app_usage_for_context_and_id (ShellAppMonitor *monitor,
                                  const char      *context,
                                  const char      *appid)
{
  AppUsage *usage;
  GHashTable *context_usages;

  context_usages = get_usages_for_context (monitor, context);

  usage = g_hash_table_lookup (context_usages, appid);
  if (usage)
    return usage;

  usage = g_new0 (AppUsage, 1);
  usage->initially_seen_sequence = ++monitor->initially_seen_sequence;
  g_hash_table_insert (context_usages, g_strdup (appid), usage);

  return usage;
}

static AppUsage *
get_app_usage_from_window (ShellAppMonitor *monitor,
                           MetaWindow      *window)
{
  ShellAppInfo *app;
  const char *context;

  app = g_hash_table_lookup (monitor->window_to_app, window);
  if (!app)
    return NULL;

  context = get_window_context (window);

  return get_app_usage_for_context_and_id (monitor, context, shell_app_info_get_id (app));
}

static MetaWindow *
get_active_window (ShellAppMonitor *monitor)
{
  MetaScreen *screen;
  MetaDisplay *display;
  screen = shell_global_get_screen (shell_global_get ());
  display = meta_screen_get_display (screen);
  return meta_display_get_focus_window (display);
}

typedef struct {
  gboolean in_context;
  GHashTableIter context_iter;
  const char *context_id;
  GHashTableIter usage_iter;
} UsageIterator;

static void
usage_iterator_init (ShellAppMonitor *self,
                     UsageIterator *iter)
{
  iter->in_context = FALSE;
  g_hash_table_iter_init (&(iter->context_iter), self->app_usages_for_context);
}

static gboolean
usage_iterator_next (ShellAppMonitor *self,
                     UsageIterator   *iter,
                     const char     **context,
                     const char     **id,
                     AppUsage       **usage)
{
  gpointer key, value;
  gboolean next_context;

  if (!iter->in_context)
    next_context = TRUE;
  else if (!g_hash_table_iter_next (&(iter->usage_iter), &key, &value))
    next_context = TRUE;
  else
    next_context = FALSE;

  while (next_context)
    {
      GHashTable *app_usages;

      if (!g_hash_table_iter_next (&(iter->context_iter), &key, &value))
        return FALSE;
      iter->in_context = TRUE;
      iter->context_id = key;
      app_usages = value;
      g_hash_table_iter_init (&(iter->usage_iter), app_usages);

      next_context = !g_hash_table_iter_next (&(iter->usage_iter), &key, &value);
    }

  *context = iter->context_id;
  *id = key;
  *usage = value;

  return TRUE;
}

static void
usage_iterator_remove (ShellAppMonitor *self,
                       UsageIterator   *iter)
{
  g_assert (iter->in_context);

  g_hash_table_iter_remove (&(iter->usage_iter));
}

/* Limit the score to a certain level so that most used apps can change */
static void
normalize_usage (ShellAppMonitor *self)
{
  UsageIterator iter;
  const char *context;
  const char *id;
  AppUsage *usage;

  usage_iterator_init (self, &iter);

  while (usage_iterator_next (self, &iter, &context, &id, &usage))
    {
      usage->score /= 2;
    }
}

static void
increment_usage_for_window_at_time (ShellAppMonitor *self,
                                    MetaWindow      *window,
                                    long             time)
{
  AppUsage *usage;
  guint elapsed;
  guint usage_count;

  usage = get_app_usage_from_window (self, window);
  if (usage == NULL)
    {
      /* This could in theory happen if we lost the app tracking, i.e.
       * the window changed WM_CLASS.  In that case, time for a punt.
       */
      return;
    }

  usage->last_seen = time;

  elapsed = time - self->watch_start_time;
  usage_count = elapsed / FOCUS_TIME_MIN_SECONDS;
  if (usage_count > 0)
    {
      usage->score += usage_count;
      if (usage->score > SCORE_MAX)
        normalize_usage (self);
      ensure_queued_save (self);
    }
}

static void
increment_usage_for_window (ShellAppMonitor *self,
                            MetaWindow      *window)
{
  long curtime = get_time ();
  increment_usage_for_window_at_time (self, window, curtime);
}

static void
track_window (ShellAppMonitor *self,
              MetaWindow      *window)
{
  ShellAppInfo *app;
  AppUsage *usage;

  app = get_app_for_window (window);
  if (!app)
    return;

  g_hash_table_insert (self->window_to_app, window, app);

  usage = get_app_usage_from_window (self, window);

  /* Ephemerally keep track of the number of windows open for this app,
   * when it switches between 0 and 1 we emit a changed signal.
   */
  usage->window_count++;
  if (usage->initially_seen_sequence == 0)
    usage->initially_seen_sequence = ++self->initially_seen_sequence;
  usage->last_seen = get_time ();
  if (usage->window_count == 1)
    g_signal_emit (self, signals[CHANGED], 0);
}

static void
shell_app_monitor_on_window_added (MetaWorkspace   *workspace,
                                   MetaWindow      *window,
                                   gpointer         user_data)
{
  ShellAppMonitor *self = SHELL_APP_MONITOR (user_data);

  track_window (self, window);
}

static void
shell_app_monitor_on_window_removed (MetaWorkspace   *workspace,
                                     MetaWindow      *window,
                                     gpointer         user_data)
{
  ShellAppMonitor *self = SHELL_APP_MONITOR (user_data);
  ShellAppInfo *app;
  AppUsage *usage;

  app = g_hash_table_lookup (self->window_to_app, window);
  if (!app)
    return;

  usage = get_app_usage_from_window (self, window);

  if (window == self->watched_window)
    self->watched_window = NULL;

  usage->window_count--;
  /* Remove before emitting */
  g_hash_table_remove (self->window_to_app, window);

  if (usage->window_count == 0)
    {
      usage->initially_seen_sequence = 0;
      g_signal_emit (self, signals[CHANGED], 0);
    }
}

static void
load_initial_windows (ShellAppMonitor *monitor)
{
  GList *workspaces, *iter;
  MetaScreen *screen = shell_global_get_screen (shell_global_get ());
  workspaces = meta_screen_get_workspaces (screen);

  for (iter = workspaces; iter; iter = iter->next)
    {
      MetaWorkspace *workspace = iter->data;
      GList *windows = meta_workspace_list_windows (workspace);
      GList *window_iter;

      for (window_iter = windows; window_iter; window_iter = window_iter->next)
        {
          MetaWindow *window = window_iter->data;
          track_window (monitor, window);
        }

      g_list_free (windows);
    }
}

static void
on_session_status_changed (DBusGProxy      *proxy,
                           guint            status,
                           ShellAppMonitor *monitor)
{
  gboolean idle;

  idle = (status >= 3);
  if (monitor->currently_idle == idle)
    return;

  monitor->currently_idle = idle;
  if (idle)
    {
      long end_time;

      /* Resync the active window, it may have changed while
       * we were idle and ignoring focus changes.
       */
      monitor->watched_window = get_active_window (monitor);

      /* The GNOME Session signal we watch is 5 minutes, but that's a long
       * time for this purpose.  Instead, just add a base 30 seconds.
       */
      if (monitor->watched_window)
        {
          end_time = monitor->watch_start_time + IDLE_TIME_TRANSITION_SECONDS;
          increment_usage_for_window_at_time (monitor, monitor->watched_window, end_time);
        }
    }
  else
    {
      /* Transitioning to !idle, reset the start time */
      monitor->watch_start_time = get_time ();
    }
}

/**
 * shell_app_monitor_get_windows_for_app:
 * @self:
 * @appid: Find windows for this id
 *
 * Returns: (transfer container) (element-type MetaWindow): List of #MetaWindow corresponding to appid
 */
GSList *
shell_app_monitor_get_windows_for_app (ShellAppMonitor *self,
                                       const char      *appid)
{
  GHashTableIter iter;
  gpointer key, value;
  GSList *ret = NULL;

  g_hash_table_iter_init (&iter, self->window_to_app);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      MetaWindow *window = key;
      ShellAppInfo *app = value;
      const char *id = shell_app_info_get_id (app);

      if (strcmp (id, appid) != 0)
        continue;

      ret = g_slist_prepend (ret, window);
    }
  return ret;
}

static void
shell_app_monitor_on_n_workspaces_changed (MetaScreen    *screen,
                                           GParamSpec    *pspec,
                                           gpointer       user_data)
{
  ShellAppMonitor *self = SHELL_APP_MONITOR (user_data);
  GList *workspaces, *iter;

  workspaces = meta_screen_get_workspaces (screen);

  for (iter = workspaces; iter; iter = iter->next)
    {
      MetaWorkspace *workspace = iter->data;

      /* This pair of disconnect/connect is idempotent if we were
       * already connected, while ensuring we get connected for
       * new workspaces.
       */
      g_signal_handlers_disconnect_by_func (workspace,
                                            shell_app_monitor_on_window_added,
                                            self);
      g_signal_handlers_disconnect_by_func (workspace,
                                            shell_app_monitor_on_window_removed,
                                            self);

      g_signal_connect (workspace, "window-added",
                        G_CALLBACK (shell_app_monitor_on_window_added), self);
      g_signal_connect (workspace, "window-removed",
                        G_CALLBACK (shell_app_monitor_on_window_removed), self);
    }
}

static void
init_window_monitoring (ShellAppMonitor *self)
{
  MetaDisplay *display;
  MetaScreen *screen = shell_global_get_screen (shell_global_get ());

  g_signal_connect (screen, "notify::n-workspaces",
                    G_CALLBACK (shell_app_monitor_on_n_workspaces_changed), self);
  display = meta_screen_get_display (screen);
  g_signal_connect (display, "notify::focus-window",
                    G_CALLBACK (on_focus_window_changed), self);

  shell_app_monitor_on_n_workspaces_changed (screen, NULL, self);
}

static void
shell_app_monitor_init (ShellAppMonitor *self)
{
  GdkDisplay *display;
  char *path;
  char *shell_config_dir;
  DBusGConnection *session_bus;

  /* FIXME: should we create as many monitors as there are GdkScreens? */
  display = gdk_display_get_default ();

  session_bus = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
  self->session_proxy = dbus_g_proxy_new_for_name (session_bus, "org.gnome.SessionManager",
                                                   "/org/gnome/SessionManager/Presence",
                                                   "org.gnome.SessionManager");
  dbus_g_proxy_add_signal (self->session_proxy, "StatusChanged",
                           G_TYPE_UINT, G_TYPE_INVALID, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (self->session_proxy, "StatusChanged",
                               G_CALLBACK (on_session_status_changed), self, NULL);

  self->display = g_object_ref (display);

  self->last_idle = 0;
  self->currently_idle = FALSE;
  self->enable_monitoring = FALSE;

  self->app_usages_for_context = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                                         (GDestroyNotify) g_hash_table_destroy);

  self->window_to_app = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                               NULL, (GDestroyNotify) shell_app_info_unref);

  g_object_get (shell_global_get(), "configdir", &shell_config_dir, NULL),
  path = g_build_filename (shell_config_dir, DATA_FILENAME, NULL);
  g_free (shell_config_dir);
  self->configfile = g_file_new_for_path (path);
  restore_from_file (self);

  load_initial_windows (self);
  init_window_monitoring (self);

  self->gconf_client = gconf_client_get_default ();
  gconf_client_add_dir (self->gconf_client, APP_MONITOR_GCONF_DIR,
                        GCONF_CLIENT_PRELOAD_NONE, NULL);
  self->gconf_notify =
    gconf_client_notify_add (self->gconf_client, ENABLE_MONITORING_KEY,
                             on_enable_monitoring_key_changed, self, NULL, NULL);
  update_enable_monitoring (self);
}

static void
shell_app_monitor_finalize (GObject *object)
{
  ShellAppMonitor *self = SHELL_APP_MONITOR (object);
  int i;

  if (self->save_id > 0)
    g_source_remove (self->save_id);
  gconf_client_notify_remove (self->gconf_client, self->gconf_notify);
  g_object_unref (self->gconf_client);
  g_object_unref (self->display);
  g_hash_table_destroy (self->app_usages_for_context);
  for (i = 0; title_patterns[i].app_id; i++)
    g_regex_unref (title_patterns[i].regex);
  g_object_unref (self->configfile);

  G_OBJECT_CLASS (shell_app_monitor_parent_class)->finalize(object);
}

/**
 * shell_app_monitor_get_most_used_apps:
 * @monitor: the app monitor instance to request
 * @context: Activity identifier
 * @max_count: how many applications are requested. Note that the actual
 *     list size may be less, or NULL if not enough applications are registered.
 *
 * Get a list of desktop identifiers representing the most popular applications
 * for a given context.
 *
 * Returns: (element-type utf8) (transfer container): List of application desktop
 *     identifiers
 */
GList *
shell_app_monitor_get_most_used_apps (ShellAppMonitor *monitor,
                                      const char      *context,
                                      gint             max_count)
{
  GHashTable *usages;
  usages = g_hash_table_lookup (monitor->app_usages_for_context, context);
  if (usages == NULL)
    return NULL;
  return g_hash_table_get_keys (usages);
}

/**
 * shell_app_monitor_get_window_app
 * @monitor: An app monitor instance
 * @metawin: A #MetaWindow
 *
 * Returns: Desktop file id associated with window
 */
ShellAppInfo *
shell_app_monitor_get_window_app (ShellAppMonitor *monitor,
                                  MetaWindow      *metawin)
{
  ShellAppInfo *info = g_hash_table_lookup (monitor->window_to_app, metawin);
  if (info)
    shell_app_info_ref (info);
  return info;
}

typedef struct {
  ShellAppMonitor *self;
  const char *context_id;
} AppOpenSequenceSortData;

static int
sort_apps_by_open_sequence (gconstpointer a,
                            gconstpointer b,
                            gpointer datap)
{
  AppOpenSequenceSortData *data = datap;
  const char *id_a = a;
  const char *id_b = b;
  AppUsage *usage_a;
  AppUsage *usage_b;

  usage_a = get_app_usage_for_context_and_id (data->self, data->context_id, id_a);
  usage_b = get_app_usage_for_context_and_id (data->self, data->context_id, id_b);
  if (usage_a->initially_seen_sequence == usage_b->initially_seen_sequence)
    return 0;
  if (usage_a->initially_seen_sequence < usage_b->initially_seen_sequence)
    return -1;
  return 1;
}

/**
 * shell_app_monitor_get_running_app_ids:
 * @monitor: An app monitor instance
 * @context: Activity identifier
 *
 * Returns the set of applications which currently have at least one open
 * window in the given context.
 *
 * Returns: (element-type utf8) (transfer container): List of application desktop
 *     identifiers
 */
GSList *
shell_app_monitor_get_running_app_ids (ShellAppMonitor *monitor,
                                       const char      *context)
{
  UsageIterator iter;
  const char *cur_context;
  const char *id;
  AppUsage *usage;
  GSList *ret;
  AppOpenSequenceSortData data;

  usage_iterator_init (monitor, &iter);

  ret = NULL;
  while (usage_iterator_next (monitor, &iter, &cur_context, &id, &usage))
    {
      if (strcmp (cur_context, context) != 0)
        continue;

      if (usage->window_count > 0)
        ret = g_slist_prepend (ret, (char*)id);
    }

  data.self = monitor;
  data.context_id = context;
  return g_slist_sort_with_data (ret, sort_apps_by_open_sequence, &data);
}

static gboolean
idle_handle_focus_change (gpointer data)
{
  ShellAppMonitor *monitor = data;
  long curtime = get_time ();

  increment_usage_for_window (monitor, monitor->watched_window);

  monitor->watched_window = get_active_window (monitor);
  monitor->watch_start_time = curtime;

  monitor->idle_focus_change_id = 0;
  return FALSE;
}

static void
on_focus_window_changed (MetaDisplay     *display,
                         GParamSpec      *spec,
                         ShellAppMonitor *self)
{
  if (!self->enable_monitoring || self->currently_idle)
    return;

  if (self->idle_focus_change_id != 0)
    return;

  /* Defensively compress notifications here in case something is going berserk,
   * we'll at least use a bit less system resources. */
  self->idle_focus_change_id = g_timeout_add (250, idle_handle_focus_change, self);
}

static void
ensure_queued_save (ShellAppMonitor *monitor)
{
  if (monitor->save_id != 0)
    return;
  monitor->save_id = g_timeout_add_seconds (SAVE_APPS_TIMEOUT_SECONDS, idle_save_application_usage, monitor);
}

/* Used to sort highest scores at the top */
static gint
usage_sort_apps (gconstpointer data1,
                 gconstpointer data2)
{
  const AppUsage *u1 = data1;
  const AppUsage *u2 = data2;

  if (u1->score > u2->score)
    return -1;
  else if (u1->score == u2->score)
    return 0;
  else
    return 1;
}

/* Clean up apps we see rarely.
 * The logic behind this is that if an app was seen less than SCORE_MIN times
 * and not seen for a week, it can probably be forgotten about.
 * This should much reduce the size of the list and avoid 'pollution'. */
static gboolean
idle_clean_usage (ShellAppMonitor *monitor)
{
  UsageIterator iter;
  const char *context;
  const char *id;
  AppUsage *usage;
  GDate *date;
  guint32 date_days;

  /* Subtract a week */
  date = g_date_new ();
  g_date_set_time_t (date, time (NULL));
  g_date_subtract_days (date, USAGE_CLEAN_DAYS);
  date_days = g_date_get_julian (date);

  usage_iterator_init (monitor, &iter);

  while (usage_iterator_next (monitor, &iter, &context, &id, &usage))
    {
      if ((usage->score < SCORE_MIN) &&
          (usage->last_seen < date_days))
        usage_iterator_remove (monitor, &iter);
    }
  g_date_free (date);

  return FALSE;
}

static gboolean
write_escaped (GDataOutputStream   *stream,
               const char          *str,
               GError             **error)
{
  gboolean ret;
  char *quoted = g_markup_escape_text (str, -1);
  ret = g_data_output_stream_put_string (stream, quoted, NULL, error);
  g_free (quoted);
  return ret;
}

static gboolean
write_attribute_string (GDataOutputStream *stream,
                        const char        *elt_name,
                        const char        *str,
                        GError           **error)
{
  gboolean ret = FALSE;
  char *elt;

  elt = g_strdup_printf (" %s=\"", elt_name);
  ret = g_data_output_stream_put_string (stream, elt, NULL, error);
  g_free (elt);
  if (!ret) goto out;

  ret = write_escaped (stream, str, error);
  if (!ret) goto out;

  ret = g_data_output_stream_put_string (stream, "\"", NULL, error);

out:
  return ret;
}

static gboolean
write_attribute_uint (GDataOutputStream *stream,
                      const char        *elt_name,
                      guint              value,
                      GError           **error)
{
  gboolean ret;
  char *buf;

  buf = g_strdup_printf ("%u", value);
  ret = write_attribute_string (stream, elt_name, buf, error);
  g_free (buf);

  return ret;
}

static gboolean
write_attribute_double (GDataOutputStream *stream,
                        const char        *elt_name,
                        double             value,
                        GError           **error)
{
  gchar buf[G_ASCII_DTOSTR_BUF_SIZE];
  gboolean ret;

  g_ascii_dtostr (buf, sizeof (buf), value);
  ret = write_attribute_string (stream, elt_name, buf, error);

  return ret;
}

/* Save app data lists to file */
static gboolean
idle_save_application_usage (gpointer data)
{
  ShellAppMonitor *monitor = SHELL_APP_MONITOR (data);
  UsageIterator iter;
  const char *current_context;
  const char *context;
  const char *id;
  AppUsage *usage;
  GFileOutputStream *output;
  GOutputStream *buffered_output;
  GDataOutputStream *data_output;
  GError *error = NULL;


  monitor->save_id = 0;

  /* Parent directory is already created by shell-global */
  output = g_file_replace (monitor->configfile, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error);
  if (!output)
    {
      g_debug ("Could not save applications usage data: %s", error->message);
      g_error_free (error);
      return FALSE;
    }
  buffered_output = g_buffered_output_stream_new (G_OUTPUT_STREAM(output));
  g_object_unref (output);
  data_output = g_data_output_stream_new (G_OUTPUT_STREAM(buffered_output));
  g_object_unref (buffered_output);

  if (!g_data_output_stream_put_string (data_output, "<?xml version=\"1.0\"?>\n<application-state>\n", NULL, &error))
    goto out;

  usage_iterator_init (monitor, &iter);

  current_context = NULL;
  while (usage_iterator_next (monitor, &iter, &context, &id, &usage))
    {
      if (context != current_context)
        {
          if (current_context != NULL)
            {
              if (!g_data_output_stream_put_string (data_output, "  </context>", NULL, &error))
                goto out;
            }
          current_context = context;
          if (!g_data_output_stream_put_string (data_output, "  <context", NULL, &error))
            goto out;
          if (!write_attribute_string (data_output, "id", context, &error))
            goto out;
          if (!g_data_output_stream_put_string (data_output, ">\n", NULL, &error))
            goto out;
        }
      if (!g_data_output_stream_put_string (data_output, "    <application", NULL, &error))
        goto out;
      if (!write_attribute_string (data_output, "id", id, &error))
        goto out;
      if (!write_attribute_uint (data_output, "open-window-count", usage->window_count > 0, &error))
        goto out;

      if (!write_attribute_double (data_output, "score", usage->score, &error))
        goto out;
      if (!write_attribute_uint (data_output, "last-seen", usage->last_seen, &error))
        goto out;
      if (!g_data_output_stream_put_string (data_output, "/>\n", NULL, &error))
        goto out;
    }
  if (current_context != NULL)
    {
      if (!g_data_output_stream_put_string (data_output, "  </context>\n", NULL, &error))
        goto out;
    }
  if (!g_data_output_stream_put_string (data_output, "</application-state>\n", NULL, &error))
    goto out;

out:
  if (!error)
    g_output_stream_close (G_OUTPUT_STREAM(data_output), NULL, &error);
  g_object_unref (data_output);
  if (error)
    {
      g_debug ("Could not save applications usage data: %s", error->message);
      g_error_free (error);
    }
  return FALSE;
}

typedef struct {
  ShellAppMonitor *monitor;
  char *context;
} ParseData;

static void
shell_app_monitor_start_element_handler  (GMarkupParseContext *context,
                                          const gchar         *element_name,
                                          const gchar        **attribute_names,
                                          const gchar        **attribute_values,
                                          gpointer             user_data,
                                          GError             **error)
{
  ParseData *data = user_data;

  if (strcmp (element_name, "application-state") == 0)
    {
    }
  else if (strcmp (element_name, "context") == 0)
    {
      char *context = NULL;
      const char **attribute;
      const char **value;

      for (attribute = attribute_names, value = attribute_values; *attribute; attribute++, value++)
        {
          if (strcmp (*attribute, "id") == 0)
            context = g_strdup (*value);
        }
      if (context < 0)
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_PARSE,
                       "Missing attribute id on <%s> element",
                       element_name);
          return;
        }
      data->context = context;
    }
  else if (strcmp (element_name, "application") == 0)
    {
      const char **attribute;
      const char **value;
      AppUsage *usage;
      char *appid = NULL;
      GHashTable *usage_table;

      for (attribute = attribute_names, value = attribute_values; *attribute; attribute++, value++)
        {
          if (strcmp (*attribute, "id") == 0)
            appid = g_strdup (*value);
        }

      if (!appid)
        {
          g_set_error (error,
                       G_MARKUP_ERROR,
                       G_MARKUP_ERROR_PARSE,
                       "Missing attribute id on <%s> element",
                       element_name);
          return;
        }

      usage_table = get_usages_for_context (data->monitor, data->context);

      usage = g_new0 (AppUsage, 1);
      usage->initially_seen_sequence = 0;
      g_hash_table_insert (usage_table, appid, usage);

      for (attribute = attribute_names, value = attribute_values; *attribute; attribute++, value++)
        {
          if (strcmp (*attribute, "open-window-count") == 0)
            {
              guint count = strtoul (*value, NULL, 10);
              if (count > 0)
                 data->monitor->previously_running = g_slist_prepend (data->monitor->previously_running,
                                                                      usage);
            }
          else if (strcmp (*attribute, "score") == 0)
            {
              usage->score = g_ascii_strtod (*value, NULL);
            }
          else if (strcmp (*attribute, "last-seen") == 0)
            {
              usage->last_seen = (guint) g_ascii_strtoull (*value, NULL, 10);
            }
        }
    }
  else
    {
      g_set_error (error,
                   G_MARKUP_ERROR,
                   G_MARKUP_ERROR_PARSE,
                   "Unknown element <%s>",
                   element_name);
    }
}

static void
shell_app_monitor_end_element_handler (GMarkupParseContext *context,
                                       const gchar         *element_name,
                                       gpointer             user_data,
                                       GError             **error)
{
  ParseData *data = user_data;

  if (strcmp (element_name, "context") == 0)
    {
      g_free (data->context);
      data->context = NULL;
    }
}

static void
shell_app_monitor_text_handler (GMarkupParseContext *context,
                                const gchar         *text,
                                gsize                text_len,
                                gpointer             user_data,
                                GError             **error)
{
  /* do nothing, very very fast */
}

static GMarkupParser app_state_parse_funcs =
{
  shell_app_monitor_start_element_handler,
  shell_app_monitor_end_element_handler,
  shell_app_monitor_text_handler,
  NULL,
  NULL
};

/* Load data about apps usage from file */
static void
restore_from_file (ShellAppMonitor *monitor)
{
  GFileInputStream *input;
  ParseData parse_data;
  GMarkupParseContext *parse_context;
  GError *error = NULL;
  char buf[1024];

  input = g_file_read (monitor->configfile, NULL, &error);
  if (error)
    {
      if (error->code != G_IO_ERROR_NOT_FOUND)
        g_warning ("Could not load applications usage data: %s", error->message);

      g_error_free (error);
      return;
    }

  memset (&parse_data, 0, sizeof (ParseData));
  parse_data.monitor = monitor;
  parse_data.context = NULL;
  parse_context = g_markup_parse_context_new (&app_state_parse_funcs, 0, &parse_data, NULL);

  while (TRUE)
    {
      gssize count = g_input_stream_read ((GInputStream*) input, buf, sizeof(buf), NULL, &error);
      if (count <= 0)
        goto out;
      if (!g_markup_parse_context_parse (parse_context, buf, count, &error))
        goto out;
     }

out:
  g_free (parse_data.context);
  g_markup_parse_context_free (parse_context);
  g_input_stream_close ((GInputStream*)input, NULL, NULL);
  g_object_unref (input);

  idle_clean_usage (monitor);
  monitor->previously_running = g_slist_sort (monitor->previously_running, usage_sort_apps);

  if (error)
    {
      g_warning ("Could not load applications usage data: %s", error->message);
      g_error_free (error);
    }
}

/* Enable or disable the timers, depending on the value of ENABLE_MONITORING_KEY
 * and taking care of the previous state.  If monitoring is disabled, we still
 * report apps usage based on (possibly) saved data, but don't collect data.
 */
static void
update_enable_monitoring (ShellAppMonitor *monitor)
{
  GConfValue *value;
  gboolean enable;

  value = gconf_client_get (monitor->gconf_client, ENABLE_MONITORING_KEY, NULL);
  if (value)
    {
      enable = gconf_value_get_bool (value);
      gconf_value_free (value);
    }
  else /* Schema is not present, set default value by hand to avoid getting FALSE */
    enable = TRUE;

  /* Be sure not to start the timers if they were already set */
  if (enable && !monitor->enable_monitoring)
    {
      on_focus_window_changed (NULL, NULL, monitor);
    }
  /* ...and don't try to stop them if they were not running */
  else if (!enable && monitor->enable_monitoring)
    {
      monitor->watched_window = NULL;
      if (monitor->save_id)
        g_source_remove (monitor->save_id);
    }

  monitor->enable_monitoring = enable;
}

/* Called when the ENABLE_MONITORING_KEY boolean has changed */
static void
on_enable_monitoring_key_changed (GConfClient *client,
                                  guint        connexion_id,
                                  GConfEntry  *entry,
                                  gpointer     monitor)
{
  update_enable_monitoring ((ShellAppMonitor *) monitor);
}

/**
 * shell_app_monitor_get_default:
 *
 * Return Value: (transfer none): The global #ShellAppMonitor instance
 */
ShellAppMonitor *
shell_app_monitor_get_default ()
{
  static ShellAppMonitor *instance;

  if (instance == NULL)
    instance = g_object_new (SHELL_TYPE_APP_MONITOR, NULL);

  return instance;
}
