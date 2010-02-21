/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include "shell-app-system.h"
#include <string.h>

#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <clutter/clutter.h>

#include "shell-app-private.h"
#include "shell-global.h"
#include "shell-texture-cache.h"
#include "display.h"

#define GMENU_I_KNOW_THIS_IS_UNSTABLE
#include <gmenu-tree.h>

#define SHELL_APP_FAVORITES_KEY "/desktop/gnome/shell/favorite_apps"

/* Vendor prefixes are something that can be preprended to a .desktop
 * file name.  Undo this.
 */
static const char*const known_vendor_prefixes[] = { "gnome",
                                                    "fedora",
                                                    "mozilla" };

enum {
   PROP_0,

};

enum {
  INSTALLED_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _ShellAppSystemPrivate {
  GMenuTree *apps_tree;
  GMenuTree *settings_tree;

  GHashTable *app_id_to_info;
  GHashTable *app_id_to_app;

  GSList *cached_flattened_apps; /* ShellAppInfo */
  GSList *cached_settings; /* ShellAppInfo */

  gint app_monitor_id;

  guint app_change_timeout_id;
};

static void shell_app_system_finalize (GObject *object);
static gboolean on_tree_changed (gpointer user_data);
static void on_tree_changed_cb (GMenuTree *tree, gpointer user_data);
static void reread_menus (ShellAppSystem *self);

G_DEFINE_TYPE(ShellAppSystem, shell_app_system, G_TYPE_OBJECT);

typedef enum {
  SHELL_APP_INFO_TYPE_ENTRY,
  SHELL_APP_INFO_TYPE_DESKTOP_FILE,
  SHELL_APP_INFO_TYPE_WINDOW
} ShellAppInfoType;

struct _ShellAppInfo {
  ShellAppInfoType type;

  /* We need this for two reasons.  First, GKeyFile doesn't have a refcount.
   * http://bugzilla.gnome.org/show_bug.cgi?id=590808
   *
   * But more generally we'll always need it so we know when to free this
   * structure (short of weak references on each item).
   */
  guint refcount;

  char *casefolded_name;
  char *name_collation_key;
  char *casefolded_description;
  char *casefolded_exec;

  GMenuTreeItem *entry;

  GKeyFile *keyfile;
  char *keyfile_path;

  MetaWindow *window;
  char *window_id;
};

ShellAppInfo*
shell_app_info_ref (ShellAppInfo *info)
{
  info->refcount++;
  return info;
}

void
shell_app_info_unref (ShellAppInfo *info)
{
  if (--info->refcount > 0)
    return;

  g_free (info->casefolded_name);
  g_free (info->name_collation_key);
  g_free (info->casefolded_description);

  switch (info->type)
  {
  case SHELL_APP_INFO_TYPE_ENTRY:
    gmenu_tree_item_unref (info->entry);
    break;
  case SHELL_APP_INFO_TYPE_DESKTOP_FILE:
    g_key_file_free (info->keyfile);
    g_free (info->keyfile_path);
    break;
  case SHELL_APP_INFO_TYPE_WINDOW:
    g_object_unref (info->window);
    g_free (info->window_id);
    break;
  }
  g_slice_free (ShellAppInfo, info);
}

static ShellAppInfo *
shell_app_info_new_from_tree_item (GMenuTreeItem *item)
{
  ShellAppInfo *info;

  if (!item)
    return NULL;

  info = g_slice_alloc0 (sizeof (ShellAppInfo));
  info->type = SHELL_APP_INFO_TYPE_ENTRY;
  info->refcount = 1;
  info->entry = gmenu_tree_item_ref (item);
  return info;
}

static ShellAppInfo *
shell_app_info_new_from_window (MetaWindow *window)
{
  ShellAppInfo *info;

  info = g_slice_alloc0 (sizeof (ShellAppInfo));
  info->type = SHELL_APP_INFO_TYPE_WINDOW;
  info->refcount = 1;
  info->window = g_object_ref (window);
  /* For windows, its id is simply its pointer address as a string.
   * There are various other alternatives, but the address is unique
   * and unchanging, which is pretty much the best we can do.
   */
  info->window_id = g_strdup_printf ("window:%p", window);
  return info;
}

static ShellAppInfo *
shell_app_info_new_from_keyfile_take_ownership (GKeyFile   *keyfile,
                                                const char *path)
{
  ShellAppInfo *info;

  info = g_slice_alloc0 (sizeof (ShellAppInfo));
  info->type = SHELL_APP_INFO_TYPE_DESKTOP_FILE;
  info->refcount = 1;
  info->keyfile = keyfile;
  info->keyfile_path = g_strdup (path);
  return info;
}

static void shell_app_system_class_init(ShellAppSystemClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;

  gobject_class->finalize = shell_app_system_finalize;

  signals[INSTALLED_CHANGED] =
    g_signal_new ("installed-changed",
		  SHELL_TYPE_APP_SYSTEM,
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ShellAppSystemClass, installed_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  g_type_class_add_private (gobject_class, sizeof (ShellAppSystemPrivate));
}

static void
shell_app_system_init (ShellAppSystem *self)
{
  ShellAppSystemPrivate *priv;
  GConfClient *client;

  self->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                                   SHELL_TYPE_APP_SYSTEM,
                                                   ShellAppSystemPrivate);

  /* The key is owned by the value */
  priv->app_id_to_info = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                NULL, (GDestroyNotify) shell_app_info_unref);

