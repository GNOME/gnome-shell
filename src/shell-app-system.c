/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "shell-app-system.h"

#include <gio/gio.h>

#define GMENU_I_KNOW_THIS_IS_UNSTABLE
#include <gmenu-tree.h>

enum {
   PROP_0,

};

enum {
  CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _ShellAppSystemPrivate {
  GMenuTree *apps_tree;
  GMenuTree *settings_tree;

  GSList *cached_app_menus; /* ShellAppMenuEntry */

  GSList *cached_setting_ids; /* utf8 */
};

static void shell_app_system_finalize (GObject *object);
static void on_tree_changed (GMenuTree *tree, gpointer user_data);
static void reread_menus (ShellAppSystem *self);

G_DEFINE_TYPE(ShellAppSystem, shell_app_system, G_TYPE_OBJECT);

static gpointer
shell_app_menu_entry_copy (gpointer entryp)
{
  ShellAppMenuEntry *entry;
  ShellAppMenuEntry *copy;
  entry = entryp;
  copy = g_new0 (ShellAppMenuEntry, 1);
  copy->name = g_strdup (entry->name);
  copy->id = g_strdup (entry->id);
  copy->icon = g_strdup (entry->icon);
  return copy;
}

static void
shell_app_menu_entry_free (gpointer entryp)
{
  ShellAppMenuEntry *entry = entryp;
  g_free (entry->name);
  g_free (entry->id);
  g_free (entry->icon);
  g_free (entry);
}

static void shell_app_system_class_init(ShellAppSystemClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;

  gobject_class->finalize = shell_app_system_finalize;

  signals[CHANGED] =
    g_signal_new ("changed",
		  SHELL_TYPE_APP_SYSTEM,
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ShellAppSystemClass, changed),
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

  priv->apps_tree = gmenu_tree_lookup ("applications.menu", GMENU_TREE_FLAGS_NONE);
  priv->settings_tree = gmenu_tree_lookup ("settings.menu", GMENU_TREE_FLAGS_NONE);

  gmenu_tree_add_monitor (priv->apps_tree, on_tree_changed, self);
  gmenu_tree_add_monitor (priv->settings_tree, on_tree_changed, self);

  reread_menus (self);
}

static void
shell_app_system_finalize (GObject *object)
{
  ShellAppSystem *self = SHELL_APP_SYSTEM (object);
  ShellAppSystemPrivate *priv = self->priv;

  gmenu_tree_remove_monitor (priv->apps_tree, on_tree_changed, self);
  gmenu_tree_remove_monitor (priv->settings_tree, on_tree_changed, self);

  gmenu_tree_unref (priv->apps_tree);
  gmenu_tree_unref (priv->settings_tree);

  g_slist_foreach (priv->cached_app_menus, (GFunc)shell_app_menu_entry_free, NULL);
  g_slist_free (priv->cached_app_menus);
  priv->cached_app_menus = NULL;

  g_slist_foreach (priv->cached_setting_ids, (GFunc)g_free, NULL);
  g_slist_free (priv->cached_setting_ids);
  priv->cached_setting_ids = NULL;

  G_OBJECT_CLASS (shell_app_system_parent_class)->finalize(object);
}

static void
reread_directories (ShellAppSystem *self, GSList **cache, GMenuTree *tree)
{
  ShellAppSystemPrivate *priv = self->priv;
  GMenuTreeDirectory *trunk;
  GSList *entries;
  GSList *iter;

  trunk = gmenu_tree_get_root_directory (tree);
  entries = gmenu_tree_directory_get_contents (trunk);

  g_slist_foreach (*cache, (GFunc)shell_app_menu_entry_free, NULL);
  g_slist_free (*cache);
  *cache = NULL;

  for (iter = entries; iter; iter = iter->next)
    {
      GMenuTreeItem *item = iter->data;

      switch (gmenu_tree_item_get_type (item))
        {
          case GMENU_TREE_ITEM_DIRECTORY:
            {
              GMenuTreeDirectory *dir = iter->data;
              ShellAppMenuEntry *shell_entry = g_new0 (ShellAppMenuEntry, 1);
              shell_entry->name = g_strdup (gmenu_tree_directory_get_name (dir));
              shell_entry->id = g_strdup (gmenu_tree_directory_get_menu_id (dir));
              shell_entry->icon = g_strdup (gmenu_tree_directory_get_icon (dir));

              *cache = g_slist_prepend (*cache, shell_entry);
            }
            break;
          default:
            break;
        }

      gmenu_tree_item_unref (item);
    }
  *cache = g_slist_reverse (*cache);

  g_slist_free (entries);
  gmenu_tree_item_unref (trunk);
}

static GSList *
gather_entries_recurse (ShellAppSystem     *monitor,
                        GSList             *ids,
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
              GMenuTreeEntry *entry = (GMenuTreeEntry *)item;
              const char *id = gmenu_tree_entry_get_desktop_file_id (entry);
              ids = g_slist_prepend (ids, g_strdup (id));
            }
            break;
          case GMENU_TREE_ITEM_DIRECTORY:
            {
              GMenuTreeDirectory *dir = (GMenuTreeDirectory*)item;
              ids = gather_entries_recurse (monitor, ids, dir);
            }
            break;
          default:
            break;
        }
      gmenu_tree_item_unref (item);
    }

  g_slist_free (contents);

  return ids;
}

