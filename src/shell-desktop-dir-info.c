/* EOS Shell desktop directory information
 *
 * Copyright © 2013 Endless Mobile, Inc.
 *
 * Based on https://git.gnome.org/browse/glib/tree/gio/gdesktopappinfo.c
 * Copyright (C) 2006-2007 Red Hat, Inc.
 * Copyright © 2007 Ryan Lortie
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_CRT_EXTERNS_H
#include <crt_externs.h>
#endif

#include "shell-desktop-dir-info.h"
#include "shell-dir-info.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

/**
 * SECTION:shelldesktopdirinfo
 * @title: ShellDesktopDirInfo
 * @short_description: Directory information from desktop files
 * @include: shell-desktop-dir-info.h
 *
 * #ShellDesktopDirInfo is an implementation of #ShellDirInfo based on
 * desktop files.
 */

#define GENERIC_NAME_KEY            "GenericName"
#define FULL_NAME_KEY               "X-GNOME-FullName"

enum {
  PROP_0,
  PROP_FILENAME
};

static void     shell_desktop_dir_info_iface_init         (ShellDirInfoIface    *iface);

/**
 * ShellDesktopDirInfo:
 *
 * Information about a desktop directory from a desktop file.
 */
struct _ShellDesktopDirInfo
{
  GObject parent_instance;

  char *desktop_id;
  char *filename;

  GKeyFile *keyfile;

  char *name;
  char *generic_name;
  char *fullname;
  char *comment;
  char *icon_name;
  GIcon *icon;
  char **only_show_in;
  char **not_show_in;

  guint nodisplay       : 1;
  guint hidden          : 1;
};

G_DEFINE_TYPE_WITH_CODE (ShellDesktopDirInfo, shell_desktop_dir_info, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (SHELL_TYPE_DIR_INFO,
                                                shell_desktop_dir_info_iface_init))

G_LOCK_DEFINE_STATIC (shell_desktop_env);
static gchar *shell_desktop_env = NULL;

static gpointer
search_path_init (gpointer data)
{
  char **args = NULL;
  const char * const *data_dirs;
  const char *user_data_dir;
  int i, length, j;

  data_dirs = g_get_system_data_dirs ();
  length = g_strv_length ((char **) data_dirs);

  args = g_new (char *, length + 2);

  j = 0;
  user_data_dir = g_get_user_data_dir ();
  args[j++] = g_build_filename (user_data_dir, "desktop-directories", NULL);
  for (i = 0; i < length; i++)
    args[j++] = g_build_filename (data_dirs[i],
                                  "desktop-directories", NULL);
  args[j++] = NULL;

  return args;
}

static const char * const *
get_directories_search_path (void)
{
  static GOnce once_init = G_ONCE_INIT;
  return g_once (&once_init, search_path_init, NULL);
}

static void
shell_desktop_dir_info_finalize (GObject *object)
{
  ShellDesktopDirInfo *info;

  info = SHELL_DESKTOP_DIR_INFO (object);

  g_free (info->desktop_id);
  g_free (info->filename);

  if (info->keyfile)
    g_key_file_unref (info->keyfile);

  g_free (info->name);
  g_free (info->generic_name);
  g_free (info->fullname);
  g_free (info->comment);
  g_free (info->icon_name);
  if (info->icon)
    g_object_unref (info->icon);
  g_strfreev (info->only_show_in);
  g_strfreev (info->not_show_in);

  G_OBJECT_CLASS (shell_desktop_dir_info_parent_class)->finalize (object);
}

