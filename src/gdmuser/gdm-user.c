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

#include <config.h>

#include <float.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <dbus/dbus-glib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "gdm-user-private.h"

#define GDM_USER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GDM_TYPE_USER, GdmUserClass))
#define GDM_IS_USER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDM_TYPE_USER))
#define GDM_USER_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), GDM_TYPE_USER, GdmUserClass))

#define GLOBAL_FACEDIR    DATADIR "/faces"
#define MAX_FILE_SIZE     65536

#define ACCOUNTS_NAME           "org.freedesktop.Accounts"
#define ACCOUNTS_USER_INTERFACE "org.freedesktop.Accounts.User"

enum {
        PROP_0,
        PROP_IS_LOADED
};

enum {
        CHANGED,
        SESSIONS_CHANGED,
        LAST_SIGNAL
};

struct _GdmUser {
        GObject         parent;

        DBusGConnection *connection;
        DBusGProxy      *accounts_proxy;
        DBusGProxy      *object_proxy;
        DBusGProxyCall  *get_all_call;
        char            *object_path;

        uid_t           uid;
        char           *user_name;
        char           *real_name;
        char           *icon_file;
        GList          *sessions;
        gulong          login_frequency;

        guint           is_loaded : 1;
};

struct _GdmUserClass
{
        GObjectClass parent_class;
};

static void gdm_user_finalize     (GObject      *object);
static gboolean check_user_file (const char *filename);

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GdmUser, gdm_user, G_TYPE_OBJECT)

static int
session_compare (const char *a,
                 const char *b)
{
        if (a == NULL) {
                return 1;
        } else if (b == NULL) {
                return -1;
        }

        return strcmp (a, b);
}

void
_gdm_user_add_session (GdmUser    *user,
                       const char *ssid)
{
        GList *li;

        g_return_if_fail (GDM_IS_USER (user));
        g_return_if_fail (ssid != NULL);

        li = g_list_find_custom (user->sessions, ssid, (GCompareFunc)session_compare);
        if (li == NULL) {
                g_debug ("GdmUser: adding session %s", ssid);
                user->sessions = g_list_prepend (user->sessions, g_strdup (ssid));
                g_signal_emit (user, signals[SESSIONS_CHANGED], 0);
        } else {
                g_debug ("GdmUser: session already present: %s", ssid);
        }
}

void
_gdm_user_remove_session (GdmUser    *user,
                          const char *ssid)
{
        GList *li;

        g_return_if_fail (GDM_IS_USER (user));
        g_return_if_fail (ssid != NULL);

        li = g_list_find_custom (user->sessions, ssid, (GCompareFunc)session_compare);
        if (li != NULL) {
                g_debug ("GdmUser: removing session %s", ssid);
                g_free (li->data);
                user->sessions = g_list_delete_link (user->sessions, li);
                g_signal_emit (user, signals[SESSIONS_CHANGED], 0);
        } else {
                g_debug ("GdmUser: session not found: %s", ssid);
        }
}

guint
gdm_user_get_num_sessions (GdmUser    *user)
{
        return g_list_length (user->sessions);
}

