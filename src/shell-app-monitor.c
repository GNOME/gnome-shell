/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#include <string.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/scrnsaver.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>


#include "shell-app-monitor.h"
#include "shell-global.h"


/* This file includes modified code from
 * desktop-data-engine/engine-dbus/hippo-application-monitor.c
 * in the functions collecting application usage data.
 * Written by Owen Taylor, originally licensed under LGPL 2.1.
 * Copyright Red Hat, Inc. 2006-2008
 */

#define APP_MONITOR_GCONF_DIR SHELL_GCONF_DIR"/app_monitor"
#define ENABLE_MONITORING_KEY APP_MONITOR_GCONF_DIR"/enable_monitoring"

/* Data is saved to file SHELL_CONFIG_DIR/DATA_FILENAME */
#define DATA_FILENAME "applications_usage"

/* How often we save internally app data, in seconds */
#define SAVE_APPS_TIMEOUT 3600    /* One hour */

/* How often we save internally app data in burst mode */
#define SAVE_APPS_BURST_TIMEOUT 120 /* Two minutes */

/* Length of the initial app burst, for each new activity */
#define SAVE_APPS_BURST_LENGTH 3600       /* One hour */

/* The ranking algorithm we use is: every time an app score reaches SCORE_MAX,
 * divide all scores by 2. Scores are raised by 1 unit every SAVE_APPS_TIMEOUT
 * seconds. This mechanism allows the list to update relatively fast when
 * a new app is used intensively.
 * To keep the list clean, and avoid being Big Brother, apps that have not been
 * seen for a week and whose score is below SCORE_MIN are removed.
 */
 
/* With this value, an app goes from bottom to top of the 
 * popularity list in 50 hours of use */
#define SCORE_MAX (3600*50/SAVE_APPS_TIMEOUT)

/* If an app's score in lower than this and the app has not been used in a week,
 * remove it */
#define SCORE_MIN 5

/* Title patterns to detect apps that don't set WM class as needed.
 * Format: pseudo/wanted WM class, title regex pattern, NULL (for GRegex) */
static struct
{
  const char *app_id;
  const char *pattern;
  GRegex *regex;
} title_patterns[] =  {
    {"openoffice.org-writer", ".* - OpenOffice.org Writer$", NULL}, \
    {"openoffice.org-calc", ".* - OpenOffice.org Calc$", NULL}, \
    {"openoffice.org-impress", ".* - OpenOffice.org Impress$", NULL}, \
    {"openoffice.org-draw", ".* - OpenOffice.org Draw$", NULL}, \
    {"openoffice.org-base", ".* - OpenOffice.org Base$", NULL}, \
    {"openoffice.org-math", ".* - OpenOffice.org Math$", NULL}, \
    {NULL, NULL, NULL}
};


typedef struct AppPopularity AppPopularity;
typedef struct ActiveAppsData ActiveAppsData;

struct _ShellAppMonitor
{
  GObject parent;


  GFile *configfile;
  XScreenSaverInfo *info;
  GdkDisplay *display;
  GConfClient *gconf_client;
  glong activity_time;
  gulong last_idle;
  guint poll_id;
  guint save_apps_id;
  guint gconf_notify;
  gboolean currently_idle;
  gboolean enable_monitoring;

  GHashTable *apps_by_wm_class; /* Seen apps by wm_class */
  GHashTable *popularities; /* One AppPopularity struct list per activity */
  int upload_apps_burst_count;
};

G_DEFINE_TYPE (ShellAppMonitor, shell_app_monitor, G_TYPE_OBJECT);

/* Represents an application record for a given activity */
struct AppPopularity
{
  gchar *wm_class;
  gdouble score; /* Based on the number of times we'e seen the app and normalized */
  guint32 last_seen; /* Used to clear old apps we've only seen a few times */
};

struct ActiveAppsData
{
  int activity;
  GSList *result;
  GTime start_time;
};

