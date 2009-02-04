/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __GDM_USER_MANAGER_H
#define __GDM_USER_MANAGER_H

#include <glib-object.h>

#include "gdm-user.h"

G_BEGIN_DECLS

#define GDM_TYPE_USER_MANAGER         (gdm_user_manager_get_type ())
#define GDM_USER_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDM_TYPE_USER_MANAGER, GdmUserManager))
#define GDM_USER_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GDM_TYPE_USER_MANAGER, GdmUserManagerClass))
#define GDM_IS_USER_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDM_TYPE_USER_MANAGER))
#define GDM_IS_USER_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GDM_TYPE_USER_MANAGER))
#define GDM_USER_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDM_TYPE_USER_MANAGER, GdmUserManagerClass))

typedef struct GdmUserManagerPrivate GdmUserManagerPrivate;

typedef struct
{
        GObject                parent;
        GdmUserManagerPrivate *priv;
} GdmUserManager;

typedef struct
{
        GObjectClass   parent_class;

        void          (* loading_users)             (GdmUserManager *user_manager);
        void          (* users_loaded)              (GdmUserManager *user_manager);
        void          (* user_added)                (GdmUserManager *user_manager,
                                                     GdmUser        *user);
        void          (* user_removed)              (GdmUserManager *user_manager,
                                                     GdmUser        *user);
        void          (* user_is_logged_in_changed) (GdmUserManager *user_manager,
                                                     GdmUser        *user);
        void          (* user_login_frequency_changed) (GdmUserManager *user_manager,
                                                        GdmUser        *user);
} GdmUserManagerClass;

typedef enum
{
        GDM_USER_MANAGER_ERROR_GENERAL,
        GDM_USER_MANAGER_ERROR_KEY_NOT_FOUND
} GdmUserManagerError;

#define GDM_USER_MANAGER_ERROR gdm_user_manager_error_quark ()

GQuark              gdm_user_manager_error_quark           (void);
GType               gdm_user_manager_get_type              (void);

GdmUserManager *    gdm_user_manager_ref_default           (void);

GSList *            gdm_user_manager_list_users            (GdmUserManager *manager);
GdmUser *           gdm_user_manager_get_user              (GdmUserManager *manager,
                                                            const char     *user_name);
GdmUser *           gdm_user_manager_get_user_by_uid       (GdmUserManager *manager,
                                                            uid_t           uid);

gboolean            gdm_user_manager_activate_user_session (GdmUserManager *manager,
                                                            GdmUser        *user);

gboolean            gdm_user_manager_goto_login_session    (GdmUserManager *manager);

G_END_DECLS

#endif /* __GDM_USER_MANAGER_H */