static void
gdm_user_set_property (GObject        *object,
                       guint           prop_id,
                       const GValue   *value,
                       GParamSpec     *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gdm_user_get_property (GObject        *object,
                       guint           prop_id,
                       GValue         *value,
                       GParamSpec     *pspec)
{
        GdmUser *user;

        user = GDM_USER (object);

        switch (prop_id) {
        case PROP_IS_LOADED:
                g_value_set_boolean (value, user->is_loaded);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gdm_user_class_init (GdmUserClass *class)
{
        GObjectClass *gobject_class;

        gobject_class = G_OBJECT_CLASS (class);

        gobject_class->finalize = gdm_user_finalize;
        gobject_class->set_property = gdm_user_set_property;
        gobject_class->get_property = gdm_user_get_property;

        g_object_class_install_property (gobject_class,
                                         PROP_IS_LOADED,
                                         g_param_spec_boolean ("is-loaded",
                                                               NULL,
                                                               NULL,
                                                               FALSE,
                                                               G_PARAM_READABLE));

        signals [CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (class),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        signals [SESSIONS_CHANGED] =
                g_signal_new ("sessions-changed",
                              G_TYPE_FROM_CLASS (class),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
}

static void
gdm_user_init (GdmUser *user)
{
        GError *error;

        user->user_name = NULL;
        user->real_name = NULL;
        user->sessions = NULL;

        error = NULL;
        user->connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
        if (user->connection == NULL) {
                g_warning ("Couldn't connect to system bus: %s", error->message);
        }
}

static void
gdm_user_finalize (GObject *object)
{
        GdmUser *user;

        user = GDM_USER (object);

        g_free (user->user_name);
        g_free (user->real_name);
        g_free (user->icon_file);
        g_free (user->object_path);

        if (user->accounts_proxy != NULL) {
                g_object_unref (user->accounts_proxy);
        }

        if (user->object_proxy != NULL) {
                g_object_unref (user->object_proxy);
        }

        if (user->connection != NULL) {
                dbus_g_connection_unref (user->connection);
        }

        if (G_OBJECT_CLASS (gdm_user_parent_class)->finalize)
                (*G_OBJECT_CLASS (gdm_user_parent_class)->finalize) (object);
}

static void
set_is_loaded (GdmUser  *user,
               gboolean  is_loaded)
{
        if (user->is_loaded != is_loaded) {
                user->is_loaded = is_loaded;
                g_object_notify (G_OBJECT (user), "is-loaded");
        }
}

/**
 * _gdm_user_update_from_pwent:
 * @user: the user object to update.
 * @pwent: the user data to use.
 *
 * Updates the properties of @user using the data in @pwent.
 **/
void
_gdm_user_update_from_pwent (GdmUser             *user,
                             const struct passwd *pwent)
{
        gchar *real_name = NULL;
        gboolean changed;

        g_return_if_fail (GDM_IS_USER (user));
        g_return_if_fail (pwent != NULL);
        g_return_if_fail (user->object_path == NULL);

        changed = FALSE;

        /* Display Name */
        if (pwent->pw_gecos && pwent->pw_gecos[0] != '\0') {
                gchar *first_comma = NULL;
                gchar *valid_utf8_name = NULL;

                if (g_utf8_validate (pwent->pw_gecos, -1, NULL)) {
                        valid_utf8_name = pwent->pw_gecos;
                        first_comma = strchr (valid_utf8_name, ',');
                } else {
                        g_warning ("User %s has invalid UTF-8 in GECOS field. "
                                   "It would be a good thing to check /etc/passwd.",
                                   pwent->pw_name ? pwent->pw_name : "");
                }

                if (first_comma) {
                        real_name = g_strndup (valid_utf8_name,
                                               (first_comma - valid_utf8_name));
                } else if (valid_utf8_name) {
                        real_name = g_strdup (valid_utf8_name);
                } else {
                        real_name = NULL;
                }

                if (real_name && real_name[0] == '\0') {
                        g_free (real_name);
                        real_name = NULL;
                }
        } else {
                real_name = NULL;
        }

        if (g_strcmp0 (real_name, user->real_name) != 0) {
                g_free (user->real_name);
                user->real_name = real_name;
                changed = TRUE;
        } else {
                g_free (real_name);
        }

        /* UID */
        if (pwent->pw_uid != user->uid) {
                user->uid = pwent->pw_uid;
                changed = TRUE;
        }

        /* Username */
        if (g_strcmp0 (pwent->pw_name, user->user_name) != 0) {
                g_free (user->icon_file);
                user->icon_file = NULL;
                if (pwent->pw_name != NULL) {
                        gboolean      res;

                        user->icon_file = g_build_filename (GDM_CACHE_DIR, pwent->pw_name, "face", NULL);

                        res = check_user_file (user->icon_file);
                        if (!res) {
                                g_free (user->icon_file);
                                user->icon_file = g_build_filename (GLOBAL_FACEDIR, pwent->pw_name, NULL);
                        }
                }

                g_free (user->user_name);
                user->user_name = g_strdup (pwent->pw_name);
                changed = TRUE;
        }

        if (!user->is_loaded) {
                set_is_loaded (user, TRUE);
        }

        if (changed) {
                g_signal_emit (user, signals[CHANGED], 0);
        }
}

/**
 * _gdm_user_update_login_frequency:
 * @user: the user object to update
 * @login_frequency: the number of times the user has logged in
 *
 * Updates the login frequency of @user
 **/
void
_gdm_user_update_login_frequency (GdmUser *user,
                                  guint64  login_frequency)
{
        g_return_if_fail (GDM_IS_USER (user));

        if (login_frequency == user->login_frequency) {
                return;
        }

        user->login_frequency = login_frequency;
        g_signal_emit (user, signals[CHANGED], 0);
}

/**
 * gdm_user_get_uid:
 * @user: the user object to examine.
 *
 * Retrieves the ID of @user.
 *
 * Returns: (transfer none): a pointer to an array of characters which must not be modified or
 *  freed, or %NULL.
 **/

gulong
gdm_user_get_uid (GdmUser *user)
{
        g_return_val_if_fail (GDM_IS_USER (user), -1);

        return user->uid;
}

/**
 * gdm_user_get_real_name:
 * @user: the user object to examine.
 *
 * Retrieves a displayable name for @user. By default this is the real name
 * of the user, but will fall back to the user name if there is no real name
 * defined.
 *
 * Returns: (transfer none): a pointer to an array of characters which must not be modified or
 *  freed, or %NULL.
 **/
const char *
gdm_user_get_real_name (GdmUser *user)
{
        g_return_val_if_fail (GDM_IS_USER (user), NULL);

        if (user->real_name == NULL ||
            user->real_name[0] == '\0') {
                return user->user_name;
        }

        return user->real_name;
}

/**
 * gdm_user_get_user_name:
 * @user: the user object to examine.
 *
 * Retrieves the login name of @user.
 *
 * Returns: (transfer none): a pointer to an array of characters which must not be modified or
 *  freed, or %NULL.
 **/

const char *
gdm_user_get_user_name (GdmUser *user)
{
        g_return_val_if_fail (GDM_IS_USER (user), NULL);

        return user->user_name;
}

/**
 * gdm_user_get_login_frequency:
 * @user: a #GdmUser
 *
 * Returns the number of times @user has logged in.
 *
 * Returns: the login frequency
 */
gulong
gdm_user_get_login_frequency (GdmUser *user)
{
        g_return_val_if_fail (GDM_IS_USER (user), 0);

        return user->login_frequency;
}

int
gdm_user_collate (GdmUser *user1,
                  GdmUser *user2)
{
        const char *str1;
        const char *str2;
        gulong      num1;
        gulong      num2;
        guint       len1;
        guint       len2;

        g_return_val_if_fail (GDM_IS_USER (user1), 0);
        g_return_val_if_fail (GDM_IS_USER (user2), 0);

        num1 = user1->login_frequency;
        num2 = user2->login_frequency;

        if (num1 > num2) {
                return -1;
        }

        if (num1 < num2) {
                return 1;
        }


        len1 = g_list_length (user1->sessions);
        len2 = g_list_length (user2->sessions);

        if (len1 > len2) {
                return -1;
        }

        if (len1 < len2) {
                return 1;
        }

        /* if login frequency is equal try names */
        if (user1->real_name != NULL) {
                str1 = user1->real_name;
        } else {
                str1 = user1->user_name;
        }

        if (user2->real_name != NULL) {
                str2 = user2->real_name;
        } else {
                str2 = user2->user_name;
        }

        if (str1 == NULL && str2 != NULL) {
                return -1;
        }

        if (str1 != NULL && str2 == NULL) {
                return 1;
        }

        if (str1 == NULL && str2 == NULL) {
                return 0;
        }

        return g_utf8_collate (str1, str2);
}

static gboolean
check_user_file (const char *filename)
{
        gssize      max_file_size = MAX_FILE_SIZE;
        struct stat fileinfo;

        /* Exists/Readable? */
        if (stat (filename, &fileinfo) < 0) {
                return FALSE;
        }

        /* Is a regular file */
        if (G_UNLIKELY (!S_ISREG (fileinfo.st_mode))) {
                return FALSE;
        }

        /* Size is kosher? */
        if (G_UNLIKELY (fileinfo.st_size > max_file_size)) {
                return FALSE;
        }

        return TRUE;
}

static void
rounded_rectangle (cairo_t *cr,
                   gdouble  aspect,
                   gdouble  x,
                   gdouble  y,
                   gdouble  corner_radius,
                   gdouble  width,
                   gdouble  height)
{
        gdouble radius;
        gdouble degrees;

        radius = corner_radius / aspect;
        degrees = G_PI / 180.0;

        cairo_new_sub_path (cr);
        cairo_arc (cr,
                   x + width - radius,
                   y + radius,
                   radius,
                   -90 * degrees,
                   0 * degrees);
        cairo_arc (cr,
                   x + width - radius,
                   y + height - radius,
                   radius,
                   0 * degrees,
                   90 * degrees);
        cairo_arc (cr,
                   x + radius,
                   y + height - radius,
                   radius,
                   90 * degrees,
                   180 * degrees);
        cairo_arc (cr,
                   x + radius,
                   y + radius,
                   radius,
                   180 * degrees,
                   270 * degrees);
        cairo_close_path (cr);
}

static cairo_surface_t *
surface_from_pixbuf (GdkPixbuf *pixbuf)
{
        cairo_surface_t *surface;
        cairo_t         *cr;

        surface = cairo_image_surface_create (gdk_pixbuf_get_has_alpha (pixbuf) ?
                                              CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24,
                                              gdk_pixbuf_get_width (pixbuf),
                                              gdk_pixbuf_get_height (pixbuf));
        cr = cairo_create (surface);
        gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
        cairo_paint (cr);
        cairo_destroy (cr);

        return surface;
}

/**
 * go_cairo_convert_data_to_pixbuf:
 * @src: a pointer to pixel data in cairo format
 * @dst: a pointer to pixel data in pixbuf format
 * @width: image width
 * @height: image height
 * @rowstride: data rowstride
 *
 * Converts the pixel data stored in @src in CAIRO_FORMAT_ARGB32 cairo format
 * to GDK_COLORSPACE_RGB pixbuf format and move them
 * to @dst. If @src == @dst, pixel are converted in place.
 **/

static void
go_cairo_convert_data_to_pixbuf (unsigned char *dst,
                                 unsigned char const *src,
                                 int width,
                                 int height,
                                 int rowstride)
{
        int i,j;
        unsigned int t;
        unsigned char a, b, c;

        g_return_if_fail (dst != NULL);

#define MULT(d,c,a,t) G_STMT_START { t = (a)? c * 255 / a: 0; d = t;} G_STMT_END

        if (src == dst || src == NULL) {
                for (i = 0; i < height; i++) {
                        for (j = 0; j < width; j++) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                                MULT(a, dst[2], dst[3], t);
                                MULT(b, dst[1], dst[3], t);
                                MULT(c, dst[0], dst[3], t);
                                dst[0] = a;
                                dst[1] = b;
                                dst[2] = c;
#else
                                MULT(a, dst[1], dst[0], t);
                                MULT(b, dst[2], dst[0], t);
                                MULT(c, dst[3], dst[0], t);
                                dst[3] = dst[0];
                                dst[0] = a;
                                dst[1] = b;
                                dst[2] = c;
#endif
                                dst += 4;
                        }
                        dst += rowstride - width * 4;
                }
        } else {
                for (i = 0; i < height; i++) {
                        for (j = 0; j < width; j++) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                                MULT(dst[0], src[2], src[3], t);
                                MULT(dst[1], src[1], src[3], t);
                                MULT(dst[2], src[0], src[3], t);
                                dst[3] = src[3];
#else
                                MULT(dst[0], src[1], src[0], t);
                                MULT(dst[1], src[2], src[0], t);
                                MULT(dst[2], src[3], src[0], t);
                                dst[3] = src[0];
#endif
                                src += 4;
                                dst += 4;
                        }
                        src += rowstride - width * 4;
                        dst += rowstride - width * 4;
                }
        }
#undef MULT
}

static void
cairo_to_pixbuf (guint8    *src_data,
                 GdkPixbuf *dst_pixbuf)
{
        unsigned char *src;
        unsigned char *dst;
        guint          w;
        guint          h;
        guint          rowstride;

        w = gdk_pixbuf_get_width (dst_pixbuf);
        h = gdk_pixbuf_get_height (dst_pixbuf);
        rowstride = gdk_pixbuf_get_rowstride (dst_pixbuf);

        dst = gdk_pixbuf_get_pixels (dst_pixbuf);
        src = src_data;

        go_cairo_convert_data_to_pixbuf (dst, src, w, h, rowstride);
}

static GdkPixbuf *
frame_pixbuf (GdkPixbuf *source)
{
        GdkPixbuf       *dest;
        cairo_t         *cr;
        cairo_surface_t *surface;
        guint            w;
        guint            h;
        guint            rowstride;
        int              frame_width;
        double           radius;
        guint8          *data;

        frame_width = 2;

        w = gdk_pixbuf_get_width (source) + frame_width * 2;
        h = gdk_pixbuf_get_height (source) + frame_width * 2;
        radius = w / 10;

        dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                               TRUE,
                               8,
                               w,
                               h);
        rowstride = gdk_pixbuf_get_rowstride (dest);


        data = g_new0 (guint8, h * rowstride);

        surface = cairo_image_surface_create_for_data (data,
                                                       CAIRO_FORMAT_ARGB32,
                                                       w,
                                                       h,
                                                       rowstride);
        cr = cairo_create (surface);
        cairo_surface_destroy (surface);

        /* set up image */
        cairo_rectangle (cr, 0, 0, w, h);
        cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0);
        cairo_fill (cr);

        rounded_rectangle (cr,
                           1.0,
                           frame_width + 0.5,
                           frame_width + 0.5,
                           radius,
                           w - frame_width * 2 - 1,
                           h - frame_width * 2 - 1);
        cairo_set_source_rgba (cr, 0.5, 0.5, 0.5, 0.3);
        cairo_fill_preserve (cr);

        surface = surface_from_pixbuf (source);
        cairo_set_source_surface (cr, surface, frame_width, frame_width);
        cairo_fill (cr);
        cairo_surface_destroy (surface);

        cairo_to_pixbuf (data, dest);

        cairo_destroy (cr);
        g_free (data);

        return dest;
}

/**
 * gdm_user_is_logged_in:
 * @user: a #GdmUser
 *
 * Returns whether or not #GdmUser is currently logged in.
 *
 * Returns: %TRUE or %FALSE
 */
gboolean
gdm_user_is_logged_in (GdmUser *user)
{
        return user->sessions != NULL;
}

/**
 * gdm_user_render_icon:
 * @user: a #GdmUser
 * @icon_size: the size to render the icon at
 *
 * Returns a #GdkPixbuf of the account icon belonging to @user
 * at the pixel size specified by @icon_size.
 *
 * Returns: (transfer full): a #GdkPixbuf
 */
GdkPixbuf *
gdm_user_render_icon (GdmUser   *user,
                      gint       icon_size)
{
        GdkPixbuf    *pixbuf;
        GdkPixbuf    *framed;
        gboolean      res;
        GError       *error;

        g_return_val_if_fail (GDM_IS_USER (user), NULL);
        g_return_val_if_fail (icon_size > 12, NULL);

        pixbuf = NULL;
        if (user->icon_file) {
                res = check_user_file (user->icon_file);
                if (res) {
                        pixbuf = gdk_pixbuf_new_from_file_at_size (user->icon_file,
                                                                   icon_size,
                                                                   icon_size,
                                                                   NULL);
                } else {
                        pixbuf = NULL;
                }
        }

        if (pixbuf != NULL) {
                goto out;
        }

        error = NULL;
        pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),

                                           "avatar-default",
                                           icon_size,
                                           GTK_ICON_LOOKUP_FORCE_SIZE,
                                           &error);
        if (error) {
                g_warning ("%s", error->message);
                g_error_free (error);
        }
 out:

        if (pixbuf != NULL) {
                framed = frame_pixbuf (pixbuf);
                if (framed != NULL) {
                        g_object_unref (pixbuf);
                        pixbuf = framed;
                }
        }

        return pixbuf;
}