enum {
  CHANGED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void shell_app_monitor_finalize (GObject *object);

static void get_active_apps (ShellAppMonitor *monitor,
                             int              in_last_seconds,
                             GSList         **wm_classes,
                             int             *activity);

static void save_active_apps (ShellAppMonitor *monitor,
                              int              collection_period,
                              GSList          *wm_classes,
                              int              activity);

static gboolean poll_for_idleness (void *data);

static gboolean on_save_apps_timeout (gpointer data);

static void save_to_file (ShellAppMonitor *monitor);

static void restore_from_file (ShellAppMonitor *monitor);

static void update_enable_monitoring (ShellAppMonitor *monitor);

static void on_enable_monitoring_key_changed (GConfClient *client,
                                              guint        connexion_id,
                                              GConfEntry  *entry,
                                              gpointer     monitor);

static glong
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

/* Little callback to destroy lists inside hash table */
static void
destroy_popularity (gpointer key,
                    gpointer value,
                    gpointer user_data)
{
  GSList *list = value;
  GSList *l;
  AppPopularity *app_popularity;
  for (l = list; l; l = l->next)
    {
      app_popularity = (AppPopularity *) l->data;
      g_free (app_popularity->wm_class);
      g_free (app_popularity);
    }
  g_slist_free (list);
}

static void
shell_app_monitor_init (ShellAppMonitor *self)
{
  int event_base, error_base;
  GdkDisplay *display;
  Display *xdisplay;
  char *path;
  char *shell_config_dir;

  /* FIXME: should we create as many monitors as there are GdkScreens? */
  display = gdk_display_get_default();
  xdisplay = GDK_DISPLAY_XDISPLAY (display);
  if (!XScreenSaverQueryExtension (xdisplay, &event_base, &error_base))
    {
      g_warning ("Screensaver extension not found on X display, can't detect user idleness");
    }
  
  self->display = g_object_ref (display);
  self->info = XScreenSaverAllocInfo ();

  self->activity_time = get_time ();
  self->last_idle = 0;
  self->currently_idle = FALSE;
  self->enable_monitoring = FALSE;

  /* No need for free functions: value is an int stored as a pointer, and keys are
   * freed manually in finalize () since we replace elements and reuse app names */
  self->popularities = g_hash_table_new (g_direct_hash, g_direct_equal);
  self->apps_by_wm_class = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  (GDestroyNotify) g_free,
                                                  (GDestroyNotify) g_free);

  g_object_get (shell_global_get(), "configdir", &shell_config_dir, NULL),
  path = g_build_filename (shell_config_dir, DATA_FILENAME, NULL);
  g_free (shell_config_dir);
  self->configfile = g_file_new_for_path (path);
  restore_from_file (self);

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

  XFree (self->info);
  g_source_remove (self->poll_id);
  g_source_remove (self->save_apps_id);
  gconf_client_notify_remove (self->gconf_client, self->gconf_notify);
  g_object_unref (self->gconf_client);
  g_object_unref (self->display);
  g_hash_table_destroy (self->apps_by_wm_class);
  g_hash_table_foreach (self->popularities, destroy_popularity, NULL);
  g_hash_table_destroy (self->popularities);
  for (i = 0; title_patterns[i].app_id; i++)
    g_regex_unref (title_patterns[i].regex);
  g_object_unref (self->configfile);

  G_OBJECT_CLASS (shell_app_monitor_parent_class)->finalize(object);
}

/**
 * shell_app_monitor_get_apps:
 *
 * Get a list of desktop identifiers representing the most popular applications
 * for a given activity.
 *
 * @monitor: the app monitor instance to request
 * @activity: the activity for which stats are considered
 * @max_count: how many applications are requested. Note that the actual 
 *     list size may be less, or NULL if not enough applications are registered.
 *
 * Returns: (element-type utf8) (transfer full): List of application desktop
 *     identifiers, in low case
 */
GSList *
shell_app_monitor_get_apps (ShellAppMonitor *monitor,
                            gint             activity,
                            gint             max_count)
{
  GSList *list = NULL;
  GSList *popularity;
  AppPopularity *app_popularity;
  int i;
  
  popularity = g_hash_table_lookup (monitor->popularities,
                                    GINT_TO_POINTER (activity));
  
  for (i = 0; i < max_count; i++)
    {
      if (!popularity)
        break;
      app_popularity = (AppPopularity *) (popularity->data);
      list = g_slist_prepend (list, g_utf8_strdown (app_popularity->wm_class, -1));
      popularity = popularity->next;
    }
  list = g_slist_reverse (list);
  return list;
}

