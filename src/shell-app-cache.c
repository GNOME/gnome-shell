/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include "shell-app-cache-private.h"

#include "shell-global-private.h"

/**
 * SECTION:shell-app-cache
 * @title: ShellAppCache
 * @short_description: application information cache
 *
 * The #ShellAppCache is responsible for caching information about #GAppInfo
 * to ensure that the compositor thread never needs to perform disk reads to
 * access them. All of the work is done off-thread. When the new data has
 * been loaded, a #ShellAppCache::changed signal is emitted.
 *
 * Additionally, the #ShellAppCache caches information about translations for
 * directories. This allows translation provided in [Desktop Entry] GKeyFiles
 * to be available when building StLabel and other elements without performing
 * costly disk reads.
 *
 * Various monitors are used to keep this information up to date while the
 * Shell is running.
 */

#define DEFAULT_TIMEOUT_SECONDS 5

struct _ShellAppCache
{
  GObject          parent_instance;

  GAppInfoMonitor *monitor;
  GPtrArray       *dir_monitors;
  GHashTable      *folders;
  GCancellable    *cancellable;
  GList           *app_infos;

  guint            queued_update;
};

typedef struct
{
  GList      *app_infos;
  GHashTable *folders;
} CacheState;

G_DEFINE_TYPE (ShellAppCache, shell_app_cache, G_TYPE_OBJECT)

enum {
  CHANGED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

static void
cache_state_free (CacheState *state)
{
  g_clear_pointer (&state->folders, g_hash_table_unref);
  g_list_free_full (state->app_infos, g_object_unref);
  g_free (state);
}

static CacheState *
cache_state_new (void)
{
  CacheState *state;

  state = g_new0 (CacheState, 1);
  state->folders = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  return g_steal_pointer (&state);
}

/**
 * shell_app_cache_get_default:
 *
 * Gets the default #ShellAppCache.
 *
 * Returns: (transfer none): a #ShellAppCache
 */
ShellAppCache *
shell_app_cache_get_default (void)
{
  return shell_global_get_app_cache (shell_global_get ());
}

static void
load_folder (GHashTable *folders,
             const char *path)
{
  g_autoptr(GDir) dir = NULL;
  const char *name;

  g_assert (folders != NULL);
  g_assert (path != NULL);

  dir = g_dir_open (path, 0, NULL);
  if (dir == NULL)
    return;

  while ((name = g_dir_read_name (dir)))
    {
      g_autofree gchar *filename = NULL;
      g_autoptr(GKeyFile) keyfile = NULL;

      /* First added wins */
      if (g_hash_table_contains (folders, name))
        continue;

      filename = g_build_filename (path, name, NULL);
      keyfile = g_key_file_new ();

      if (g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, NULL))
        {
          gchar *translated;

          translated = g_key_file_get_locale_string (keyfile,
                                                     "Desktop Entry", "Name",
                                                     NULL, NULL);

          if (translated != NULL)
            g_hash_table_insert (folders, g_strdup (name), translated);
        }
    }
}

static void
load_folders (GHashTable *folders)
{
  const char * const *dirs;
  g_autofree gchar *userdir = NULL;
  guint i;

  g_assert (folders != NULL);

  userdir = g_build_filename (g_get_user_data_dir (), "desktop-directories", NULL);
  load_folder (folders, userdir);

  dirs = g_get_system_data_dirs ();
  for (i = 0; dirs[i] != NULL; i++)
    {
      g_autofree gchar *sysdir = g_build_filename (dirs[i], "desktop-directories", NULL);
      load_folder (folders, sysdir);
    }
}

static void
shell_app_cache_worker (GTask        *task,
                        gpointer      source_object,
                        gpointer      task_data,
                        GCancellable *cancellable)
{
  CacheState *state;

  g_assert (G_IS_TASK (task));
  g_assert (SHELL_IS_APP_CACHE (source_object));

  state = cache_state_new ();
  state->app_infos = g_app_info_get_all ();
  load_folders (state->folders);

  g_task_return_pointer (task, state, (GDestroyNotify) cache_state_free);
}

static void
apply_update_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  ShellAppCache *cache = (ShellAppCache *)object;
  g_autoptr(GError) error = NULL;
  CacheState *state;

  g_assert (SHELL_IS_APP_CACHE (cache));
  g_assert (G_IS_TASK (result));
  g_assert (user_data == NULL);

  state = g_task_propagate_pointer (G_TASK (result), &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  g_list_free_full (cache->app_infos, g_object_unref);
  cache->app_infos = g_steal_pointer (&state->app_infos);

  g_clear_pointer (&cache->folders, g_hash_table_unref);
  cache->folders = g_steal_pointer (&state->folders);

  g_signal_emit (cache, signals[CHANGED], 0);

  cache_state_free (state);
}

static gboolean
shell_app_cache_do_update (gpointer user_data)
{
  ShellAppCache *cache = user_data;
  g_autoptr(GTask) task = NULL;

  cache->queued_update = 0;

  /* Reset the cancellable state so we don't race with
   * two updates coming back overlapped and applying the
   * information in the wrong order.
   */
  g_cancellable_cancel (cache->cancellable);
  g_clear_object (&cache->cancellable);
  cache->cancellable = g_cancellable_new ();

  task = g_task_new (cache, cache->cancellable, apply_update_cb, NULL);
  g_task_set_source_tag (task, shell_app_cache_do_update);
  g_task_run_in_thread (task, shell_app_cache_worker);

  return G_SOURCE_REMOVE;
}

