/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "shell-uri-util.h"
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>
#include <gtk/gtk.h>

/* The code in this file adapted under the GPLv2+ from:
 *
 * GNOME panel utils: gnome-panel/gnome-panel/panel-util.c
 * (C) 1997, 1998, 1999, 2000 The Free Software Foundation
 * Copyright 2000 Helix Code, Inc.
 * Copyright 2000,2001 Eazel, Inc.
 * Copyright 2001 George Lebl
 * Copyright 2002 Sun Microsystems Inc.
 *
 * Authors: George Lebl
 *          Jacob Berkman
 *          Mark McLoughlin
 */

static GFile *
shell_util_get_gfile_root (GFile *file)
{
  GFile *parent;
  GFile *parent_old;

  /* search for the root on the URI */
  parent_old = g_object_ref (file);
  parent = g_file_get_parent (file);
  while (parent != NULL)
    {
      g_object_unref (parent_old);
      parent_old = parent;
      parent = g_file_get_parent (parent);
    }

  return parent_old;
}

static char *
shell_util_get_file_display_name_if_mount (GFile *file)
{
  GFile *compare;
  GVolumeMonitor *monitor;
  GList *mounts, *l;
  char *ret;

  ret = NULL;

  /* compare with all mounts */
  monitor = g_volume_monitor_get ();
  mounts = g_volume_monitor_get_mounts (monitor);
  for (l = mounts; l != NULL; l = l->next)
    {
      GMount *mount;
      mount = G_MOUNT(l->data);
      compare = g_mount_get_root (mount);
      if (!ret && g_file_equal (file, compare))
        ret = g_mount_get_name (mount);
      g_object_unref (mount);
    }
  g_list_free (mounts);
  g_object_unref (monitor);

  return ret;
}

#define HOME_NAME_KEY           "/apps/nautilus/desktop/home_icon_name"
static char *
shell_util_get_file_display_for_common_files (GFile *file)
{
  GFile *compare;

  compare = g_file_new_for_path (g_get_home_dir ());
  if (g_file_equal (file, compare))
    {
      char *gconf_name;

      g_object_unref (compare);

      gconf_name = gconf_client_get_string (gconf_client_get_default (),
                                            HOME_NAME_KEY, NULL);
      if (!(gconf_name && gconf_name[0]))
        {
          g_free (gconf_name);
          return g_strdup (_("Home Folder"));
        }
      else
        {
          return gconf_name;
        }
    }
  g_object_unref (compare);

  compare = g_file_new_for_path ("/");
  if (g_file_equal (file, compare))
    {
      g_object_unref (compare);
      /* Translators: this is the same string as the one found in
       * nautilus */
      return g_strdup (_("File System"));
    }
  g_object_unref (compare);

  return NULL;
}

char *
shell_util_get_file_description (GFile *file)
{
  GFileInfo *info;
  char *ret;

  ret = NULL;

  info = g_file_query_info (file, "standard::description",
                            G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);

  if (info)
    {
      ret = g_strdup (g_file_info_get_attribute_string(info,
          G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION));
      g_object_unref (info);
    }

  return ret;
}

static char *
shell_util_get_file_display_name (GFile *file, gboolean use_fallback)
{
  GFileInfo *info;
  char *ret;

  ret = NULL;

  info = g_file_query_info (file, "standard::display-name",
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);

  if (info)
    {
      ret = g_strdup (g_file_info_get_display_name (info));
      g_object_unref (info);
    }

  if (!ret && use_fallback)
    {
      /* can happen with URI schemes non supported by gvfs */
      char *basename;

      basename = g_file_get_basename (file);
      ret = g_filename_display_name (basename);
      g_free (basename);
    }

  return ret;
}

static GIcon *
shell_util_get_file_icon_if_mount (GFile *file)
{
  GFile *compare;
  GVolumeMonitor *monitor;
  GList *mounts, *l;
  GIcon *ret;

  ret = NULL;

  /* compare with all mounts */
  monitor = g_volume_monitor_get ();
  mounts = g_volume_monitor_get_mounts (monitor);
  for (l = mounts; l != NULL; l = l->next)
    {
      GMount *mount;
      mount = G_MOUNT (l->data);
      compare = g_mount_get_root (mount);
      if (!ret && g_file_equal (file, compare))
        {
          ret = g_mount_get_icon (mount);
        }
      g_object_unref (mount);
    }
  g_list_free (mounts);
  g_object_unref (monitor);

  return ret;
}

static const char *
shell_util_get_icon_for_uri_known_folders (const char *uri)
{
  const char *icon;
  char *path;
  int len;

  icon = NULL;

  if (!g_str_has_prefix (uri, "file:"))
    return NULL;

  path = g_filename_from_uri (uri, NULL, NULL);

  len = strlen (path);
  if (path[len] == '/')
    path[len] = '\0';

  if (strcmp (path, "/") == 0)
    icon = "drive-harddisk";
  else if (strcmp (path, g_get_home_dir ()) == 0)
    icon = "user-home";
  else if (strcmp (path, g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP))
      == 0)
    icon = "user-desktop";

  g_free (path);

  return icon;
}