/**
 * gdm_user_get_icon_file:
 * @user: a #GdmUser
 *
 * Returns the path to the account icon belonging to @user.
 *
 * Returns: (transfer none): a path to an icon
 */
const char *
gdm_user_get_icon_file (GdmUser *user)
{
        g_return_val_if_fail (GDM_IS_USER (user), NULL);

        return user->icon_file;
}

/**
 * gdm_user_get_object_path:
 * @user: a #GdmUser
 *
 * Returns the user accounts service object path of @user,
 * or %NULL if @user doesn't have an object path associated
 * with it.
 *
 * Returns: (transfer none): the primary ConsoleKit session id of the user
 */
const char *
gdm_user_get_object_path (GdmUser *user)
{
        g_return_val_if_fail (GDM_IS_USER (user), NULL);

        return user->object_path;
}

/**
 * gdm_user_get_primary_session_id:
 * @user: a #GdmUser
 *
 * Returns the primary ConsoleKit session id of @user, or %NULL if @user isn't
 * logged in.
 *
 * Returns: (transfer none): the primary ConsoleKit session id of the user
 */
const char *
gdm_user_get_primary_session_id (GdmUser *user)
{
        if (!gdm_user_is_logged_in (user)) {
                g_debug ("User %s is not logged in, so has no primary session",
                         gdm_user_get_user_name (user));
                return NULL;
        }

        /* FIXME: better way to choose? */
        return user->sessions->data;
}

