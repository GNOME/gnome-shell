/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2004-2005 James M. Cape <jcape@ignore-your.tv>.
 * Copyright (C) 2007-2008 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Facade object for user data, owned by GdmUserManager
 */

#ifndef __GDM_USER__
#define __GDM_USER__ 1

#include <sys/types.h>
#include <gtk/gtkwidget.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define GDM_TYPE_USER (gdm_user_get_type ())
#define GDM_USER(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), GDM_TYPE_USER, GdmUser))
#define GDM_IS_USER(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDM_TYPE_USER))

typedef struct _GdmUser GdmUser;

GType                 gdm_user_get_type            (void) G_GNUC_CONST;

uid_t                 gdm_user_get_uid             (GdmUser   *user);
G_CONST_RETURN char  *gdm_user_get_user_name       (GdmUser   *user);
G_CONST_RETURN char  *gdm_user_get_real_name       (GdmUser   *user);
G_CONST_RETURN char  *gdm_user_get_home_directory  (GdmUser   *user);
G_CONST_RETURN char  *gdm_user_get_shell           (GdmUser   *user);
guint                 gdm_user_get_num_sessions    (GdmUser   *user);
GList                *gdm_user_get_sessions        (GdmUser   *user);
gulong                gdm_user_get_login_frequency (GdmUser   *user);

GdkPixbuf            *gdm_user_render_icon         (GdmUser   *user,
                                                    gint       icon_size);

gint                  gdm_user_collate             (GdmUser   *user1,
                                                    GdmUser   *user2);

G_END_DECLS

#endif
