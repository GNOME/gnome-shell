/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "shell-app-system.h"
#include <string.h>

#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include "shell-global.h"
#include "shell-texture-cache.h"
#include "display.h"

#define GMENU_I_KNOW_THIS_IS_UNSTABLE
#include <gmenu-tree.h>

#define SHELL_APP_FAVORITES_KEY "/desktop/gnome/shell/favorite_apps"

enum {
   PROP_0,

};

enum {
  INSTALLED_CHANGED,
  FAVORITES_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _ShellAppSystemPrivate {
  GMenuTree *apps_tree;
  GMenuTree *settings_tree;

  GHashTable *app_id_to_app;

  GSList *cached_app_menus; /* ShellAppMenuEntry */

  GSList *cached_settings; /* ShellAppInfo */

  GHashTable *cached_favorites; /* <utf8,integer> */

  gint app_monitor_id;
};

static void shell_app_system_finalize (GObject *object);
static void on_tree_changed (GMenuTree *tree, gpointer user_data);
static void reread_menus (ShellAppSystem *self);
static void on_favorite_apps_changed (GConfClient *client, guint id, GConfEntry *entry, gpointer user_data);
static void reread_favorite_apps (ShellAppSystem *system);

G_DEFINE_TYPE(ShellAppSystem, shell_app_system, G_TYPE_OBJECT);

ShellAppInfo*
shell_app_info_ref (ShellAppInfo *info)
{
  return gmenu_tree_item_ref ((GMenuTreeItem*)info);
}

void
shell_app_info_unref (ShellAppInfo *info)
{
  gmenu_tree_item_unref ((GMenuTreeItem *)info);
}

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

  signals[INSTALLED_CHANGED] =
    g_signal_new ("installed-changed",
		  SHELL_TYPE_APP_SYSTEM,
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ShellAppSystemClass, installed_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  signals[FAVORITES_CHANGED] =
    g_signal_new ("favorites-changed",
                  SHELL_TYPE_APP_SYSTEM,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ShellAppSystemClass, favorites_changed),
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

  priv->cached_favorites = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  (GDestroyNotify)g_free,
                                                  NULL);

  /* The key is owned by the value */
  priv->app_id_to_app = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               NULL, (GDestroyNotify) shell_app_info_unref);

  /* For now, we want to pick up Evince, Nautilus, etc.  We'll
   * handle NODISPLAY semantics at a higher level or investigate them
   * case by case.
   */
  priv->apps_tree = gmenu_tree_lookup ("applications.menu", GMENU_TREE_FLAGS_INCLUDE_NODISPLAY);
  priv->settings_tree = gmenu_tree_lookup ("settings.menu", GMENU_TREE_FLAGS_NONE);

  gmenu_tree_add_monitor (priv->apps_tree, on_tree_changed, self);
  gmenu_tree_add_monitor (priv->settings_tree, on_tree_changed, self);

  reread_menus (self);

  client = gconf_client_get_default ();

  self->priv->app_monitor_id = gconf_client_notify_add (client, SHELL_APP_FAVORITES_KEY,
                                                        on_favorite_apps_changed, self, NULL, NULL);
  reread_favorite_apps (self);
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

  g_hash_table_destroy (priv->app_id_to_app);

  g_slist_foreach (priv->cached_app_menus, (GFunc)shell_app_menu_entry_free, NULL);
  g_slist_free (priv->cached_app_menus);
  priv->cached_app_menus = NULL;

  g_slist_foreach (priv->cached_settings, (GFunc)shell_app_info_unref, NULL);
  g_slist_free (priv->cached_settings);
  priv->cached_settings = NULL;

  g_hash_table_destroy (priv->cached_favorites);

  gconf_client_notify_remove (gconf_client_get_default (), priv->app_monitor_id);

  G_OBJECT_CLASS (shell_app_system_parent_class)->finalize(object);
}

static void
reread_directories (ShellAppSystem *self, GSList **cache, GMenuTree *tree)
{
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
                        GSList             *apps,
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
              apps = g_slist_prepend (apps, item);
            }
            break;
          case GMENU_TREE_ITEM_DIRECTORY:
            {
              GMenuTreeDirectory *dir = (GMenuTreeDirectory*)item;
              apps = gather_entries_recurse (monitor, apps, dir);
            }
            gmenu_tree_item_unref (item);
            break;
          default:
            gmenu_tree_item_unref (item);
            break;
        }
    }

  g_slist_free (contents);

  return apps;
}

static void
reread_entries (ShellAppSystem     *self,
                GSList            **cache,
                GMenuTree          *tree)
{
  GMenuTreeDirectory *trunk;

  trunk = gmenu_tree_get_root_directory (tree);

  g_slist_foreach (*cache, (GFunc)shell_app_info_unref, NULL);
  g_slist_free (*cache);
  *cache = NULL;

  *cache = gather_entries_recurse (self, *cache, trunk);

  gmenu_tree_item_unref (trunk);
}

