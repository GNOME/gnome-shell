/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef HAVE_PATHS_H
#include <paths.h>
#endif /* HAVE_PATHS_H */

#include <glib.h>
/* Note on sync with gdm; need to use -lib here */
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "gdm-user-manager.h"
#include "gdm-user-private.h"

#define GDM_USER_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GDM_TYPE_USER_MANAGER, GdmUserManagerPrivate))

#define CK_NAME      "org.freedesktop.ConsoleKit"
#define CK_PATH      "/org/freedesktop/ConsoleKit"
#define CK_INTERFACE "org.freedesktop.ConsoleKit"

#define CK_MANAGER_PATH      "/org/freedesktop/ConsoleKit/Manager"
#define CK_MANAGER_INTERFACE "org.freedesktop.ConsoleKit.Manager"
#define CK_SEAT_INTERFACE    "org.freedesktop.ConsoleKit.Seat"
#define CK_SESSION_INTERFACE "org.freedesktop.ConsoleKit.Session"

/* Prefs Defaults */
#define DEFAULT_ALLOW_ROOT      TRUE
#define DEFAULT_MAX_ICON_SIZE   128
#define DEFAULT_USER_MAX_FILE   65536

#ifdef __sun
#define DEFAULT_MINIMAL_UID     100
#else
#define DEFAULT_MINIMAL_UID     500
#endif

#ifndef _PATH_SHELLS
#define _PATH_SHELLS    "/etc/shells"
#endif
#define PATH_PASSWD     "/etc/passwd"

#define DEFAULT_GLOBAL_FACE_DIR DATADIR "/faces"
#define DEFAULT_USER_ICON       "stock_person"
#define DEFAULT_EXCLUDE         { "bin",        \
                                  "root",       \
                                  "daemon",     \
                                  "adm",        \
                                  "lp",         \
                                  "sync",       \
                                  "shutdown",   \
                                  "halt",       \
                                  "mail",       \
                                  "news",       \
                                  "uucp",       \
                                  "operator",   \
                                  "nobody",     \
                                  "gdm",        \
                                  "postgres",   \
                                  "pvm",        \
                                  "rpm",        \
                                  "nfsnobody",  \
                                  "pcap",       \
                                  NULL }

struct GdmUserManagerPrivate
{
        GHashTable            *users;
        GHashTable            *sessions;
        GHashTable            *exclusions;
        GHashTable            *shells;
        DBusGConnection       *connection;
        DBusGProxy            *seat_proxy;
        char                  *seat_id;

        GFileMonitor          *passwd_monitor;
        GFileMonitor          *shells_monitor;

        guint                  reload_id;
        guint                  ck_history_id;

        guint8                 users_dirty : 1;
};

enum {
        LOADING_USERS,
        USERS_LOADED,
        USER_ADDED,
        USER_REMOVED,
        USER_IS_LOGGED_IN_CHANGED,
        USER_LOGIN_FREQUENCY_CHANGED,
        LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };

static void     gdm_user_manager_class_init (GdmUserManagerClass *klass);
static void     gdm_user_manager_init       (GdmUserManager      *user_manager);
static void     gdm_user_manager_finalize   (GObject             *object);

static gpointer user_manager_object = NULL;

G_DEFINE_TYPE (GdmUserManager, gdm_user_manager, G_TYPE_OBJECT)

GQuark
gdm_user_manager_error_quark (void)
{
        static GQuark ret = 0;
        if (ret == 0) {
                ret = g_quark_from_static_string ("gdm_user_manager_error");
        }

        return ret;
}

static gboolean
start_new_login_session (GdmUserManager *manager)
{
        GError  *error;
        gboolean res;

        res = g_spawn_command_line_async ("gdmflexiserver -s", &error);
        if (! res) {
                g_warning ("Unable to start new login: %s", error->message);
                g_error_free (error);
        }

        return res;
}

