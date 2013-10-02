/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include "shell-app-system.h"
#include "shell-app-usage.h"
#include <string.h>

#include <gio/gio.h>
#include <glib/gi18n.h>

#include "shell-app-private.h"
#include "shell-window-tracker-private.h"
#include "shell-app-system-private.h"
#include "shell-global.h"
#include "shell-util.h"

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

struct _ShellAppSystemPrivate {
  GMenuTree *apps_tree;

  GHashTable *running_apps;
  GHashTable *visible_id_to_app;
  GHashTable *id_to_app;
  GHashTable *startup_wm_class_to_app;

  GSList *known_vendor_prefixes;
};

static void shell_app_system_finalize (GObject *object);
static void on_apps_tree_changed_cb (GMenuTree *tree, gpointer user_data);

G_DEFINE_TYPE(ShellAppSystem, shell_app_system, G_TYPE_OBJECT);

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
		  G_STRUCT_OFFSET (ShellAppSystemClass, installed_changed),
          NULL, NULL, NULL,
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

  priv->running_apps = g_hash_table_new_full (NULL, NULL, (GDestroyNotify) g_object_unref, NULL);
  priv->id_to_app = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           NULL,
                                           (GDestroyNotify)g_object_unref);

  /* All the objects in this hash table are owned by id_to_app */
  priv->visible_id_to_app = g_hash_table_new (g_str_hash, g_str_equal);

  priv->startup_wm_class_to_app = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                         NULL,
                                                         (GDestroyNotify)g_object_unref);

  /* We want to track NoDisplay apps, so we add INCLUDE_NODISPLAY. We'll
   * filter NoDisplay apps out when showing them to the user. */
  priv->apps_tree = gmenu_tree_new ("applications.menu", GMENU_TREE_FLAGS_INCLUDE_NODISPLAY);
  g_signal_connect (priv->apps_tree, "changed", G_CALLBACK (on_apps_tree_changed_cb), self);

  on_apps_tree_changed_cb (priv->apps_tree, self);
}