  /* Key is owned by info */
  priv->app_id_to_app = g_hash_table_new (g_str_hash, g_str_equal);

  /* For now, we want to pick up Evince, Nautilus, etc.  We'll
   * handle NODISPLAY semantics at a higher level or investigate them
   * case by case.
   */
  priv->apps_tree = gmenu_tree_lookup ("applications.menu", GMENU_TREE_FLAGS_INCLUDE_NODISPLAY);
  priv->settings_tree = gmenu_tree_lookup ("settings.menu", GMENU_TREE_FLAGS_NONE);

  priv->app_change_timeout_id = 0;

  gmenu_tree_add_monitor (priv->apps_tree, on_tree_changed_cb, self);
  gmenu_tree_add_monitor (priv->settings_tree, on_tree_changed_cb, self);

  reread_menus (self);

  client = gconf_client_get_default ();
}

static void
shell_app_system_finalize (GObject *object)
{
  ShellAppSystem *self = SHELL_APP_SYSTEM (object);
  ShellAppSystemPrivate *priv = self->priv;

  gmenu_tree_remove_monitor (priv->apps_tree, on_tree_changed_cb, self);
  gmenu_tree_remove_monitor (priv->settings_tree, on_tree_changed_cb, self);

  gmenu_tree_unref (priv->apps_tree);
  gmenu_tree_unref (priv->settings_tree);

  g_hash_table_destroy (priv->app_id_to_info);
  g_hash_table_destroy (priv->app_id_to_app);

  g_slist_foreach (priv->cached_flattened_apps, (GFunc)shell_app_info_unref, NULL);
  g_slist_free (priv->cached_flattened_apps);
  priv->cached_flattened_apps = NULL;
  g_slist_foreach (priv->cached_settings, (GFunc)shell_app_info_unref, NULL);
  g_slist_free (priv->cached_settings);
  priv->cached_settings = NULL;

  G_OBJECT_CLASS (shell_app_system_parent_class)->finalize(object);
}

static GSList *
gather_entries_recurse (ShellAppSystem     *monitor,
                        GSList             *apps,
                        GHashTable         *unique,
                        GMenuTreeDirectory *root)
{
  GSList *contents;
  GSList *iter;

  contents = gmenu_tree_directory_get_contents (root);

  for (iter = contents; iter; iter = iter->next)
    {
      GMenuTreeItem *item = iter->data;
      switch (gmenu_tree_item_get_type (item))
        {
          case GMENU_TREE_ITEM_ENTRY:
            {
              ShellAppInfo *app = shell_app_info_new_from_tree_item (item);
              if (!g_hash_table_lookup (unique, shell_app_info_get_id (app)))
                {
                  apps = g_slist_prepend (apps, app);
                  g_hash_table_insert (unique, (char*)shell_app_info_get_id (app), app);
                }
            }
            break;
          case GMENU_TREE_ITEM_DIRECTORY:
            {
              GMenuTreeDirectory *dir = (GMenuTreeDirectory*)item;
              apps = gather_entries_recurse (monitor, apps, unique, dir);
            }
            break;
          default:
            break;
        }
      gmenu_tree_item_unref (item);
    }

  g_slist_free (contents);

  return apps;
}

static void
reread_entries (ShellAppSystem     *self,
                GSList            **cache,
                GHashTable         *unique,
                GMenuTree          *tree)
{
  GMenuTreeDirectory *trunk;

  trunk = gmenu_tree_get_root_directory (tree);

  g_slist_foreach (*cache, (GFunc)shell_app_info_unref, NULL);
  g_slist_free (*cache);
  *cache = NULL;

  *cache = gather_entries_recurse (self, *cache, unique, trunk);

  gmenu_tree_item_unref (trunk);
}