/* needs to stay in sync with gdm-slave */
static char *
_get_primary_user_session_id (GdmUserManager *manager,
                              GdmUser        *user)
{
        gboolean    res;
        gboolean    can_activate_sessions;
        GError     *error;
        GList      *sessions;
        GList      *l;
        char       *primary_ssid;

        if (manager->priv->seat_id == NULL || manager->priv->seat_id[0] == '\0') {
                g_debug ("GdmUserManager: display seat id is not set; can't switch sessions");
                return NULL;
        }

        primary_ssid = NULL;
        sessions = NULL;

        g_debug ("GdmUserManager: checking if seat can activate sessions");

        error = NULL;
        res = dbus_g_proxy_call (manager->priv->seat_proxy,
                                 "CanActivateSessions",
                                 &error,
                                 G_TYPE_INVALID,
                                 G_TYPE_BOOLEAN, &can_activate_sessions,
                                 G_TYPE_INVALID);
        if (! res) {
                g_warning ("unable to determine if seat can activate sessions: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        if (! can_activate_sessions) {
                g_debug ("GdmUserManager: seat is unable to activate sessions");
                goto out;
        }

        sessions = gdm_user_get_sessions (user);
        if (sessions == NULL) {
                g_warning ("unable to determine sessions for user: %s",
                           gdm_user_get_user_name (user));
                goto out;
        }

        for (l = sessions; l != NULL; l = l->next) {
                const char *ssid;

                ssid = l->data;

                /* FIXME: better way to choose? */
                if (ssid != NULL) {
                        primary_ssid = g_strdup (ssid);
                        break;
                }
        }

 out:

        return primary_ssid;
}

static gboolean
activate_session_id (GdmUserManager *manager,
                     const char     *seat_id,
                     const char     *session_id)
{
        DBusError    local_error;
        DBusMessage *message;
        DBusMessage *reply;
        gboolean     ret;

        ret = FALSE;
        reply = NULL;

        dbus_error_init (&local_error);
        message = dbus_message_new_method_call ("org.freedesktop.ConsoleKit",
                                                seat_id,
                                                "org.freedesktop.ConsoleKit.Seat",
                                                "ActivateSession");
        if (message == NULL) {
                goto out;
        }

        if (! dbus_message_append_args (message,
                                        DBUS_TYPE_OBJECT_PATH, &session_id,
                                        DBUS_TYPE_INVALID)) {
                goto out;
        }


        dbus_error_init (&local_error);
        reply = dbus_connection_send_with_reply_and_block (dbus_g_connection_get_connection (manager->priv->connection),
                                                           message,
                                                           -1,
                                                           &local_error);
        if (reply == NULL) {
                if (dbus_error_is_set (&local_error)) {
                        g_warning ("Unable to activate session: %s", local_error.message);
                        dbus_error_free (&local_error);
                        goto out;
                }
        }

        ret = TRUE;
 out:
        if (message != NULL) {
                dbus_message_unref (message);
        }
        if (reply != NULL) {
                dbus_message_unref (reply);
        }

        return ret;
}

static gboolean
session_is_login_window (GdmUserManager *manager,
                         const char     *session_id)
{
        DBusGProxy      *proxy;
        GError          *error;
        gboolean         res;
        gboolean         ret;
        char            *session_type;

        ret = FALSE;

        proxy = dbus_g_proxy_new_for_name (manager->priv->connection,
                                           CK_NAME,
                                           session_id,
                                           CK_SESSION_INTERFACE);
        if (proxy == NULL) {
                g_warning ("Failed to connect to the ConsoleKit seat object");
                goto out;
        }

        session_type = NULL;
        error = NULL;
        res = dbus_g_proxy_call (proxy,
                                 "GetSessionType",
                                 &error,
                                 G_TYPE_INVALID,
                                 G_TYPE_STRING, &session_type,
                                 G_TYPE_INVALID);
        if (! res) {
                g_debug ("Failed to identify the session type: %s", error->message);
                g_error_free (error);
                goto out;
        }

        if (session_type == NULL || session_type[0] == '\0' || strcmp (session_type, "LoginWindow") != 0) {
                goto out;
        }

        ret = TRUE;

 out:
        if (proxy != NULL) {
                g_object_unref (proxy);
        }

        return ret;
}

static char *
_get_login_window_session_id (GdmUserManager *manager)
{
        gboolean    res;
        gboolean    can_activate_sessions;
        GError     *error;
        GPtrArray  *sessions;
        char       *primary_ssid;
        int         i;

        if (manager->priv->seat_id == NULL || manager->priv->seat_id[0] == '\0') {
                g_debug ("GdmUserManager: display seat id is not set; can't switch sessions");
                return NULL;
        }

        primary_ssid = NULL;
        sessions = NULL;

        g_debug ("GdmSlave: checking if seat can activate sessions");

        error = NULL;
        res = dbus_g_proxy_call (manager->priv->seat_proxy,
                                 "CanActivateSessions",
                                 &error,
                                 G_TYPE_INVALID,
                                 G_TYPE_BOOLEAN, &can_activate_sessions,
                                 G_TYPE_INVALID);
        if (! res) {
                g_warning ("unable to determine if seat can activate sessions: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        if (! can_activate_sessions) {
                g_debug ("GdmSlave: seat is unable to activate sessions");
                goto out;
        }

        error = NULL;
        res = dbus_g_proxy_call (manager->priv->seat_proxy,
                                 "GetSessions",
                                 &error,
                                 G_TYPE_INVALID,
                                 dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_G_OBJECT_PATH), &sessions,
                                 G_TYPE_INVALID);
        if (! res) {
                g_warning ("unable to determine sessions for user: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        for (i = 0; i < sessions->len; i++) {
                char *ssid;

                ssid = g_ptr_array_index (sessions, i);

                if (session_is_login_window (manager, ssid)) {
                        primary_ssid = g_strdup (ssid);
                        break;
                }
        }
        g_ptr_array_foreach (sessions, (GFunc)g_free, NULL);
        g_ptr_array_free (sessions, TRUE);

 out:

        return primary_ssid;
}

gboolean
gdm_user_manager_goto_login_session (GdmUserManager *manager)
{
        gboolean ret;
        gboolean res;
        char    *ssid;

        g_return_val_if_fail (GDM_IS_USER_MANAGER (manager), FALSE);

        ret = FALSE;

        /* First look for any existing LoginWindow sessions on the seat.
           If none are found, create a new one. */

        ssid = _get_login_window_session_id (manager);
        if (ssid != NULL) {
                res = activate_session_id (manager, manager->priv->seat_id, ssid);
                if (res) {
                        ret = TRUE;
                }
        }

        if (! ret) {
                res = start_new_login_session (manager);
                if (res) {
                        ret = TRUE;
                }
        }

        return ret;
}

gboolean
gdm_user_manager_activate_user_session (GdmUserManager *manager,
                                        GdmUser        *user)
{
        gboolean ret;
        char    *ssid;
        gboolean res;

        g_return_val_if_fail (GDM_IS_USER_MANAGER (manager), FALSE);
        g_return_val_if_fail (GDM_IS_USER (user), FALSE);

        ret = FALSE;

        ssid = _get_primary_user_session_id (manager, user);
        if (ssid == NULL) {
                goto out;
        }

        res = activate_session_id (manager, manager->priv->seat_id, ssid);
        if (! res) {
                g_debug ("GdmUserManager: unable to activate session: %s", ssid);
                goto out;
        }

        ret = TRUE;
 out:
        return ret;
}

static void
on_user_sessions_changed (GdmUser        *user,
                          GdmUserManager *manager)
{
        guint nsessions;

        nsessions = gdm_user_get_num_sessions (user);

        g_debug ("GdmUserManager: sessions changed user=%s num=%d",
                 gdm_user_get_user_name (user),
                 nsessions);

        /* only signal on zero and one */
        if (nsessions > 1) {
                return;
        }

        g_signal_emit (manager, signals [USER_IS_LOGGED_IN_CHANGED], 0, user);
}

static void
on_user_icon_changed (GdmUser        *user,
                      GdmUserManager *manager)
{
        g_debug ("GdmUserManager: user icon changed");
}

static char *
get_seat_id_for_session (DBusGConnection *connection,
                         const char      *session_id)
{
        DBusGProxy      *proxy;
        GError          *error;
        char            *seat_id;
        gboolean         res;

        proxy = NULL;
        seat_id = NULL;

        proxy = dbus_g_proxy_new_for_name (connection,
                                           CK_NAME,
                                           session_id,
                                           CK_SESSION_INTERFACE);
        if (proxy == NULL) {
                g_warning ("Failed to connect to the ConsoleKit session object");
                goto out;
        }

        error = NULL;
        res = dbus_g_proxy_call (proxy,
                                 "GetSeatId",
                                 &error,
                                 G_TYPE_INVALID,
                                 DBUS_TYPE_G_OBJECT_PATH, &seat_id,
                                 G_TYPE_INVALID);
        if (! res) {
                g_debug ("Failed to identify the current seat: %s", error->message);
                g_error_free (error);
        }
 out:
        if (proxy != NULL) {
                g_object_unref (proxy);
        }

        return seat_id;
}

static char *
get_x11_display_for_session (DBusGConnection *connection,
                             const char      *session_id)
{
        DBusGProxy      *proxy;
        GError          *error;
        char            *x11_display;
        gboolean         res;

        proxy = NULL;
        x11_display = NULL;

        proxy = dbus_g_proxy_new_for_name (connection,
                                           CK_NAME,
                                           session_id,
                                           CK_SESSION_INTERFACE);
        if (proxy == NULL) {
                g_warning ("Failed to connect to the ConsoleKit session object");
                goto out;
        }

        error = NULL;
        res = dbus_g_proxy_call (proxy,
                                 "GetX11Display",
                                 &error,
                                 G_TYPE_INVALID,
                                 G_TYPE_STRING, &x11_display,
                                 G_TYPE_INVALID);
        if (! res) {
                g_debug ("Failed to identify the x11 display: %s", error->message);
                g_error_free (error);
        }
 out:
        if (proxy != NULL) {
                g_object_unref (proxy);
        }

        return x11_display;
}

static gboolean
maybe_add_session_for_user (GdmUserManager *manager,
                            GdmUser        *user,
                            const char     *ssid)
{
        char    *sid;
        char    *x11_display;
        gboolean ret;

        ret = FALSE;
        sid = NULL;
        x11_display = NULL;

        /* skip if on another seat */
        sid = get_seat_id_for_session (manager->priv->connection, ssid);
        if (sid == NULL
            || manager->priv->seat_id == NULL
            || strcmp (sid, manager->priv->seat_id) != 0) {
                g_debug ("GdmUserManager: not adding session on other seat: %s", ssid);
                goto out;
        }

        /* skip if doesn't have an x11 display */
        x11_display = get_x11_display_for_session (manager->priv->connection, ssid);
        if (x11_display == NULL || x11_display[0] == '\0') {
                g_debug ("GdmUserManager: not adding session without a x11 display: %s", ssid);
                goto out;
        }

        if (g_hash_table_lookup (manager->priv->exclusions, gdm_user_get_user_name (user))) {
                g_debug ("GdmUserManager: excluding user '%s'", gdm_user_get_user_name (user));
                goto out;
        }

        g_hash_table_insert (manager->priv->sessions,
                             g_strdup (ssid),
                             g_strdup (gdm_user_get_user_name (user)));

        _gdm_user_add_session (user, ssid);
        g_debug ("GdmUserManager: added session for user: %s", gdm_user_get_user_name (user));

        ret = TRUE;

 out:
        g_free (sid);
        g_free (x11_display);

        return ret;
}

static void
add_sessions_for_user (GdmUserManager *manager,
                       GdmUser        *user)
{
        DBusGProxy      *proxy;
        GError          *error;
        gboolean         res;
        guint32          uid;
        GPtrArray       *sessions;
        int              i;

        proxy = dbus_g_proxy_new_for_name (manager->priv->connection,
                                           CK_NAME,
                                           CK_MANAGER_PATH,
                                           CK_MANAGER_INTERFACE);
        if (proxy == NULL) {
                g_warning ("Failed to connect to the ConsoleKit manager object");
                goto out;
        }

        uid = gdm_user_get_uid (user);

        g_debug ("Getting list of sessions for user %u", uid);

        error = NULL;
        res = dbus_g_proxy_call (proxy,
                                 "GetSessionsForUnixUser",
                                 &error,
                                 G_TYPE_UINT, uid,
                                 G_TYPE_INVALID,
                                 dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_G_OBJECT_PATH),
                                 &sessions,
                                 G_TYPE_INVALID);
        if (! res) {
                g_debug ("Failed to find sessions for user: %s", error->message);
                g_error_free (error);
                goto out;
        }

        g_debug ("Found %d sessions for user %s", sessions->len, gdm_user_get_user_name (user));

        for (i = 0; i < sessions->len; i++) {
                char *ssid;

                ssid = g_ptr_array_index (sessions, i);
                maybe_add_session_for_user (manager, user, ssid);
        }

        g_ptr_array_foreach (sessions, (GFunc)g_free, NULL);
        g_ptr_array_free (sessions, TRUE);

 out:
        if (proxy != NULL) {
                g_object_unref (proxy);
        }
}

static GdmUser *
create_user (GdmUserManager *manager)
{
        GdmUser *user;

        user = g_object_new (GDM_TYPE_USER, "manager", manager, NULL);
        g_signal_connect (user,
                          "sessions-changed",
                          G_CALLBACK (on_user_sessions_changed),
                          manager);
        g_signal_connect (user,
                          "icon-changed",
                          G_CALLBACK (on_user_icon_changed),
                          manager);
        return user;
}

static void
add_user (GdmUserManager *manager,
          GdmUser        *user)
{
        add_sessions_for_user (manager, user);
        g_hash_table_insert (manager->priv->users,
                             g_strdup (gdm_user_get_user_name (user)),
                             g_object_ref (user));

        g_signal_emit (manager, signals[USER_ADDED], 0, user);
}

static GdmUser *
add_new_user_for_pwent (GdmUserManager *manager,
                        struct passwd  *pwent)
{
        GdmUser *user;

        g_debug ("Creating new user");

        user = create_user (manager);
        _gdm_user_update (user, pwent);

        add_user (manager, user);

        return user;
}

static char *
get_current_seat_id (DBusGConnection *connection)
{
        DBusGProxy      *proxy;
        GError          *error;
        char            *session_id;
        char            *seat_id;
        gboolean         res;

        proxy = NULL;
        session_id = NULL;
        seat_id = NULL;

        proxy = dbus_g_proxy_new_for_name (connection,
                                           CK_NAME,
                                           CK_MANAGER_PATH,
                                           CK_MANAGER_INTERFACE);
        if (proxy == NULL) {
                g_warning ("Failed to connect to the ConsoleKit manager object");
                goto out;
        }

        error = NULL;
        res = dbus_g_proxy_call (proxy,
                                 "GetCurrentSession",
                                 &error,
                                 G_TYPE_INVALID,
                                 DBUS_TYPE_G_OBJECT_PATH,
                                 &session_id,
                                 G_TYPE_INVALID);
        if (! res) {
                g_debug ("Failed to identify the current session: %s", error->message);
                g_error_free (error);
                goto out;
        }

        seat_id = get_seat_id_for_session (connection, session_id);

 out:
        if (proxy != NULL) {
                g_object_unref (proxy);
        }
        g_free (session_id);

        return seat_id;
}

static gboolean
get_uid_from_session_id (GdmUserManager *manager,
                         const char     *session_id,
                         uid_t          *uidp)
{
        DBusGProxy      *proxy;
        GError          *error;
        guint            uid;
        gboolean         res;

        proxy = dbus_g_proxy_new_for_name (manager->priv->connection,
                                           CK_NAME,
                                           session_id,
                                           CK_SESSION_INTERFACE);
        if (proxy == NULL) {
                g_warning ("Failed to connect to the ConsoleKit session object");
                return FALSE;
        }

        error = NULL;
        res = dbus_g_proxy_call (proxy,
                                 "GetUnixUser",
                                 &error,
                                 G_TYPE_INVALID,
                                 G_TYPE_UINT, &uid,
                                 G_TYPE_INVALID);
        g_object_unref (proxy);

        if (! res) {
                g_warning ("Failed to query the session: %s", error->message);
                g_error_free (error);
                return FALSE;
        }

        if (uidp != NULL) {
                *uidp = (uid_t) uid;
        }

        return TRUE;
}

static void
seat_session_added (DBusGProxy     *seat_proxy,
                    const char     *session_id,
                    GdmUserManager *manager)
{
        uid_t          uid;
        gboolean       res;
        struct passwd *pwent;
        GdmUser       *user;
        gboolean       is_new;

        g_debug ("Session added: %s", session_id);

        res = get_uid_from_session_id (manager, session_id, &uid);
        if (! res) {
                g_warning ("Unable to lookup user for session");
                return;
        }

        errno = 0;
        pwent = getpwuid (uid);
        if (pwent == NULL) {
                g_warning ("Unable to lookup user id %d: %s", (int)uid, g_strerror (errno));
                return;
        }

        /* check exclusions up front */
        if (g_hash_table_lookup (manager->priv->exclusions, pwent->pw_name)) {
                g_debug ("GdmUserManager: excluding user '%s'", pwent->pw_name);
                return;
        }

        user = g_hash_table_lookup (manager->priv->users, pwent->pw_name);
        if (user == NULL) {
                g_debug ("Creating new user");

                user = create_user (manager);
                _gdm_user_update (user, pwent);
                is_new = TRUE;
        } else {
                is_new = FALSE;
        }

        res = maybe_add_session_for_user (manager, user, session_id);

        /* only add the user if we added a session */
        if (is_new) {
                if (res) {
                        add_user (manager, user);
                } else {
                        g_object_unref (user);
                }
        }
}

static void
seat_session_removed (DBusGProxy     *seat_proxy,
                      const char     *session_id,
                      GdmUserManager *manager)
{
        GdmUser *user;
        char    *username;

        g_debug ("Session removed: %s", session_id);

        /* since the session object may already be gone
         * we can't query CK directly */

        username = g_hash_table_lookup (manager->priv->sessions, session_id);
        if (username == NULL) {
                return;
        }

        user = g_hash_table_lookup (manager->priv->users, username);
        if (user == NULL) {
                /* nothing to do */
                return;
        }

        g_debug ("GdmUserManager: Session removed for %s", username);
        _gdm_user_remove_session (user, session_id);
}

static void
on_proxy_destroy (DBusGProxy     *proxy,
                  GdmUserManager *manager)
{
        g_debug ("GdmUserManager: seat proxy destroyed");

        manager->priv->seat_proxy = NULL;
}

static void
get_seat_proxy (GdmUserManager *manager)
{
        DBusGProxy      *proxy;
        GError          *error;

        g_assert (manager->priv->seat_proxy == NULL);

        error = NULL;
        manager->priv->connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
        if (manager->priv->connection == NULL) {
                g_warning ("Failed to connect to the D-Bus daemon: %s", error->message);
                g_error_free (error);
                return;
        }

        manager->priv->seat_id = get_current_seat_id (manager->priv->connection);
        if (manager->priv->seat_id == NULL) {
                return;
        }

        g_debug ("GdmUserManager: Found current seat: %s", manager->priv->seat_id);

        error = NULL;
        proxy = dbus_g_proxy_new_for_name_owner (manager->priv->connection,
                                                 CK_NAME,
                                                 manager->priv->seat_id,
                                                 CK_SEAT_INTERFACE,
                                                 &error);

        if (proxy == NULL) {
                g_warning ("Failed to connect to the ConsoleKit seat object: %s", error->message);
                g_error_free (error);
                return;
        }

        g_signal_connect (proxy, "destroy", G_CALLBACK (on_proxy_destroy), manager);

        dbus_g_proxy_add_signal (proxy,
                                 "SessionAdded",
                                 DBUS_TYPE_G_OBJECT_PATH,
                                 G_TYPE_INVALID);
        dbus_g_proxy_add_signal (proxy,
                                 "SessionRemoved",
                                 DBUS_TYPE_G_OBJECT_PATH,
                                 G_TYPE_INVALID);
        dbus_g_proxy_connect_signal (proxy,
                                     "SessionAdded",
                                     G_CALLBACK (seat_session_added),
                                     manager,
                                     NULL);
        dbus_g_proxy_connect_signal (proxy,
                                     "SessionRemoved",
                                     G_CALLBACK (seat_session_removed),
                                     manager,
                                     NULL);
        manager->priv->seat_proxy = proxy;

}

/**
 * gdm_manager_get_user:
 * @manager: the manager to query.
 * @username: the login name of the user to get.
 *
 * Retrieves a pointer to the #GdmUser object for the login named @username
 * from @manager. This pointer is not a reference, and should not be released.
 *
 * Returns: a pointer to a #GdmUser object.
 **/
GdmUser *
gdm_user_manager_get_user (GdmUserManager *manager,
                           const char     *username)
{
        GdmUser *user;

        g_return_val_if_fail (GDM_IS_USER_MANAGER (manager), NULL);
        g_return_val_if_fail (username != NULL && username[0] != '\0', NULL);

        user = g_hash_table_lookup (manager->priv->users, username);

        if (user == NULL) {
                struct passwd *pwent;

                pwent = getpwnam (username);

                if (pwent != NULL) {
                        user = add_new_user_for_pwent (manager, pwent);
                }
        }

        return user;
}

GdmUser *
gdm_user_manager_get_user_by_uid (GdmUserManager *manager,
                                  gulong          uid)
{
        GdmUser       *user;
        struct passwd *pwent;

        g_return_val_if_fail (GDM_IS_USER_MANAGER (manager), NULL);

        pwent = getpwuid (uid);
        if (pwent == NULL) {
                g_warning ("GdmUserManager: unable to lookup uid %d", (int)uid);
                return NULL;
        }

        user = g_hash_table_lookup (manager->priv->users, pwent->pw_name);

        if (user == NULL) {
                user = add_new_user_for_pwent (manager, pwent);
        }

        return user;
}

static void
listify_hash_values_hfunc (gpointer key,
                           gpointer value,
                           gpointer user_data)
{
        GSList **list = user_data;

        *list = g_slist_prepend (*list, value);
}

GSList *
gdm_user_manager_list_users (GdmUserManager *manager)
{
        GSList *retval;

        g_return_val_if_fail (GDM_IS_USER_MANAGER (manager), NULL);

        retval = NULL;
        g_hash_table_foreach (manager->priv->users, listify_hash_values_hfunc, &retval);

        return g_slist_sort (retval, (GCompareFunc) gdm_user_collate);
}

static gboolean
parse_value_as_ulong (const char *value,
                      gulong     *ulongval)
{
        char  *end_of_valid_long;
        glong  long_value;
        gulong ulong_value;

        errno = 0;
        long_value = strtol (value, &end_of_valid_long, 10);

        if (*value == '\0' || *end_of_valid_long != '\0') {
                return FALSE;
        }

        ulong_value = long_value;
        if (ulong_value != long_value || errno == ERANGE) {
                return FALSE;
        }

        *ulongval = ulong_value;

        return TRUE;
}

static gboolean
parse_ck_history_line (const char *line,
                       char      **user_namep,
                       gulong     *frequencyp)
{
        GRegex     *re;
        GMatchInfo *match_info;
        gboolean    res;
        gboolean    ret;
        GError     *error;

        ret = FALSE;
        re = NULL;
        match_info = NULL;

        error = NULL;
        re = g_regex_new ("(?P<username>[0-9a-zA-Z]+)[ ]+(?P<frequency>[0-9]+)", 0, 0, &error);
        if (re == NULL) {
                g_critical ("%s", error->message);
                goto out;
        }

        g_regex_match (re, line, 0, &match_info);

        res = g_match_info_matches (match_info);
        if (! res) {
                g_warning ("Unable to parse history: %s", line);
                goto out;
        }

        if (user_namep != NULL) {
                *user_namep = g_match_info_fetch_named (match_info, "username");
        }

        if (frequencyp != NULL) {
                char *freq;
                freq = g_match_info_fetch_named (match_info, "frequency");
                res = parse_value_as_ulong (freq, frequencyp);
                g_free (freq);
                if (! res) {
                        goto out;
                }
        }

        ret = TRUE;

 out:
        if (match_info != NULL) {
                g_match_info_free (match_info);
        }
        if (re != NULL) {
                g_regex_unref (re);
        }
        return ret;
}

static void
process_ck_history_line (GdmUserManager *manager,
                         const char     *line)
{
        gboolean res;
        char    *username;
        gulong   frequency;
        GdmUser *user;

        frequency = 0;
        username = NULL;
        res = parse_ck_history_line (line, &username, &frequency);
        if (! res) {
                return;
        }

        if (g_hash_table_lookup (manager->priv->exclusions, username)) {
                g_debug ("GdmUserManager: excluding user '%s'", username);
                g_free (username);
                return;
        }

        user = gdm_user_manager_get_user (manager, username);
        if (user == NULL) {
                g_debug ("GdmUserManager: unable to lookup user '%s'", username);
                g_free (username);
                return;
        }

        g_object_set (user, "login-frequency", frequency, NULL);
        g_signal_emit (manager, signals [USER_LOGIN_FREQUENCY_CHANGED], 0, user);
        g_free (username);
}

static gboolean
ck_history_watch (GIOChannel     *source,
                  GIOCondition    condition,
                  GdmUserManager *manager)
{
        GIOStatus status;
        gboolean  done  = FALSE;

        g_return_val_if_fail (manager != NULL, FALSE);

        if (condition & G_IO_IN) {
                char   *str;
                GError *error;

                error = NULL;
                status = g_io_channel_read_line (source, &str, NULL, NULL, &error);
                if (error != NULL) {
                        g_warning ("GdmUserManager: unable to read line: %s", error->message);
                        g_error_free (error);
                }

                if (status == G_IO_STATUS_NORMAL) {
                        g_debug ("GdmUserManager: history output: %s", str);
                        process_ck_history_line (manager, str);
                } else if (status == G_IO_STATUS_EOF) {
                        done = TRUE;
                }

                g_free (str);
        } else if (condition & G_IO_HUP) {
                done = TRUE;
        }

        if (done) {
                g_signal_emit (G_OBJECT (manager), signals[USERS_LOADED], 0);

                manager->priv->ck_history_id = 0;
                return FALSE;
        }

        return TRUE;
}

static void
reload_ck_history (GdmUserManager *manager)
{
        char       *command;
        const char *seat_id;
        GError     *error;
        gboolean    res;
        char      **argv;
        int         standard_out;
        GIOChannel *channel;

        seat_id = NULL;
        if (manager->priv->seat_id != NULL
            && g_str_has_prefix (manager->priv->seat_id, "/org/freedesktop/ConsoleKit/")) {

                seat_id = manager->priv->seat_id + strlen ("/org/freedesktop/ConsoleKit/");
        }

        if (seat_id == NULL) {
                g_debug ("Unable to find users: no seat-id found");
                return;
        }

        command = g_strdup_printf ("ck-history --frequent --seat='%s' --session-type=''",
                                   seat_id);
        g_debug ("GdmUserManager: running '%s'", command);
        error = NULL;
        if (! g_shell_parse_argv (command, NULL, &argv, &error)) {
                g_warning ("Could not parse command: %s", error->message);
                g_error_free (error);
                goto out;
        }

        error = NULL;
        res = g_spawn_async_with_pipes (NULL,
                                        argv,
                                        NULL,
                                        G_SPAWN_SEARCH_PATH,
                                        NULL,
                                        NULL,
                                        NULL, /* pid */
                                        NULL,
                                        &standard_out,
                                        NULL,
                                        &error);
        g_strfreev (argv);
        if (! res) {
                g_warning ("Unable to run ck-history: %s", error->message);
                g_error_free (error);
                goto out;
        }

        channel = g_io_channel_unix_new (standard_out);
        g_io_channel_set_close_on_unref (channel, TRUE);
        g_io_channel_set_flags (channel,
                                g_io_channel_get_flags (channel) | G_IO_FLAG_NONBLOCK,
                                NULL);
        manager->priv->ck_history_id = g_io_add_watch (channel,
                                                       G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
                                                       (GIOFunc)ck_history_watch,
                                                       manager);
        g_io_channel_unref (channel);

 out:
        g_free (command);
}

static void
reload_passwd (GdmUserManager *manager)
{
        struct passwd *pwent;
        GSList        *old_users;
        GSList        *new_users;
        GSList        *list;
        FILE          *fp;

        old_users = NULL;
        new_users = NULL;

        errno = 0;
        fp = fopen (PATH_PASSWD, "r");
        if (fp == NULL) {
                g_warning ("Unable to open %s: %s", PATH_PASSWD, g_strerror (errno));
                goto out;
        }

        g_hash_table_foreach (manager->priv->users, listify_hash_values_hfunc, &old_users);
        g_slist_foreach (old_users, (GFunc) g_object_ref, NULL);

        /* Make sure we keep users who are logged in no matter what. */
        for (list = old_users; list; list = list->next) {
                if (gdm_user_get_num_sessions (list->data) > 0) {
                        g_object_freeze_notify (G_OBJECT (list->data));
                        new_users = g_slist_prepend (new_users, g_object_ref (list->data));
                }
        }

        for (pwent = fgetpwent (fp); pwent != NULL; pwent = fgetpwent (fp)) {
                GdmUser *user;

                user = NULL;

                /* Skip users below MinimalUID... */
                if (pwent->pw_uid < DEFAULT_MINIMAL_UID) {
                        continue;
                }

                /* ...And users w/ invalid shells... */
                if (pwent->pw_shell == NULL ||
                    !g_hash_table_lookup (manager->priv->shells, pwent->pw_shell)) {
                        g_debug ("GdmUserManager: skipping user with bad shell: %s", pwent->pw_name);
                        continue;
                }

                /* ...And explicitly excluded users */
                if (g_hash_table_lookup (manager->priv->exclusions, pwent->pw_name)) {
                        g_debug ("GdmUserManager: explicitly skipping user: %s", pwent->pw_name);
                        continue;
                }

                user = g_hash_table_lookup (manager->priv->users, pwent->pw_name);

                /* Update users already in the *new* list */
                if (g_slist_find (new_users, user)) {
                        _gdm_user_update (user, pwent);
                        continue;
                }

                if (user == NULL) {
                        user = create_user (manager);
                } else {
                        g_object_ref (user);
                }

                /* Freeze & update users not already in the new list */
                g_object_freeze_notify (G_OBJECT (user));
                _gdm_user_update (user, pwent);

                new_users = g_slist_prepend (new_users, user);
        }

        /* Go through and handle added users */
        for (list = new_users; list; list = list->next) {
                if (! g_slist_find (old_users, list->data)) {
                        add_user (manager, list->data);
                }
        }

        /* Go through and handle removed users */
        for (list = old_users; list; list = list->next) {
                if (! g_slist_find (new_users, list->data)) {
                        g_signal_emit (manager, signals[USER_REMOVED], 0, list->data);
                        g_hash_table_remove (manager->priv->users,
                                             gdm_user_get_user_name (list->data));
                }
        }

 out:
        /* Cleanup */

        fclose (fp);

        g_slist_foreach (new_users, (GFunc) g_object_thaw_notify, NULL);
        g_slist_foreach (new_users, (GFunc) g_object_unref, NULL);
        g_slist_free (new_users);

        g_slist_foreach (old_users, (GFunc) g_object_unref, NULL);
        g_slist_free (old_users);
}

static void
reload_users (GdmUserManager *manager)
{
        reload_ck_history (manager);
        reload_passwd (manager);
}

static gboolean
reload_users_timeout (GdmUserManager *manager)
{
        reload_users (manager);
        manager->priv->reload_id = 0;

        return FALSE;
}

static void
queue_reload_users (GdmUserManager *manager)
{
        if (manager->priv->reload_id > 0) {
                return;
        }

        g_signal_emit (G_OBJECT (manager), signals[LOADING_USERS], 0);
        manager->priv->reload_id = g_idle_add ((GSourceFunc)reload_users_timeout, manager);
}

static void
reload_shells (GdmUserManager *manager)
{
        char *shell;

        setusershell ();

        g_hash_table_remove_all (manager->priv->shells);
        for (shell = getusershell (); shell != NULL; shell = getusershell ()) {
                /* skip well known not-real shells */
                if (shell == NULL
                    || strcmp (shell, "/sbin/nologin") == 0
                    || strcmp (shell, "/bin/false") == 0) {
                        g_debug ("GdmUserManager: skipping shell %s", shell);
                        continue;
                }
                g_hash_table_insert (manager->priv->shells,
                                     g_strdup (shell),
                                     GUINT_TO_POINTER (TRUE));
        }

        endusershell ();
}

static void
on_shells_monitor_changed (GFileMonitor     *monitor,
                           GFile            *file,
                           GFile            *other_file,
                           GFileMonitorEvent event_type,
                           GdmUserManager   *manager)
{
        if (event_type != G_FILE_MONITOR_EVENT_CHANGED &&
            event_type != G_FILE_MONITOR_EVENT_CREATED) {
                return;
        }

        reload_shells (manager);
        reload_passwd (manager);
}

static void
on_passwd_monitor_changed (GFileMonitor     *monitor,
                           GFile            *file,
                           GFile            *other_file,
                           GFileMonitorEvent event_type,
                           GdmUserManager   *manager)
{
        if (event_type != G_FILE_MONITOR_EVENT_CHANGED &&
            event_type != G_FILE_MONITOR_EVENT_CREATED) {
                return;
        }

        reload_passwd (manager);
}

static void
ignore_log_handler (const char     *log_domain,
                    GLogLevelFlags  log_level,
                    const char     *message,
                    gpointer        user_data)
{
        return;
}

static void
check_gdm_debug (void)
{
        GDebugKey gdmkeys[] = { { "gdm", 1 } };
        const char *gnome_shell_debug = g_getenv ("GNOME_SHELL_DEBUG");

        if (!gnome_shell_debug ||
            !g_parse_debug_string (gnome_shell_debug, gdmkeys, 1)) {
                g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                                   ignore_log_handler, NULL);
        }
}