/* Find the active window in order to collect stats */
void
get_active_app_properties (ShellAppMonitor *monitor,
                           char           **wm_class,
                           char           **title)
{
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (monitor->display);
  int n_screens = gdk_display_get_n_screens (monitor->display);
  Atom net_active_window_x =
    gdk_x11_get_xatom_by_name_for_display (monitor->display,
                                           "_NET_ACTIVE_WINDOW");
  GdkAtom net_active_window_gdk =
    gdk_atom_intern ("_NET_ACTIVE_WINDOW", FALSE);
  Window active_window = None;
  int i;

  Atom type;
  int format;
  unsigned long n_items;
  unsigned long bytes_after;
  guchar *data;
  gboolean is_desktop = FALSE;

  if (wm_class)
    *wm_class = NULL;
  if (title)
    *title = NULL;

  /* Find the currently focused window by looking at the _NET_ACTIVE_WINDOW property
   * on all the screens of the display.
   */
  for (i = 0; i < n_screens; i++)
    {
      GdkScreen *screen = gdk_display_get_screen (monitor->display, i);
      GdkWindow *root = gdk_screen_get_root_window (screen);

      if (!gdk_x11_screen_supports_net_wm_hint (screen, net_active_window_gdk))
        continue;

      XGetWindowProperty (xdisplay, GDK_DRAWABLE_XID (root),
                          net_active_window_x,
                          0, 1, False, XA_WINDOW,
                          &type, &format, &n_items, &bytes_after, &data);
      if (type == XA_WINDOW)
        {
          active_window = *(Window *) data;
          XFree (data);
          break;
        }
    }

  /* Now that we have the active window, figure out the app name and WM class
   */
  gdk_error_trap_push ();

  if (active_window && wm_class)
    {
      if (XGetWindowProperty (xdisplay, active_window,
                              XA_WM_CLASS,
                              0, G_MAXLONG, False, XA_STRING,
                              &type, &format, &n_items, &bytes_after,
                              &data) == Success && type == XA_STRING)
        {
          if (format == 8)
            {
              char **list;
              int count;

              count =
                gdk_text_property_to_utf8_list_for_display (monitor->display,
                                                            GDK_TARGET_STRING,
                                                            8, data, n_items,
                                                            &list);

              if (count > 1)
                {
                  /* This is a check for Nautilus, which sets the instance to this
                   * value for the desktop window; we do this rather than check for
                   * the more general _NET_WM_WINDOW_TYPE_DESKTOP to avoid having
                   * to do another XGetProperty on every iteration. We generally
                   * don't want to count the desktop being focused as app
                   * usage because it frequently can be a false-positive on an
                   * empty workspace.
                   */
                  if (strcmp (list[0], "desktop_window") == 0)
                    is_desktop = TRUE;
                  else
                    *wm_class = g_strdup (list[1]);
                }

              if (list)
                g_strfreev (list);
            }

          XFree (data);
        }
    }

  if (is_desktop)
    active_window = None;

  if (active_window && title)
    {
      Atom utf8_string =
        gdk_x11_get_xatom_by_name_for_display (monitor->display,
                                               "UTF8_STRING");

      if (XGetWindowProperty (xdisplay, active_window,
                              gdk_x11_get_xatom_by_name_for_display
                              (monitor->display, "_NET_WM_NAME"), 0,
                              G_MAXLONG, False, utf8_string, &type, &format,
                              &n_items, &bytes_after, &data) == Success
          && type == utf8_string)
        {
          if (format == 8 && g_utf8_validate ((char *) data, -1, NULL))
            {
              *title = g_strdup ((char *) data);
            }

          XFree (data);
        }
    }

  if (active_window && title && *title == NULL)
    {
      if (XGetWindowProperty (xdisplay, active_window,
                              XA_WM_NAME,
                              0, G_MAXLONG, False, AnyPropertyType,
                              &type, &format, &n_items, &bytes_after,
                              &data) == Success && type != None)
        {
          if (format == 8)
            {
              char **list;
              int count;

              count =
                gdk_text_property_to_utf8_list_for_display (monitor->display,
                                                            gdk_x11_xatom_to_atom_for_display
                                                            (monitor->display,
                                                             type), 8, data,
                                                            n_items, &list);

              if (count > 0)
                *title = g_strdup (list[0]);

              if (list)
                g_strfreev (list);
            }

          XFree (data);
        }
    }
  gdk_error_trap_pop ();
}