static void
cache_by_id (ShellAppSystem *self, GSList *apps)
{
  GSList *iter;

  for (iter = apps; iter; iter = iter->next)
    {
      ShellAppInfo *info = iter->data;
      shell_app_info_ref (info);
      /* the name is owned by the info itself */
      g_hash_table_replace (self->priv->app_id_to_info, (char*)shell_app_info_get_id (info),
                            info);
    }
}

static void
reread_menus (ShellAppSystem *self)
{
  GHashTable *unique = g_hash_table_new (g_str_hash, g_str_equal);

  reread_entries (self, &(self->priv->cached_flattened_apps), unique, self->priv->apps_tree);
  g_hash_table_remove_all (unique);
  reread_entries (self, &(self->priv->cached_settings), unique, self->priv->settings_tree);
  g_hash_table_destroy (unique);

  g_hash_table_remove_all (self->priv->app_id_to_info);

  cache_by_id (self, self->priv->cached_flattened_apps);
  cache_by_id (self, self->priv->cached_settings);
}

static gboolean
on_tree_changed (gpointer user_data)
{
  ShellAppSystem *self = SHELL_APP_SYSTEM (user_data);

  reread_menus (self);

  g_signal_emit (self, signals[INSTALLED_CHANGED], 0);

  self->priv->app_change_timeout_id = 0;
  return FALSE;
}

static void
on_tree_changed_cb (GMenuTree *monitor, gpointer user_data)
{
  ShellAppSystem *self = SHELL_APP_SYSTEM (user_data);

  /* GMenu currently gives us a separate notification on the entire
   * menu tree for each node in the tree that might potentially have
   * changed. (See http://bugzilla.gnome.org/show_bug.cgi?id=172046.)
   * We need to compress these to avoid doing large extra amounts of
   * work.
   *
   * Even when that bug is fixed, compression is still useful; for one
   * thing we want to need to compress across notifications of changes
   * to the settings tree. Second we want to compress if multiple
   * changes are made to the desktop files at different times but in
   * short succession.
   */

  if (self->priv->app_change_timeout_id != 0)
    return;
  self->priv->app_change_timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE, 3000,
                                                          (GSourceFunc) on_tree_changed,
                                                          self, NULL);
}

GType
shell_app_info_get_type (void)
{
  static GType gtype = G_TYPE_INVALID;
  if (gtype == G_TYPE_INVALID)
    {
      gtype = g_boxed_type_register_static ("ShellAppInfo",
          (GBoxedCopyFunc)shell_app_info_ref,
          (GBoxedFreeFunc)shell_app_info_unref);
    }
  return gtype;
}

/**
 * shell_app_system_get_flattened_apps:
 *
 * Traverses a toplevel menu, and returns all items under it.  Nested items
 * are flattened.  This value is computed on initial call and cached thereafter
 * until the set of installed applications changes.
 *
 * Return value: (transfer none) (element-type ShellAppInfo): List of applications
 */
GSList *
shell_app_system_get_flattened_apps (ShellAppSystem *self)
{
  return self->priv->cached_flattened_apps;
}

/**
 * shell_app_system_get_all_settings:
 *
 * Returns a list of application items under "settings.menu".
 *
 * Return value: (transfer none) (element-type ShellAppInfo): List of applications
 */
GSList *
shell_app_system_get_all_settings (ShellAppSystem *monitor)
{
  return monitor->priv->cached_settings;
}

/**
 * shell_app_system_get_default:
 *
 * Return Value: (transfer none): The global #ShellAppSystem singleton
 */
ShellAppSystem *
shell_app_system_get_default ()
{
  static ShellAppSystem *instance = NULL;

  if (instance == NULL)
    instance = g_object_new (SHELL_TYPE_APP_SYSTEM, NULL);

  return instance;
}

typedef struct {
  ShellAppSystem *appsys;
  ShellAppInfo *info;
} ShellAppRef;

static void
shell_app_system_on_app_weakref (gpointer  data,
                                 GObject  *location)
{
  ShellAppRef *ref = data;

  g_hash_table_remove (ref->appsys->priv->app_id_to_app, shell_app_info_get_id (ref->info));
  shell_app_info_unref (ref->info);
  g_free (ref);
}

/**
 * shell_app_system_get_app:
 *
 * Find or create a #ShellApp corresponding to an id; if already cached
 * elsewhere in memory, return that instance.  Otherwise, create a new
 * one.
 *
 * Return value: (transfer full): The #ShellApp for id, or %NULL if none
 */
