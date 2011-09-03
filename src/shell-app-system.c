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
#include "shell-app-system-private.h"
#include "shell-global.h"
#include "shell-util.h"
#include "st.h"

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
  APP_STATE_CHANGED,
  INSTALLED_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _ShellAppSystemPrivate {
  GMenuTree *apps_tree;

  GHashTable *entry_to_app;

  GHashTable *running_apps;

  GSList *known_vendor_prefixes;

  GMenuTree *settings_tree;
  GHashTable *setting_entry_to_app;
};

static void shell_app_system_finalize (GObject *object);
static void on_apps_tree_changed_cb (GMenuTree *tree, gpointer user_data);
static void on_settings_tree_changed_cb (GMenuTree *tree, gpointer user_data);

G_DEFINE_TYPE(ShellAppSystem, shell_app_system, G_TYPE_OBJECT);

static void shell_app_system_class_init(ShellAppSystemClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;

  gobject_class->finalize = shell_app_system_finalize;

  signals[APP_STATE_CHANGED] = g_signal_new ("app-state-changed",
                                             SHELL_TYPE_APP_SYSTEM,
                                             G_SIGNAL_RUN_LAST,
                                             0,
                                             NULL, NULL,
                                             g_cclosure_marshal_VOID__OBJECT,
                                             G_TYPE_NONE, 1,
                                             SHELL_TYPE_APP);
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

  priv->running_apps = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              NULL, (GDestroyNotify) g_object_unref);

  priv->entry_to_app = g_hash_table_new_full (NULL, NULL,
                                              (GDestroyNotify)gmenu_tree_item_unref,
                                              (GDestroyNotify)g_object_unref);
  priv->setting_entry_to_app = g_hash_table_new_full (NULL, NULL,
                                                      (GDestroyNotify)gmenu_tree_item_unref,
                                                      (GDestroyNotify)g_object_unref);

  /* For now, we want to pick up Evince, Nautilus, etc.  We'll
   * handle NODISPLAY semantics at a higher level or investigate them
   * case by case.
   */
  priv->apps_tree = gmenu_tree_new ("applications.menu", GMENU_TREE_FLAGS_INCLUDE_NODISPLAY);
  g_signal_connect (priv->apps_tree, "changed", G_CALLBACK (on_apps_tree_changed_cb), self);

  priv->settings_tree = gmenu_tree_new ("gnomecc.menu", 0);
  g_signal_connect (priv->settings_tree, "changed", G_CALLBACK (on_settings_tree_changed_cb), self);

  on_apps_tree_changed_cb (priv->apps_tree, self);
  on_settings_tree_changed_cb (priv->settings_tree, self);
}

static void
shell_app_system_finalize (GObject *object)
{
  ShellAppSystem *self = SHELL_APP_SYSTEM (object);
  ShellAppSystemPrivate *priv = self->priv;

  g_object_unref (priv->apps_tree);
  g_object_unref (priv->settings_tree);

  g_hash_table_destroy (priv->running_apps);
  g_hash_table_destroy (priv->entry_to_app);
  g_hash_table_destroy (priv->setting_entry_to_app);

  g_slist_foreach (priv->known_vendor_prefixes, (GFunc)g_free, NULL);
  g_slist_free (priv->known_vendor_prefixes);
  priv->known_vendor_prefixes = NULL;

  G_OBJECT_CLASS (shell_app_system_parent_class)->finalize (object);
}