void
update_app_info (ShellAppMonitor *monitor)
{
  char *wm_class;
  char *title;
  GHashTable *app_active_times = NULL; /* GTime spent per activity */
  static gboolean first_time = TRUE;
  int activity;
  guint32 timestamp;
  int i;
  
  if (first_time) /* Generate match patterns once for all */
    {
      first_time = FALSE;
      for (i = 0; title_patterns[i].app_id; i++)
        {
          title_patterns[i].regex = g_regex_new (title_patterns[i].pattern,
                                                 0, 0, NULL);
        }
    }

  get_active_app_properties (monitor, &wm_class, &title);

  /* Match window title patterns to identifiers for non-standard apps */
  if (title)
    {
      for (i = 0; title_patterns[i].app_id; i++)
        {
          if ( g_regex_match (title_patterns[i].regex, title, 0, NULL) )
            {
              /* Set a pseudo WM class, handled like true ones */
              g_free (wm_class);              
              wm_class = g_strdup(title_patterns[i].app_id);
              break;
            }
        }
      g_free (title);
    }  
  
  if (!wm_class)
    return;
  
  app_active_times = g_hash_table_lookup (monitor->apps_by_wm_class, wm_class);
  if (!app_active_times)
    {
      /* Create a hash table to save times per activity
       * Value and key are int stored as pointers, no need to free them */
      app_active_times = g_hash_table_new (g_direct_hash, g_direct_equal);
      g_hash_table_replace (monitor->apps_by_wm_class, g_strdup (wm_class),
                            app_active_times);
    }

  timestamp = get_time ();
  activity = 0;
  g_hash_table_replace (app_active_times, GINT_TO_POINTER (activity),
                        GINT_TO_POINTER (timestamp));

  g_free (wm_class);
}

static gboolean
poll_for_idleness (gpointer data)
{
  ShellAppMonitor *monitor = data;
  int i;
  int n_screens;
  unsigned long idle_time;
  gboolean was_idle;

  idle_time = G_MAXINT;
  n_screens = gdk_display_get_n_screens (monitor->display);
  for (i = 0; i < n_screens; ++i)
    {
      int result = 0;
      GdkScreen *screen;

      screen = gdk_display_get_screen (monitor->display, i);
      result = XScreenSaverQueryInfo (GDK_DISPLAY_XDISPLAY (monitor->display),
                                      GDK_SCREEN_XSCREEN (screen)->root,
                                      monitor->info);
      if (result == 0)
        {
          g_warning ("Failed to get idle time from screensaver extension");
          break;
        }

      /* monitor->info->idle is time in milliseconds since last user interaction event */
      idle_time = MIN (monitor->info->idle, idle_time);
    }

  was_idle = monitor->currently_idle;

  /* If the idle time has gone down, there must have been activity since we last checked */
  if (idle_time < monitor->last_idle)
    {
      monitor->activity_time = get_time ();
      monitor->currently_idle = FALSE;
    }
  else
    {
      /* If no activity, see how long ago it was and count ourselves idle 
       * if it's been a short while. We keep this idle really short,
       * so it can be "more aggressive" about idle detection than
       * a screensaver would be.
       */
      GTime now = get_time ();
      if (now < monitor->activity_time)
        {
          /* clock went backward... just "catch up" 
           * then wait until the idle timeout expires again
           */
          monitor->activity_time = now;
        }
      else if ((now - monitor->activity_time) > 120)
        {                       /* 120 = 2 minutes */
          monitor->currently_idle = TRUE;
        }
    }

  monitor->last_idle = idle_time;

  if (!monitor->currently_idle)
    {
      update_app_info (monitor);
    }

  return TRUE;
}

/* Used to iterate over apps to create a list of those that have been active
 * since the activity specified by app_data has been started */
static void
active_apps_foreach (const gpointer key,
                     const gpointer value,
                     gpointer       data)
{
  char *name = key;
  GHashTable *app_active_times = value; /* GTime spent per activity */
  ActiveAppsData *app_data = data;
  GTime active_time;
  
  /* Only return apps that have been used in the current activity */
  active_time = GPOINTER_TO_INT (g_hash_table_lookup
                                 (app_active_times, GINT_TO_POINTER (app_data->activity)));
  if (active_time > app_data->start_time)
    app_data->result = g_slist_prepend (app_data->result, g_strdup (name));
}

