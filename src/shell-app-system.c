/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include "shell-app-system.h"
#include <string.h>

#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <clutter/clutter.h>
#include <glib/gi18n.h>
#include <meta/display.h>

#include "shell-app-private.h"
#include "shell-window-tracker-private.h"
#include "shell-global.h"
#include "st.h"

#define GMENU_I_KNOW_THIS_IS_UNSTABLE
#include <gmenu-tree.h>

/* Vendor prefixes are something that can be preprended to a .desktop
 * file name.  Undo this.
 */
static const char*const vendor_prefixes[] = { "gnome-",
                                              "fedora-",
                                              "mozilla-",
                                              NULL };

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
  GSList *known_vendor_prefixes;

  gint app_monitor_id;

  guint app_change_timeout_id;
};

static char *shell_app_info_get_prefix (ShellAppInfo *info);
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

  g_slist_foreach (priv->known_vendor_prefixes, (GFunc)g_free, NULL);
  g_slist_free (priv->known_vendor_prefixes);
  priv->known_vendor_prefixes = NULL;

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

  if (!trunk)
    {
      *cache = NULL;
    }
  else
    {
      *cache = gather_entries_recurse (self, *cache, unique, trunk);
      gmenu_tree_item_unref (trunk);
    }
}

static void
cache_by_id (ShellAppSystem *self, GSList *apps)
{
  GSList *iter;

  for (iter = apps; iter; iter = iter->next)
    {
      ShellAppInfo *info = iter->data;
      const char *id = shell_app_info_get_id (info);
      char *prefix = shell_app_info_get_prefix (info);

      shell_app_info_ref (info);
      /* the name is owned by the info itself */

      if (prefix
          && !g_slist_find_custom (self->priv->known_vendor_prefixes, prefix,
                                   (GCompareFunc)g_strcmp0))
        self->priv->known_vendor_prefixes = g_slist_append (self->priv->known_vendor_prefixes,
                                                            prefix);
      else
        g_free (prefix);
      g_hash_table_replace (self->priv->app_id_to_info, (char*)id, info);
    }
}