static void
cache_by_id (ShellAppSystem *self, GSList *apps, gboolean ref)
{
  GSList *iter;

  for (iter = apps; iter; iter = iter->next)
    {
      ShellAppInfo *info = iter->data;
      if (ref)
        gmenu_tree_item_ref ((GMenuTreeItem*) info);
      /* the name is owned by the info itself */
      g_hash_table_insert (self->priv->app_id_to_app, (char*)shell_app_info_get_id (info),
                           info);
    }
}

static void
reread_menus (ShellAppSystem *self)
{
  GSList *apps;
  GMenuTreeDirectory *trunk;

  reread_directories (self, &(self->priv->cached_app_menus), self->priv->apps_tree);

  reread_entries (self, &(self->priv->cached_settings), self->priv->settings_tree);

  /* Now loop over applications.menu and settings.menu, inserting each by desktop file
   * ID into a hash */
  g_hash_table_remove_all (self->priv->app_id_to_app);
  trunk = gmenu_tree_get_root_directory (self->priv->apps_tree);
  apps = gather_entries_recurse (self, NULL, trunk);
  gmenu_tree_item_unref (trunk);
  cache_by_id (self, apps, FALSE);
  g_slist_free (apps);
  cache_by_id (self, self->priv->cached_settings, TRUE);
}

static void
on_tree_changed (GMenuTree *monitor, gpointer user_data)
{
  ShellAppSystem *self = SHELL_APP_SYSTEM (user_data);

  g_signal_emit (self, signals[INSTALLED_CHANGED], 0);

  reread_menus (self);
}

static void
copy_gconf_value_string_list_to_hashset (GConfValue *value,
                                         GHashTable *dest)
{
  GSList *list;
  GSList *tmp;

  list = gconf_value_get_list (value);

  for (tmp = list ; tmp; tmp = tmp->next)
    {
      GConfValue *value = tmp->data;
      char *str = g_strdup (gconf_value_get_string (value));
      if (!str)
        continue;
      g_hash_table_insert (dest, str, GUINT_TO_POINTER(1));
    }
}

static void
reread_favorite_apps (ShellAppSystem *system)
{
  GConfClient *client = gconf_client_get_default ();
  GConfValue *val;

  val = gconf_client_get (client, SHELL_APP_FAVORITES_KEY, NULL);

  if (!(val && val->type == GCONF_VALUE_LIST && gconf_value_get_list_type (val) == GCONF_VALUE_STRING))
    return;

  g_hash_table_remove_all (system->priv->cached_favorites);
  copy_gconf_value_string_list_to_hashset (val, system->priv->cached_favorites);

  gconf_value_free (val);
}

