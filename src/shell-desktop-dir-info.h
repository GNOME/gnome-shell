/* EOS Shell desktop directory information
 *
 * Copyright © 2013 Endless Mobile, Inc.
 *
 * Based on https://git.gnome.org/browse/glib/tree/gio/gdesktopappinfo.h
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

#ifndef __SHELL_DESKTOP_DIR_INFO_H__
#define __SHELL_DESKTOP_DIR_INFO_H__

#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _ShellDesktopDirInfo        ShellDesktopDirInfo;
typedef struct _ShellDesktopDirInfoClass   ShellDesktopDirInfoClass;

#define SHELL_TYPE_DESKTOP_DIR_INFO         (shell_desktop_dir_info_get_type ())
#define SHELL_DESKTOP_DIR_INFO(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), SHELL_TYPE_DESKTOP_DIR_INFO, ShellDesktopDirInfo))
#define SHELL_DESKTOP_DIR_INFO_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), SHELL_TYPE_DESKTOP_DIR_INFO, ShellDesktopDirInfoClass))
#define SHELL_IS_DESKTOP_DIR_INFO(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), SHELL_TYPE_DESKTOP_DIR_INFO))
#define SHELL_IS_DESKTOP_DIR_INFO_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), SHELL_TYPE_DESKTOP_DIR_INFO))
#define SHELL_DESKTOP_DIR_INFO_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), SHELL_TYPE_DESKTOP_DIR_INFO, ShellDesktopDirInfoClass))

struct _ShellDesktopDirInfoClass
{
  GObjectClass parent_class;
};


GType            shell_desktop_dir_info_get_type          (void) G_GNUC_CONST;

ShellDesktopDirInfo *shell_desktop_dir_info_new_from_filename (const char      *filename);

ShellDesktopDirInfo *shell_desktop_dir_info_new_from_keyfile  (GKeyFile        *key_file);

const char *     shell_desktop_dir_info_get_filename      (ShellDesktopDirInfo *info);

const char *     shell_desktop_dir_info_get_generic_name  (ShellDesktopDirInfo *info);

gboolean         shell_desktop_dir_info_get_nodisplay     (ShellDesktopDirInfo *info);

gboolean         shell_desktop_dir_info_get_show_in       (ShellDesktopDirInfo *info,
                                                           const gchar     *desktop_env);

ShellDesktopDirInfo *shell_desktop_dir_info_new           (const char      *desktop_id);

gboolean         shell_desktop_dir_info_get_is_hidden     (ShellDesktopDirInfo *info);

void             shell_desktop_dir_info_set_desktop_env   (const char      *desktop_env);

gboolean         shell_desktop_dir_info_has_key           (ShellDesktopDirInfo *info,
                                                           const char      *key);

char *           shell_desktop_dir_info_get_string        (ShellDesktopDirInfo *info,
                                                           const char      *key);

gboolean         shell_desktop_dir_info_get_boolean       (ShellDesktopDirInfo *info,
                                                           const char      *key);

gboolean         shell_desktop_dir_info_create_custom_with_name (ShellDesktopDirInfo *info,
                                                                 const char          *name,
                                                                 GError             **error);

G_END_DECLS

#endif /* __SHELL_DESKTOP_DIR_INFO_H__ */