static void
collect_props (const gchar    *key,
               const GValue   *value,
               GdmUser        *user)
{
        gboolean handled = TRUE;

        if (strcmp (key, "Uid") == 0) {
                user->uid = g_value_get_uint64 (value);
        } else if (strcmp (key, "UserName") == 0) {
                g_free (user->user_name);
                user->user_name = g_value_dup_string (value);
        } else if (strcmp (key, "RealName") == 0) {
                g_free (user->real_name);
                user->real_name = g_value_dup_string (value);
        } else if (strcmp (key, "AccountType") == 0) {
                /* ignore */
        } else if (strcmp (key, "Email") == 0) {
                /* ignore */
        } else if (strcmp (key, "Language") == 0) {
                /* ignore */
        } else if (strcmp (key, "Location") == 0) {
                /* ignore */
        } else if (strcmp (key, "LoginFrequency") == 0) {
                user->login_frequency = g_value_get_uint64 (value);
        } else if (strcmp (key, "IconFile") == 0) {
                gboolean res;

                g_free (user->icon_file);
                user->icon_file = g_value_dup_string (value);

                res = check_user_file (user->icon_file);
                if (!res) {
                        g_free (user->icon_file);
                        user->icon_file = g_build_filename (GLOBAL_FACEDIR, user->user_name, NULL);
                }
        } else if (strcmp (key, "Locked") == 0) {
                /* ignore */
        } else if (strcmp (key, "AutomaticLogin") == 0) {
                /* ignore */
        } else if (strcmp (key, "PasswordMode") == 0) {
                /* ignore */
        } else if (strcmp (key, "PasswordHint") == 0) {
                /* ignore */
        } else if (strcmp (key, "HomeDirectory") == 0) {
                /* ignore */
        } else if (strcmp (key, "Shell") == 0) {
                /* ignore */
        } else {
                handled = FALSE;
        }

        if (!handled) {
                g_debug ("unhandled property %s", key);
        }
}