static void
gdm_user_manager_class_init (GdmUserManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gdm_user_manager_finalize;

        signals [LOADING_USERS] =
                g_signal_new ("loading-users",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdmUserManagerClass, loading_users),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        signals [USERS_LOADED] =
                g_signal_new ("users-loaded",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdmUserManagerClass, users_loaded),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        signals [USER_ADDED] =
                g_signal_new ("user-added",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdmUserManagerClass, user_added),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1, GDM_TYPE_USER);
        signals [USER_REMOVED] =
                g_signal_new ("user-removed",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdmUserManagerClass, user_removed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1, GDM_TYPE_USER);
        signals [USER_IS_LOGGED_IN_CHANGED] =
                g_signal_new ("user-is-logged-in-changed",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdmUserManagerClass, user_is_logged_in_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1, GDM_TYPE_USER);
        signals [USER_LOGIN_FREQUENCY_CHANGED] =
                g_signal_new ("user-login-frequency-changed",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GdmUserManagerClass, user_login_frequency_changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__OBJECT,
                              G_TYPE_NONE, 1, GDM_TYPE_USER);

        g_type_class_add_private (klass, sizeof (GdmUserManagerPrivate));
        check_gdm_debug ();
}

static void
gdm_user_manager_init (GdmUserManager *manager)
{
        int            i;
        GFile         *file;
        GError        *error;
        const char    *exclude_default[] = DEFAULT_EXCLUDE;

        manager->priv = GDM_USER_MANAGER_GET_PRIVATE (manager);

        /* sessions */
        manager->priv->sessions = g_hash_table_new_full (g_str_hash,
                                                         g_str_equal,
                                                         g_free,
                                                         g_free);

        /* exclusions */
        manager->priv->exclusions = g_hash_table_new_full (g_str_hash,
                                                           g_str_equal,
                                                           g_free,
                                                           NULL);
        for (i = 0; exclude_default[i] != NULL; i++) {
                g_hash_table_insert (manager->priv->exclusions,
                                     g_strdup (exclude_default [i]),
                                     GUINT_TO_POINTER (TRUE));
        }

        /* /etc/shells */
        manager->priv->shells = g_hash_table_new_full (g_str_hash,
                                                       g_str_equal,
                                                       g_free,
                                                       NULL);
        reload_shells (manager);
        file = g_file_new_for_path (_PATH_SHELLS);
        error = NULL;
        manager->priv->shells_monitor = g_file_monitor_file (file,
                                                             G_FILE_MONITOR_NONE,
                                                             NULL,
                                                             &error);
        if (manager->priv->shells_monitor != NULL) {
                g_signal_connect (manager->priv->shells_monitor,
                                  "changed",
                                  G_CALLBACK (on_shells_monitor_changed),
                                  manager);
        } else {
                g_warning ("Unable to monitor %s: %s", _PATH_SHELLS, error->message);
                g_error_free (error);
        }
        g_object_unref (file);

        /* /etc/passwd */
        manager->priv->users = g_hash_table_new_full (g_str_hash,
                                                      g_str_equal,
                                                      g_free,
                                                      (GDestroyNotify) g_object_run_dispose);
        file = g_file_new_for_path (PATH_PASSWD);
        manager->priv->passwd_monitor = g_file_monitor_file (file,
                                                             G_FILE_MONITOR_NONE,
                                                             NULL,
                                                             &error);
        if (manager->priv->passwd_monitor != NULL) {
                g_signal_connect (manager->priv->passwd_monitor,
                                  "changed",
                                  G_CALLBACK (on_passwd_monitor_changed),
                                  manager);
        } else {
                g_warning ("Unable to monitor %s: %s", PATH_PASSWD, error->message);
                g_error_free (error);
        }
        g_object_unref (file);


        get_seat_proxy (manager);

        queue_reload_users (manager);

        manager->priv->users_dirty = FALSE;
}