static void
reread_menus (ShellAppSystem *self)
{
  GHashTable *unique = g_hash_table_new (g_str_hash, g_str_equal);

  g_slist_foreach (self->priv->known_vendor_prefixes, (GFunc)g_free, NULL);
  g_slist_free (self->priv->known_vendor_prefixes);
  self->priv->known_vendor_prefixes = NULL;

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
 * shell_app_system_get_app_for_path:
 * @system: a #ShellAppSystem
 * @desktop_path: (type utf8): UTF-8 encoded absolute file name
 *
 * Find or create a #ShellApp corresponding to a given absolute
 * file name which must be in the standard paths (XDG_DATA_DIRS).
 * For files outside the datadirs, this function returns %NULL.
 *
 * If already cached elsewhere in memory, return that instance.
 * Otherwise, create a new one.
 *
 * Return value: (transfer full): The #ShellApp for id, or %NULL if none
 */
ShellApp *
shell_app_system_get_app_for_path (ShellAppSystem   *system,
                                   const char       *desktop_path)
{
  const char *basename;
  ShellAppInfo *info;

  basename = g_strrstr (desktop_path, "/");
  if (basename)
    basename += 1;
  else
    basename = desktop_path;

  info = g_hash_table_lookup (system->priv->app_id_to_info, basename);
  if (!info)
    return NULL;

  if (info->type == SHELL_APP_INFO_TYPE_ENTRY)
    {
      const char *full_path = gmenu_tree_entry_get_desktop_file_path ((GMenuTreeEntry*) info->entry);
      if (strcmp (desktop_path, full_path) != 0)
        return NULL;
    }
  else
    return NULL;

  return shell_app_system_get_app (system, basename);
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
 * @system: a #ShellAppSystem
 * @id: Probable application identifier
 *
 * Find a valid application corresponding to a given
 * heuristically determined application identifier
 * string, or %NULL if none.
 *
 * Returns: (transfer full): A #ShellApp for @name
 */
ShellApp *
shell_app_system_lookup_heuristic_basename (ShellAppSystem *system,
                                            const char *name)
{
  ShellApp *result;
  GSList *prefix;
  result = shell_app_system_get_app (system, name);
  if (result != NULL)
    return result;
  for (prefix = system->priv->known_vendor_prefixes; prefix; prefix = g_slist_next (prefix))
    {
      char *tmpid = g_strconcat ((char*)prefix->data, name, NULL);
      result = shell_app_system_get_app (system, tmpid);
      g_free (tmpid);
      if (result != NULL)
        return result;
    }

  return NULL;
}

typedef enum {
  MATCH_NONE,
  MATCH_SUBSTRING, /* Not prefix, substring */
  MATCH_MULTIPLE_SUBSTRING, /* Matches multiple criteria with substrings */
  MATCH_PREFIX, /* Strict prefix */
  MATCH_MULTIPLE_PREFIX, /* Matches multiple criteria, at least one prefix */
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
      ShellAppInfoSearchMatch current_match;
      const char *term = iter->data;
      const char *p;

      current_match = MATCH_NONE;

      p = strstr (info->casefolded_name, term);
      if (p == info->casefolded_name)
        current_match = MATCH_PREFIX;
      else if (p != NULL)
        current_match = MATCH_SUBSTRING;

      p = strstr (info->casefolded_exec, term);
      if (p != NULL)
        {
          if (p == info->casefolded_exec)
            current_match = (current_match == MATCH_NONE) ? MATCH_PREFIX
                                                          : MATCH_MULTIPLE_PREFIX;
          else if (current_match < MATCH_PREFIX)
            current_match = (current_match == MATCH_NONE) ? MATCH_SUBSTRING
                                                          : MATCH_MULTIPLE_SUBSTRING;
        }

      if (info->casefolded_description && current_match < MATCH_PREFIX)
        {
          /* Only do substring matches, as prefix matches are not meaningful
           * enough for descriptions
           */
          p = strstr (info->casefolded_description, term);
          if (p != NULL)
            current_match = (current_match == MATCH_NONE) ? MATCH_SUBSTRING
                                                          : MATCH_MULTIPLE_SUBSTRING;
        }

      if (current_match == MATCH_NONE)
        return current_match;

      if (current_match > match)
        match = current_match;
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
                         GSList         *multiple_prefix_matches,
                         GSList         *prefix_matches,
                         GSList         *multiple_substring_matches,
                         GSList         *substring_matches)
{
  multiple_prefix_matches = g_slist_sort_with_data (multiple_prefix_matches,
                                                    shell_app_info_compare,
                                                    system);
  prefix_matches = g_slist_sort_with_data (prefix_matches,
                                           shell_app_info_compare,
                                           system);
  multiple_substring_matches = g_slist_sort_with_data (multiple_substring_matches,
                                                       shell_app_info_compare,
                                                       system);
  substring_matches = g_slist_sort_with_data (substring_matches,
                                              shell_app_info_compare,
                                              system);
  return g_slist_concat (multiple_prefix_matches, g_slist_concat (prefix_matches, g_slist_concat (multiple_substring_matches, substring_matches)));
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
                           GSList          **multiple_prefix_results,
                           GSList          **prefix_results,
                           GSList          **multiple_substring_results,
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
      case MATCH_MULTIPLE_PREFIX:
        *multiple_prefix_results = g_slist_prepend (*multiple_prefix_results,
                                                    (char *) id);
        break;
      case MATCH_PREFIX:
        *prefix_results = g_slist_prepend (*prefix_results, (char *) id);
        break;
      case MATCH_MULTIPLE_SUBSTRING:
        *multiple_substring_results = g_slist_prepend (*multiple_substring_results,
                                                    (char *) id);
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
  GSList *multiple_prefix_results = NULL;
  GSList *prefix_results = NULL;
  GSList *multiple_subtring_results = NULL;
  GSList *substring_results = NULL;
  GSList *iter;
  GSList *normalized_terms = normalize_terms (terms);

  for (iter = source; iter; iter = iter->next)
    {
      ShellAppInfo *info = iter->data;

      shell_app_system_do_match (self, info, normalized_terms,
                                 &multiple_prefix_results, &prefix_results,
                                 &multiple_subtring_results, &substring_results);
    }
  g_slist_foreach (normalized_terms, (GFunc)g_free, NULL);
  g_slist_free (normalized_terms);

  return sort_and_concat_results (self, multiple_prefix_results, prefix_results, multiple_subtring_results, substring_results);
}

/**
 * shell_app_system_initial_search:
 * @system: A #ShellAppSystem
 * @prefs: %TRUE if we should search preferences instead of apps
 * @terms: (element-type utf8): List of terms, logical AND
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
 * @system: A #ShellAppSystem
 * @prefs: %TRUE if we should search preferences instead of apps
 * @previous_results: (element-type utf8): List of previous results
 * @terms: (element-type utf8): List of terms, logical AND
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
  GSList *multiple_prefix_results = NULL;
  GSList *prefix_results = NULL;
  GSList *multiple_substring_results = NULL;
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

      shell_app_system_do_match (system, info, normalized_terms,
                                 &multiple_prefix_results, &prefix_results,
                                 &multiple_substring_results, &substring_results);
    }
  g_slist_foreach (normalized_terms, (GFunc)g_free, NULL);
  g_slist_free (normalized_terms);

  /* Note that a shorter term might have matched as a prefix, but
     when extended only as a substring, so we have to redo the
     sort rather than reusing the existing ordering */
  return sort_and_concat_results (system, multiple_prefix_results, prefix_results, multiple_substring_results, substring_results);
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

static char *
shell_app_info_get_prefix (ShellAppInfo *info)
{
  char *prefix = NULL, *file_prefix = NULL;
  const char *id;
  GFile *file;
  char *name;
  int i = 0;

  if (info->type != SHELL_APP_INFO_TYPE_ENTRY)
    return NULL;

  id = gmenu_tree_entry_get_desktop_file_id ((GMenuTreeEntry*)info->entry);
  file = g_file_new_for_path (gmenu_tree_entry_get_desktop_file_path ((GMenuTreeEntry*)info->entry));
  name = g_file_get_basename (file);

  if (!name)
    {
      g_object_unref (file);
      return NULL;
    }
  for (i = 0; vendor_prefixes[i]; i++)
    {
      if (g_str_has_prefix (name, vendor_prefixes[i]))
        {
          file_prefix = g_strdup (vendor_prefixes[i]);
          break;
        }
    }

  while (strcmp (name, id) != 0)
    {
      char *t;
      char *pname;
      GFile *parent = g_file_get_parent (file);

      if (!parent)
        {
          g_warn_if_reached ();
          break;
        }

      pname = g_file_get_basename (parent);
      if (!pname)
        {
          g_object_unref (parent);
          break;
        }
      if (!g_strstr_len (id, -1, pname))
        {
          /* handle <LegacyDir prefix="..."> */
          char *t;
          size_t name_len = strlen (name);
          size_t id_len = strlen (id);
          char *t_id = g_strdup (id);

          t_id[id_len - name_len] = '\0';
          t = g_strdup(t_id);
          g_free (prefix);
          g_free (t_id);
          g_free (name);
          name = g_strdup (id);
          prefix = t;

          g_object_unref (file);
          file = parent;
          g_free (pname);
          g_free (file_prefix);
          file_prefix = NULL;
          break;
        }

      t = g_strconcat (pname, "-", name, NULL);
      g_free (name);
      name = t;

      t = g_strconcat (pname, "-", prefix, NULL);
      g_free (prefix);
      prefix = t;

      g_object_unref (file);
      file = parent;
      g_free (pname);
    }

  if (file)
    g_object_unref (file);

  if (strcmp (name, id) == 0)
    {
      g_free (name);
      if (file_prefix && !prefix)
        return file_prefix;
      if (file_prefix)
        {
          char *t = g_strconcat (prefix, "-", file_prefix, NULL);
          g_free (prefix);
          g_free (file_prefix);
          prefix = t;
        }
      return prefix;
    }

  g_free (name);
  g_free (prefix);
  g_free (file_prefix);
  g_return_val_if_reached (NULL);
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
        const char *name;

        name = meta_window_get_wm_class (info->window);
        if (!name)
          name = _("Unknown");
        return g_strdup (name);
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

/**
 * shell_app_info_get_icon:
 * @info: A #ShellAppInfo
 *
 * Get the #GIcon associated with this app; for apps "faked" from a #MetaWindow,
 * return %NULL.
 *
 * Returns: (transfer full): The icon for @info, or %NULL
 */
GIcon *
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

/**
 * shell_app_system_get_sections:
 *
 * return names of sections in applications menu.
 *
 * Returns: (element-type utf8) (transfer full): List of Names
 */
GList *
shell_app_system_get_sections (ShellAppSystem *system)
{
  GList *res = NULL;
  GSList *i, *contents;
  GMenuTreeDirectory *root;

  root = gmenu_tree_get_root_directory (system->priv->apps_tree);

  if (G_UNLIKELY (!root))
    g_error ("applications.menu not found.");

  contents = gmenu_tree_directory_get_contents (root);

  for (i = contents; i; i = i->next)
    {
      GMenuTreeItem *item = i->data;
      if (gmenu_tree_item_get_type (item) == GMENU_TREE_ITEM_DIRECTORY)
        {
          char *name = g_strdup (gmenu_tree_directory_get_name ((GMenuTreeDirectory*)item));

          g_assert (name);

          res = g_list_append (res, name);
        }
      gmenu_tree_item_unref (item);
    }

  g_slist_free (contents);

  return res;
}

/**
 * shell_app_info_get_section:
 *
 * return name of section, that contain this application.
 * Returns: (transfer full): section name
 */
char *
shell_app_info_get_section (ShellAppInfo *info)
{
  char *name;
  GMenuTreeDirectory *dir, *parent;

  if (info->type != SHELL_APP_INFO_TYPE_ENTRY)
    return NULL;

  dir = gmenu_tree_item_get_parent ((GMenuTreeItem*)info->entry);
  if (!dir)
    return NULL;

  parent = gmenu_tree_item_get_parent ((GMenuTreeItem*)dir);
  if (!parent)
    return NULL;

  while (TRUE)
    {
      GMenuTreeDirectory *pparent = gmenu_tree_item_get_parent ((GMenuTreeItem*)parent);
      if (!pparent)
        break;
      gmenu_tree_item_unref ((GMenuTreeItem*)dir);
      dir = parent;
      parent = pparent;
    }

  name = g_strdup (gmenu_tree_directory_get_name (dir));

  gmenu_tree_item_unref ((GMenuTreeItem*)dir);
  gmenu_tree_item_unref ((GMenuTreeItem*)parent);
  return name;
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

  ret = NULL;

  if (info->type == SHELL_APP_INFO_TYPE_WINDOW)
    {
      ret = st_texture_cache_bind_pixbuf_property (st_texture_cache_get_default (),
                                                   G_OBJECT (info->window),
                                                   "icon");
    }
  else
    {
      icon = shell_app_info_get_icon (info);
      if (icon != NULL)
        {
          ret = st_texture_cache_load_gicon (st_texture_cache_get_default (), NULL, icon, (int)size);
          g_object_unref (icon);
        }
    }

  if (ret == NULL)
    {
      icon = g_themed_icon_new ("application-x-executable");
      ret = st_texture_cache_load_gicon (st_texture_cache_get_default (), NULL, icon, (int)size);
      g_object_unref (icon);
    }

  return ret;
}

/**
 * shell_app_info_get_source_window:
 * @info: A #ShellAppInfo
 *
 * Returns: (transfer none): If @info is tracking a #MetaWindow,
 *   return that window.  Otherwise, return %NULL.
 */
MetaWindow *
shell_app_info_get_source_window (ShellAppInfo *info)
{
  if (info->type == SHELL_APP_INFO_TYPE_WINDOW)
    return info->window;
  return NULL;
}

static void
_gather_pid_callback (GDesktopAppInfo   *gapp,
                      GPid               pid,
                      gpointer           data)
{
  ShellApp *app;
  ShellWindowTracker *tracker;

  g_return_if_fail (data != NULL);

  app = SHELL_APP (data);
  tracker = shell_window_tracker_get_default ();

  _shell_window_tracker_add_child_process_app (tracker,
                                               pid,
                                               app);
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
  ShellApp *shell_app;
  GDesktopAppInfo *gapp;
  GdkAppLaunchContext *context;
  gboolean ret;
  ShellGlobal *global;
  MetaScreen *screen;

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
  else if (info->type == SHELL_APP_INFO_TYPE_ENTRY)
    {
      /* Can't use g_desktop_app_info_new, see bug 614879 */
      const char *filename = gmenu_tree_entry_get_desktop_file_path ((GMenuTreeEntry *)info->entry);
      gapp = g_desktop_app_info_new_from_filename (filename);
    }
  else
    {
      char *filename = shell_app_info_get_desktop_file_path (info);
      gapp = g_desktop_app_info_new_from_filename (filename);
      g_free (filename);
    }

  if (!gapp)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Not found");
      return FALSE;
    }

  global = shell_global_get ();
  screen = shell_global_get_screen (global);

  if (timestamp == 0)
    timestamp = clutter_get_current_event_time ();

  if (workspace < 0)
    workspace = meta_screen_get_active_workspace_index (screen);

  context = gdk_app_launch_context_new ();
  gdk_app_launch_context_set_timestamp (context, timestamp);
  gdk_app_launch_context_set_desktop (context, workspace);

  shell_app = shell_app_system_get_app (shell_app_system_get_default (),
                                        shell_app_info_get_id (info));

  /* In the case where we know an app, we handle reaping the child internally,
   * in the window tracker.
   */
  if (shell_app != NULL)
    ret = g_desktop_app_info_launch_uris_as_manager (gapp, uris,
                                                     G_APP_LAUNCH_CONTEXT (context),
                                                     G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                                                     NULL, NULL,
                                                     _gather_pid_callback, shell_app,
                                                     error);
  else
    ret = g_desktop_app_info_launch_uris_as_manager (gapp, uris,
                                                     G_APP_LAUNCH_CONTEXT (context),
                                                     G_SPAWN_SEARCH_PATH,
                                                     NULL, NULL,
                                                     NULL, NULL,
                                                     error);

  g_object_unref (G_OBJECT (gapp));

  return ret;
}

gboolean
shell_app_info_launch (ShellAppInfo    *info,
                       GError         **error)
{
  return shell_app_info_launch_full (info, 0, NULL, -1, NULL, error);
}