static char *
get_prefix_for_entry (GMenuTreeEntry *entry)
{
  char *prefix = NULL, *file_prefix = NULL;
  const char *id;
  GFile *file;
  char *name;
  int i = 0;

  id = gmenu_tree_entry_get_desktop_file_id (entry);
  file = g_file_new_for_path (gmenu_tree_entry_get_desktop_file_path (entry));
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

static void
load_app_entry (ShellAppSystem *self,
                GMenuTreeEntry *entry)
{
  char *prefix;
  ShellApp *app;

  if (g_hash_table_lookup (self->priv->entry_to_app, entry))
    return;

  prefix = get_prefix_for_entry (entry);

  if (prefix
      && !g_slist_find_custom (self->priv->known_vendor_prefixes, prefix,
                               (GCompareFunc)g_strcmp0))
    self->priv->known_vendor_prefixes = g_slist_append (self->priv->known_vendor_prefixes,
                                                        prefix);
  else
    g_free (prefix);

  /* Here we check to see whether the app is still running; if so, we
   * keep the old data around.
   */
  app = g_hash_table_lookup (self->priv->running_apps, gmenu_tree_entry_get_desktop_file_id (entry));
  if (app != NULL)
    app = g_object_ref (app);
  else
    app = _shell_app_new (entry);

  g_hash_table_insert (self->priv->entry_to_app, gmenu_tree_item_ref (entry), app);
}

static void
gather_apps_recurse (ShellAppSystem     *self,
                     GMenuTreeDirectory *root)
{
  GMenuTreeIter *iter = gmenu_tree_directory_iter (root);
  GMenuTreeItemType next_type;

  while ((next_type = gmenu_tree_iter_next (iter)) != GMENU_TREE_ITEM_INVALID)
    {
      gpointer item = NULL;

      switch (next_type)
        {
        case GMENU_TREE_ITEM_ENTRY:
          {
            item = gmenu_tree_iter_get_entry (iter);
            load_app_entry (self, (GMenuTreeEntry*)item);
          }
          break;
        case GMENU_TREE_ITEM_DIRECTORY:
          {
            item = gmenu_tree_iter_get_directory (iter);
            gather_apps_recurse (self, (GMenuTreeDirectory*)item);
          }
          break;
        default:
          break;
        }
      if (item != NULL)
        gmenu_tree_item_unref (item);
    }

  gmenu_tree_iter_unref (iter);
}

static void
gather_settings_recurse (ShellAppSystem     *self,
                         GMenuTreeDirectory *root)
{
  GMenuTreeIter *iter = gmenu_tree_directory_iter (root);
  GMenuTreeItemType next_type;

  while ((next_type = gmenu_tree_iter_next (iter)) != GMENU_TREE_ITEM_INVALID)
    {
      gpointer item = NULL;

      switch (next_type)
        {
        case GMENU_TREE_ITEM_ENTRY:
          {
            ShellApp *app;

            item = gmenu_tree_iter_get_entry (iter);
            if (g_hash_table_lookup (self->priv->setting_entry_to_app, item))
              return;
            
            app = _shell_app_new (item);

            g_hash_table_insert (self->priv->setting_entry_to_app, gmenu_tree_item_ref (item), app);
          }
          break;
        case GMENU_TREE_ITEM_DIRECTORY:
          {
            item = gmenu_tree_iter_get_directory (iter);
            gather_settings_recurse (self, (GMenuTreeDirectory*)item);
          }
          break;
        default:
          break;
        }
      if (item != NULL)
        gmenu_tree_item_unref (item);
    }

  gmenu_tree_iter_unref (iter);
}

static void
on_apps_tree_changed_cb (GMenuTree *tree,
                         gpointer   user_data)
{
  ShellAppSystem *self = SHELL_APP_SYSTEM (user_data);
  GError *error = NULL;
  GMenuTreeDirectory *root;

  g_assert (tree == self->priv->apps_tree);

  g_hash_table_remove_all (self->priv->entry_to_app);
  g_slist_foreach (self->priv->known_vendor_prefixes, (GFunc)g_free, NULL);
  g_slist_free (self->priv->known_vendor_prefixes);
  self->priv->known_vendor_prefixes = NULL;

  if (!gmenu_tree_load_sync (self->priv->apps_tree, &error))
    {
      g_warning ("Failed to load apps: %s", error->message);
      return;
    }

  root = gmenu_tree_get_root_directory (self->priv->apps_tree);

  if (root)
    {
      gather_apps_recurse (self, root);
      gmenu_tree_item_unref (root);
    }

  g_signal_emit (self, signals[INSTALLED_CHANGED], 0);
}

static void
on_settings_tree_changed_cb (GMenuTree *tree,
                             gpointer   user_data)
{
  ShellAppSystem *self = SHELL_APP_SYSTEM (user_data);
  GError *error = NULL;
  GMenuTreeDirectory *root;

  g_assert (tree == self->priv->settings_tree);

  g_hash_table_remove_all (self->priv->setting_entry_to_app);
  if (!gmenu_tree_load_sync (self->priv->settings_tree, &error))
    {
      g_warning ("Failed to load settings: %s", error->message);
      return;
    }

  root = gmenu_tree_get_root_directory (self->priv->settings_tree);

  if (root)
    {
      gather_settings_recurse (self, root);
      gmenu_tree_item_unref (root);
    }
}

/**
 * shell_app_system_get_tree:
 *
 * Return Value: (transfer none): The #GMenuTree for apps
 */
GMenuTree *
shell_app_system_get_tree (ShellAppSystem *self)
{
  return self->priv->apps_tree;
}

/**
 * shell_app_system_get_settings_tree:
 *
 * Return Value: (transfer none): The #GMenuTree for apps
 */
GMenuTree *
shell_app_system_get_settings_tree (ShellAppSystem *self)
{
  return self->priv->settings_tree;
}

/**
 * shell_app_system_lookup_setting:
 * @self:
 * @id: desktop file id
 *
 * Returns: (transfer none): Application in gnomecc.menu, or %NULL if none
 */
ShellApp *
shell_app_system_lookup_setting (ShellAppSystem *self,
                                 const char     *id)
{
  GMenuTreeEntry *entry;
  ShellApp *app;

  /* Actually defer to the main app set if there's overlap */
  app = shell_app_system_lookup_app (self, id);
  if (app != NULL)
    return app;

  entry = gmenu_tree_get_entry_by_id (self->priv->settings_tree, id);
  if (entry == NULL)
    return NULL;
  
  app = g_hash_table_lookup (self->priv->setting_entry_to_app, entry);
  if (app != NULL)
    return app;
  
  app = _shell_app_new (entry);
  g_hash_table_insert (self->priv->setting_entry_to_app, gmenu_tree_item_ref (entry), app); 

  return app;
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
  GMenuTreeEntry *entry;

  entry = gmenu_tree_get_entry_by_id (self->priv->apps_tree, id);
  if (entry == NULL)
    return NULL;

  return g_hash_table_lookup (self->priv->entry_to_app, entry);
}

/**
 * shell_app_system_lookup_app_by_tree_entry:
 * @system: a #ShellAppSystem
 * @entry: a #GMenuTreeEntry
 *
 * Find a #ShellApp corresponding to a #GMenuTreeEntry.
 *
 * Return value: (transfer none): The #ShellApp for @entry, or %NULL if none
 */
ShellApp *
shell_app_system_lookup_app_by_tree_entry (ShellAppSystem  *self,
                                           GMenuTreeEntry  *entry)
{
  /* If we looked up directly in ->entry_to_app, we'd lose the
   * override of running apps.  Thus, indirect through the id.
   */
  return shell_app_system_lookup_app (self, gmenu_tree_entry_get_desktop_file_id (entry));
}

/**
 * shell_app_system_lookup_app_for_path:
 * @system: a #ShellAppSystem
 * @desktop_path: (type utf8): UTF-8 encoded absolute file name
 *
 * Find or create a #ShellApp corresponding to a given absolute file
 * name which must be in the standard paths (XDG_DATA_DIRS).  For
 * files outside the datadirs, this function returns %NULL.
 *
 * Return value: (transfer none): The #ShellApp for id, or %NULL if none
 */
ShellApp *
shell_app_system_lookup_app_for_path (ShellAppSystem   *system,
                                      const char       *desktop_path)
{
  const char *basename;
  const char *app_path;
  ShellApp *app;

  basename = g_strrstr (desktop_path, "/");
  if (basename)
    basename += 1;
  else
    basename = desktop_path;

  app = shell_app_system_lookup_app (system, basename);
  if (!app)
    return NULL;

  app_path = gmenu_tree_entry_get_desktop_file_path (shell_app_get_tree_entry (app));
  if (strcmp (desktop_path, app_path) != 0)
    return NULL;

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
  GSList *prefix;

  result = shell_app_system_lookup_app (system, name);
  if (result != NULL)
    return result;

  for (prefix = system->priv->known_vendor_prefixes; prefix; prefix = g_slist_next (prefix))
    {
      char *tmpid = g_strconcat ((char*)prefix->data, name, NULL);
      result = shell_app_system_lookup_app (system, tmpid);
      g_free (tmpid);
      if (result != NULL)
        return result;
    }

  return NULL;
}

/**
 * shell_app_system_get_all:
 * @self:
 *
 * Returns: (transfer container) (element-type ShellApp): All installed applications
 */
GSList *
shell_app_system_get_all (ShellAppSystem  *self)
{
  GSList *result = NULL;
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, self->priv->entry_to_app);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      ShellApp *app = value;
      
      if (!g_desktop_app_info_get_nodisplay (shell_app_get_app_info (app)))
        result = g_slist_prepend (result, app);
    }
  return result;
}