ShellApp *
shell_app_system_get_app (ShellAppSystem   *self,
                          const char       *id)
{
  ShellAppInfo *info;
  ShellApp *app;

  app = g_hash_table_lookup (self->priv->app_id_to_app, id);
  if (app)
    return g_object_ref (app);

  info = g_hash_table_lookup (self->priv->app_id_to_info, id);
  if (!info)
    return NULL;

  app = _shell_app_new (info);

  return app;
}

/**
 * shell_app_system_get_app_for_window:
 * @self: A #ShellAppSystem
 * @window: A #MetaWindow
 *
 * Find or create a #ShellApp for window
 *
 * Return value: (transfer full): The #ShellApp for window, or %NULL if none
 */
ShellApp *
shell_app_system_get_app_for_window (ShellAppSystem *self,
                                     MetaWindow *window)
{
  char *id = g_strdup_printf ("window:%p", window);
  ShellApp *app = g_hash_table_lookup (self->priv->app_id_to_app, id);

  if (app)
    g_object_ref (G_OBJECT (app));
  else
    app = _shell_app_new_for_window (window);

  g_free (id);

  return app;
}

/* ShellAppSystem ensures we have a unique instance of
 * apps per id.
 */
void
_shell_app_system_register_app (ShellAppSystem   *self,
                                ShellApp         *app)
{
  const char *id;
  ShellAppRef *ref;

  id = shell_app_get_id (app);

  g_return_if_fail (g_hash_table_lookup (self->priv->app_id_to_app, id) == NULL);

  ref = g_new0 (ShellAppRef, 1);
  ref->appsys = self;
  ref->info = shell_app_info_ref (_shell_app_get_info (app));
  g_hash_table_insert (self->priv->app_id_to_app, (char*)shell_app_info_get_id (ref->info), app);
  g_object_weak_ref (G_OBJECT (app), shell_app_system_on_app_weakref, ref);
}

ShellAppInfo *
shell_app_system_load_from_desktop_file (ShellAppSystem   *system,
                                         const char       *filename,
                                         GError          **error)
{
  ShellAppInfo *appinfo;
  GKeyFile *keyfile;
  char *full_path = NULL;
  gboolean success;

  keyfile = g_key_file_new ();

  if (strchr (filename, '/') != NULL)
    {
      success = g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, error);
      full_path = g_strdup (filename);
    }
  else
    {
      char *app_path = g_build_filename ("applications", filename, NULL);
      success = g_key_file_load_from_data_dirs (keyfile, app_path, &full_path,
                                                G_KEY_FILE_NONE, error);
      g_free (app_path);
    }

  if (!success)
    {
      g_key_file_free (keyfile);
      g_free (full_path);
      return NULL;
    }

  appinfo = shell_app_info_new_from_keyfile_take_ownership (keyfile, full_path);
  g_free (full_path);

  return appinfo;
}

/**
 * shell_app_system_create_from_window:
 *
 * In the case where we can't otherwise determine an application
 * associated with a window, this function can create a "fake"
 * application just backed by information from the window itself.
 *
 * Return value: (transfer full): A new #ShellAppInfo
 */
ShellAppInfo *
shell_app_system_create_from_window (ShellAppSystem *system, MetaWindow *window)
{
  return shell_app_info_new_from_window (window);
}

/**
 * shell_app_system_lookup_heuristic_basename:
 * @name: Probable application identifier
 *
 * Find a valid application corresponding to a given
 * heuristically determined application identifier
 * string, or %NULL if none.
 *
 * Returns: (transfer full): A #ShellApp for name
 */
ShellApp *
shell_app_system_lookup_heuristic_basename (ShellAppSystem *system,
                                            const char *name)
{
  ShellApp *result;
  char **vendor_prefixes;

  result = shell_app_system_get_app (system, name);
  if (result != NULL)
    return result;

  for (vendor_prefixes = (char**)known_vendor_prefixes;
       *vendor_prefixes; vendor_prefixes++)
    {
      char *tmpid = g_strjoin (NULL, *vendor_prefixes, "-", name, NULL);
      result = shell_app_system_get_app (system, tmpid);
      g_free (tmpid);
      if (result != NULL)
        return result;
    }

  return NULL;
}

typedef enum {
  MATCH_NONE,
  MATCH_MULTIPLE, /* Matches multiple terms */
  MATCH_PREFIX, /* Strict prefix */
  MATCH_SUBSTRING /* Not prefix, substring */
} ShellAppInfoSearchMatch;

