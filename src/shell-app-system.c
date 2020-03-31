/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include "shell-app-system.h"
#include "shell-app-usage.h"
#include <string.h>

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <eosmetrics/eosmetrics.h>

#include "shell-app-cache-private.h"
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

/* Occurs when an application visible to the shell is opened or closed. The
 * payload varies depending on whether it is given as an opening event or a
 * closed event. If it is an opening event, the payload is a human-readable
 * application name. If it is a closing event, the payload is empty. The key
 * used is a pointer to the corresponding ShellApp.
 */
#define SHELL_APP_IS_OPEN_EVENT "b5e11a3d-13f8-4219-84fd-c9ba0bf3d1f0"

/* Additional key used to map a renamed desktop file to its previous name.
 * For instance, org.gnome.Totem.desktop contains:
 *
 *   X-Endless-Alias=totem
 *
 * (without the .desktop suffix).
 */
#define X_ENDLESS_ALIAS_KEY     "X-Endless-Alias"

/* Additional key listing 0 or more previous names for an application. This is
 * added by flatpak-builder when the manifest contains a rename-desktop-file
 * key, and by Endless-specific tools to migrate from an app in our eos-apps
 * repository to the same app with a different ID on Flathub. For example,
 * org.inkscape.Inkscape.desktop contains:
 *
 *   X-Flatpak-RenamedFrom=inkscape.desktop;
 *
 * (with the .desktop suffix).
 */
#define X_FLATPAK_RENAMED_FROM_KEY "X-Flatpak-RenamedFrom"

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
  APP_INFO_CHANGED,
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
  GHashTable *starting_apps;
  GHashTable *id_to_app;
  GHashTable *startup_wm_class_to_id;
  GList *installed_apps;

  guint rescan_icons_timeout_id;
  guint n_rescan_retries;

  GHashTable *alias_to_id;
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

  signals[APP_INFO_CHANGED] =
    g_signal_new ("app-info-changed",
                  SHELL_TYPE_APP_SYSTEM,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  SHELL_TYPE_APP);
}

static void
add_aliases (ShellAppSystem  *self,
             GDesktopAppInfo *info)
{
  ShellAppSystemPrivate *priv = self->priv;
  const char *id = g_app_info_get_id (G_APP_INFO (info));
  const char *alias;
  g_autofree char **renamed_from_list = NULL;
  size_t i;

  alias = g_desktop_app_info_get_string (info, X_ENDLESS_ALIAS_KEY);
  if (alias != NULL)
    {
      char *desktop_alias = g_strconcat (alias, ".desktop", NULL);
      g_hash_table_insert (priv->alias_to_id, desktop_alias, g_strdup (id));
    }

  renamed_from_list = g_desktop_app_info_get_string_list (info, X_FLATPAK_RENAMED_FROM_KEY, NULL);
  for (i = 0; renamed_from_list != NULL && renamed_from_list[i] != NULL; i++)
    {
      g_hash_table_insert (priv->alias_to_id,
                           g_steal_pointer (&renamed_from_list[i]),
                           g_strdup (id));
    }
}

static void
scan_alias_to_id (ShellAppSystem *self)
{
  ShellAppSystemPrivate *priv = self->priv;
  GList *apps, *l;

  g_hash_table_remove_all (priv->alias_to_id);

  apps = g_app_info_get_all ();
  for (l = apps; l != NULL; l = l->next)
    add_aliases (self, G_DESKTOP_APP_INFO (l->data));

  g_list_free_full (apps, g_object_unref);
}