static void
reread_entries (ShellAppSystem     *self,
                GSList            **cache,
                GMenuTree          *tree)
{
  GMenuTreeDirectory *trunk;

  trunk = gmenu_tree_get_root_directory (tree);

  g_slist_foreach (*cache, (GFunc)g_free, NULL);
  g_slist_free (*cache);
  *cache = NULL;

  *cache = gather_entries_recurse (self, *cache, trunk);

  gmenu_tree_item_unref (trunk);
}

static void
reread_menus (ShellAppSystem *self)
{
  reread_directories (self, &(self->priv->cached_app_menus), self->priv->apps_tree);
  reread_entries (self, &(self->priv->cached_setting_ids), self->priv->settings_tree);
}

static void
on_tree_changed (GMenuTree *monitor, gpointer user_data)
{
  ShellAppSystem *self = SHELL_APP_SYSTEM (user_data);

  g_signal_emit (self, signals[CHANGED], 0);

  reread_menus (self);
}

GType
shell_app_menu_entry_get_type (void)
{
  static GType gtype = G_TYPE_INVALID;
  if (gtype == G_TYPE_INVALID)
    {
      gtype = g_boxed_type_register_static ("ShellAppMenuEntry",
          shell_app_menu_entry_copy,
          shell_app_menu_entry_free);
    }
  return gtype;
}

/**
 * shell_app_system_get_applications_for_menu:
 *
 * Traverses a toplevel menu, and returns all items under it.  Nested items
 * are flattened.
 *
 * Return value: (transfer full) (element-type utf8): List of desktop file ids
 */
GSList *
shell_app_system_get_applications_for_menu (ShellAppSystem *monitor,
                                            const char *menu)
{
  char *path;
  GMenuTreeDirectory *menu_entry;
  GSList *apps;

  path = g_strdup_printf ("/%s", menu);
  menu_entry = gmenu_tree_get_directory_from_path (monitor->priv->apps_tree, path);
  g_free (path);
  g_assert (menu_entry != NULL);

  apps = gather_entries_recurse (monitor, NULL, menu_entry);

  gmenu_tree_item_unref (menu_entry);

  return apps;
}

/**
 * shell_app_system_get_menus:
 *
 * Returns a list of toplevel menu names, like "Accessories", "Programming", etc.
 *
 * Return value: (transfer none) (element-type AppMenuEntry): List of toplevel menus
 */
GSList *
shell_app_system_get_menus (ShellAppSystem *monitor)
{
  return monitor->priv->cached_app_menus;
}

/**
 * shell_app_system_get_all_settings:
 *
 * Returns a list of all desktop file ids under "settings.menu".
 *
 * Return value: (transfer none) (element-type utf8): List of desktop file ids
 */
GSList *
shell_app_system_get_all_settings (ShellAppSystem *monitor)
{
  return monitor->priv->cached_setting_ids;
}