/*
 * Returns list of application names we've seen the user interacting with
 * within the last 'in_last_seconds' seconds and in the specified activity.
 * Free the names in the GSList with g_free(), the lists themselves with
 * g_slist_free().
 */
static void
get_active_apps (ShellAppMonitor *monitor,
                 int              in_last_seconds,
                 GSList         **wm_classes,
                 int             *activity)
{
  ActiveAppsData app_data;
  guint32 now;

  now = get_time ();
  app_data.activity = 0;
  *activity = app_data.activity; /* Be sure we use the exact same timestamp everywhere */
  app_data.start_time = now - in_last_seconds;

  if (wm_classes && g_hash_table_size (monitor->apps_by_wm_class))
    {
      app_data.result = NULL;
      g_hash_table_foreach (monitor->apps_by_wm_class, active_apps_foreach,
                            &app_data);
      *wm_classes = app_data.result;
    }
}

static gboolean
on_save_apps_timeout (gpointer data)
{
  ShellAppMonitor *monitor = (ShellAppMonitor *) data;
  static guint32 period = SAVE_APPS_TIMEOUT;

  if (monitor->upload_apps_burst_count >= 0)
    {
      period = SAVE_APPS_BURST_TIMEOUT;
      monitor->upload_apps_burst_count--;

      if (monitor->upload_apps_burst_count == 0)
        {
          g_source_remove (monitor->save_apps_id);
          g_timeout_add_seconds (SAVE_APPS_TIMEOUT, on_save_apps_timeout, monitor);
        }
    }

  GSList *wm_classes = NULL;
  int activity;

  get_active_apps (monitor, period, &wm_classes, &activity);

  if (wm_classes)
    {
      save_active_apps (monitor, period, wm_classes, activity);
      save_to_file (monitor);

      if (wm_classes)
        {
          g_slist_foreach (wm_classes, (GFunc) g_free, NULL);
          g_slist_free (wm_classes);
        }
    }

  return TRUE;
}

/* Used to find an app item from its wm_class, when non empty */
static gint
popularity_find_app (gconstpointer list_data,
                     gconstpointer user_data)
{ 
  AppPopularity *list_pop = (AppPopularity *) list_data;
  AppPopularity *user_pop = (AppPopularity *) user_data;
  return strcmp (list_pop->wm_class, user_pop->wm_class);
}

/* Used to sort highest scores at the top */
static gint popularity_sort_apps (gconstpointer data1,
                                  gconstpointer data2)
{
  const AppPopularity *pop1 = data1;
  const AppPopularity *pop2 = data2;
  
  if (pop1->score > pop2->score)
    return -1;
  else if (pop1->score == pop2->score)
    return 0;
  else
    return 1;
}

/* Limit the score to a certain level so that most popular apps can change */
static void
normalize_popularity (GSList *list)
{
  if (!list)
    return;
  
  AppPopularity *app_popularity;

  /* Highest score since list is sorted */
  app_popularity = (AppPopularity *) (list->data);
  /* Limiting score allows new apps to catch up (see SCORE_MAX definition) */
  if (app_popularity->score > SCORE_MAX)
    while (list)
      {
        app_popularity = (AppPopularity *) (list->data);
        app_popularity->score /= 2;
        list = list->next;
      }
}
  
/* Clean up apps we see rarely.
 * The logic behind this is that if an app was seen less than SCORE_MIN times
 * and not seen for a week, it can probably be forgotten about.
 * This should much reduce the size of the list and avoid 'pollution'. */
static GSList *
clean_popularity (GSList *list)
{
  AppPopularity *app_popularity;
  GDate *date;
  guint32 date_days;
  GSList *next, *head;

  date = g_date_new ();
  g_date_set_time_t (date, time (NULL));
  g_date_subtract_days (date, 7);
  date_days = g_date_get_julian (date);
  head = list;
  while (list)
  {
    next = list->next;
    app_popularity = (AppPopularity *) (list->data);
    if ((app_popularity->score < SCORE_MIN) &&
          (app_popularity->last_seen < date_days))
      head = g_slist_remove (head, list);
    list = next;
  }
  g_date_free (date);
  return head;
}