void
on_favorite_apps_changed (GConfClient *client,
                          guint        id,
                          GConfEntry  *entry,
                          gpointer     user_data)
{
  ShellAppSystem *system = SHELL_APP_SYSTEM (user_data);
  reread_favorite_apps (system);
  g_signal_emit (G_OBJECT (system), signals[FAVORITES_CHANGED], 0);
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
 * Return value: (transfer container) (element-type ShellAppInfo): List of applications
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
 * Returns a list of toplevel #ShellAppMenuEntry items
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

/**
 * shell_app_system_get_favorites:
 *
 * Return the list of applications which have been explicitly added to the
 * favorites.
 *
 * Return value: (transfer container) (element-type utf8): List of favorite application ids
 */
GList *
shell_app_system_get_favorites (ShellAppSystem *system)
{
  return g_hash_table_get_keys (system->priv->cached_favorites);
}

static void
set_gconf_value_string_list (GConfValue *val, GList *items)
{
  GList *iter;
  GSList *tmp = NULL;

  for (iter = items; iter; iter = iter->next)
    {
      const char *str = iter->data;
      GConfValue *strval = gconf_value_new (GCONF_VALUE_STRING);
      gconf_value_set_string (strval, str);
      tmp = g_slist_prepend (tmp, strval);
    }
  tmp = g_slist_reverse (tmp);

  gconf_value_set_list (val, tmp);
  g_slist_free (tmp);
}

void
shell_app_system_add_favorite (ShellAppSystem *system, const char *id)
{
  GConfClient *client = gconf_client_get_default ();
  GConfValue *val;
  GList *favorites;

  val = gconf_value_new (GCONF_VALUE_LIST);
  gconf_value_set_list_type (val, GCONF_VALUE_STRING);

  g_hash_table_insert (system->priv->cached_favorites, g_strdup (id), GUINT_TO_POINTER (1));

  favorites = g_hash_table_get_keys (system->priv->cached_favorites);
  set_gconf_value_string_list (val, favorites);
  g_list_free (favorites);

  gconf_client_set (client, SHELL_APP_FAVORITES_KEY, val, NULL);
}

void
shell_app_system_remove_favorite (ShellAppSystem *system, const char *id)
{
  GConfClient *client = gconf_client_get_default ();
  GConfValue *val;
  GList *favorites;

  if (!g_hash_table_remove (system->priv->cached_favorites, id))
    return;

  val = gconf_value_new (GCONF_VALUE_LIST);
  gconf_value_set_list_type (val, GCONF_VALUE_STRING);

  favorites = g_hash_table_get_keys (system->priv->cached_favorites);
  set_gconf_value_string_list (val, favorites);
  g_list_free (favorites);

  gconf_client_set (client, SHELL_APP_FAVORITES_KEY, val, NULL);
}

/**
 * shell_app_system_lookup_app:
 *
 * Return value: (transfer full): The #ShellAppInfo for id, or %NULL if none
 */
ShellAppInfo *
shell_app_system_lookup_app (ShellAppSystem *self, const char *id)
{
  GMenuTreeEntry *entry;

  entry = g_hash_table_lookup (self->priv->app_id_to_app, id);
  if (entry)
    gmenu_tree_item_ref ((GMenuTreeItem*) entry);
  return (ShellAppInfo*)entry;
}

/**
 * shell_app_system_lookup_heuristic_basename:
 * @name: Probable application identifier
 *
 * Find a valid application corresponding to a given
 * heuristically determined application identifier
 * string, or %NULL if none.
 *
 * Returns: (transfer full): A #ShellAppInfo for name
 */
ShellAppInfo *
shell_app_system_lookup_heuristic_basename (ShellAppSystem *system,
                                            const char *name)
{
  char *tmpid;
  ShellAppInfo *result;

  result = shell_app_system_lookup_app (system, name);
  if (result != NULL)
    return result;

  /* These are common "vendor prefixes".  But using
   * WM_CLASS as a source, we don't get the vendor
   * prefix.  So try stripping them.
   */
  tmpid = g_strjoin ("", "gnome-", name, NULL);
  result = shell_app_system_lookup_app (system, tmpid);
  g_free (tmpid);
  if (result != NULL)
    return result;

  tmpid = g_strjoin ("", "fedora-", name, NULL);
  result = shell_app_system_lookup_app (system, tmpid);
  g_free (tmpid);
  if (result != NULL)
    return result;

  return NULL;
}

const char *
shell_app_info_get_id (ShellAppInfo *info)
{
  return gmenu_tree_entry_get_desktop_file_id ((GMenuTreeEntry*)info);
}

const char *
shell_app_info_get_name (ShellAppInfo *info)
{
  return gmenu_tree_entry_get_name ((GMenuTreeEntry*)info);
}

const char *
shell_app_info_get_description (ShellAppInfo *info)
{
  return gmenu_tree_entry_get_comment ((GMenuTreeEntry*)info);
}

const char *
shell_app_info_get_executable (ShellAppInfo *info)
{
  return gmenu_tree_entry_get_exec ((GMenuTreeEntry*)info);
}

const char *
shell_app_info_get_desktop_file_path (ShellAppInfo *info)
{
  return gmenu_tree_entry_get_desktop_file_path ((GMenuTreeEntry*)info);
}

GIcon *
shell_app_info_get_icon (ShellAppInfo *info)
{
  const char *iconname;
  GIcon *icon;

  /* This code adapted from gdesktopappinfo.c
   * Copyright (C) 2006-2007 Red Hat, Inc.
   * Copyright © 2007 Ryan Lortie
   * LGPL
   */

  iconname = gmenu_tree_entry_get_icon ((GMenuTreeEntry*)info);
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

GSList *
shell_app_info_get_categories (ShellAppInfo *info)
{
  return NULL; /* TODO */
}

gboolean
shell_app_info_get_is_nodisplay (ShellAppInfo *info)
{
  return gmenu_tree_entry_get_is_nodisplay ((GMenuTreeEntry*)info);
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

  icon = shell_app_info_get_icon (info);
  if (!icon)
    {
      ret = clutter_texture_new ();
      g_object_set (ret, "width", size, "height", size, NULL);
      return ret;
    }

  return shell_texture_cache_load_gicon (shell_texture_cache_get_default (), icon, (int)size);
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
  const char *filename;
  GdkAppLaunchContext *context;
  gboolean ret;
  ShellGlobal *global;
  MetaScreen *screen;
  MetaDisplay *display;

  if (startup_id)
    *startup_id = NULL;

  filename = gmenu_tree_entry_get_desktop_file_path ((GMenuTreeEntry*) info);

  gapp = g_desktop_app_info_new_from_filename (filename);
  if (!gapp)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Not found");
      return FALSE;
    }

  global = shell_global_get ();
  screen = shell_global_get_screen (global);
  display = meta_screen_get_display (screen);

  if (timestamp == 0)
    timestamp = meta_display_get_current_time (display);
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