static char *
normalize_and_casefold (const char *str)
{
  char *normalized, *result;

  if (str == NULL)
    return NULL;

  normalized = g_utf8_normalize (str, -1, G_NORMALIZE_ALL);
  result = g_utf8_casefold (normalized, -1);
  g_free (normalized);
  return result;
}

static char *
trim_exec_line (const char *str)
{
  const char *start, *end, *pos;

  end = strchr (str, ' ');
  if (end == NULL)
    end = str + strlen (str);

  start = str;
  while ((pos = strchr (start, '/')) && pos < end)
    start = ++pos;

  return g_strndup (start, end - start);
}

static void
shell_app_info_init_search_data (ShellAppInfo *info)
{
  const char *name;
  const char *exec;
  const char *comment;
  char *normalized_exec;

  g_assert (info->type == SHELL_APP_INFO_TYPE_ENTRY);

  name = gmenu_tree_entry_get_name ((GMenuTreeEntry*)info->entry);
  info->casefolded_name = normalize_and_casefold (name);

  comment = gmenu_tree_entry_get_comment ((GMenuTreeEntry*)info->entry);
  info->casefolded_description = normalize_and_casefold (comment);

  exec = gmenu_tree_entry_get_exec ((GMenuTreeEntry*)info->entry);
  normalized_exec = normalize_and_casefold (exec);
  info->casefolded_exec = trim_exec_line (normalized_exec);
  g_free (normalized_exec);
}

static ShellAppInfoSearchMatch
shell_app_info_match_terms (ShellAppInfo  *info,
                            GSList        *terms)
{
  GSList *iter;
  ShellAppInfoSearchMatch match;

  if (G_UNLIKELY(!info->casefolded_name))
    shell_app_info_init_search_data (info);

  match = MATCH_NONE;
  for (iter = terms; iter; iter = iter->next)
    {
      const char *term = iter->data;
      const char *p;

      p = strstr (info->casefolded_name, term);
      if (p == info->casefolded_name)
        {
          if (match != MATCH_NONE)
            return MATCH_MULTIPLE;
          else
            match = MATCH_PREFIX;
         }
      else if (p != NULL)
        match = MATCH_SUBSTRING;

      p = strstr (info->casefolded_exec, term);
      if (p == info->casefolded_exec)
        {
          if (match != MATCH_NONE)
            return MATCH_MULTIPLE;
          else
            match = MATCH_PREFIX;
         }
      else if (p != NULL)
        match = MATCH_SUBSTRING;

      if (!info->casefolded_description)
        continue;
      p = strstr (info->casefolded_description, term);
      if (p != NULL)
        match = MATCH_SUBSTRING;
    }
  return match;
}

static gint
shell_app_info_compare (gconstpointer a,
                        gconstpointer b,
                        gpointer      data)
{
  ShellAppSystem *system = data;
  const char *id_a = a;
  const char *id_b = b;
  ShellAppInfo *info_a = g_hash_table_lookup (system->priv->app_id_to_info, id_a);
  ShellAppInfo *info_b = g_hash_table_lookup (system->priv->app_id_to_info, id_b);

  if (!info_a->name_collation_key)
    info_a->name_collation_key = g_utf8_collate_key (gmenu_tree_entry_get_name ((GMenuTreeEntry*)info_a->entry), -1);
  if (!info_b->name_collation_key)
    info_b->name_collation_key = g_utf8_collate_key (gmenu_tree_entry_get_name ((GMenuTreeEntry*)info_b->entry), -1);

  return strcmp (info_a->name_collation_key, info_b->name_collation_key);
}

static GSList *
sort_and_concat_results (ShellAppSystem *system,
                         GSList         *multiple_matches,
                         GSList         *prefix_matches,
                         GSList         *substring_matches)
{
  multiple_matches = g_slist_sort_with_data (multiple_matches, shell_app_info_compare, system);
  prefix_matches = g_slist_sort_with_data (prefix_matches, shell_app_info_compare, system);
  substring_matches = g_slist_sort_with_data (substring_matches, shell_app_info_compare, system);
  return g_slist_concat (multiple_matches, g_slist_concat (prefix_matches, substring_matches));
}

/**
 * normalize_terms:
 * @terms: (element-type utf8): Input search terms
 *
 * Returns: (element-type utf8) (transfer full): Unicode-normalized and lowercased terms
 */
static GSList *
normalize_terms (GSList *terms)
{
  GSList *normalized_terms = NULL;
  GSList *iter;
  for (iter = terms; iter; iter = iter->next)
    {
      const char *term = iter->data;
      normalized_terms = g_slist_prepend (normalized_terms, normalize_and_casefold (term));
    }
  return normalized_terms;
}

