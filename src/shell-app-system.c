/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include "shell-app-system.h"
#include "shell-app-usage.h"
#include <string.h>

#include <gio/gio.h>
#include <glib/gi18n.h>

#include "shell-app-cache.h"
#include "shell-app-private.h"
#include "shell-window-tracker-private.h"
#include "shell-app-system-private.h"
#include "shell-global.h"
#include "shell-util.h"
#include "st.h"

/* Rescan for at most RESCAN_TIMEOUT_MS * MAX_RESCAN_RETRIES. That
 * should be plenty of time for even a slow spinning drive to update
 * the icon cache.
 */
#define RESCAN_TIMEOUT_MS 2500
#define MAX_RESCAN_RETRIES 6

/* Vendor prefixes are something that can be preprended to a .desktop
 * file name.  Undo this.
 */
static const char*const vendor_prefixes[] = { "gnome-",
                                              "fedora-",
                                              "mozilla-",
                                              "debian-",
                                              NULL };

enum {
   PROP_0,

};

enum {
  APP_STATE_CHANGED,
  INSTALLED_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct _ShellAppSystemPrivate ShellAppSystemPrivate;

struct _ShellAppSystem
{
  GObject parent;

  ShellAppSystemPrivate *priv;
};

struct _ShellAppSystemPrivate {
  GHashTable *running_apps;
  GHashTable *id_to_app;
  GHashTable *startup_wm_class_to_id;