/* Save apps data internally to lists, merging if necessary */
static void
save_active_apps (ShellAppMonitor *monitor,
                  int              collection_period,
                  GSList          *wm_classes,
                  int              activity)
{
  AppPopularity *app_popularity;
  AppPopularity temp; /* We only set/use two fields here */
  GDate *date;
  guint32 date_days;
  GSList *popularity;
  GSList *item;
  GSList *l;
  
  popularity = g_hash_table_lookup (monitor->popularities,
                                    GINT_TO_POINTER (activity));
  date = g_date_new ();
  g_date_set_time_t (date, time (NULL));
  date_days = g_date_get_julian (date);
  if (!popularity) /* Just create the list using provided information */
    {
      for (l = wm_classes; l; l = l->next)
        {
          app_popularity = g_new (AppPopularity, 1);
          app_popularity->last_seen = date_days;
          app_popularity->score = 1;
          /* Copy data from the old list */
          app_popularity->wm_class = g_strdup ((gchar *) l->data);
          popularity = g_slist_prepend (popularity, app_popularity);
        }
    }
  else /* Merge with old data */
    {
      for (l = wm_classes; l; l = l->next)
        {
          temp.wm_class = (gchar *) l->data;
          
          item = g_slist_find_custom (popularity, &temp, popularity_find_app);
          if (!item)
            {
              app_popularity = g_new (AppPopularity, 1);
              app_popularity->score = 1;
              /* Copy data from other lists */
              app_popularity->wm_class = g_strdup ((gchar *) l->data);
              popularity = g_slist_prepend (popularity, app_popularity);
            }
          else
            {
              app_popularity = (AppPopularity *) item->data;
              app_popularity->score++;
            }
          app_popularity->last_seen = date_days;
        }
    }

    /* Clean once in a while, doing so at start may no be enough if uptime is high */
    popularity = clean_popularity (popularity);
    /* Need to do this often since SCORE_MAX should be relatively low */
    normalize_popularity (popularity);
    popularity = g_slist_sort (popularity, popularity_sort_apps);
    g_hash_table_replace (monitor->popularities, GINT_TO_POINTER (activity),
                          popularity);
    g_date_free (date);

    g_signal_emit (monitor, signals[CHANGED], 0);
}

/* Save app data lists to file */
static void
save_to_file (ShellAppMonitor *monitor)
{
  GHashTableIter iter;
  int activity;
  GSList *popularity;
  AppPopularity *app_popularity;
  GFileOutputStream *output;
  GDataOutputStream *data_output;
  GError *error = NULL;
  gchar *line;
  gchar score_buf[G_ASCII_DTOSTR_BUF_SIZE];
  static int last_error_code = 0;

  /* Parent directory is already created by shell-global */
  output = g_file_replace (monitor->configfile, NULL, FALSE, G_FILE_CREATE_NONE, NULL, &error);
  if (!output)
    {
      if (last_error_code == error->code)
        {
          g_error_free (error);
          return;
        }
      last_error_code = error->code;
      g_warning ("Could not save applications usage data: %s. This warning will be printed only once.", error->message);
      g_error_free (error);
      return;
    }
  data_output = g_data_output_stream_new (G_OUTPUT_STREAM(output));
  g_object_unref (output);
    
  g_hash_table_iter_init (&iter, monitor->popularities);
  while (g_hash_table_iter_next (&iter, (gpointer *) &activity, (gpointer *) &popularity)
         && popularity)
    { 
      line = g_strdup_printf ("%i\n", activity);
      g_data_output_stream_put_string (data_output, "--\n", NULL, NULL);
      g_data_output_stream_put_string (data_output, line, NULL, NULL);
      g_free (line);

      do
        {
          app_popularity = (AppPopularity *) popularity->data;
          g_ascii_dtostr (score_buf, sizeof (score_buf), app_popularity->score);
          line = g_strdup_printf ("%s,%s,%u\n", app_popularity->wm_class,
                                  score_buf, app_popularity->last_seen);
          g_data_output_stream_put_string (data_output, line, NULL, &error);
          g_free (line);
          if (error)
            goto out;
        }
      while ( (popularity = popularity->next) );
    }

  out:
  g_output_stream_close (G_OUTPUT_STREAM(data_output), NULL, &error);
  g_object_unref (data_output);
  if (error)
    {
      if (last_error_code == error->code)
        {
          g_error_free (error);
          return;
        }
      last_error_code = error->code;
      g_warning ("Could not save applications usage data: %s. This warning will be printed only once.", error->message);
      g_error_free (error);
    }
}

