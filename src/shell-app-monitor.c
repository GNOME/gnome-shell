/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "shell-app-monitor.h"

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

struct _ShellAppMonitorPrivate {
  GMenuTree *tree;
  GMenuTreeDirectory *trunk;

  GList *cached_menus;
};

static void shell_app_monitor_finalize (GObject *object);
static void on_tree_changed (GMenuTree *tree, gpointer user_data);
static void reread_menus (ShellAppMonitor *self);

G_DEFINE_TYPE(ShellAppMonitor, shell_app_monitor, G_TYPE_OBJECT);

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

static void shell_app_monitor_class_init(ShellAppMonitorClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;

  gobject_class->finalize = shell_app_monitor_finalize;

  signals[CHANGED] =
    g_signal_new ("changed",
		  SHELL_TYPE_APP_MONITOR,
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ShellAppMonitorClass, changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  g_type_class_add_private (gobject_class, sizeof (ShellAppMonitorPrivate));
}

static void
shell_app_monitor_init (ShellAppMonitor *self)
{
  ShellAppMonitorPrivate *priv;
  self->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                                   SHELL_TYPE_APP_MONITOR,
                                                   ShellAppMonitorPrivate);

  priv->tree = gmenu_tree_lookup ("applications.menu", GMENU_TREE_FLAGS_NONE);

  priv->trunk = gmenu_tree_get_root_directory (priv->tree);

  gmenu_tree_add_monitor (priv->tree, on_tree_changed, self);

  reread_menus (self);
}

static void
shell_app_monitor_finalize (GObject *object)
{
  ShellAppMonitor *self = SHELL_APP_MONITOR (object);

  G_OBJECT_CLASS (shell_app_monitor_parent_class)->finalize(object);
}

static void
reread_menus (ShellAppMonitor *self)
{
  GSList *entries = gmenu_tree_directory_get_contents (self->priv->trunk);
  GSList *iter;
  ShellAppMonitorPrivate *priv = self->priv;

  g_list_foreach (self->priv->cached_menus, (GFunc)shell_app_menu_entry_free, NULL);
  g_list_free (self->priv->cached_menus);
  self->priv->cached_menus = NULL;

  for (iter = entries; iter; iter = iter->next)
    {
      GMenuTreeEntry *entry = iter->data;
      ShellAppMenuEntry *shell_entry = g_new0 (ShellAppMenuEntry, 1);

      shell_entry->name = g_strdup (gmenu_tree_entry_get_name (entry));
      shell_entry->id = g_strdup (gmenu_tree_entry_get_desktop_file_id (entry));
      shell_entry->icon = g_strdup (gmenu_tree_entry_get_icon (entry));

      priv->cached_menus = g_list_prepend (priv->cached_menus, shell_entry);

      gmenu_tree_item_unref (entry);
    }
  priv->cached_menus = g_list_reverse (priv->cached_menus);

  g_slist_free (entries);
}

static void
on_tree_changed (GMenuTree *monitor, gpointer user_data)
{
  ShellAppMonitor *self = SHELL_APP_MONITOR (user_data);

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
 * shell_app_monitor_get_applications_for_menu:
 *
 * Return value: (transfer full) (element-type utf8): List of desktop file ids
 */
GList *
shell_app_monitor_get_applications_for_menu (ShellAppMonitor *monitor,
                                             const char *menu)
{
  GList *ret = NULL;
  GSList *contents;
  GSList *iter;
  char *path;
  GMenuTreeDirectory *menu_entry;

  path = g_strdup_printf ("/%s", menu);
  menu_entry = gmenu_tree_get_directory_from_path (monitor->priv->tree, path);
  g_free (path);
  g_assert (menu_entry != NULL);

  contents = gmenu_tree_directory_get_contents (menu_entry);

  for (iter = contents; iter; iter = iter->next)
    {
      GMenuTreeItem *item = iter->data;
      switch (gmenu_tree_item_get_type (item))
        {
          case GMENU_TREE_ITEM_ENTRY:
            {
              GMenuTreeEntry *entry = (GMenuTreeEntry *)item;
              const char *id = gmenu_tree_entry_get_desktop_file_id (entry);
              ret = g_list_prepend (ret, g_strdup (id));
            }
            break;
          default:
            break;
        }
      gmenu_tree_item_unref (item);
    }
  g_slist_free (contents);

  return ret;
}

/**
 * shell_app_monitor_get_menus:
 *
 * Return value: (transfer none) (element-type AppMenuEntry): List of toplevel menus
 */
GList *
shell_app_monitor_get_menus (ShellAppMonitor *monitor)
{
  return monitor->priv->cached_menus;
}