/* This is based on nautilus_compute_title_for_uri() and
 * nautilus_file_get_display_name_nocopy() */
char *
shell_util_get_label_for_uri (const char *text_uri)
{
  GFile *file;
  char  *label;
  GFile *root;
  char  *root_display;

  /* Here's what we do:
   *  + x-nautilus-search: URI
   *  + check if the URI is a mount
   *  + if file: URI:
   *   - check for known file: URI
   *   - check for description of the GFile
   *   - use display name of the GFile
   *  + else:
   *   - check for description of the GFile
   *   - if the URI is a root: "root displayname"
   *   - else: "root displayname: displayname"
   */

  label = NULL;

  //FIXME: see nautilus_query_to_readable_string() to have a nice name
  if (g_str_has_prefix (text_uri, "x-nautilus-search:"))
    return g_strdup (_("Search"));

  file = g_file_new_for_uri (text_uri);

  label = shell_util_get_file_display_name_if_mount (file);
  if (label)
    {
      g_object_unref (file);
      return label;
    }

  if (g_str_has_prefix (text_uri, "file:"))
    {
      label = shell_util_get_file_display_for_common_files (file);
      if (!label)
        label = shell_util_get_file_description (file);
      if (!label)
        label = shell_util_get_file_display_name (file, TRUE);
        g_object_unref (file);

      return label;
    }

  label = shell_util_get_file_description (file);
  if (label)
    {
      g_object_unref (file);
      return label;
    }

  root = shell_util_get_gfile_root (file);
  root_display = shell_util_get_file_description (root);
  if (!root_display)
    root_display = shell_util_get_file_display_name (root, FALSE);
  if (!root_display)
    /* can happen with URI schemes non supported by gvfs */
    root_display = g_file_get_uri_scheme (root);

  if (g_file_equal (file, root))
    label = root_display;
  else
    {
      char *displayname;

      displayname = shell_util_get_file_display_name (file, TRUE);
      /* Translators: the first string is the name of a gvfs
       * method, and the second string is a path. For
       * example, "Trash: some-directory". It means that the
       * directory called "some-directory" is in the trash.
       */
       label = g_strdup_printf (_("%1$s: %2$s"),
                                root_display, displayname);
       g_free (root_display);
       g_free (displayname);
    }

  g_object_unref (root);
  g_object_unref (file);

  return label;
}

/**
 * shell_util_get_icon_for_uri:
 * @text_uri: A URI
 *
 * Look up the icon that should be associated with a given URI.  Handles
 * various special GNOME-internal cases like x-nautilus-search, etc.
 *
 * Return Value: (transfer none): A new #GIcon
 */
GIcon *
shell_util_get_icon_for_uri (const char *text_uri)
{
  const char *name;
  GFile *file;
  GFileInfo *info;
  GIcon *retval;

  /* Here's what we do:
   *  + check for known file: URI
   *  + x-nautilus-search: URI
   *  + override burn: URI icon
   *  + check if the URI is a mount
   *  + override trash: URI icon for subfolders
   *  + check for application/x-gnome-saved-search mime type and override
   *    icon of the GFile
   *  + use icon of the GFile
   */

  /* this only checks file: URI */
  name = shell_util_get_icon_for_uri_known_folders (text_uri);
  if (name)
    return g_themed_icon_new (name);

  if (g_str_has_prefix (text_uri, "x-nautilus-search:"))
    return g_themed_icon_new ("folder-saved-search");

  /* gvfs doesn't give us a nice icon, so overriding */
  if (g_str_has_prefix (text_uri, "burn:"))
    return g_themed_icon_new ("nautilus-cd-burner");

  file = g_file_new_for_uri (text_uri);

  retval = shell_util_get_file_icon_if_mount (file);
  if (retval)
    return retval;

  /* gvfs doesn't give us a nice icon for subfolders of the trash, so
   * overriding */
  if (g_str_has_prefix (text_uri, "trash:"))
    {
      GFile *root;

      root = shell_util_get_gfile_root (file);
      g_object_unref (file);
      file = root;
    }

  info = g_file_query_info (file, "standard::icon", G_FILE_QUERY_INFO_NONE,
                            NULL, NULL);
  g_object_unref (file);

  if (!info)
    return g_themed_icon_new ("gtk-file");

  retval = g_file_info_get_icon (info);
  if (retval)
    g_object_ref (retval);
  g_object_unref (info);

  if (retval)
    return retval;

  return g_themed_icon_new ("gtk-file");
}
