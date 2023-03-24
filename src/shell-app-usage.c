/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <glib.h>
#include <gio/gio.h>
#include <meta/display.h>
#include <meta/group.h>
#include <meta/window.h>

#include "shell-app-usage.h"
#include "shell-window-tracker.h"
#include "shell-global.h"

/* This file includes modified code from
 * desktop-data-engine/engine-dbus/hippo-application-monitor.c
 * in the functions collecting application usage data.
 * Written by Owen Taylor, originally licensed under LGPL 2.1.
 * Copyright Red Hat, Inc. 2006-2008
 */

/**
 * SECTION:shell-app-usage
 * @short_description: Track application usage/state data
 *
 * This class maintains some usage and state statistics for
 * applications by keeping track of the approximate time an application's
 * windows are focused, as well as the last workspace it was seen on.
 * This time tracking is implemented by watching for focus notifications,
 * and computing a time delta between them.  Also we watch the
 * GNOME Session "StatusChanged" signal which by default is emitted after 5
 * minutes to signify idle.
 */

#define PRIVACY_SCHEMA "org.gnome.desktop.privacy"
#define ENABLE_MONITORING_KEY "remember-app-usage"

#define FOCUS_TIME_MIN_SECONDS 7 /* Need 7 continuous seconds of focus */

#define USAGE_CLEAN_DAYS 7 /* If after 7 days we haven't seen an app, purge it */

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
#define SAVE_APPS_TIMEOUT_SECONDS (5 * 60)

/* With this value, an app goes from bottom to top of the
 * usage list in 50 hours of use */
#define SCORE_MAX (3600 * 50 / FOCUS_TIME_MIN_SECONDS)

/* If an app's score in lower than this and the app has not been used in a week,
 * remove it */
#define SCORE_MIN (SCORE_MAX >> 3)

/* http://www.gnome.org/~mccann/gnome-session/docs/gnome-session.html#org.gnome.SessionManager.Presence */
#define GNOME_SESSION_STATUS_IDLE 3

typedef struct UsageData UsageData;

struct _ShellAppUsage
{
  GObject parent;

  GFile *configfile;
  GDBusProxy *session_proxy;
  GSettings *privacy_settings;
  guint idle_focus_change_id;
  guint save_id;
  gboolean currently_idle;
  gboolean enable_monitoring;

  long watch_start_time;
  ShellApp *watched_app;

  /* <char *appid, UsageData *usage> */
  GHashTable *app_usages;
};

G_DEFINE_TYPE (ShellAppUsage, shell_app_usage, G_TYPE_OBJECT);

/* Represents an application record for a given context */
struct UsageData
{
  gdouble score; /* Based on the number of times we'e seen the app and normalized */
  long last_seen; /* Used to clear old apps we've only seen a few times */
};

static void shell_app_usage_finalize (GObject *object);

static void on_session_status_changed (GDBusProxy *proxy, guint status, ShellAppUsage *self);
static void on_focus_app_changed (ShellWindowTracker *tracker, GParamSpec *spec, ShellAppUsage *self);
static void ensure_queued_save (ShellAppUsage *self);

static gboolean idle_save_application_usage (gpointer data);

static void restore_from_file (ShellAppUsage *self);

static void update_enable_monitoring (ShellAppUsage *self);

static void on_enable_monitoring_key_changed (GSettings     *settings,
                                              const gchar   *key,
                                              ShellAppUsage *self);

static long
get_time (void)
{
  return g_get_real_time () / G_TIME_SPAN_SECOND;
}

static void
shell_app_usage_class_init (ShellAppUsageClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = shell_app_usage_finalize;
}

static UsageData *
get_usage_for_app (ShellAppUsage *self,
                   ShellApp      *app)
{
  UsageData *usage;
  const char *appid = shell_app_get_id (app);

  usage = g_hash_table_lookup (self->app_usages, appid);
  if (usage)
    return usage;

  usage = g_new0 (UsageData, 1);
  g_hash_table_insert (self->app_usages, g_strdup (appid), usage);

  return usage;
}

/* Limit the score to a certain level so that most used apps can change */
static void
normalize_usage (ShellAppUsage *self)
{
  GHashTableIter iter;
  UsageData *usage;

  g_hash_table_iter_init (&iter, self->app_usages);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &usage))
    usage->score /= 2;
}

