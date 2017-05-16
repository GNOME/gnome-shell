/* EOS Shell directory information
 *
 * Copyright © 2013 Endless Mobile, Inc.
 *
 * Based on https://git.gnome.org/browse/glib/tree/gio/gappinfo.c
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
#include "shell-dir-info.h"

#include <gio/gio.h>


/**
 * SECTION:shelldirinfo
 * @short_description: Desktop directory information
 * @include: gio/gio.h
 *
 * #ShellDirInfo is used for describing directories on the desktop.
 *
 **/

typedef ShellDirInfoIface ShellDirInfoInterface;
G_DEFINE_INTERFACE (ShellDirInfo, shell_dir_info, G_TYPE_OBJECT)

static void
shell_dir_info_default_init (ShellDirInfoInterface *iface)
{
}


/**
 * shell_dir_info_dup:
 * @dirinfo: a #ShellDirInfo.
 *
 * Creates a duplicate of a #ShellDirInfo.
 *
 * Returns: (transfer full): a duplicate of @dirinfo.
 **/
ShellDirInfo *
shell_dir_info_dup (ShellDirInfo *dirinfo)
{
  ShellDirInfoIface *iface;

  g_return_val_if_fail (SHELL_IS_DIR_INFO (dirinfo), NULL);

  iface = SHELL_DIR_INFO_GET_IFACE (dirinfo);

  return (* iface->dup) (dirinfo);
}

/**
 * shell_dir_info_equal:
 * @dirinfo1: the first #ShellDirInfo.
 * @dirinfo2: the second #ShellDirInfo.
 *
 * Checks if two #ShellDirInfo<!-- -->s are equal.
 *
 * Returns: %TRUE if @dirinfo1 is equal to @dirinfo2. %FALSE otherwise.
 **/
gboolean
shell_dir_info_equal (ShellDirInfo *dirinfo1,
                      ShellDirInfo *dirinfo2)
{
  ShellDirInfoIface *iface;

  g_return_val_if_fail (SHELL_IS_DIR_INFO (dirinfo1), FALSE);
  g_return_val_if_fail (SHELL_IS_DIR_INFO (dirinfo2), FALSE);

  if (G_TYPE_FROM_INSTANCE (dirinfo1) != G_TYPE_FROM_INSTANCE (dirinfo2))
    return FALSE;

  iface = SHELL_DIR_INFO_GET_IFACE (dirinfo1);

  return (* iface->equal) (dirinfo1, dirinfo2);
}

/**
 * shell_dir_info_get_id:
 * @dirinfo: a #ShellDirInfo.
 *
 * Gets the ID of a directory. An id is a string that
 * identifies the directory. The exact format of the id is
 * platform dependent. For instance, on Unix this is the
 * desktop file id from the xdg menu specification.
 *
 * Note that the returned ID may be %NULL, depending on how
 * the @dirinfo has been constructed.
 *
 * Returns: a string containing the directory's ID.
 **/
const char *
shell_dir_info_get_id (ShellDirInfo *dirinfo)
{
  ShellDirInfoIface *iface;

  g_return_val_if_fail (SHELL_IS_DIR_INFO (dirinfo), NULL);

  iface = SHELL_DIR_INFO_GET_IFACE (dirinfo);

  return (* iface->get_id) (dirinfo);
}

/**
 * shell_dir_info_get_name:
 * @dirinfo: a #ShellDirInfo.
 *
 * Gets the name of the directory.
 *
 * Returns: the name of the directory for @dirinfo.
 **/
const char *
shell_dir_info_get_name (ShellDirInfo *dirinfo)
{
  ShellDirInfoIface *iface;

  g_return_val_if_fail (SHELL_IS_DIR_INFO (dirinfo), NULL);

  iface = SHELL_DIR_INFO_GET_IFACE (dirinfo);

  return (* iface->get_name) (dirinfo);
}

/**
 * shell_dir_info_get_display_name:
 * @dirinfo: a #ShellDirInfo.
 *
 * Gets the display name of the directory. The display name is often more
 * descriptive to the user than the name itself.
 *
 * Returns: the display name of the directory for @dirinfo, or the name if
 * no display name is available.
 **/