static void
scan_startup_wm_class_to_id (ShellAppSystem *self)
{
  ShellAppSystemPrivate *priv = self->priv;
  const GList *l;
  GList *all;

  g_hash_table_remove_all (priv->startup_wm_class_to_id);

  all = shell_app_cache_get_all (shell_app_cache_get_default ());

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

/**
 * shell_app_system_app_info_equal:
 * @one: (transfer none): a #GDesktopAppInfo
 * @two: (transfer none): a possibly-different #GDesktopAppInfo
 *
 * Returns %TRUE if @one and @two can be treated as equal. Compared to
 * g_app_info_equal(), which just compares app IDs, this function also compares
 * fields of interest to the shell: icon, name, description, executable, and
 * should_show().
 *
 * Returns: %TRUE if @one and @two are equivalent; %FALSE otherwise
 */
gboolean
shell_app_system_app_info_equal (GDesktopAppInfo *one,
                                 GDesktopAppInfo *two)
{
  GAppInfo *one_info, *two_info;

  g_return_val_if_fail (G_IS_DESKTOP_APP_INFO (one), FALSE);
  g_return_val_if_fail (G_IS_DESKTOP_APP_INFO (two), FALSE);

  one_info = G_APP_INFO (one);
  two_info = G_APP_INFO (two);

  return
    g_app_info_equal (one_info, two_info) &&
    g_app_info_should_show (one_info) == g_app_info_should_show (two_info) &&
    g_strcmp0 (g_desktop_app_info_get_filename (one),
               g_desktop_app_info_get_filename (two)) == 0 &&
    g_strcmp0 (g_app_info_get_executable (one_info),
               g_app_info_get_executable (two_info)) == 0 &&
    g_strcmp0 (g_app_info_get_commandline (one_info),
               g_app_info_get_commandline (two_info)) == 0 &&
    g_strcmp0 (g_app_info_get_name (one_info),
               g_app_info_get_name (two_info)) == 0 &&
    g_strcmp0 (g_app_info_get_description (one_info),
               g_app_info_get_description (two_info)) == 0 &&
    g_strcmp0 (g_app_info_get_display_name (one_info),
               g_app_info_get_display_name (two_info)) == 0 &&
    g_icon_equal (g_app_info_get_icon (one_info),
                  g_app_info_get_icon (two_info));
}

static gboolean
app_is_stale (ShellApp *app)
{
  GDesktopAppInfo *info, *old;
  gboolean is_unchanged;

  if (shell_app_is_window_backed (app))
    return FALSE;

  info = shell_app_cache_get_info (shell_app_cache_get_default (),
                                   shell_app_get_id (app));
  if (!info)
    return TRUE;

  old = shell_app_get_app_info (app);

  is_unchanged = shell_app_system_app_info_equal (old, info);

  return !is_unchanged;
}

static GDesktopAppInfo *
get_new_desktop_app_info_from_app (ShellApp *app)
{
  const char *id;

  if (shell_app_is_window_backed (app))
    return NULL;

  /* If g_app_info_delete() was called, such as when a custom desktop
   * icon is removed, the desktop ID of the underlying GDesktopAppInfo
   * will be set to NULL.
   * So we explicitly check for that case and mark the app as stale.
   * See https://git.gnome.org/browse/glib/tree/gio/gdesktopappinfo.c?h=glib-2-44&id=2.44.0#n3682
   */
  id = shell_app_get_id (app);
  if (id == NULL)
    return NULL;

  return g_desktop_app_info_new (id);
}

static gboolean
app_info_changed (ShellApp        *app,
                  GDesktopAppInfo *desk_new_info)
{
  GDesktopAppInfo *desk_app_info = shell_app_get_app_info (app);

  if (!desk_app_info)
    return TRUE;

  return !shell_app_system_app_info_equal (desk_app_info, desk_new_info);
}

static void
remove_or_update_app_from_info (ShellAppSystem *self)
{
  GHashTableIter iter;
  ShellApp *app;

  g_hash_table_iter_init (&iter, self->priv->id_to_app);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer) &app))
    {
      g_autoptr(GDesktopAppInfo) app_info = NULL;

      if (app_is_stale (app))
        {
          /* App is stale, we remove it */
          g_hash_table_iter_remove (&iter);
          continue;
        }

      app_info = get_new_desktop_app_info_from_app (app);
      if (app_info_changed (app, app_info))
        {
          _shell_app_set_app_info (app, app_info);
          g_signal_emit (self, signals[APP_INFO_CHANGED], 0, app);
        }
    }
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
  scan_alias_to_id (self);

  scan_startup_wm_class_to_id (self);

  remove_or_update_app_from_info (self);

  g_signal_emit (self, signals[INSTALLED_CHANGED], 0, NULL);
}

static void
shell_app_system_init (ShellAppSystem *self)
{
  ShellAppSystemPrivate *priv;
  ShellAppCache *cache;

  self->priv = priv = shell_app_system_get_instance_private (self);

  priv->running_apps = g_hash_table_new_full (NULL, NULL, (GDestroyNotify) g_object_unref, NULL);
  priv->starting_apps = g_hash_table_new_full (NULL, NULL, (GDestroyNotify) g_object_unref, NULL);
  priv->id_to_app = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           NULL,
                                           (GDestroyNotify)g_object_unref);

  priv->startup_wm_class_to_id = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  priv->alias_to_id = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

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
  g_hash_table_destroy (priv->starting_apps);
  g_hash_table_destroy (priv->id_to_app);
  g_hash_table_destroy (priv->startup_wm_class_to_id);
  g_list_free_full (priv->installed_apps, g_object_unref);
  g_clear_handle_id (&priv->rescan_icons_timeout_id, g_source_remove);
  g_hash_table_destroy (priv->alias_to_id);

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

  info = shell_app_cache_get_info (shell_app_cache_get_default (), id);
  if (!info)
    return NULL;

  app = _shell_app_new (info);
  g_hash_table_insert (priv->id_to_app, (char *) shell_app_get_id (app), app);

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
 * shell_app_system_lookup_alias:
 * @system: a #ShellAppSystem
 * @alias: alternative application id
 *
 * Find a valid application corresponding to a given
 * alias string, or %NULL if none.
 *
 * Returns: (transfer none): A #ShellApp for @alias
 */