static void
increment_usage_for_app_at_time (ShellAppUsage *self,
                                 ShellApp      *app,
                                 long           time)
{
  UsageData *usage;
  guint elapsed;
  guint usage_count;

  usage = get_usage_for_app (self, app);

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
increment_usage_for_app (ShellAppUsage *self,
                         ShellApp      *app)
{
  long curtime = get_time ();
  increment_usage_for_app_at_time (self, app, curtime);
}

static void
on_app_state_changed (ShellAppSystem *app_system,
                      ShellApp       *app,
                      gpointer        user_data)
{
  ShellAppUsage *self = SHELL_APP_USAGE (user_data);
  UsageData *usage;
  gboolean running;

  if (shell_app_is_window_backed (app))
    return;

  usage = get_usage_for_app (self, app);

  running = shell_app_get_state (app) == SHELL_APP_STATE_RUNNING;

  if (running)
    usage->last_seen = get_time ();
}

static void
on_focus_app_changed (ShellWindowTracker *tracker,
                      GParamSpec         *spec,
                      ShellAppUsage      *self)
{
  if (self->watched_app != NULL)
    increment_usage_for_app (self, self->watched_app);

  if (self->watched_app)
    g_object_unref (self->watched_app);

  g_object_get (tracker, "focus-app", &(self->watched_app), NULL);
  self->watch_start_time = get_time ();
}

static void
on_session_status_changed (GDBusProxy      *proxy,
                           guint            status,
                           ShellAppUsage *self)
{
  gboolean idle;

  idle = (status >= GNOME_SESSION_STATUS_IDLE);
  if (self->currently_idle == idle)
    return;

  self->currently_idle = idle;
  if (idle)
    {
      long end_time;

      /* The GNOME Session signal we watch is 5 minutes, but that's a long
       * time for this purpose.  Instead, just add a base 30 seconds.
       */
      if (self->watched_app)
        {
          end_time = self->watch_start_time + IDLE_TIME_TRANSITION_SECONDS;
          increment_usage_for_app_at_time (self, self->watched_app, end_time);
        }
    }
  else
    {
      /* Transitioning to !idle, reset the start time */
      self->watch_start_time = get_time ();
    }
}

static void
session_proxy_signal (GDBusProxy *proxy, gchar *sender_name, gchar *signal_name, GVariant *parameters, gpointer user_data)
{
  if (g_str_equal (signal_name, "StatusChanged"))
    {
      guint status;
      g_variant_get (parameters, "(u)", &status);
      on_session_status_changed (proxy, status, SHELL_APP_USAGE (user_data));
    }
}

static void
shell_app_usage_init (ShellAppUsage *self)
{
  ShellGlobal *global;
  char *shell_userdata_dir, *path;
  GDBusConnection *session_bus;
  ShellWindowTracker *tracker;
  ShellAppSystem *app_system;

  global = shell_global_get ();

  self->app_usages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  tracker = shell_window_tracker_get_default ();
  g_signal_connect (tracker, "notify::focus-app", G_CALLBACK (on_focus_app_changed), self);

  app_system = shell_app_system_get_default ();
  g_signal_connect (app_system, "app-state-changed", G_CALLBACK (on_app_state_changed), self);

  session_bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  self->session_proxy = g_dbus_proxy_new_sync (session_bus,
                                               G_DBUS_PROXY_FLAGS_NONE,
                                               NULL, /* interface info */
                                               "org.gnome.SessionManager",
                                               "/org/gnome/SessionManager/Presence",
                                               "org.gnome.SessionManager",
                                               NULL, /* cancellable */
                                               NULL /* error */);
  g_signal_connect (self->session_proxy, "g-signal", G_CALLBACK (session_proxy_signal), self);
  g_object_unref (session_bus);

  self->currently_idle = FALSE;
  self->enable_monitoring = FALSE;

  g_object_get (global, "userdatadir", &shell_userdata_dir, NULL),
  path = g_build_filename (shell_userdata_dir, DATA_FILENAME, NULL);
  g_free (shell_userdata_dir);
  self->configfile = g_file_new_for_path (path);
  g_free (path);
  restore_from_file (self);

  self->privacy_settings = g_settings_new(PRIVACY_SCHEMA);
  g_signal_connect (self->privacy_settings,
                    "changed::" ENABLE_MONITORING_KEY,
                    G_CALLBACK (on_enable_monitoring_key_changed),
                    self);
  update_enable_monitoring (self);
}

static void
shell_app_usage_finalize (GObject *object)
{
  ShellAppUsage *self = SHELL_APP_USAGE (object);

  g_clear_handle_id (&self->save_id, g_source_remove);

  g_object_unref (self->privacy_settings);

  g_object_unref (self->configfile);

  g_object_unref (self->session_proxy);

  G_OBJECT_CLASS (shell_app_usage_parent_class)->finalize(object);
}

static int
sort_apps_by_usage (gconstpointer a,
                    gconstpointer b,
                    gpointer      datap)
{
  ShellAppUsage *self = datap;
  ShellApp *app_a, *app_b;
  UsageData *usage_a, *usage_b;

  app_a = (ShellApp*)a;
  app_b = (ShellApp*)b;

  usage_a = g_hash_table_lookup (self->app_usages, shell_app_get_id (app_a));
  usage_b = g_hash_table_lookup (self->app_usages, shell_app_get_id (app_b));

  return usage_b->score - usage_a->score;
}

/**
 * shell_app_usage_get_most_used:
 * @usage: the usage instance to request
 *
 * Returns: (element-type ShellApp) (transfer full): List of applications
 */
GSList *
shell_app_usage_get_most_used (ShellAppUsage   *self)
{
  GSList *apps;
  char *appid;
  ShellAppSystem *appsys;
  GHashTableIter iter;

  appsys = shell_app_system_get_default ();

  g_hash_table_iter_init (&iter, self->app_usages);
  apps = NULL;
  while (g_hash_table_iter_next (&iter, (gpointer *) &appid, NULL))
    {
      ShellApp *app;

      app = shell_app_system_lookup_app (appsys, appid);
      if (!app)
        continue;

      apps = g_slist_prepend (apps, g_object_ref (app));
    }

  apps = g_slist_sort_with_data (apps, sort_apps_by_usage, self);

  return apps;
}


/**
 * shell_app_usage_compare:
 * @self: the usage instance to request
 * @id_a: ID of first app
 * @id_b: ID of second app
 *
 * Compare @id_a and @id_b based on frequency of use.
 *
 * Returns: -1 if @id_a ranks higher than @id_b, 1 if @id_b ranks higher
 *          than @id_a, and 0 if both rank equally.
 */
int
shell_app_usage_compare (ShellAppUsage *self,
                         const char    *id_a,
                         const char    *id_b)
{
  UsageData *usage_a, *usage_b;

  usage_a = g_hash_table_lookup (self->app_usages, id_a);
  usage_b = g_hash_table_lookup (self->app_usages, id_b);

  if (usage_a == NULL && usage_b == NULL)
    return 0;
  else if (usage_a == NULL)
    return 1;
  else if (usage_b == NULL)
    return -1;

  return usage_b->score - usage_a->score;
}

static void
ensure_queued_save (ShellAppUsage *self)
{
  if (self->save_id != 0)
    return;
  self->save_id = g_timeout_add_seconds (SAVE_APPS_TIMEOUT_SECONDS, idle_save_application_usage, self);
  g_source_set_name_by_id (self->save_id, "[gnome-shell] idle_save_application_usage");
}

/* Clean up apps we see rarely.
 * The logic behind this is that if an app was seen less than SCORE_MIN times
 * and not seen for a week, it can probably be forgotten about.
 * This should much reduce the size of the list and avoid 'pollution'. */
static gboolean
idle_clean_usage (ShellAppUsage *self)
{
  GHashTableIter iter;
  UsageData *usage;
  long current_time;
  long week_ago;

  current_time = get_time ();
  week_ago = current_time - (7 * 24 * 60 * 60);

  g_hash_table_iter_init (&iter, self->app_usages);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &usage))
    {
      if ((usage->score < SCORE_MIN) &&
          (usage->last_seen < week_ago))
        g_hash_table_iter_remove (&iter);
    }

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
  if (!ret)
    goto out;

  ret = write_escaped (stream, str, error);
  if (!ret)
    goto out;

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
  ShellAppUsage *self = SHELL_APP_USAGE (data);
  char *id;
  GHashTableIter iter;
  UsageData *usage;
  GFileOutputStream *output;
  GOutputStream *buffered_output;
  GDataOutputStream *data_output;
  GError *error = NULL;

  self->save_id = 0;

  /* Parent directory is already created by shell-global */
  output = g_file_replace (self->configfile, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error);
  if (!output)
    {
      g_debug ("Could not save applications usage data: %s", error->message);
      g_error_free (error);
      return FALSE;
    }
  buffered_output = g_buffered_output_stream_new (G_OUTPUT_STREAM (output));
  g_object_unref (output);
  data_output = g_data_output_stream_new (G_OUTPUT_STREAM (buffered_output));
  g_object_unref (buffered_output);

  if (!g_data_output_stream_put_string (data_output, "<?xml version=\"1.0\"?>\n<application-state>\n", NULL, &error))
    goto out;
  if (!g_data_output_stream_put_string (data_output, "  <context id=\"\">\n", NULL, &error))
    goto out;

  g_hash_table_iter_init (&iter, self->app_usages);

  while (g_hash_table_iter_next (&iter, (gpointer *) &id, (gpointer *) &usage))
    {
      ShellApp *app;

      app = shell_app_system_lookup_app (shell_app_system_get_default(), id);

      if (!app)
        continue;

      if (!g_data_output_stream_put_string (data_output, "    <application", NULL, &error))
        goto out;
      if (!write_attribute_string (data_output, "id", id, &error))
        goto out;
      if (!write_attribute_double (data_output, "score", usage->score, &error))
        goto out;
      if (!write_attribute_uint (data_output, "last-seen", usage->last_seen, &error))
        goto out;
      if (!g_data_output_stream_put_string (data_output, "/>\n", NULL, &error))
        goto out;
    }
  if (!g_data_output_stream_put_string (data_output, "  </context>\n", NULL, &error))
    goto out;
  if (!g_data_output_stream_put_string (data_output, "</application-state>\n", NULL, &error))
    goto out;