static inline void
shell_app_system_do_match (ShellAppSystem   *system,
                           ShellAppInfo     *info,
                           GSList           *terms,
                           GSList          **multiple_results,
                           GSList          **prefix_results,
                           GSList          **substring_results)
{
  const char *id = shell_app_info_get_id (info);
  ShellAppInfoSearchMatch match;

  if (shell_app_info_get_is_nodisplay (info))
    return;

  match = shell_app_info_match_terms (info, terms);
  switch (match)
    {
      case MATCH_NONE:
        break;
      case MATCH_MULTIPLE:
        *multiple_results = g_slist_prepend (*multiple_results, (char *) id);
        break;
      case MATCH_PREFIX:
        *prefix_results = g_slist_prepend (*prefix_results, (char *) id);
        break;
      case MATCH_SUBSTRING:
        *substring_results = g_slist_prepend (*substring_results, (char *) id);
        break;
    }
}

static GSList *
shell_app_system_initial_search_internal (ShellAppSystem  *self,
                                          GSList          *terms,
                                          GSList          *source)
{
  GSList *multiple_results = NULL;
  GSList *prefix_results = NULL;
  GSList *substring_results = NULL;
  GSList *iter;
  GSList *normalized_terms = normalize_terms (terms);

  for (iter = source; iter; iter = iter->next)
    {
      ShellAppInfo *info = iter->data;

      shell_app_system_do_match (self, info, normalized_terms, &multiple_results, &prefix_results, &substring_results);
    }
  g_slist_foreach (normalized_terms, (GFunc)g_free, NULL);
  g_slist_free (normalized_terms);

  return sort_and_concat_results (self, multiple_results, prefix_results, substring_results);
}

/**
 * shell_app_system_initial_search:
 * @self: A #ShellAppSystem
 * @prefs: %TRUE if we should search preferences instead of apps
 * @terms: (element-type utf8): List of terms, logical OR
 *
 * Search through applications for the given search terms.  Note that returned
 * strings are only valid until a return to the main loop.
 *
 * Returns: (transfer container) (element-type utf8): List of application identifiers
 */
GSList *
shell_app_system_initial_search (ShellAppSystem  *self,
                                 gboolean         prefs,
                                 GSList          *terms)
{
  return shell_app_system_initial_search_internal (self, terms,
            prefs ? self->priv->cached_settings : self->priv->cached_flattened_apps);
}

/**
 * shell_app_system_subsearch:
 * @self: A #ShellAppSystem
 * @prefs: %TRUE if we should search preferences instead of apps
 * @previous_results: (element-type utf8): List of previous results
 * @terms: (element-type utf8): List of terms, logical OR
 *
 * Search through a previous result set; for more information, see
 * js/ui/search.js.  Note the value of @prefs must be
 * the same as passed to shell_app_system_initial_search().  Note that returned
 * strings are only valid until a return to the main loop.
 *
 * Returns: (transfer container) (element-type utf8): List of application identifiers
 */
GSList *
shell_app_system_subsearch (ShellAppSystem   *system,
                            gboolean          prefs,
                            GSList           *previous_results,
                            GSList           *terms)
{
  GSList *iter;
  GSList *multiple_results = NULL;
  GSList *prefix_results = NULL;
  GSList *substring_results = NULL;
  GSList *normalized_terms = normalize_terms (terms);

  /* Note prefs is deliberately ignored; both apps and prefs are in app_id_to_app,
   * but we have the parameter for consistency and in case in the future
   * they're not in the same data structure.
   */

  for (iter = previous_results; iter; iter = iter->next)
    {
      const char *id = iter->data;
      ShellAppInfo *info;

      info = g_hash_table_lookup (system->priv->app_id_to_info, id);
      if (!info)
        continue;

      shell_app_system_do_match (system, info, normalized_terms, &multiple_results, &prefix_results, &substring_results);
    }
  g_slist_foreach (normalized_terms, (GFunc)g_free, NULL);
  g_slist_free (normalized_terms);

  /* Note that a shorter term might have matched as a prefix, but
     when extended only as a substring, so we have to redo the
     sort rather than reusing the existing ordering */
  return sort_and_concat_results (system, multiple_results, prefix_results, substring_results);
}