/* Load data about apps usage from file */
static void
restore_from_file (ShellAppMonitor *monitor)
{
  int activity = -1; /* Means invalid ID */
  GSList *popularity = NULL;
  AppPopularity *app_popularity;
  GFileInputStream *input;
  GDataInputStream *data_input;
  GError *error = NULL;
  gchar *line;
  gchar **info;
  
  input = g_file_read (monitor->configfile, NULL, &error);
  if (error)
    {
      if (error->code != G_IO_ERROR_NOT_FOUND)
        g_warning ("Could not load applications usage data: %s", error->message);

      g_error_free (error);
      return;
    }

  data_input = g_data_input_stream_new (G_INPUT_STREAM(input));
  g_object_unref (input);

  while (TRUE)
    {
      line = g_data_input_stream_read_line (data_input, NULL, NULL, &error);
      if (!line)
        goto out;
      if (strcmp (line, "--") == 0) /* Line starts a new activity */
        {
          g_free (line);
          line = g_data_input_stream_read_line (data_input, NULL, NULL, &error);
          if (line && (strcmp (line, "") != 0))
            {
              if (activity != -1) /* Save previous activity, cleaning and sorting it */
                {
                  popularity = clean_popularity (popularity);
                  popularity = g_slist_sort (popularity, popularity_sort_apps);
                  g_hash_table_replace (monitor->popularities,
                                        GINT_TO_POINTER (activity), popularity);
                  popularity = NULL;
                }
              activity = atoi (line);
              /* FIXME: do something if conversion fails! */
              /* like: errno = NULL; ... if (errno) { g_free (line); goto out; } */
            }
          else if (error)
            {
              g_free (line);
              goto out;
            }
          else /* End of file */
            goto out;
        }
      /* Line is about an app.
       * If no activity was provided yet, just skip */
      else if ((activity != -1) && (strcmp (line, "") != 0))
        {
          info = g_strsplit (line, ",", 0);
          if (info[0] && info [1] && info[2]) /* Skip on wrong syntax */
            {
              app_popularity = g_new (AppPopularity, 1);
              app_popularity->wm_class = g_strdup(info[0]);
              app_popularity->score = g_ascii_strtod (info[1], NULL);
              app_popularity->last_seen = (guint32) strtoul (info[2], NULL, 10);
              popularity = g_slist_prepend (popularity, app_popularity);
            }

          g_strfreev (info);
          g_free (line);
        }
      else
        g_free (line); /* Just skip to next app */
     }

out:
  if (activity != -1) /* Save last activity, cleaning and sorting it */
    {
      popularity = clean_popularity (popularity);
      popularity = g_slist_sort  (popularity, popularity_sort_apps);
      g_hash_table_replace (monitor->popularities, GINT_TO_POINTER (activity),
                            popularity);
    }

    g_input_stream_close (G_INPUT_STREAM (data_input), NULL, NULL);
    g_object_unref (data_input);
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
      /* If no stats are available so far, set burst mode on */
      if (g_hash_table_size (monitor->popularities))
        monitor->upload_apps_burst_count = 0;
      else
        monitor->upload_apps_burst_count = SAVE_APPS_BURST_LENGTH / SAVE_APPS_BURST_TIMEOUT;

      monitor->poll_id = g_timeout_add_seconds (5, poll_for_idleness, monitor);
      if (monitor->upload_apps_burst_count > 0)
        monitor->save_apps_id =
          g_timeout_add_seconds (SAVE_APPS_BURST_TIMEOUT, on_save_apps_timeout, monitor);
      else
        monitor->save_apps_id =
          g_timeout_add_seconds (SAVE_APPS_TIMEOUT, on_save_apps_timeout, monitor);
    }
  /* ...and don't try to stop them if they were not running */
  else if (!enable && monitor->enable_monitoring)
    {
      g_source_remove (monitor->poll_id);
      g_source_remove (monitor->save_apps_id);
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
