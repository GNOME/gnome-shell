/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"

#include "shell-util.h"
#include <glib/gi18n-lib.h>
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

static char *
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

/**
 * shell_util_icon_from_string:
 * @string: a stringified #GIcon
 *
 * A static method equivalent to g_icon_new_for_string, workaround
 * for GJS not being able to represent Gio.Icon (which is an interface,
 * not a class).
 *
 * Returns: (transfer full): the icon which is represented by @string
 */
GIcon *
shell_util_icon_from_string (const char *string, GError **error)
{
  return g_icon_new_for_string (string, error);
}

static void
stop_pick (ClutterActor       *actor,
           const ClutterColor *color)
{
  g_signal_stop_emission_by_name (actor, "pick");
}

/**
 * shell_util_set_hidden_from_pick:
 * @actor: A #ClutterActor
 * @hidden: Whether @actor should be hidden from pick
 *
 * If @hidden is %TRUE, hide @actor from pick even with a mode of
 * %CLUTTER_PICK_ALL; if @hidden is %FALSE, unhide @actor.
 */
void
shell_util_set_hidden_from_pick (ClutterActor *actor,
                                 gboolean      hidden)
{
  gpointer existing_handler_data;

  existing_handler_data = g_object_get_data (G_OBJECT (actor),
                                             "shell-stop-pick");
  if (hidden)
    {
      if (existing_handler_data != NULL)
        return;
      g_signal_connect (actor, "pick", G_CALLBACK (stop_pick), NULL);
      g_object_set_data (G_OBJECT (actor),
                         "shell-stop-pick", GUINT_TO_POINTER (1));
    }
  else
    {
      if (existing_handler_data == NULL)
        return;
      g_signal_handlers_disconnect_by_func (actor, stop_pick, NULL);
      g_object_set_data (G_OBJECT (actor), "shell-stop-pick", NULL);
    }
}

/**
 * shell_util_get_transformed_allocation:
 * @actor: a #ClutterActor
 * @box: (out): location to store returned box in stage coordinates
 *
 * This function is similar to a combination of clutter_actor_get_transformed_position(),
 * and clutter_actor_get_transformed_size(), but unlike
 * clutter_actor_get_transformed_size(), it always returns a transform
 * of the current allocation, while clutter_actor_get_transformed_size() returns
 * bad values (the transform of the requested size) if a relayout has been
 * queued.
 *
 * This function is more convenient to use than
 * clutter_actor_get_abs_allocation_vertices() if no transformation is in effect
 * and also works around limitations in the GJS binding of arrays.
 */
void
shell_util_get_transformed_allocation (ClutterActor    *actor,
                                       ClutterActorBox *box)
{
  /* Code adapted from clutter-actor.c:
   * Copyright 2006, 2007, 2008 OpenedHand Ltd
   */
  ClutterVertex v[4];
  gfloat x_min, x_max, y_min, y_max;
  gint i;

  g_return_if_fail (CLUTTER_IS_ACTOR (actor));

  clutter_actor_get_abs_allocation_vertices (actor, v);

  x_min = x_max = v[0].x;
  y_min = y_max = v[0].y;

  for (i = 1; i < G_N_ELEMENTS (v); ++i)
    {
      if (v[i].x < x_min)
	x_min = v[i].x;

      if (v[i].x > x_max)
	x_max = v[i].x;

      if (v[i].y < y_min)
	y_min = v[i].y;

      if (v[i].y > y_max)
	y_max = v[i].y;
    }

  box->x1 = x_min;
  box->y1 = y_min;
  box->x2 = x_max;
  box->y2 = y_max;
}

/**
 * shell_util_format_date:
 * @format: a strftime-style string format, as parsed by
 *   g_date_time_format()
 * @time_ms: milliseconds since 1970-01-01 00:00:00 UTC; the
 *   value returned by Date.getTime()
 *
 * Formats a date for the current locale. This should be
 * used instead of the Spidermonkey Date.toLocaleFormat()
 * extension because Date.toLocaleFormat() is buggy for
 * Unicode format strings:
 * https://bugzilla.mozilla.org/show_bug.cgi?id=508783
 *
 * Return value: the formatted date. If the date is
 *  outside of the range of a GDateTime (which contains
 *  any plausible dates we actually care about), will
 *  return an empty string.
 */
char *
shell_util_format_date (const char *format,
                        gint64      time_ms)
{
  GDateTime *datetime;
  GTimeVal tv;
  char *result;

  tv.tv_sec = time_ms / 1000;
  tv.tv_usec = (time_ms % 1000) * 1000;

  datetime = g_date_time_new_from_timeval_local (&tv);
  if (!datetime) /* time_ms is out of range of GDateTime */
    return g_strdup ("");

  result = g_date_time_format (datetime, format);

  g_date_time_unref (datetime);
  return result;
}