const char *
shell_app_info_get_id (ShellAppInfo *info)
{
  switch (info->type)
  {
    case SHELL_APP_INFO_TYPE_ENTRY:
      return gmenu_tree_entry_get_desktop_file_id ((GMenuTreeEntry*)info->entry);
    case SHELL_APP_INFO_TYPE_DESKTOP_FILE:
      return info->keyfile_path;
    case SHELL_APP_INFO_TYPE_WINDOW:
      return info->window_id;
  }
  g_assert_not_reached ();
  return NULL;
}

#define DESKTOP_ENTRY_GROUP "Desktop Entry"

char *
shell_app_info_get_name (ShellAppInfo *info)
{
  switch (info->type)
  {
    case SHELL_APP_INFO_TYPE_ENTRY:
      return g_strdup (gmenu_tree_entry_get_name ((GMenuTreeEntry*)info->entry));
    case SHELL_APP_INFO_TYPE_DESKTOP_FILE:
      return g_key_file_get_locale_string (info->keyfile, DESKTOP_ENTRY_GROUP, "Name", NULL, NULL);
    case SHELL_APP_INFO_TYPE_WINDOW:
      {
        char *title;
        g_object_get (info->window, "title", &title, NULL);
        if (!title)
          title = g_strdup ("");
        return title;
      }
  }
  g_assert_not_reached ();
  return NULL;
}

char *
shell_app_info_get_description (ShellAppInfo *info)
{
  switch (info->type)
  {
    case SHELL_APP_INFO_TYPE_ENTRY:
      return g_strdup (gmenu_tree_entry_get_comment ((GMenuTreeEntry*)info->entry));
    case SHELL_APP_INFO_TYPE_DESKTOP_FILE:
      return g_key_file_get_locale_string (info->keyfile, DESKTOP_ENTRY_GROUP, "Comment", NULL, NULL);
    case SHELL_APP_INFO_TYPE_WINDOW:
      return NULL;
  }
  g_assert_not_reached ();
  return NULL;
}

char *
shell_app_info_get_executable (ShellAppInfo *info)
{
  switch (info->type)
  {
    case SHELL_APP_INFO_TYPE_ENTRY:
      return g_strdup (gmenu_tree_entry_get_exec ((GMenuTreeEntry*)info->entry));
    case SHELL_APP_INFO_TYPE_DESKTOP_FILE:
      return g_key_file_get_string (info->keyfile, DESKTOP_ENTRY_GROUP, "Exec", NULL);
    case SHELL_APP_INFO_TYPE_WINDOW:
      return NULL;
  }
  g_assert_not_reached ();
  return NULL;
}

char *
shell_app_info_get_desktop_file_path (ShellAppInfo *info)
{
  switch (info->type)
  {
    case SHELL_APP_INFO_TYPE_ENTRY:
      return g_strdup (gmenu_tree_entry_get_desktop_file_path ((GMenuTreeEntry*)info->entry));
    case SHELL_APP_INFO_TYPE_DESKTOP_FILE:
      return g_strdup (info->keyfile_path);
    case SHELL_APP_INFO_TYPE_WINDOW:
      return NULL;
  }
  g_assert_not_reached ();
  return NULL;
}

static GIcon *
themed_icon_from_name (const char *iconname)
{
  GIcon *icon;

  if (!iconname)
     return NULL;

  if (g_path_is_absolute (iconname))
    {
      GFile *file;
      file = g_file_new_for_path (iconname);
      icon = G_ICON (g_file_icon_new (file));
      g_object_unref (file);
     }
  else
    {
      char *tmp_name, *p;
      tmp_name = strdup (iconname);
      /* Work around a common mistake in desktop files */
      if ((p = strrchr (tmp_name, '.')) != NULL &&
          (strcmp (p, ".png") == 0 ||
           strcmp (p, ".xpm") == 0 ||
           strcmp (p, ".svg") == 0))
        {
          *p = 0;
        }
      icon = g_themed_icon_new (tmp_name);
      g_free (tmp_name);
    }

  return icon;
}

static GIcon *
shell_app_info_get_icon (ShellAppInfo *info)
{
  char *iconname = NULL;
  GIcon *icon;

  /* This code adapted from gdesktopappinfo.c
   * Copyright (C) 2006-2007 Red Hat, Inc.
   * Copyright © 2007 Ryan Lortie
   * LGPL
   */

  switch (info->type)
  {
    case SHELL_APP_INFO_TYPE_ENTRY:
      return themed_icon_from_name (gmenu_tree_entry_get_icon ((GMenuTreeEntry*)info->entry));
    case SHELL_APP_INFO_TYPE_DESKTOP_FILE:
      iconname = g_key_file_get_locale_string (info->keyfile, DESKTOP_ENTRY_GROUP, "Icon", NULL, NULL);
      icon = themed_icon_from_name (iconname);
      g_free (iconname);
      return icon;
      break;
    case SHELL_APP_INFO_TYPE_WINDOW:
      return NULL;
  }
  g_assert_not_reached ();
  return NULL;
}