static void
on_get_all_finished (DBusGProxy     *proxy,
                     DBusGProxyCall *call,
                     GdmUser        *user)
{
        GError      *error;
        GHashTable  *hash_table;
        gboolean     res;

        g_assert (user->get_all_call == call);
        g_assert (user->object_proxy == proxy);

        error = NULL;
        res = dbus_g_proxy_end_call (proxy,
                                     call,
                                     &error,
                                     dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
                                     &hash_table,
                                     G_TYPE_INVALID);
        user->get_all_call = NULL;
        user->object_proxy = NULL;

        if (! res) {
                g_debug ("Error calling GetAll() when retrieving properties for %s: %s",
                         user->object_path, error->message);
                g_error_free (error);
                goto out;
        }
        g_hash_table_foreach (hash_table, (GHFunc) collect_props, user);
        g_hash_table_unref (hash_table);

        if (!user->is_loaded) {
                set_is_loaded (user, TRUE);
        }

        g_signal_emit (user, signals[CHANGED], 0);

out:
        g_object_unref (proxy);
}

static gboolean
update_info (GdmUser *user)
{
        DBusGProxy     *proxy;
        DBusGProxyCall *call;

        proxy = dbus_g_proxy_new_for_name (user->connection,
                                           ACCOUNTS_NAME,
                                           user->object_path,
                                           DBUS_INTERFACE_PROPERTIES);

        call = dbus_g_proxy_begin_call (proxy,
                                        "GetAll",
                                        (DBusGProxyCallNotify)
                                        on_get_all_finished,
                                        user,
                                        NULL,
                                        G_TYPE_STRING,
                                        ACCOUNTS_USER_INTERFACE,
                                        G_TYPE_INVALID);

        if (call == NULL) {
                g_warning ("GdmUser: failed to make GetAll call");
                goto failed;
        }

        user->get_all_call = call;
        user->object_proxy = proxy;
        return TRUE;

failed:
        if (proxy != NULL) {
                g_object_unref (proxy);
        }

        return FALSE;
}