  guint rescan_icons_timeout_id;
  guint n_rescan_retries;
};

static void shell_app_system_finalize (GObject *object);

G_DEFINE_TYPE_WITH_PRIVATE (ShellAppSystem, shell_app_system, G_TYPE_OBJECT);

static void shell_app_system_class_init(ShellAppSystemClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;

  gobject_class->finalize = shell_app_system_finalize;

  signals[APP_STATE_CHANGED] = g_signal_new ("app-state-changed",
                                             SHELL_TYPE_APP_SYSTEM,
                                             G_SIGNAL_RUN_LAST,
                                             0,
                                             NULL, NULL, NULL,
                                             G_TYPE_NONE, 1,
                                             SHELL_TYPE_APP);
  signals[INSTALLED_CHANGED] =
    g_signal_new ("installed-changed",
		  SHELL_TYPE_APP_SYSTEM,
		  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
		  G_TYPE_NONE, 0);
}

static void
scan_startup_wm_class_to_id (ShellAppSystem *self)
{
  ShellAppSystemPrivate *priv = self->priv;
  g_autolist(GAppInfo) all = NULL;
  const GList *l;

  g_hash_table_remove_all (priv->startup_wm_class_to_id);

  all = shell_app_cache_get_all (NULL);

  for (l = all; l != NULL; l = l->next)
    {
      GAppInfo *info = l->data;
      const char *startup_wm_class, *id, *old_id;

      id = g_app_info_get_id (info);
      startup_wm_class = g_desktop_app_info_get_startup_wm_class (G_DESKTOP_APP_INFO (info));

      if (startup_wm_class == NULL)
        continue;

      /* In case multiple .desktop files set the same StartupWMClass, prefer
       * the one where ID and StartupWMClass match */
      old_id = g_hash_table_lookup (priv->startup_wm_class_to_id, startup_wm_class);
      if (old_id == NULL || strcmp (id, startup_wm_class) == 0)
        g_hash_table_insert (priv->startup_wm_class_to_id,
                             g_strdup (startup_wm_class), g_strdup (id));
    }
}

static gboolean
app_is_stale (ShellApp *app)
{
  GDesktopAppInfo *info, *old;
  GAppInfo *old_info, *new_info;
  gboolean is_unchanged;

  if (shell_app_is_window_backed (app))
    return FALSE;

  info = shell_app_cache_get_info (NULL, shell_app_get_id (app));
  if (!info)
    return TRUE;

  old = shell_app_get_app_info (app);
  old_info = G_APP_INFO (old);
  new_info = G_APP_INFO (info);

  is_unchanged =
    g_app_info_should_show (old_info) == g_app_info_should_show (new_info) &&
    strcmp (g_desktop_app_info_get_filename (old),
            g_desktop_app_info_get_filename (info)) == 0 &&
    g_strcmp0 (g_app_info_get_executable (old_info),
               g_app_info_get_executable (new_info)) == 0 &&
    g_strcmp0 (g_app_info_get_commandline (old_info),
               g_app_info_get_commandline (new_info)) == 0 &&
    strcmp (g_app_info_get_name (old_info),
            g_app_info_get_name (new_info)) == 0 &&
    g_strcmp0 (g_app_info_get_description (old_info),
               g_app_info_get_description (new_info)) == 0 &&
    strcmp (g_app_info_get_display_name (old_info),
            g_app_info_get_display_name (new_info)) == 0 &&
    g_icon_equal (g_app_info_get_icon (old_info),
                  g_app_info_get_icon (new_info));

  g_object_unref (info);
  return !is_unchanged;
}

static gboolean
stale_app_remove_func (gpointer key,
                       gpointer value,
                       gpointer user_data)
{
  return app_is_stale (value);
}

static gboolean
rescan_icon_theme_cb (gpointer user_data)
{
  ShellAppSystemPrivate *priv;
  ShellAppSystem *self;
  StTextureCache *texture_cache;
  gboolean rescanned;

  self = (ShellAppSystem *) user_data;
  priv = self->priv;

  texture_cache = st_texture_cache_get_default ();
  rescanned = st_texture_cache_rescan_icon_theme (texture_cache);

  priv->n_rescan_retries++;

  if (rescanned || priv->n_rescan_retries >= MAX_RESCAN_RETRIES)
    {
      priv->n_rescan_retries = 0;
      priv->rescan_icons_timeout_id = 0;
      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

static void
rescan_icon_theme (ShellAppSystem *self)
{
  ShellAppSystemPrivate *priv = self->priv;

  priv->n_rescan_retries = 0;

  if (priv->rescan_icons_timeout_id > 0)
    return;

  priv->rescan_icons_timeout_id = g_timeout_add (RESCAN_TIMEOUT_MS,
                                                 rescan_icon_theme_cb,
                                                 self);
}

static void
installed_changed (ShellAppCache  *cache,
                   ShellAppSystem *self)
{
  rescan_icon_theme (self);
  scan_startup_wm_class_to_id (self);

  g_hash_table_foreach_remove (self->priv->id_to_app, stale_app_remove_func, NULL);

  g_signal_emit (self, signals[INSTALLED_CHANGED], 0, NULL);
}

static void
shell_app_system_init (ShellAppSystem *self)
{
  ShellAppSystemPrivate *priv;
  ShellAppCache *cache;

  self->priv = priv = shell_app_system_get_instance_private (self);

  priv->running_apps = g_hash_table_new_full (NULL, NULL, (GDestroyNotify) g_object_unref, NULL);
  priv->id_to_app = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           NULL,
                                           (GDestroyNotify)g_object_unref);

  priv->startup_wm_class_to_id = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  cache = shell_app_cache_get_default ();
  g_signal_connect (cache, "changed", G_CALLBACK (installed_changed), self);
  installed_changed (cache, self);
}

static void
shell_app_system_finalize (GObject *object)
{
  ShellAppSystem *self = SHELL_APP_SYSTEM (object);
  ShellAppSystemPrivate *priv = self->priv;

  g_hash_table_destroy (priv->running_apps);
  g_hash_table_destroy (priv->id_to_app);
  g_hash_table_destroy (priv->startup_wm_class_to_id);

  if (priv->rescan_icons_timeout_id)
    g_source_remove (priv->rescan_icons_timeout_id);

  G_OBJECT_CLASS (shell_app_system_parent_class)->finalize (object);
}

/**
 * shell_app_system_get_default:
 *
 * Return Value: (transfer none): The global #ShellAppSystem singleton
 */
ShellAppSystem *
shell_app_system_get_default (void)
{
  static ShellAppSystem *instance = NULL;

  if (instance == NULL)
    instance = g_object_new (SHELL_TYPE_APP_SYSTEM, NULL);

  return instance;
}

/**
 * shell_app_system_lookup_app:
 *
 * Find a #ShellApp corresponding to an id.
 *
 * Return value: (transfer none): The #ShellApp for id, or %NULL if none
 */
ShellApp *
shell_app_system_lookup_app (ShellAppSystem   *self,
                             const char       *id)
{
  ShellAppSystemPrivate *priv = self->priv;
  ShellApp *app;
  GDesktopAppInfo *info;

  app = g_hash_table_lookup (priv->id_to_app, id);
  if (app)
    return app;

  info = shell_app_cache_get_info (NULL, id);
  if (!info)
    return NULL;

  app = _shell_app_new (info);
  g_hash_table_insert (priv->id_to_app, (char *) shell_app_get_id (app), app);
  g_object_unref (info);
  return app;
}

/**
 * shell_app_system_lookup_heuristic_basename:
 * @system: a #ShellAppSystem
 * @id: Probable application identifier
 *
 * Find a valid application corresponding to a given
 * heuristically determined application identifier
 * string, or %NULL if none.
 *
 * Returns: (transfer none): A #ShellApp for @name
 */
ShellApp *
shell_app_system_lookup_heuristic_basename (ShellAppSystem *system,
                                            const char     *name)
{
  ShellApp *result;
  const char *const *prefix;

  result = shell_app_system_lookup_app (system, name);
  if (result != NULL)
    return result;

  for (prefix = vendor_prefixes; *prefix != NULL; prefix++)
    {
      char *tmpid = g_strconcat (*prefix, name, NULL);
      result = shell_app_system_lookup_app (system, tmpid);
      g_free (tmpid);
      if (result != NULL)
        return result;
    }

  return NULL;
}

/**
 * shell_app_system_lookup_desktop_wmclass:
 * @system: a #ShellAppSystem
 * @wmclass: (nullable): A WM_CLASS value
 *
 * Find a valid application whose .desktop file, without the extension
 * and properly canonicalized, matches @wmclass.
 *
 * Returns: (transfer none): A #ShellApp for @wmclass
 */
ShellApp *
shell_app_system_lookup_desktop_wmclass (ShellAppSystem *system,
                                         const char     *wmclass)
{
  char *canonicalized;
  char *desktop_file;
  ShellApp *app;

  if (wmclass == NULL)
    return NULL;

  /* First try without changing the case (this handles
     org.example.Foo.Bar.desktop applications)

     Note that is slightly wrong in that Gtk+ would set
     the WM_CLASS to Org.example.Foo.Bar, but it also
     sets the instance part to org.example.Foo.Bar, so we're ok
  */
  desktop_file = g_strconcat (wmclass, ".desktop", NULL);
  app = shell_app_system_lookup_heuristic_basename (system, desktop_file);
  g_free (desktop_file);

  if (app)
    return app;

  canonicalized = g_ascii_strdown (wmclass, -1);

  /* This handles "Fedora Eclipse", probably others.
   * Note g_strdelimit is modify-in-place. */
  g_strdelimit (canonicalized, " ", '-');

  desktop_file = g_strconcat (canonicalized, ".desktop", NULL);

  app = shell_app_system_lookup_heuristic_basename (system, desktop_file);

  g_free (canonicalized);
  g_free (desktop_file);

  return app;
}

/**
 * shell_app_system_lookup_startup_wmclass:
 * @system: a #ShellAppSystem
 * @wmclass: (nullable): A WM_CLASS value
 *
 * Find a valid application whose .desktop file contains a
 * StartupWMClass entry matching @wmclass.
 *
 * Returns: (transfer none): A #ShellApp for @wmclass
 */
ShellApp *
shell_app_system_lookup_startup_wmclass (ShellAppSystem *system,
                                         const char     *wmclass)
{
  const char *id;

  if (wmclass == NULL)
    return NULL;

  id = g_hash_table_lookup (system->priv->startup_wm_class_to_id, wmclass);
  if (id == NULL)
    return NULL;

  return shell_app_system_lookup_app (system, id);
}

void
_shell_app_system_notify_app_state_changed (ShellAppSystem *self,
                                            ShellApp       *app)
{
  ShellAppState state = shell_app_get_state (app);

  switch (state)
    {
    case SHELL_APP_STATE_RUNNING:
      g_hash_table_insert (self->priv->running_apps, g_object_ref (app), NULL);
      break;
    case SHELL_APP_STATE_STARTING:
      break;
    case SHELL_APP_STATE_STOPPED:
      g_hash_table_remove (self->priv->running_apps, app);
      break;
    default:
      g_warn_if_reached();
      break;
    }
  g_signal_emit (self, signals[APP_STATE_CHANGED], 0, app);
}

/**
 * shell_app_system_get_running:
 * @self: A #ShellAppSystem
 *
 * Returns the set of applications which currently have at least one
 * open window in the given context.  The returned list will be sorted
 * by shell_app_compare().
 *
 * Returns: (element-type ShellApp) (transfer container): Active applications
 */
GSList *
shell_app_system_get_running (ShellAppSystem *self)
{
  gpointer key, value;
  GSList *ret;
  GHashTableIter iter;

  g_hash_table_iter_init (&iter, self->priv->running_apps);

  ret = NULL;
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      ShellApp *app = key;

      ret = g_slist_prepend (ret, app);
    }

  ret = g_slist_sort (ret, (GCompareFunc)shell_app_compare);

  return ret;
}

/**
 * shell_app_system_search:
 * @search_string: the search string to use
 *
 * Wrapper around g_desktop_app_info_search() that replaces results that
 * don't validate as UTF-8 with the empty string.
 *
 * Returns: (array zero-terminated=1) (element-type GStrv) (transfer full): a
 *   list of strvs.  Free each item with g_strfreev() and free the outer
 *   list with g_free().
 */
char ***
shell_app_system_search (const char *search_string)
{
  char ***results = g_desktop_app_info_search (search_string);
  char ***groups, **ids;

  for (groups = results; *groups; groups++)
    for (ids = *groups; *ids; ids++)
      if (!g_utf8_validate (*ids, -1, NULL))
        **ids = '\0';

  return results;
}