void
_shell_app_system_notify_app_state_changed (ShellAppSystem *self,
                                            ShellApp       *app)
{
  ShellAppState state = shell_app_get_state (app);

  switch (state)
    {
    case SHELL_APP_STATE_RUNNING:
      /* key is owned by the app */
      g_hash_table_insert (self->priv->running_apps, (char*)shell_app_get_id (app), g_object_ref (app));
      break;
    case SHELL_APP_STATE_STARTING:
      break;
    case SHELL_APP_STATE_STOPPED:
      g_hash_table_remove (self->priv->running_apps, shell_app_get_id (app));
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
      ShellApp *app = value;

      ret = g_slist_prepend (ret, app);
    }

  ret = g_slist_sort (ret, (GCompareFunc)shell_app_compare);

  return ret;
}


static gint
compare_apps_by_name (gconstpointer a,
                      gconstpointer b,
                      gpointer      data)
{
  ShellApp *app_a = (ShellApp*)a;
  ShellApp *app_b = (ShellApp*)b;

  return shell_app_compare_by_name (app_a, app_b);
}

static GSList *
sort_and_concat_results (ShellAppSystem *system,
                         GSList         *multiple_prefix_matches,
                         GSList         *prefix_matches,
                         GSList         *multiple_substring_matches,
                         GSList         *substring_matches)
{
  multiple_prefix_matches = g_slist_sort_with_data (multiple_prefix_matches,
                                                    compare_apps_by_name,
                                                    system);
  prefix_matches = g_slist_sort_with_data (prefix_matches,
                                           compare_apps_by_name,
                                           system);
  multiple_substring_matches = g_slist_sort_with_data (multiple_substring_matches,
                                                       compare_apps_by_name,
                                                       system);
  substring_matches = g_slist_sort_with_data (substring_matches,
                                              compare_apps_by_name,
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
      normalized_terms = g_slist_prepend (normalized_terms, shell_util_normalize_and_casefold (term));
    }
  return normalized_terms;
}