static void
shell_desktop_dir_info_set_property(GObject         *object,
                                    guint            prop_id,
                                    const GValue    *value,
                                    GParamSpec      *pspec)
{
  ShellDesktopDirInfo *self = SHELL_DESKTOP_DIR_INFO (object);

  switch (prop_id)
    {
    case PROP_FILENAME:
      self->filename = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shell_desktop_dir_info_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ShellDesktopDirInfo *self = SHELL_DESKTOP_DIR_INFO (object);

  switch (prop_id)
    {
    case PROP_FILENAME:
      g_value_set_string (value, self->filename);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
shell_desktop_dir_info_class_init (ShellDesktopDirInfoClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = shell_desktop_dir_info_get_property;
  gobject_class->set_property = shell_desktop_dir_info_set_property;
  gobject_class->finalize = shell_desktop_dir_info_finalize;

  /**
   * ShellDesktopDirInfo:filename:
   *
   * The origin filename of this #ShellDesktopDirInfo
   */
  g_object_class_install_property (gobject_class,
                                   PROP_FILENAME,
                                   g_param_spec_string ("filename", "Filename", "",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
shell_desktop_dir_info_init (ShellDesktopDirInfo *local)
{
}

static gboolean
shell_desktop_dir_info_load_from_keyfile (ShellDesktopDirInfo *info,
                                          GKeyFile        *key_file)
{
  char *start_group;
  char *type;

  start_group = g_key_file_get_start_group (key_file);
  if (start_group == NULL || strcmp (start_group, G_KEY_FILE_DESKTOP_GROUP) != 0)
    {
      g_free (start_group);
      return FALSE;
    }
  g_free (start_group);

  type = g_key_file_get_string (key_file,
                                G_KEY_FILE_DESKTOP_GROUP,
                                G_KEY_FILE_DESKTOP_KEY_TYPE,
                                NULL);
  if (type == NULL || strcmp (type, G_KEY_FILE_DESKTOP_TYPE_DIRECTORY) != 0)
    {
      g_free (type);
      return FALSE;
    }
  g_free (type);

  info->name = g_key_file_get_locale_string (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NAME, NULL, NULL);
  info->generic_name = g_key_file_get_locale_string (key_file, G_KEY_FILE_DESKTOP_GROUP, GENERIC_NAME_KEY, NULL, NULL);
  info->fullname = g_key_file_get_locale_string (key_file, G_KEY_FILE_DESKTOP_GROUP, FULL_NAME_KEY, NULL, NULL);
  info->comment = g_key_file_get_locale_string (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_COMMENT, NULL, NULL);
  info->nodisplay = g_key_file_get_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY, NULL) != FALSE;
  info->icon_name =  g_key_file_get_locale_string (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ICON, NULL, NULL);
  info->only_show_in = g_key_file_get_string_list (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN, NULL, NULL);
  info->not_show_in = g_key_file_get_string_list (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NOT_SHOW_IN, NULL, NULL);
  info->hidden = g_key_file_get_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_HIDDEN, NULL) != FALSE;

  info->icon = NULL;
  if (info->icon_name)
    {
      if (g_path_is_absolute (info->icon_name))
        {
          GFile *file;

          file = g_file_new_for_path (info->icon_name);
          info->icon = g_file_icon_new (file);
          g_object_unref (file);
        }
      else
        {
          char *p;

          /* Work around a common mistake in desktop files */
          if ((p = strrchr (info->icon_name, '.')) != NULL &&
              (strcmp (p, ".png") == 0 ||
               strcmp (p, ".xpm") == 0 ||
               strcmp (p, ".svg") == 0))
            *p = 0;

          info->icon = g_themed_icon_new (info->icon_name);
        }
    }

  info->keyfile = g_key_file_ref (key_file);

  return TRUE;
}

static gboolean
shell_desktop_dir_info_load_file (ShellDesktopDirInfo *self)
{
  GKeyFile *key_file;
  gboolean retval = FALSE;

  g_return_val_if_fail (self->filename != NULL, FALSE);

  self->desktop_id = g_path_get_basename (self->filename);

  key_file = g_key_file_new ();

  if (g_key_file_load_from_file (key_file,
                                 self->filename,
                                 G_KEY_FILE_NONE,
                                 NULL))
    {
      retval = shell_desktop_dir_info_load_from_keyfile (self, key_file);
    }

  g_key_file_unref (key_file);
  return retval;
}

/**
 * shell_desktop_dir_info_new_from_keyfile:
 * @key_file: an opened #GKeyFile
 *
 * Creates a new #ShellDesktopDirInfo.
 *
 * Returns: a new #ShellDesktopDirInfo or %NULL on error.
 **/
ShellDesktopDirInfo *
shell_desktop_dir_info_new_from_keyfile (GKeyFile *key_file)
{
  ShellDesktopDirInfo *info;

  info = g_object_new (SHELL_TYPE_DESKTOP_DIR_INFO, NULL);
  info->filename = NULL;
  if (!shell_desktop_dir_info_load_from_keyfile (info, key_file))
    {
      g_object_unref (info);
      return NULL;
    }
  return info;
}

/**
 * shell_desktop_dir_info_new_from_filename:
 * @filename: the path of a desktop file, in the GLib filename encoding
 *
 * Creates a new #ShellDesktopDirInfo.
 *
 * Returns: a new #ShellDesktopDirInfo or %NULL on error.
 **/
ShellDesktopDirInfo *
shell_desktop_dir_info_new_from_filename (const char *filename)
{
  ShellDesktopDirInfo *info = NULL;

  info = g_object_new (SHELL_TYPE_DESKTOP_DIR_INFO, "filename", filename, NULL);
  if (!shell_desktop_dir_info_load_file (info))
    {
      g_object_unref (info);
      return NULL;
    }
  return info;
}

/**
 * shell_desktop_dir_info_new:
 * @desktop_id: the desktop file id
 *
 * Creates a new #ShellDesktopDirInfo based on a desktop file id.
 *
 * A desktop file id is the basename of the desktop file, including the
 * .directory extension. GIO is looking for a desktop file with this name
 * in the <filename>desktop-directories</filename> subdirectories of the XDG data
 * directories (i.e. the directories specified in the
 * <envar>XDG_DATA_HOME</envar> and <envar>XDG_DATA_DIRS</envar> environment
 * variables). GIO also supports the prefix-to-subdirectory mapping that is
 * described in the <ulink url="http://standards.freedesktop.org/menu-spec/latest/">Menu Spec</ulink>
 * (i.e. a desktop id of kde-foo.directory will match
 * <filename>/usr/share/desktop-directories/kde/foo.directory</filename>).
 *
 * Returns: a new #ShellDesktopDirInfo, or %NULL if no desktop file with that id
 */
ShellDesktopDirInfo *
shell_desktop_dir_info_new (const char *desktop_id)
{
  ShellDesktopDirInfo *dirinfo;
  const char * const *dirs;
  char *basename;
  int i;

  dirs = get_directories_search_path ();

  basename = g_strdup (desktop_id);

  for (i = 0; dirs[i] != NULL; i++)
    {
      char *filename;
      char *p;

      filename = g_build_filename (dirs[i], desktop_id, NULL);
      dirinfo = shell_desktop_dir_info_new_from_filename (filename);
      g_free (filename);
      if (dirinfo != NULL)
        goto found;

      p = basename;
      while ((p = strchr (p, '-')) != NULL)
        {
          *p = '/';

          filename = g_build_filename (dirs[i], basename, NULL);
          dirinfo = shell_desktop_dir_info_new_from_filename (filename);
          g_free (filename);
          if (dirinfo != NULL)
            goto found;
          *p = '-';
          p++;
        }
    }

  g_free (basename);
  return NULL;

 found:
  g_free (basename);

  g_free (dirinfo->desktop_id);
  dirinfo->desktop_id = g_strdup (desktop_id);

  if (shell_desktop_dir_info_get_is_hidden (dirinfo))
    {
      g_object_unref (dirinfo);
      dirinfo = NULL;
    }

  return dirinfo;
}

static ShellDirInfo *
shell_desktop_dir_info_dup (ShellDirInfo *dirinfo)
{
  ShellDesktopDirInfo *info = SHELL_DESKTOP_DIR_INFO (dirinfo);
  ShellDesktopDirInfo *new_info;

  new_info = g_object_new (SHELL_TYPE_DESKTOP_DIR_INFO, NULL);

  new_info->filename = g_strdup (info->filename);
  new_info->desktop_id = g_strdup (info->desktop_id);

  if (info->keyfile)
    new_info->keyfile = g_key_file_ref (info->keyfile);

  new_info->name = g_strdup (info->name);
  new_info->generic_name = g_strdup (info->generic_name);
  new_info->fullname = g_strdup (info->fullname);
  new_info->comment = g_strdup (info->comment);
  new_info->nodisplay = info->nodisplay;
  new_info->icon_name = g_strdup (info->icon_name);
  if (info->icon)
    new_info->icon = g_object_ref (info->icon);
  new_info->only_show_in = g_strdupv (info->only_show_in);
  new_info->not_show_in = g_strdupv (info->not_show_in);
  new_info->hidden = info->hidden;

  return SHELL_DIR_INFO (new_info);
}

static gboolean
shell_desktop_dir_info_equal (ShellDirInfo *dirinfo1,
                              ShellDirInfo *dirinfo2)
{
  ShellDesktopDirInfo *info1 = SHELL_DESKTOP_DIR_INFO (dirinfo1);
  ShellDesktopDirInfo *info2 = SHELL_DESKTOP_DIR_INFO (dirinfo2);

  if (info1->desktop_id == NULL ||
      info2->desktop_id == NULL)
    return info1 == info2;

  return strcmp (info1->desktop_id, info2->desktop_id) == 0;
}

static const char *
shell_desktop_dir_info_get_id (ShellDirInfo *dirinfo)
{
  ShellDesktopDirInfo *info = SHELL_DESKTOP_DIR_INFO (dirinfo);

  return info->desktop_id;
}

static const char *
shell_desktop_dir_info_get_name (ShellDirInfo *dirinfo)
{
  ShellDesktopDirInfo *info = SHELL_DESKTOP_DIR_INFO (dirinfo);

  if (info->name == NULL)
    return _("Unnamed");
  return info->name;
}

static const char *
shell_desktop_dir_info_get_display_name (ShellDirInfo *dirinfo)
{
  ShellDesktopDirInfo *info = SHELL_DESKTOP_DIR_INFO (dirinfo);

  if (info->fullname == NULL)
    return shell_desktop_dir_info_get_name (dirinfo);
  return info->fullname;
}

/**
 * shell_desktop_dir_info_get_is_hidden:
 * @info: a #ShellDesktopDirInfo.
 *
 * A desktop file is hidden if the Hidden key in it is
 * set to True.
 *
 * Returns: %TRUE if hidden, %FALSE otherwise.
 **/
gboolean
shell_desktop_dir_info_get_is_hidden (ShellDesktopDirInfo *info)
{
  return info->hidden;
}

/**
 * shell_desktop_dir_info_get_filename:
 * @info: a #ShellDesktopDirInfo
 *
 * When @info was created from a known filename, return it.  In some
 * situations such as the #ShellDesktopDirInfo returned from
 * shell_desktop_dir_info_new_from_keyfile(), this function will return %NULL.
 *
 * Returns: The full path to the file for @info, or %NULL if not known.
 */
const char *
shell_desktop_dir_info_get_filename (ShellDesktopDirInfo *info)
{
  return info->filename;
}

static const char *
shell_desktop_dir_info_get_description (ShellDirInfo *dirinfo)
{
  ShellDesktopDirInfo *info = SHELL_DESKTOP_DIR_INFO (dirinfo);

  return info->comment;
}

static GIcon *
shell_desktop_dir_info_get_icon (ShellDirInfo *dirinfo)
{
  ShellDesktopDirInfo *info = SHELL_DESKTOP_DIR_INFO (dirinfo);

  return info->icon;
}

/**
 * shell_desktop_dir_info_get_generic_name:
 * @info: a #ShellDesktopDirInfo
 *
 * Gets the generic name from the destkop file.
 *
 * Returns: The value of the GenericName key
 */
const char *
shell_desktop_dir_info_get_generic_name (ShellDesktopDirInfo *info)
{
  return info->generic_name;
}

/**
 * shell_desktop_dir_info_get_nodisplay:
 * @info: a #ShellDesktopDirInfo
 *
 * Gets the value of the NoDisplay key, which helps determine if the
 * directory info should be shown in menus. See
 * #G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY and shell_dir_info_should_show().
 *
 * Returns: The value of the NoDisplay key
 */
gboolean
shell_desktop_dir_info_get_nodisplay (ShellDesktopDirInfo *info)
{
  return info->nodisplay;
}

/**
 * shell_desktop_dir_info_get_show_in:
 * @info: a #ShellDesktopDirInfo
 * @desktop_env: a string specifying a desktop name
 *
 * Checks if the directory info should be shown in menus that list available
 * directories for a specific name of the desktop, based on the
 * <literal>OnlyShowIn</literal> and <literal>NotShowIn</literal> keys.
 *
 * If @desktop_env is %NULL, then the name of the desktop set with
 * shell_desktop_dir_info_set_desktop_env() is used.
 *
 * Note that shell_dir_info_should_show() for @info will include this check (with
 * %NULL for @desktop_env) as well as additional checks.
 *
 * Returns: %TRUE if the @info should be shown in @desktop_env according to the
 * <literal>OnlyShowIn</literal> and <literal>NotShowIn</literal> keys, %FALSE
 * otherwise.
 */
gboolean
shell_desktop_dir_info_get_show_in (ShellDesktopDirInfo *info,
                                    const gchar         *desktop_env)
{
  gboolean found;
  int i;

  g_return_val_if_fail (SHELL_IS_DESKTOP_DIR_INFO (info), FALSE);

  if (!desktop_env  ) {
    G_LOCK (shell_desktop_env);
    desktop_env = shell_desktop_env;
    G_UNLOCK (shell_desktop_env);
  }

  if (info->only_show_in)
    {
      if (desktop_env == NULL)
        return FALSE;

      found = FALSE;
      for (i = 0; info->only_show_in[i] != NULL; i++)
        {
          if (strcmp (info->only_show_in[i], desktop_env) == 0)
            {
              found = TRUE;
              break;
            }
        }
      if (!found)
        return FALSE;
    }

  if (info->not_show_in && desktop_env)
    {
      for (i = 0; info->not_show_in[i] != NULL; i++)
        {
          if (strcmp (info->not_show_in[i], desktop_env) == 0)
            return FALSE;
        }
    }

  return TRUE;
}

/**
 * shell_desktop_dir_info_set_desktop_env:
 * @desktop_env: a string specifying what desktop this is
 *
 * Sets the name of the desktop that the application is running in.
 * This is used by shell_dir_info_should_show() and
 * shell_desktop_dir_info_get_show_in() to evaluate the
 * <literal>OnlyShowIn</literal> and <literal>NotShowIn</literal>
 * desktop entry fields.
 *
 * The <ulink url="http://standards.freedesktop.org/menu-spec/latest/">Desktop
 * Menu specification</ulink> recognizes the following:
 * <simplelist>
 *   <member>GNOME</member>
 *   <member>KDE</member>
 *   <member>ROX</member>
 *   <member>XFCE</member>
 *   <member>LXDE</member>
 *   <member>Unity</member>
 *   <member>Old</member>
 * </simplelist>
 *
 * Should be called only once; subsequent calls are ignored.
 */
void
shell_desktop_dir_info_set_desktop_env (const gchar *desktop_env)
{
  G_LOCK (shell_desktop_env);
  if (!shell_desktop_env)
    shell_desktop_env = g_strdup (desktop_env);
  G_UNLOCK (shell_desktop_env);
}

static gboolean
shell_desktop_dir_info_should_show (ShellDirInfo *dirinfo)
{
  ShellDesktopDirInfo *info = SHELL_DESKTOP_DIR_INFO (dirinfo);

  if (info->nodisplay)
    return FALSE;

  return shell_desktop_dir_info_get_show_in (info, NULL);
}

static gboolean
shell_desktop_dir_info_can_delete (ShellDirInfo *dirinfo)
{
  ShellDesktopDirInfo *info = SHELL_DESKTOP_DIR_INFO (dirinfo);

  if (info->filename)
    {
      if (strstr (info->filename, "/userdir-"))
        return g_access (info->filename, W_OK) == 0;
    }

  return FALSE;
}

static gboolean
shell_desktop_dir_info_delete (ShellDirInfo *dirinfo)
{
  ShellDesktopDirInfo *info = SHELL_DESKTOP_DIR_INFO (dirinfo);

  if (info->filename)
    {
      if (g_remove (info->filename) == 0)
        {
          g_free (info->filename);
          info->filename = NULL;
          g_free (info->desktop_id);
          info->desktop_id = NULL;

          return TRUE;
        }
    }

  return FALSE;
}

/**
 * shell_dir_info_create_from_directory_name:
 * @directory_name: the directory name
 * @error: a #GError location to store the error occurring, %NULL to ignore.
 *
 * Creates a new #ShellDirInfo from the given information.
 *
 * Returns: (transfer full): new #ShellDirInfo for given directory name.
 **/
ShellDirInfo *
shell_dir_info_create_from_directory_name (const char           *directory_name,
                                           GError              **error)
{
  ShellDesktopDirInfo *info;

  g_return_val_if_fail (directory_name, NULL);

  info = g_object_new (SHELL_TYPE_DESKTOP_DIR_INFO, NULL);

  info->filename = NULL;
  info->desktop_id = NULL;

  info->hidden = FALSE;
  info->nodisplay = TRUE;

  info->name = g_strdup (directory_name);
  info->comment = g_strdup_printf (_("Custom definition for %s"), info->name);

  return SHELL_DIR_INFO (info);
}

static void
shell_desktop_dir_info_iface_init (ShellDirInfoIface *iface)
{
  iface->dup = shell_desktop_dir_info_dup;
  iface->equal = shell_desktop_dir_info_equal;
  iface->get_id = shell_desktop_dir_info_get_id;
  iface->get_name = shell_desktop_dir_info_get_name;
  iface->get_description = shell_desktop_dir_info_get_description;
  iface->get_icon = shell_desktop_dir_info_get_icon;
  iface->should_show = shell_desktop_dir_info_should_show;
  iface->can_delete = shell_desktop_dir_info_can_delete;
  iface->do_delete = shell_desktop_dir_info_delete;
  iface->get_display_name = shell_desktop_dir_info_get_display_name;
}

static void
get_entries_from_dir (GHashTable *entries,
                      const char *dirname,
                      const char *prefix)
{
  GDir *dir;
  const char *basename;
  char *filename, *subprefix, *desktop_id;
  gboolean hidden;
  ShellDesktopDirInfo *dirinfo;

  dir = g_dir_open (dirname, 0, NULL);
  if (dir)
    {
      while ((basename = g_dir_read_name (dir)) != NULL)
        {
          filename = g_build_filename (dirname, basename, NULL);
          if (g_str_has_suffix (basename, ".directory"))
            {
              desktop_id = g_strconcat (prefix, basename, NULL);

              /* Use _extended so we catch NULLs too (hidden) */
              if (!g_hash_table_lookup_extended (entries, desktop_id, NULL, NULL))
                {
                  dirinfo = shell_desktop_dir_info_new_from_filename (filename);
                  hidden = FALSE;

                  if (dirinfo && shell_desktop_dir_info_get_is_hidden (dirinfo))
                    {
                      g_object_unref (dirinfo);
                      dirinfo = NULL;
                      hidden = TRUE;
                    }

                  if (dirinfo || hidden)
                    {
                      g_hash_table_insert (entries, g_strdup (desktop_id), dirinfo);

                      if (dirinfo)
                        {
                          /* Reuse instead of strdup here */
                          dirinfo->desktop_id = desktop_id;
                          desktop_id = NULL;
                        }
                    }
                }
              g_free (desktop_id);
            }
          else
            {
              if (g_file_test (filename, G_FILE_TEST_IS_DIR))
                {
                  subprefix = g_strconcat (prefix, basename, "-", NULL);
                  get_entries_from_dir (entries, filename, subprefix);
                  g_free (subprefix);
                }
            }
          g_free (filename);
        }
      g_dir_close (dir);
    }
}


/**
 * shell_dir_info_get_all:
 *
 * Gets a list of all of the desktop directories currently registered
 * on this system.
 *
 * For desktop files, this includes directories that have
 * <literal>NoDisplay=true</literal> set or are excluded from
 * display by means of <literal>OnlyShowIn</literal> or
 * <literal>NotShowIn</literal>. See shell_dir_info_should_show().
 * The returned list does not include directories which have
 * the <literal>Hidden</literal> key set.
 *
 * Returns: (element-type ShellDirInfo) (transfer full): a newly allocated #GList of references to #ShellDirInfo<!---->s.
 **/
GList *
shell_dir_info_get_all (void)
{
  const char * const *dirs;
  GHashTable *entries;
  GHashTableIter iter;
  gpointer value;
  int i;
  GList *infos;

  dirs = get_directories_search_path ();

  entries = g_hash_table_new_full (g_str_hash, g_str_equal,
                                   g_free, NULL);


  for (i = 0; dirs[i] != NULL; i++)
    get_entries_from_dir (entries, dirs[i], "");


  infos = NULL;
  g_hash_table_iter_init (&iter, entries);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      if (value)
        infos = g_list_prepend (infos, value);
    }

  g_hash_table_destroy (entries);

  return g_list_reverse (infos);
}

/**
 * shell_desktop_dir_info_get_string:
 * @info: a #ShellDesktopDirInfo
 * @key: the key to look up
 *
 * Looks up a string value in the keyfile backing @info.
 *
 * The @key is looked up in the "Desktop Entry" group.
 *
 * Returns: a newly allocated string, or %NULL if the key
 *     is not found
 */
char *
shell_desktop_dir_info_get_string (ShellDesktopDirInfo *info,
                                   const char      *key)
{
  g_return_val_if_fail (SHELL_IS_DESKTOP_DIR_INFO (info), NULL);

  return g_key_file_get_string (info->keyfile,
                                G_KEY_FILE_DESKTOP_GROUP, key, NULL);
}

/**
 * shell_desktop_dir_info_get_boolean:
 * @info: a #ShellDesktopDirInfo
 * @key: the key to look up
 *
 * Looks up a boolean value in the keyfile backing @info.
 *
 * The @key is looked up in the "Desktop Entry" group.
 *
 * Returns: the boolean value, or %FALSE if the key
 *     is not found
 */
gboolean
shell_desktop_dir_info_get_boolean (ShellDesktopDirInfo *info,
                                    const char      *key)
{
  g_return_val_if_fail (SHELL_IS_DESKTOP_DIR_INFO (info), FALSE);

  return g_key_file_get_boolean (info->keyfile,
                                 G_KEY_FILE_DESKTOP_GROUP, key, NULL);
}

/**
 * shell_desktop_dir_info_has_key:
 * @info: a #ShellDesktopDirInfo
 * @key: the key to look up
 *
 * Returns whether @key exists in the "Desktop Entry" group
 * of the keyfile backing @info.
 *
 * Returns: %TRUE if the @key exists
 */
gboolean
shell_desktop_dir_info_has_key (ShellDesktopDirInfo *info,
                                const char      *key)
{
  g_return_val_if_fail (SHELL_IS_DESKTOP_DIR_INFO (info), FALSE);

  return g_key_file_has_key (info->keyfile,
                             G_KEY_FILE_DESKTOP_GROUP, key, NULL);
}

gboolean
shell_desktop_dir_info_create_custom_with_name (ShellDesktopDirInfo *info,
                                                const char          *name,
                                                GError             **error)
{
  GError *internal_error = NULL;
  char *user_path, *buf;
  gsize len;
  char **keys;
  gsize i;

  g_free (info->name);
  info->name = g_strdup (name);

  /* no keyfile, we just store it in the DirInfo */
  if (info->keyfile == NULL)
    return TRUE;

  /* remove all translated 'Name' keys */
  keys = g_key_file_get_keys (info->keyfile,
                              G_KEY_FILE_DESKTOP_GROUP,
                              &len,
                              NULL);
  for (i = 0; i < len; i++)
    {
      if (strncmp (keys[i], G_KEY_FILE_DESKTOP_KEY_NAME,
                   strlen(G_KEY_FILE_DESKTOP_KEY_NAME)) == 0)
        {
          g_key_file_remove_key (info->keyfile,
                                 G_KEY_FILE_DESKTOP_GROUP,
                                 keys[i],
                                 NULL);
        }
    }

  /* create a new 'Name' key with the new name */
  g_key_file_set_string (info->keyfile,
                         G_KEY_FILE_DESKTOP_GROUP,
                         G_KEY_FILE_DESKTOP_KEY_NAME,
                         name);

  buf = g_key_file_to_data (info->keyfile, &len, &internal_error);
  if (internal_error != NULL)
    {
      g_propagate_error (error, internal_error);
      return FALSE;
    }

  user_path = g_build_filename (g_get_user_data_dir (),
                                "desktop-directories",
                                NULL);

  if (g_mkdir_with_parents (user_path, 0755) < 0)
    {
      int saved_errno = errno;

      g_set_error (error, G_FILE_ERROR,
                   G_FILE_ERROR_FAILED,
                   "Unable to create '%s': %s",
                   user_path,
                   g_strerror (saved_errno));

      g_free (user_path);
      g_free (buf);

      return FALSE;
    }

  g_free (user_path);

  /* store the keyfile in the user's data directory; it will take
   * precedence over the system one when we reload the list of
   * folders
   */
  user_path = g_build_filename (g_get_user_data_dir (),
                                "desktop-directories",
                                info->desktop_id,
                                NULL);

  g_file_set_contents (user_path, buf, len, &internal_error);
  if (internal_error != NULL)
    {
      g_propagate_error (error, internal_error);
      g_free (user_path);
      g_free (buf);

      return FALSE;
    }

  g_free (user_path);
  g_free (buf);

  return TRUE;
}