out:
  if (!error)
    g_output_stream_close_async (G_OUTPUT_STREAM (data_output), 0, NULL, NULL, NULL);
  g_object_unref (data_output);
  if (error)
    {
      g_debug ("Could not save applications usage data: %s", error->message);
      g_error_free (error);
    }
  return FALSE;
}

static void
shell_app_usage_start_element_handler  (GMarkupParseContext *context,
                                        const gchar         *element_name,
                                        const gchar        **attribute_names,
                                        const gchar        **attribute_values,
                                        gpointer             user_data,
                                        GError             **error)
{
  ShellAppUsage *self = user_data;

  if (strcmp (element_name, "application-state") == 0)
    {
    }
  else if (strcmp (element_name, "context") == 0)
    {
    }
  else if (strcmp (element_name, "application") == 0)
    {
      const char **attribute;
      const char **value;
      UsageData *usage;
      char *appid = NULL;

      for (attribute = attribute_names, value = attribute_values; *attribute; attribute++, value++)
        {
          if (strcmp (*attribute, "id") == 0)
            {
              appid = g_strdup (*value);
              break;
            }
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

      usage = g_new0 (UsageData, 1);
      g_hash_table_insert (self->app_usages, appid, usage);

      for (attribute = attribute_names, value = attribute_values; *attribute; attribute++, value++)
        {
          if (strcmp (*attribute, "score") == 0)
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

static GMarkupParser app_state_parse_funcs =
{
  shell_app_usage_start_element_handler,
  NULL,
  NULL,
  NULL,
  NULL
};

/* Load data about apps usage from file */
static void
restore_from_file (ShellAppUsage *self)
{
  GFileInputStream *input;
  GMarkupParseContext *parse_context;
  GError *error = NULL;
  char buf[1024];

  input = g_file_read (self->configfile, NULL, &error);
  if (error)
    {
      if (error->code != G_IO_ERROR_NOT_FOUND)
        g_warning ("Could not load applications usage data: %s", error->message);

      g_error_free (error);
      return;
    }

  parse_context = g_markup_parse_context_new (&app_state_parse_funcs, 0, self, NULL);

  while (TRUE)
    {
      gssize count = g_input_stream_read ((GInputStream*) input, buf, sizeof(buf), NULL, &error);
      if (count <= 0)
        goto out;
      if (!g_markup_parse_context_parse (parse_context, buf, count, &error))
        goto out;
     }

out:
  g_markup_parse_context_free (parse_context);
  g_input_stream_close ((GInputStream*)input, NULL, NULL);
  g_object_unref (input);

  idle_clean_usage (self);

  if (error)
    {
      g_warning ("Could not load applications usage data: %s", error->message);
      g_error_free (error);
    }
}

/* Enable or disable the timers, depending on the value of ENABLE_MONITORING_KEY
 * and taking care of the previous state.  If selfing is disabled, we still
 * report apps usage based on (possibly) saved data, but don't collect data.
 */
static void
update_enable_monitoring (ShellAppUsage *self)
{
  gboolean enable;

  enable = g_settings_get_boolean (self->privacy_settings,
                                   ENABLE_MONITORING_KEY);

  /* Be sure not to start the timers if they were already set */
  if (enable && !self->enable_monitoring)
    {
      on_focus_app_changed (shell_window_tracker_get_default (), NULL, self);
    }
  /* ...and don't try to stop them if they were not running */
  else if (!enable && self->enable_monitoring)
    {
      if (self->watched_app)
        g_object_unref (self->watched_app);
      self->watched_app = NULL;
      g_clear_handle_id (&self->save_id, g_source_remove);
    }

  self->enable_monitoring = enable;
}

/* Called when the ENABLE_MONITORING_KEY boolean has changed */
static void
on_enable_monitoring_key_changed (GSettings     *settings,
                                  const gchar   *key,
                                  ShellAppUsage *self)
{
  update_enable_monitoring (self);
}

/**
 * shell_app_usage_get_default:
 *
 * Return Value: (transfer none): The global #ShellAppUsage instance
 */
ShellAppUsage *
shell_app_usage_get_default (void)
{
  return shell_global_get_app_usage (shell_global_get ());
}
