/* EOS Shell directory information
 *
 * Copyright © 2013 Endless Mobile, Inc.
 *
 * Based on https://git.gnome.org/browse/glib/tree/gio/gappinfo.h
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

#ifndef __SHELL_DIR_INFO_H__
#define __SHELL_DIR_INFO_H__

#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _ShellDirInfo ShellDirInfo;

#define SHELL_TYPE_DIR_INFO            (shell_dir_info_get_type ())
#define SHELL_DIR_INFO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_TYPE_DIR_INFO, ShellDirInfo))
#define SHELL_IS_DIR_INFO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_TYPE_DIR_INFO))
#define SHELL_DIR_INFO_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), SHELL_TYPE_DIR_INFO, ShellDirInfoIface))


/**
 * ShellDirInfo:
 *
 * Information about a desktop directory.
 */

/**
 * ShellDirInfoIface:
 * @g_iface: The parent interface.
 * @dup: Copies a #ShellDirInfo.
 * @equal: Checks two #ShellDirInfo<!-- -->s for equality.
 * @get_id: Gets a string identifier for a #ShellDirInfo.
 * @get_name: Gets the name of the directory for a #ShellDirInfo.
 * @get_description: Gets a short description for the directory described by the #ShellDirInfo.
 * @get_icon: Gets the #GIcon for the #ShellDirInfo.
 * @should_show: Returns whether a directory should be shown (e.g. when getting a list of desktop directories).
 * @can_delete: Checks if a #ShellDirInfo can be deleted.
 * @do_delete: Deletes a #ShellDirInfo.
 * @get_display_name: Gets the display name for the #ShellDirInfo.
 *
 * Directory Information interface, for operating system portability.
 */
typedef struct _ShellDirInfoIface    ShellDirInfoIface;

struct _ShellDirInfoIface
{
  GTypeInterface g_iface;

  /* Virtual Table */

  ShellDirInfo *   (* dup)                      (ShellDirInfo   *dirinfo);
  gboolean     (* equal)                        (ShellDirInfo   *dirinfo1,
                                                 ShellDirInfo   *dirinfo2);
  const char * (* get_id)                       (ShellDirInfo   *dirinfo);
  const char * (* get_name)                     (ShellDirInfo   *dirinfo);
  const char * (* get_description)              (ShellDirInfo   *dirinfo);
  GIcon *      (* get_icon)                     (ShellDirInfo   *dirinfo);
  gboolean     (* should_show)                  (ShellDirInfo   *dirinfo);
  gboolean     (* can_delete)                   (ShellDirInfo   *dirinfo);
  gboolean     (* do_delete)                    (ShellDirInfo   *dirinfo);
  const char * (* get_display_name)             (ShellDirInfo   *dirinfo);
};

GType       shell_dir_info_get_type             (void) G_GNUC_CONST;

ShellDirInfo *  shell_dir_info_create_from_directory_name
                                                (const char     *directory_name,
                                                 GError        **error);

ShellDirInfo *  shell_dir_info_dup              (ShellDirInfo   *dirinfo);

gboolean    shell_dir_info_equal                (ShellDirInfo   *dirinfo1,
                                                 ShellDirInfo   *dirinfo2);

const char *shell_dir_info_get_id               (ShellDirInfo   *dirinfo);

const char *shell_dir_info_get_name             (ShellDirInfo   *dirinfo);

const char *shell_dir_info_get_display_name     (ShellDirInfo   *dirinfo);

const char *shell_dir_info_get_description      (ShellDirInfo   *dirinfo);

GIcon *     shell_dir_info_get_icon             (ShellDirInfo   *dirinfo);

gboolean    shell_dir_info_should_show          (ShellDirInfo   *dirinfo);

gboolean    shell_dir_info_can_delete           (ShellDirInfo   *dirinfo);

gboolean    shell_dir_info_delete               (ShellDirInfo   *dirinfo);

GList *     shell_dir_info_get_all              (void);

G_END_DECLS

#endif /* __SHELL_DIR_INFO_H__ */