static void
shell_app_system_finalize (GObject *object)
{
  ShellAppSystem *self = SHELL_APP_SYSTEM (object);
  ShellAppSystemPrivate *priv = self->priv;

  g_object_unref (priv->apps_tree);

  g_hash_table_destroy (priv->running_apps);
  g_hash_table_destroy (priv->id_to_app);
  g_hash_table_destroy (priv->visible_id_to_app);
  g_hash_table_destroy (priv->startup_wm_class_to_app);

  g_slist_free_full (priv->known_vendor_prefixes, g_free);
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
get_flattened_entries_recurse (GMenuTreeDirectory *dir,
                               GHashTable         *entry_set)
{
  GMenuTreeIter *iter = gmenu_tree_directory_iter (dir);
  GMenuTreeItemType next_type;

  while ((next_type = gmenu_tree_iter_next (iter)) != GMENU_TREE_ITEM_INVALID)
    {
      gpointer item = NULL;

      switch (next_type)
        {
        case GMENU_TREE_ITEM_ENTRY:
          {
            GMenuTreeEntry *entry;
            item = entry = gmenu_tree_iter_get_entry (iter);
            /* Key is owned by entry */
            g_hash_table_replace (entry_set,
                                  (char*)gmenu_tree_entry_get_desktop_file_id (entry),
                                  gmenu_tree_item_ref (entry));
          }
          break;
        case GMENU_TREE_ITEM_DIRECTORY:
          {
            item = gmenu_tree_iter_get_directory (iter);
            get_flattened_entries_recurse ((GMenuTreeDirectory*)item, entry_set);
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

static GHashTable *
get_flattened_entries_from_tree (GMenuTree *tree)
{
  GHashTable *table;
  GMenuTreeDirectory *root;

  table = g_hash_table_new_full (g_str_hash, g_str_equal,
                                 (GDestroyNotify) NULL,
                                 (GDestroyNotify) gmenu_tree_item_unref);

  root = gmenu_tree_get_root_directory (tree);
  
  if (root != NULL)
    get_flattened_entries_recurse (root, table);

  gmenu_tree_item_unref (root);
  
  return table;
}

static void
on_apps_tree_changed_cb (GMenuTree *tree,
                         gpointer   user_data)
{
  ShellAppSystem *self = SHELL_APP_SYSTEM (user_data);
  GError *error = NULL;
  GHashTable *new_apps;
  GHashTableIter iter;
  gpointer key, value;

  g_assert (tree == self->priv->apps_tree);

  g_hash_table_remove_all (self->priv->visible_id_to_app);
  g_slist_free_full (self->priv->known_vendor_prefixes, g_free);
  self->priv->known_vendor_prefixes = NULL;

  if (!gmenu_tree_load_sync (self->priv->apps_tree, &error))
    {
      if (error)
        {
          g_warning ("Failed to load apps: %s", error->message);
          g_error_free (error);
        }
      else
        {
          g_warning ("Failed to load apps");
        }
      return;
    }

  new_apps = get_flattened_entries_from_tree (self->priv->apps_tree);
  g_hash_table_iter_init (&iter, new_apps);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const char *id = key;
      GMenuTreeEntry *entry = value;
      char *prefix;
      ShellApp *app;
      GDesktopAppInfo *info;
      const char *startup_wm_class;

      prefix = get_prefix_for_entry (entry);

      if (prefix != NULL
          && !g_slist_find_custom (self->priv->known_vendor_prefixes, prefix,
                                   (GCompareFunc)g_strcmp0))
        self->priv->known_vendor_prefixes = g_slist_append (self->priv->known_vendor_prefixes,
                                                            prefix);
      else
        g_free (prefix);

      info = g_desktop_app_info_new (gmenu_tree_entry_get_desktop_file_id (entry));

      app = g_hash_table_lookup (self->priv->id_to_app, id);
      if (app != NULL)
        {
          GDesktopAppInfo *old_info;
          const gchar *old_startup_wm_class;

          old_info = shell_app_get_app_info (app);
          old_startup_wm_class = g_desktop_app_info_get_startup_wm_class (old_info);

          if (old_startup_wm_class)
            g_hash_table_remove (self->priv->startup_wm_class_to_app, old_startup_wm_class);

          _shell_app_set_app_info (app, info);
          g_object_ref (app);  /* Extra ref, removed in _replace below */
        }
      else
        {
          app = _shell_app_new (info);
        }

      g_object_unref (info);

      g_hash_table_replace (self->priv->id_to_app, (char*)id, app);
      if (!gmenu_tree_entry_get_is_nodisplay_recurse (entry))
        g_hash_table_replace (self->priv->visible_id_to_app, (char*)id, app);

      startup_wm_class = g_desktop_app_info_get_startup_wm_class (info);
      if (startup_wm_class)
        g_hash_table_replace (self->priv->startup_wm_class_to_app,
                              (char*)startup_wm_class, g_object_ref (app));
    }
  /* Now iterate over the apps again; we need to unreference any apps
   * which have been removed.  The JS code may still be holding a
   * reference; that's fine.
   */
  g_hash_table_iter_init (&iter, self->priv->id_to_app);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      const char *id = key;
      
      if (!g_hash_table_lookup (new_apps, id))
        g_hash_table_iter_remove (&iter);
    }

  g_hash_table_destroy (new_apps);

  g_signal_emit (self, signals[INSTALLED_CHANGED], 0);
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
  return g_hash_table_lookup (self->priv->id_to_app, id);
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

  app_path = g_desktop_app_info_get_filename (shell_app_get_app_info (app));
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
 * shell_app_system_lookup_desktop_wmclass:
 * @system: a #ShellAppSystem
 * @wmclass: (allow-none): A WM_CLASS value
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
 * @wmclass: (allow-none): A WM_CLASS value
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
  if (wmclass == NULL)
    return NULL;

  return g_hash_table_lookup (system->priv->startup_wm_class_to_app, wmclass);
}

void
_shell_app_system_notify_app_state_changed (ShellAppSystem *self,
                                            ShellApp       *app)
{
  ShellAppState state = shell_app_get_state (app);

  switch (state)
    {
    case SHELL_APP_STATE_RUNNING:
    case SHELL_APP_STATE_BUSY:
      g_hash_table_insert (self->priv->running_apps, g_object_ref (app), NULL);
      break;
    case SHELL_APP_STATE_STARTING:
      break;
    case SHELL_APP_STATE_STOPPED:
      g_hash_table_remove (self->priv->running_apps, app);
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


static gint
compare_apps_by_usage (gconstpointer a,
                       gconstpointer b,
                       gpointer      data)
{
  ShellAppUsage *usage = shell_app_usage_get_default ();

  ShellApp *app_a = (ShellApp*)a;
  ShellApp *app_b = (ShellApp*)b;

  return shell_app_usage_compare (usage, "", app_a, app_b);
}

static GSList *
sort_and_concat_results (ShellAppSystem *system,
                         GSList         *prefix_matches,
                         GSList         *substring_matches)
{
  GSList *matches = NULL;
  GSList *l;

  prefix_matches = g_slist_sort_with_data (prefix_matches,
                                           compare_apps_by_usage,
                                           system);
  substring_matches = g_slist_sort_with_data (substring_matches,
                                              compare_apps_by_usage,
                                              system);

  for (l = substring_matches; l != NULL; l = l->next)
    matches = g_slist_prepend (matches, (char *) shell_app_get_id (SHELL_APP (l->data)));
  for (l = prefix_matches; l != NULL; l = l->next)
    matches = g_slist_prepend (matches, (char *) shell_app_get_id (SHELL_APP (l->data)));

  return g_slist_reverse (matches);
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
      normalized_terms = g_slist_prepend (normalized_terms,
                                          shell_util_normalize_casefold_and_unaccent (term));
    }
  return normalized_terms;
}

static GSList *
search_tree (ShellAppSystem *self,
             GSList         *terms,
             GHashTable     *apps)
{
  GSList *prefix_results = NULL;
  GSList *substring_results = NULL;
  GSList *normalized_terms;
  GHashTableIter iter;
  gpointer key, value;

  normalized_terms = normalize_terms (terms);

  g_hash_table_iter_init (&iter, apps);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      ShellApp *app = value;
      _shell_app_do_match (app, normalized_terms,
                           &prefix_results,
                           &substring_results);
    }
  g_slist_free_full (normalized_terms, g_free);

  return sort_and_concat_results (self, prefix_results, substring_results);
}

/**
 * shell_app_system_initial_search:
 * @system: A #ShellAppSystem
 * @terms: (element-type utf8): List of terms, logical AND
 *
 * Search through applications for the given search terms.
 *
 * Returns: (transfer container) (element-type utf8): List of applications
 */
GSList *
shell_app_system_initial_search (ShellAppSystem  *self,
                                 GSList          *terms)
{
  return search_tree (self, terms, self->priv->visible_id_to_app);
}

/**
 * shell_app_system_subsearch:
 * @system: A #ShellAppSystem
 * @previous_results: (element-type utf8): List of previous results
 * @terms: (element-type utf8): List of terms, logical AND
 *
 * Search through a previous result set; for more information, see
 * js/ui/search.js. Note that returned strings are only valid until
 * a return to the main loop.
 *
 * Returns: (transfer container) (element-type utf8): List of application identifiers
 */
GSList *
shell_app_system_subsearch (ShellAppSystem   *system,
                            GSList           *previous_results,
                            GSList           *terms)
{
  GSList *iter;
  GSList *prefix_results = NULL;
  GSList *substring_results = NULL;
  GSList *normalized_terms = normalize_terms (terms);

  previous_results = g_slist_reverse (previous_results);

  for (iter = previous_results; iter; iter = iter->next)
    {
      ShellApp *app = shell_app_system_lookup_app (system, iter->data);

      _shell_app_do_match (app, normalized_terms,
                           &prefix_results,
                           &substring_results);
    }
  g_slist_free_full (normalized_terms, g_free);

  /* Note that a shorter term might have matched as a prefix, but
     when extended only as a substring, so we have to redo the
     sort rather than reusing the existing ordering */
  return sort_and_concat_results (system, prefix_results, substring_results);
}