static GSList *
search_tree (ShellAppSystem *self,
             GSList         *terms,
             GHashTable     *apps)
{
  GSList *multiple_prefix_results = NULL;
  GSList *prefix_results = NULL;
  GSList *multiple_subtring_results = NULL;
  GSList *substring_results = NULL;
  GSList *normalized_terms;
  GHashTableIter iter;
  gpointer key, value;

  normalized_terms = normalize_terms (terms);

  g_hash_table_iter_init (&iter, apps);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const char *id = key;
      ShellApp *app = value;
      (void)id;
      _shell_app_do_match (app, normalized_terms,
                           &multiple_prefix_results, &prefix_results,
                           &multiple_subtring_results, &substring_results);
    }
  g_slist_foreach (normalized_terms, (GFunc)g_free, NULL);
  g_slist_free (normalized_terms);

  return sort_and_concat_results (self, multiple_prefix_results, prefix_results,
                                  multiple_subtring_results, substring_results);

}

/**
 * shell_app_system_initial_search:
 * @system: A #ShellAppSystem
 * @terms: (element-type utf8): List of terms, logical AND
 *
 * Search through applications for the given search terms.
 *
 * Returns: (transfer container) (element-type ShellApp): List of applications
 */
GSList *
shell_app_system_initial_search (ShellAppSystem  *self,
                                 GSList          *terms)
{
  return search_tree (self, terms, self->priv->entry_to_app);
}

/**
 * shell_app_system_subsearch:
 * @system: A #ShellAppSystem
 * @previous_results: (element-type ShellApp): List of previous results
 * @terms: (element-type utf8): List of terms, logical AND
 *
 * Search through a previous result set; for more information, see
 * js/ui/search.js.  Note the value of @prefs must be
 * the same as passed to shell_app_system_initial_search().  Note that returned
 * strings are only valid until a return to the main loop.
 *
 * Returns: (transfer container) (element-type ShellApp): List of application identifiers
 */
GSList *
shell_app_system_subsearch (ShellAppSystem   *system,
                            GSList           *previous_results,
                            GSList           *terms)
{
  GSList *iter;
  GSList *multiple_prefix_results = NULL;
  GSList *prefix_results = NULL;
  GSList *multiple_substring_results = NULL;
  GSList *substring_results = NULL;
  GSList *normalized_terms = normalize_terms (terms);

  for (iter = previous_results; iter; iter = iter->next)
    {
      ShellApp *app = iter->data;
      
      _shell_app_do_match (app, normalized_terms,
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

/**
 * shell_app_system_search_settings:
 * @system: A #ShellAppSystem
 * @terms: (element-type utf8): List of terms, logical AND
 *
 * Search through settings for the given search terms.
 *
 * Returns: (transfer container) (element-type ShellApp): List of setting applications
 */
GSList *
shell_app_system_search_settings (ShellAppSystem  *self,
                                  GSList          *terms)
{
  return search_tree (self, terms, self->priv->setting_entry_to_app);
}