const char *
shell_dir_info_get_display_name (ShellDirInfo *dirinfo)
{
  ShellDirInfoIface *iface;

  g_return_val_if_fail (SHELL_IS_DIR_INFO (dirinfo), NULL);

  iface = SHELL_DIR_INFO_GET_IFACE (dirinfo);

  if (iface->get_display_name == NULL)
    return (* iface->get_name) (dirinfo);

  return (* iface->get_display_name) (dirinfo);
}

/**
 * shell_dir_info_get_description:
 * @dirinfo: a #ShellDirInfo.
 *
 * Gets a human-readable description of a directory.
 *
 * Returns: a string containing a description of the
 * directory @dirinfo, or %NULL if none.
 **/
const char *
shell_dir_info_get_description (ShellDirInfo *dirinfo)
{
  ShellDirInfoIface *iface;

  g_return_val_if_fail (SHELL_IS_DIR_INFO (dirinfo), NULL);

  iface = SHELL_DIR_INFO_GET_IFACE (dirinfo);

  return (* iface->get_description) (dirinfo);
}


/**
 * shell_dir_info_get_icon:
 * @dirinfo: a #ShellDirInfo.
 *
 * Gets the icon for the directory.
 *
 * Returns: (transfer none): the default #GIcon for @dirinfo or %NULL
 * if there is no default icon.
 **/
GIcon *
shell_dir_info_get_icon (ShellDirInfo *dirinfo)
{
  ShellDirInfoIface *iface;

  g_return_val_if_fail (SHELL_IS_DIR_INFO (dirinfo), NULL);

  iface = SHELL_DIR_INFO_GET_IFACE (dirinfo);

  return (* iface->get_icon) (dirinfo);
}


/**
 * shell_dir_info_should_show:
 * @dirinfo: a #ShellDirInfo.
 *
 * Checks if the directory info should be shown in menus that
 * list available directories.
 *
 * Returns: %TRUE if the @dirinfo should be shown, %FALSE otherwise.
 **/
gboolean
shell_dir_info_should_show (ShellDirInfo *dirinfo)
{
  ShellDirInfoIface *iface;

  g_return_val_if_fail (SHELL_IS_DIR_INFO (dirinfo), FALSE);

  iface = SHELL_DIR_INFO_GET_IFACE (dirinfo);

  return (* iface->should_show) (dirinfo);
}


/**
 * shell_dir_info_can_delete:
 * @dirinfo: a #ShellDirInfo
 *
 * Obtains the information whether the #ShellDirInfo can be deleted.
 * See shell_dir_info_delete().
 *
 * Returns: %TRUE if @dirinfo can be deleted
 */
gboolean
shell_dir_info_can_delete (ShellDirInfo *dirinfo)
{
  ShellDirInfoIface *iface;

  g_return_val_if_fail (SHELL_IS_DIR_INFO (dirinfo), FALSE);

  iface = SHELL_DIR_INFO_GET_IFACE (dirinfo);

  if (iface->can_delete)
    return (* iface->can_delete) (dirinfo);

  return FALSE;
}


/**
 * shell_dir_info_delete: (virtual do_delete)
 * @dirinfo: a #ShellDirInfo
 *
 * Tries to delete a #ShellDirInfo.
 *
 * On some platforms, there may be a difference between user-defined
 * #ShellDirInfo<!-- -->s which can be deleted, and system-wide ones which
 * cannot. See shell_dir_info_can_delete().
 *
 * Returns: %TRUE if @dirinfo has been deleted
 */
gboolean
shell_dir_info_delete (ShellDirInfo *dirinfo)
{
  ShellDirInfoIface *iface;

  g_return_val_if_fail (SHELL_IS_DIR_INFO (dirinfo), FALSE);

  iface = SHELL_DIR_INFO_GET_IFACE (dirinfo);

  if (iface->do_delete)
    return (* iface->do_delete) (dirinfo);

  return FALSE;
}