GSList *
shell_app_info_get_categories (ShellAppInfo *info)
{
  return NULL; /* TODO */
}

gboolean
shell_app_info_get_is_nodisplay (ShellAppInfo *info)
{
  switch (info->type)
  {
    case SHELL_APP_INFO_TYPE_ENTRY:
      return gmenu_tree_entry_get_is_nodisplay ((GMenuTreeEntry*)info->entry);
    case SHELL_APP_INFO_TYPE_DESKTOP_FILE:
    case SHELL_APP_INFO_TYPE_WINDOW:
      return FALSE;
  }
  g_assert_not_reached ();
  return TRUE;
}

/**
 * shell_app_info_is_transient:
 *
 * A "transient" application is one which represents
 * just an open window, i.e. we don't know how to launch it
 * again.
 */
gboolean
shell_app_info_is_transient (ShellAppInfo *info)
{
  return info->type == SHELL_APP_INFO_TYPE_WINDOW;
}

/**
 * shell_app_info_create_icon_texture:
 *
 * Look up the icon for this application, and create a #ClutterTexture
 * for it at the given size.
 *
 * Return value: (transfer none): A floating #ClutterActor
 */
ClutterActor *
shell_app_info_create_icon_texture (ShellAppInfo *info, float size)
{
  GIcon *icon;
  ClutterActor *ret;

  if (info->type == SHELL_APP_INFO_TYPE_WINDOW)
    {
      return shell_texture_cache_bind_pixbuf_property (shell_texture_cache_get_default (),
                                                       G_OBJECT (info->window),
                                                       "icon");
    }

  icon = shell_app_info_get_icon (info);
  if (icon == NULL)
    {
      ret = clutter_texture_new ();
      g_object_set (ret, "opacity", 0, "width", size, "height", size, NULL);
    }
  else
    {
      ret = shell_texture_cache_load_gicon (shell_texture_cache_get_default (), icon, (int)size);
      g_object_unref (icon);
    }

  return ret;
}

/**
 * shell_app_info_launch_full:
 * @timestamp: Event timestamp, or 0 for current event timestamp
 * @uris: List of uris to pass to application
 * @workspace: Start on this workspace, or -1 for default
 * @startup_id: (out): Returned startup notification ID, or %NULL if none
 * @error: A #GError
 */
gboolean
shell_app_info_launch_full (ShellAppInfo *info,
                            guint         timestamp,
                            GList        *uris,
                            int           workspace,
                            char        **startup_id,
                            GError      **error)
{
  GDesktopAppInfo *gapp;
  char *filename;
  GdkAppLaunchContext *context;
  gboolean ret;
  ShellGlobal *global;
  MetaScreen *screen;
  MetaDisplay *display;

  if (startup_id)
    *startup_id = NULL;

  if (info->type == SHELL_APP_INFO_TYPE_WINDOW)
    {
      /* We can't pass URIs into a window; shouldn't hit this
       * code path.  If we do, fix the caller to disallow it.
       */
      g_return_val_if_fail (uris == NULL, TRUE);

      meta_window_activate (info->window, timestamp);
      return TRUE;
    }

  filename = shell_app_info_get_desktop_file_path (info);
  gapp = g_desktop_app_info_new_from_filename (filename);
  g_free (filename);

  if (!gapp)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Not found");
      return FALSE;
    }

  global = shell_global_get ();
  screen = shell_global_get_screen (global);
  display = meta_screen_get_display (screen);

  if (timestamp == 0)
    timestamp = clutter_get_current_event_time ();

  /* Shell design calls for on application launch, no window is focused,
   * and we have startup notification displayed.
   */
  meta_display_focus_the_no_focus_window (display, screen, timestamp);

  if (workspace < 0)
    workspace = meta_screen_get_active_workspace_index (screen);

  context = gdk_app_launch_context_new ();
  gdk_app_launch_context_set_timestamp (context, timestamp);
  gdk_app_launch_context_set_desktop (context, workspace);

  ret = g_app_info_launch (G_APP_INFO (gapp), uris, (GAppLaunchContext*) context, error);

  g_object_unref (G_OBJECT (gapp));

  return ret;
}

gboolean
shell_app_info_launch (ShellAppInfo    *info,
                       GError         **error)
{
  return shell_app_info_launch_full (info, 0, NULL, -1, NULL, error);
}