static void
shell_app_cache_queue_update (ShellAppCache *self)
{
  g_assert (SHELL_IS_APP_CACHE (self));

  if (self->queued_update != 0)
    g_source_remove (self->queued_update);

  self->queued_update = g_timeout_add_seconds (DEFAULT_TIMEOUT_SECONDS,
                                               shell_app_cache_do_update,
                                               self);
}

static void
monitor_desktop_directories_for_data_dir (ShellAppCache *self,
                                          const gchar   *directory)
{
  g_autofree gchar *subdir = NULL;
  g_autoptr(GFile) file = NULL;
  g_autoptr(GFileMonitor) monitor = NULL;

  g_assert (SHELL_IS_APP_CACHE (self));

  if (directory == NULL)
    return;

  subdir = g_build_filename (directory, "desktop-directories", NULL);
  file = g_file_new_for_path (subdir);
  monitor = g_file_monitor_directory (file, G_FILE_MONITOR_NONE, NULL, NULL);

  if (monitor != NULL)
    {
      g_file_monitor_set_rate_limit (monitor, DEFAULT_TIMEOUT_SECONDS * 1000);
      g_signal_connect_object (monitor,
                               "changed",
                               G_CALLBACK (shell_app_cache_queue_update),
                               self,
                               G_CONNECT_SWAPPED);
      g_ptr_array_add (self->dir_monitors, g_steal_pointer (&monitor));
    }
}

static void
shell_app_cache_finalize (GObject *object)
{
  ShellAppCache *self = (ShellAppCache *)object;

  g_clear_object (&self->monitor);

  if (self->queued_update)
    {
      g_source_remove (self->queued_update);
      self->queued_update = 0;
    }

  g_clear_pointer (&self->dir_monitors, g_ptr_array_unref);
  g_clear_pointer (&self->folders, g_hash_table_unref);
  g_list_free_full (self->app_infos, g_object_unref);

  G_OBJECT_CLASS (shell_app_cache_parent_class)->finalize (object);
}

static void
shell_app_cache_class_init (ShellAppCacheClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = shell_app_cache_finalize;

  /**
   * ShellAppCache::changed:
   *
   * The "changed" signal is emitted when the cache has updated
   * information about installed applications.
   */
  signals [CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
shell_app_cache_init (ShellAppCache *self)
{
  const gchar * const *sysdirs;
  guint i;

  /* Monitor directories for translation changes */
  self->dir_monitors = g_ptr_array_new_with_free_func (g_object_unref);
  monitor_desktop_directories_for_data_dir (self, g_get_user_data_dir ());
  sysdirs = g_get_system_data_dirs ();
  for (i = 0; sysdirs[i] != NULL; i++)
    monitor_desktop_directories_for_data_dir (self, sysdirs[i]);

  /* Load translated directory names immediately */
  self->folders = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  load_folders (self->folders);

  /* Setup AppMonitor to track changes */
  self->monitor = g_app_info_monitor_get ();
  g_signal_connect_object (self->monitor,
                           "changed",
                           G_CALLBACK (shell_app_cache_queue_update),
                           self,
                           G_CONNECT_SWAPPED);
  self->app_infos = g_app_info_get_all ();
}

/**
 * shell_app_cache_get_all:
 * @cache: (nullable): a #ShellAppCache or %NULL
 *
 * Like g_app_info_get_all() but always returns a
 * cached set of application info so the caller can be
 * sure that I/O will not happen on the current thread.
 *
 * Returns: (transfer none) (element-type GAppInfo):
 *   a #GList of references to #GAppInfo.
 */
GList *
shell_app_cache_get_all (ShellAppCache *cache)
{
  g_return_val_if_fail (SHELL_IS_APP_CACHE (cache), NULL);

  return cache->app_infos;
}

/**
 * shell_app_cache_get_info:
 * @cache: (nullable): a #ShellAppCache or %NULL
 * @id: the application id
 *
 * A replacement for g_desktop_app_info_new() that will lookup the
 * information from the cache instead of (re)loading from disk.
 *
 * Returns: (nullable) (transfer none): a #GDesktopAppInfo or %NULL
 */
GDesktopAppInfo *
shell_app_cache_get_info (ShellAppCache *cache,
                          const char    *id)
{
  const GList *iter;

  g_return_val_if_fail (SHELL_IS_APP_CACHE (cache), NULL);

  for (iter = cache->app_infos; iter != NULL; iter = iter->next)
    {
      GAppInfo *info = iter->data;

      if (g_strcmp0 (id, g_app_info_get_id (info)) == 0)
        return G_DESKTOP_APP_INFO (info);
    }

  return NULL;
}

/**
 * shell_app_cache_translate_folder:
 * @cache: (nullable): a #ShellAppCache or %NULL
 * @name: the folder name
 *
 * Gets the translated folder name for @name if any exists.
 *
 * Returns: (nullable): the translated string or %NULL if there is no
 *   translation.
 */
char *
shell_app_cache_translate_folder (ShellAppCache *cache,
                                  const char    *name)
{
  g_return_val_if_fail (SHELL_IS_APP_CACHE (cache), NULL);

  if (name == NULL)
    return NULL;

  return g_strdup (g_hash_table_lookup (cache->folders, name));
}
