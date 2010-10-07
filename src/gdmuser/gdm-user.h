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

#ifndef __GDM_USER_H__
#define __GDM_USER_H__

#include <sys/types.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define GDM_TYPE_USER (gdm_user_get_type ())
#define GDM_USER(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), GDM_TYPE_USER, GdmUser))
#define GDM_IS_USER(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDM_TYPE_USER))

typedef struct _GdmUser GdmUser;
typedef struct _GdmUserClass GdmUserClass;

GType                 gdm_user_get_type            (void) G_GNUC_CONST;

GdmUser              *gdm_user_new_from_object_path (const char *path);
const char           *gdm_user_get_object_path      (GdmUser *user);

gulong                gdm_user_get_uid             (GdmUser   *user);
const char           *gdm_user_get_user_name       (GdmUser   *user);
const char           *gdm_user_get_real_name       (GdmUser   *user);
guint                 gdm_user_get_num_sessions    (GdmUser   *user);
gboolean              gdm_user_is_logged_in        (GdmUser   *user);
gulong                gdm_user_get_login_frequency (GdmUser   *user);
const char           *gdm_user_get_icon_file       (GdmUser   *user);
const char           *gdm_user_get_primary_session_id (GdmUser *user);

GdkPixbuf            *gdm_user_render_icon         (GdmUser   *user,
                                                    gint       icon_size);

gint                  gdm_user_collate             (GdmUser   *user1,
                                                    GdmUser   *user2);
gboolean              gdm_user_is_loaded           (GdmUser *user);

G_END_DECLS

#endif