static void
gdm_user_manager_finalize (GObject *object)
{
        GdmUserManager *manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GDM_IS_USER_MANAGER (object));

        manager = GDM_USER_MANAGER (object);

        g_return_if_fail (manager->priv != NULL);

        if (manager->priv->seat_proxy != NULL) {
                g_object_unref (manager->priv->seat_proxy);
        }

        if (manager->priv->ck_history_id != 0) {
                g_source_remove (manager->priv->ck_history_id);
                manager->priv->ck_history_id = 0;
        }

        if (manager->priv->reload_id > 0) {
                g_source_remove (manager->priv->reload_id);
                manager->priv->reload_id = 0;
        }

        g_hash_table_destroy (manager->priv->sessions);

        g_file_monitor_cancel (manager->priv->passwd_monitor);
        g_hash_table_destroy (manager->priv->users);

        g_file_monitor_cancel (manager->priv->shells_monitor);
        g_hash_table_destroy (manager->priv->shells);

        g_free (manager->priv->seat_id);

        G_OBJECT_CLASS (gdm_user_manager_parent_class)->finalize (object);
}

GdmUserManager *
gdm_user_manager_ref_default (void)
{
        if (user_manager_object != NULL) {
                g_object_ref (user_manager_object);
        } else {
                user_manager_object = g_object_new (GDM_TYPE_USER_MANAGER, NULL);
                g_object_add_weak_pointer (user_manager_object,
                                           (gpointer *) &user_manager_object);
        }

        return GDM_USER_MANAGER (user_manager_object);
}