ShellApp *
shell_app_system_lookup_alias (ShellAppSystem *system,
                               const char     *alias)
{
  ShellApp *result;
  const char *id;

  g_return_val_if_fail (alias != NULL, NULL);

  result = shell_app_system_lookup_app (system, alias);
  if (result != NULL)
    return result;

  id = g_hash_table_lookup (system->priv->alias_to_id, alias);
  if (id == NULL)
    return NULL;

  result = shell_app_system_lookup_app (system, id);

  return result;
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

  g_autofree gchar *app_address = g_strdup_printf ("%p", app);
  GDesktopAppInfo *app_info = shell_app_get_app_info (app);
  const gchar *app_info_id = NULL;
  if (app_info != NULL)
    app_info_id = g_app_info_get_id (G_APP_INFO (app_info));

  switch (state)
    {
    case SHELL_APP_STATE_RUNNING:
      if (app_info_id != NULL)
        {
          emtr_event_recorder_record_start (emtr_event_recorder_get_default (),
                                            SHELL_APP_IS_OPEN_EVENT,
                                            g_variant_new_string (app_address),
                                            g_variant_new_string (app_info_id));
        }
      g_hash_table_insert (self->priv->running_apps, g_object_ref (app), NULL);
      g_hash_table_remove (self->priv->starting_apps, app);
      break;
    case SHELL_APP_STATE_STARTING:
      g_hash_table_insert (self->priv->starting_apps, g_object_ref (app), NULL);
      break;
    case SHELL_APP_STATE_STOPPED:
      /* Applications associated to multiple .desktop files (e.g. gnome-control-center)
       * will create different ShellApp instances during the startup process when not
       * launched via the main .desktop file: one initial instance for the .desktop file
       * originally launched (that will end up in the starting_apps table) and a different
       * one associated to the main .desktop file for the application.
       *
       * Thus, we can not rely on the initial ShellApp being removed from the starting_apps
       * table in the SHELL_APP_STATE_RUNNING case above because the instance will be different
       * than the one being added to running_apps, resulting in a rogue ShellApp instance being
       * kept forever in the starting_apps table, that will confuse the shell.
       *
       * The solution is to make sure that we remove that rogue ShellApp instance from the
       * starting_apps table, if needed, when moving to SHELL_APP_STATE_STOPPED, since that
       * state change will be enforced from _shell_app_handle_startup_sequence() for this kind
       * of apps launched from a secondary .desktop file, before moving the real ShellApp instance
       * into the running state.
       */
      if (g_hash_table_contains (self->priv->starting_apps, app))
        g_hash_table_remove (self->priv->starting_apps, app);

      if (g_hash_table_remove (self->priv->running_apps, app) && app_info_id != NULL)
        {
          emtr_event_recorder_record_stop (emtr_event_recorder_get_default (),
                                           SHELL_APP_IS_OPEN_EVENT,
                                           g_variant_new_string (app_address),
                                           NULL);
        }
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
 * open window.  The returned list will be sorted by shell_app_compare().
 *
 * Returns: (element-type ShellApp) (transfer container): Active applications
 */
GSList *
shell_app_system_get_running (ShellAppSystem *self)
{
  gpointer key, value;
  GSList *ret = NULL;
  GHashTableIter iter;

  g_hash_table_iter_init (&iter, self->priv->running_apps);

  ret = NULL;
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      ShellApp *app = key;

      ret = g_slist_prepend (ret, app);
    }

  g_hash_table_iter_init (&iter, self->priv->starting_apps);
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

/**
 * shell_app_system_get_installed:
 * @self: the #ShellAppSystem
 *
 * Returns all installed apps, as a list of #GAppInfo
 *
 * Returns: (transfer none) (element-type GAppInfo): a list of #GAppInfo
 *   describing all known applications. This memory is owned by the
 *   #ShellAppSystem and should not be freed.
 **/
GList *
shell_app_system_get_installed (ShellAppSystem *self)
{
  return shell_app_cache_get_all (shell_app_cache_get_default ());
}

gboolean
shell_app_system_has_starting_apps (ShellAppSystem *self)
{
  return g_hash_table_size (self->priv->starting_apps) > 0;
}