static void
changed_handler (DBusGProxy *proxy,
                 gpointer   *data)
{
        GdmUser *user = GDM_USER (data);

        update_info (user);
}

/**
 * _gdm_user_update_from_object_path:
 * @user: the user object to update.
 * @object_path: the object path of the user to use.
 *
 * Updates the properties of @user from the accounts service via
 * the object path in @object_path.
 **/
void
_gdm_user_update_from_object_path (GdmUser    *user,
                                   const char *object_path)
{
        g_return_if_fail (GDM_IS_USER (user));
        g_return_if_fail (object_path != NULL);
        g_return_if_fail (user->object_path == NULL);

        user->object_path = g_strdup (object_path);

        user->accounts_proxy = dbus_g_proxy_new_for_name (user->connection,
                                                          ACCOUNTS_NAME,
                                                          user->object_path,
                                                          ACCOUNTS_USER_INTERFACE);
        dbus_g_proxy_set_default_timeout (user->accounts_proxy, INT_MAX);
        dbus_g_proxy_add_signal (user->accounts_proxy, "Changed", G_TYPE_INVALID);

        dbus_g_proxy_connect_signal (user->accounts_proxy, "Changed",
                                     G_CALLBACK (changed_handler), user, NULL);

        if (!update_info (user)) {
                g_warning ("Couldn't update info for user with object path %s", object_path);
        }
}

/**
 * gdm_user_is_loaded:
 * @user: a #GdmUser
 *
 * Determines whether or not the user object is loaded and ready to read from.
 * #GdmUserManager:is-loaded property must be %TRUE before calling
 * gdm_user_manager_list_users()
 *
 * Returns: %TRUE or %FALSE
 */
gboolean
gdm_user_is_loaded (GdmUser *user)
{
        return user->is_loaded;
}
