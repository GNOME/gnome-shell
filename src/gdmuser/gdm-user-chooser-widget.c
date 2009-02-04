/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2007 Ray Strode <rstrode@redhat.com>
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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <gconf/gconf-client.h>

#include "gdm-user-manager.h"
#include "gdm-user-chooser-widget.h"


#define KEY_DISABLE_USER_LIST "/apps/gdm/simple-greeter/disable_user_list"

enum {
        USER_NO_DISPLAY              = 1 << 0,
        USER_ACCOUNT_DISABLED        = 1 << 1,
};

#define DEFAULT_USER_ICON "stock_person"

#define GDM_USER_CHOOSER_WIDGET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GDM_TYPE_USER_CHOOSER_WIDGET, GdmUserChooserWidgetPrivate))

#define MAX_ICON_SIZE 128

struct GdmUserChooserWidgetPrivate
{
        GdmUserManager *manager;
        GtkIconTheme   *icon_theme;

        GdkPixbuf      *logged_in_pixbuf;
        GdkPixbuf      *stock_person_pixbuf;

        guint           loaded : 1;
        guint           show_user_other : 1;
        guint           show_user_guest : 1;
        guint           show_user_auto : 1;
        guint           show_normal_users : 1;

        guint           load_idle_id;
};

enum {
        PROP_0,
        PROP_SHOW_USER_GUEST,
        PROP_SHOW_USER_AUTO,
        PROP_SHOW_USER_OTHER,
};

static void     gdm_user_chooser_widget_class_init  (GdmUserChooserWidgetClass *klass);
static void     gdm_user_chooser_widget_init        (GdmUserChooserWidget      *user_chooser_widget);
static void     gdm_user_chooser_widget_finalize    (GObject                   *object);

G_DEFINE_TYPE (GdmUserChooserWidget, gdm_user_chooser_widget, GDM_TYPE_CHOOSER_WIDGET)

static int
get_font_height_for_widget (GtkWidget *widget)
{
        PangoFontMetrics *metrics;
        PangoContext     *context;
        int               ascent;
        int               descent;
        int               height;

        gtk_widget_ensure_style (widget);
        context = gtk_widget_get_pango_context (widget);
        metrics = pango_context_get_metrics (context,
                                             widget->style->font_desc,
                                             pango_context_get_language (context));

        ascent = pango_font_metrics_get_ascent (metrics);
        descent = pango_font_metrics_get_descent (metrics);
        height = PANGO_PIXELS (ascent + descent);
        pango_font_metrics_unref (metrics);
        return height;
}

static int
get_icon_height_for_widget (GtkWidget *widget)
{
        int font_height;
        int height;

        font_height = get_font_height_for_widget (widget);
        height = 3 * font_height;
        if (height > MAX_ICON_SIZE) {
                height = MAX_ICON_SIZE;
        }

        g_debug ("GdmUserChooserWidget: font height %d; using icon size %d", font_height, height);

        return height;
}

static void
add_user_other (GdmUserChooserWidget *widget)
{
        gdm_chooser_widget_add_item (GDM_CHOOSER_WIDGET (widget),
                                     GDM_USER_CHOOSER_USER_OTHER,
                                     NULL,
                                     _("Other..."),
                                     _("Choose a different account"),
                                     0,
                                     FALSE,
                                     TRUE);
}

static void
add_user_guest (GdmUserChooserWidget *widget)
{
        gdm_chooser_widget_add_item (GDM_CHOOSER_WIDGET (widget),
                                     GDM_USER_CHOOSER_USER_GUEST,
                                     widget->priv->stock_person_pixbuf,
                                     _("Guest"),
                                     _("Login as a temporary guest"),
                                     0,
                                     FALSE,
                                     TRUE);
}

static void
add_user_auto (GdmUserChooserWidget *widget)
{
        gdm_chooser_widget_add_item (GDM_CHOOSER_WIDGET (widget),
                                     GDM_USER_CHOOSER_USER_AUTO,
                                     NULL,
                                     _("Automatic Login"),
                                     _("Automatically login to the system after selecting options"),
                                     0,
                                     FALSE,
                                     TRUE);
}

static void
remove_user_other (GdmUserChooserWidget *widget)
{
        gdm_chooser_widget_remove_item (GDM_CHOOSER_WIDGET (widget),
                                        GDM_USER_CHOOSER_USER_OTHER);
}

static void
remove_user_guest (GdmUserChooserWidget *widget)
{
        gdm_chooser_widget_remove_item (GDM_CHOOSER_WIDGET (widget),
                                        GDM_USER_CHOOSER_USER_GUEST);
}

static void
remove_user_auto (GdmUserChooserWidget *widget)
{
        gdm_chooser_widget_remove_item (GDM_CHOOSER_WIDGET (widget),
                                        GDM_USER_CHOOSER_USER_AUTO);
}

void
gdm_user_chooser_widget_set_show_user_other (GdmUserChooserWidget *widget,
                                             gboolean              show_user)
{
        g_return_if_fail (GDM_IS_USER_CHOOSER_WIDGET (widget));

        if (widget->priv->show_user_other != show_user) {
                widget->priv->show_user_other = show_user;
                if (show_user) {
                        add_user_other (widget);
                } else {
                        remove_user_other (widget);
                }
        }
}

void
gdm_user_chooser_widget_set_show_user_guest (GdmUserChooserWidget *widget,
                                             gboolean              show_user)
{
        g_return_if_fail (GDM_IS_USER_CHOOSER_WIDGET (widget));

        if (widget->priv->show_user_guest != show_user) {
                widget->priv->show_user_guest = show_user;
                if (show_user) {
                        add_user_guest (widget);
                } else {
                        remove_user_guest (widget);
                }
        }
}

void
gdm_user_chooser_widget_set_show_user_auto (GdmUserChooserWidget *widget,
                                            gboolean              show_user)
{
        g_return_if_fail (GDM_IS_USER_CHOOSER_WIDGET (widget));

        if (widget->priv->show_user_auto != show_user) {
                widget->priv->show_user_auto = show_user;
                if (show_user) {
                        add_user_auto (widget);
                } else {
                        remove_user_auto (widget);
                }
        }
}

char *
gdm_user_chooser_widget_get_chosen_user_name (GdmUserChooserWidget *widget)
{
        g_return_val_if_fail (GDM_IS_USER_CHOOSER_WIDGET (widget), NULL);

        return gdm_chooser_widget_get_active_item (GDM_CHOOSER_WIDGET (widget));
}

void
gdm_user_chooser_widget_set_chosen_user_name (GdmUserChooserWidget *widget,
                                              const char           *name)
{
        g_return_if_fail (GDM_IS_USER_CHOOSER_WIDGET (widget));

        gdm_chooser_widget_set_active_item (GDM_CHOOSER_WIDGET (widget), name);
}

void
gdm_user_chooser_widget_set_show_only_chosen (GdmUserChooserWidget *widget,
                                              gboolean              show_only) {
        g_return_if_fail (GDM_IS_USER_CHOOSER_WIDGET (widget));

        gdm_chooser_widget_set_hide_inactive_items (GDM_CHOOSER_WIDGET (widget),
                                                    show_only);

}
static void
gdm_user_chooser_widget_set_property (GObject        *object,
                                      guint           prop_id,
                                      const GValue   *value,
                                      GParamSpec     *pspec)
{
        GdmUserChooserWidget *self;

        self = GDM_USER_CHOOSER_WIDGET (object);

        switch (prop_id) {
        case PROP_SHOW_USER_AUTO:
                gdm_user_chooser_widget_set_show_user_auto (self, g_value_get_boolean (value));
                break;
        case PROP_SHOW_USER_GUEST:
                gdm_user_chooser_widget_set_show_user_guest (self, g_value_get_boolean (value));
                break;
        case PROP_SHOW_USER_OTHER:
                gdm_user_chooser_widget_set_show_user_other (self, g_value_get_boolean (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gdm_user_chooser_widget_get_property (GObject        *object,
                                      guint           prop_id,
                                      GValue         *value,
                                      GParamSpec     *pspec)
{
        GdmUserChooserWidget *self;

        self = GDM_USER_CHOOSER_WIDGET (object);

        switch (prop_id) {
        case PROP_SHOW_USER_AUTO:
                g_value_set_boolean (value, self->priv->show_user_auto);
                break;
        case PROP_SHOW_USER_GUEST:
                g_value_set_boolean (value, self->priv->show_user_guest);
                break;
        case PROP_SHOW_USER_OTHER:
                g_value_set_boolean (value, self->priv->show_user_other);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static gboolean
is_user_list_disabled (GdmUserChooserWidget *widget)
{
        GConfClient *client;
        GError      *error;
        gboolean     result;

        client = gconf_client_get_default ();
        error = NULL;
        result = gconf_client_get_bool (client, KEY_DISABLE_USER_LIST, &error);
        if (error != NULL) {
                g_debug ("GdmUserChooserWidget: unable to get disable-user-list configuration: %s", error->message);
                g_error_free (error);
        }
        g_object_unref (client);

        return result;
}

static void
add_user (GdmUserChooserWidget *widget,
          GdmUser              *user)
{
        GdkPixbuf    *pixbuf;
        char         *tooltip;
        gboolean      is_logged_in;
        int           size;

        if (!widget->priv->show_normal_users) {
                return;
        }

        size = get_icon_height_for_widget (widget);
        pixbuf = gdm_user_render_icon (user, size);
        if (pixbuf == NULL && widget->priv->stock_person_pixbuf != NULL) {
                pixbuf = g_object_ref (widget->priv->stock_person_pixbuf);
        }

        tooltip = g_strdup_printf (_("Log in as %s"),
                                   gdm_user_get_user_name (user));

        is_logged_in = gdm_user_get_num_sessions (user) > 0;

        g_debug ("GdmUserChooserWidget: User added name:%s logged-in:%d pixbuf:%p",
                 gdm_user_get_user_name (user),
                 is_logged_in,
                 pixbuf);

        gdm_chooser_widget_add_item (GDM_CHOOSER_WIDGET (widget),
                                     gdm_user_get_user_name (user),
                                     pixbuf,
                                     gdm_user_get_real_name (user),
                                     tooltip,
                                     gdm_user_get_login_frequency (user),
                                     is_logged_in,
                                     FALSE);
        g_free (tooltip);

        if (pixbuf != NULL) {
                g_object_unref (pixbuf);
        }
}

static void
on_user_added (GdmUserManager       *manager,
               GdmUser              *user,
               GdmUserChooserWidget *widget)
{
        /* wait for all users to be loaded */
        if (! widget->priv->loaded) {
                return;
        }
        add_user (widget, user);
}

static void
on_user_removed (GdmUserManager       *manager,
                 GdmUser              *user,
                 GdmUserChooserWidget *widget)
{
        const char *user_name;

        g_debug ("GdmUserChooserWidget: User removed: %s", gdm_user_get_user_name (user));
        /* wait for all users to be loaded */
        if (! widget->priv->loaded) {
                return;
        }

        user_name = gdm_user_get_user_name (user);

        gdm_chooser_widget_remove_item (GDM_CHOOSER_WIDGET (widget),
                                        user_name);
}

static void
on_user_is_logged_in_changed (GdmUserManager       *manager,
                              GdmUser              *user,
                              GdmUserChooserWidget *widget)
{
        const char *user_name;
        gboolean    is_logged_in;

        g_debug ("GdmUserChooserWidget: User logged in changed: %s", gdm_user_get_user_name (user));

        user_name = gdm_user_get_user_name (user);
        is_logged_in = gdm_user_get_num_sessions (user) > 0;

        gdm_chooser_widget_set_item_in_use (GDM_CHOOSER_WIDGET (widget),
                                            user_name,
                                            is_logged_in);
}

static void
on_user_login_frequency_changed (GdmUserManager       *manager,
                                 GdmUser              *user,
                                 GdmUserChooserWidget *widget)
{
        const char *user_name;
        gulong      freq;

        g_debug ("GdmUserChooserWidget: User login frequency changed: %s", gdm_user_get_user_name (user));

        user_name = gdm_user_get_user_name (user);
        freq = gdm_user_get_login_frequency (user);

        gdm_chooser_widget_set_item_priority (GDM_CHOOSER_WIDGET (widget),
                                              user_name,
                                              freq);
}

static void
on_users_loaded (GdmUserManager       *manager,
                 GdmUserChooserWidget *widget)
{
        GSList *users;

        widget->priv->loaded = TRUE;

        g_debug ("GdmUserChooserWidget: Users loaded");

        users = gdm_user_manager_list_users (manager);
        while (users != NULL) {
                add_user (widget, users->data);
                users = g_slist_delete_link (users, users);
        }

        gtk_widget_grab_focus (GTK_WIDGET (widget));

        gdm_chooser_widget_loaded (GDM_CHOOSER_WIDGET (widget));
}

static gboolean
load_users (GdmUserChooserWidget *widget)
{

        if (widget->priv->show_normal_users) {
                widget->priv->manager = gdm_user_manager_ref_default ();
                g_signal_connect (widget->priv->manager,
                                  "user-added",
                                  G_CALLBACK (on_user_added),
                                  widget);
                g_signal_connect (widget->priv->manager,
                                  "user-removed",
                                  G_CALLBACK (on_user_removed),
                                  widget);
                g_signal_connect (widget->priv->manager,
                                  "users-loaded",
                                  G_CALLBACK (on_users_loaded),
                                  widget);
                g_signal_connect (widget->priv->manager,
                                  "user-is-logged-in-changed",
                                  G_CALLBACK (on_user_is_logged_in_changed),
                                  widget);
                g_signal_connect (widget->priv->manager,
                                  "user-login-frequency-changed",
                                  G_CALLBACK (on_user_login_frequency_changed),
                                  widget);
        } else {
                gdm_chooser_widget_loaded (GDM_CHOOSER_WIDGET (widget));
        }

        widget->priv->load_idle_id = 0;

        return FALSE;
}

static GObject *
gdm_user_chooser_widget_constructor (GType                  type,
                                     guint                  n_construct_properties,
                                     GObjectConstructParam *construct_properties)
{
        GdmUserChooserWidget      *widget;

        widget = GDM_USER_CHOOSER_WIDGET (G_OBJECT_CLASS (gdm_user_chooser_widget_parent_class)->constructor (type,
                                                                                                              n_construct_properties,
                                                                                                              construct_properties));

        widget->priv->show_normal_users = !is_user_list_disabled (widget);

        widget->priv->load_idle_id = g_idle_add ((GSourceFunc)load_users, widget);

        return G_OBJECT (widget);
}

static void
gdm_user_chooser_widget_dispose (GObject *object)
{
        GdmUserChooserWidget *widget;

        widget = GDM_USER_CHOOSER_WIDGET (object);

        G_OBJECT_CLASS (gdm_user_chooser_widget_parent_class)->dispose (object);

        if (widget->priv->load_idle_id > 0) {
                g_source_remove (widget->priv->load_idle_id);
                widget->priv->load_idle_id = 0;
        }

        if (widget->priv->logged_in_pixbuf != NULL) {
                g_object_unref (widget->priv->logged_in_pixbuf);
                widget->priv->logged_in_pixbuf = NULL;
        }

        if (widget->priv->stock_person_pixbuf != NULL) {
                g_object_unref (widget->priv->stock_person_pixbuf);
                widget->priv->stock_person_pixbuf = NULL;
        }
}

static void
gdm_user_chooser_widget_class_init (GdmUserChooserWidgetClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = gdm_user_chooser_widget_get_property;
        object_class->set_property = gdm_user_chooser_widget_set_property;
        object_class->constructor = gdm_user_chooser_widget_constructor;
        object_class->dispose = gdm_user_chooser_widget_dispose;
        object_class->finalize = gdm_user_chooser_widget_finalize;


        g_object_class_install_property (object_class,
                                         PROP_SHOW_USER_AUTO,
                                         g_param_spec_boolean ("show-user-auto",
                                                               "show user auto",
                                                               "show user auto",
                                                               FALSE,
                                                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
        g_object_class_install_property (object_class,
                                         PROP_SHOW_USER_GUEST,
                                         g_param_spec_boolean ("show-user-guest",
                                                               "show user guest",
                                                               "show user guest",
                                                               FALSE,
                                                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
        g_object_class_install_property (object_class,
                                         PROP_SHOW_USER_OTHER,
                                         g_param_spec_boolean ("show-user-other",
                                                               "show user other",
                                                               "show user other",
                                                               TRUE,
                                                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

        g_type_class_add_private (klass, sizeof (GdmUserChooserWidgetPrivate));
}

static GdkPixbuf *
get_stock_person_pixbuf (GdmUserChooserWidget *widget)
{
        GdkPixbuf *pixbuf;
        int        size;

        size = get_icon_height_for_widget (widget);

        pixbuf = gtk_icon_theme_load_icon (widget->priv->icon_theme,
                                           DEFAULT_USER_ICON,
                                           size,
                                           0,
                                           NULL);

        return pixbuf;
}

static GdkPixbuf *
get_logged_in_pixbuf (GdmUserChooserWidget *widget)
{
        GdkPixbuf *pixbuf;
        int        size;

        size = get_icon_height_for_widget (widget);

        pixbuf = gtk_icon_theme_load_icon (widget->priv->icon_theme,
                                           "emblem-default",
                                           size / 3,
                                           0,
                                           NULL);

        return pixbuf;
}

typedef struct {
        GdkPixbuf *old_icon;
        GdkPixbuf *new_icon;
} IconUpdateData;

static gboolean
update_icons (GdmChooserWidget *widget,
              const char       *id,
              GdkPixbuf       **image,
              char            **name,
              char            **comment,
              gulong           *priority,
              gboolean         *is_in_use,
              gboolean         *is_separate,
              IconUpdateData   *data)
{
        if (data->old_icon == *image) {
                *image = data->new_icon;
                return TRUE;
        }

        return FALSE;
}

static void
load_icons (GdmUserChooserWidget *widget)
{
        GdkPixbuf     *old_pixbuf;
        IconUpdateData data;

        if (widget->priv->logged_in_pixbuf != NULL) {
                g_object_unref (widget->priv->logged_in_pixbuf);
        }
        widget->priv->logged_in_pixbuf = get_logged_in_pixbuf (widget);

        old_pixbuf = widget->priv->stock_person_pixbuf;
        widget->priv->stock_person_pixbuf = get_stock_person_pixbuf (widget);
        /* update the icons in the model */
        data.old_icon = old_pixbuf;
        data.new_icon = widget->priv->stock_person_pixbuf;
        gdm_chooser_widget_update_foreach_item (GDM_CHOOSER_WIDGET (widget),
                                                (GdmChooserUpdateForeachFunc)update_icons,
                                                &data);
        if (old_pixbuf != NULL) {
                g_object_unref (old_pixbuf);
        }
}

static void
on_icon_theme_changed (GtkIconTheme         *icon_theme,
                       GdmUserChooserWidget *widget)
{
        g_debug ("GdmUserChooserWidget: icon theme changed");
        load_icons (widget);
}

static void
setup_icons (GdmUserChooserWidget *widget)
{
        if (gtk_widget_has_screen (GTK_WIDGET (widget))) {
                widget->priv->icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (widget)));
        } else {
                widget->priv->icon_theme = gtk_icon_theme_get_default ();
        }

        if (widget->priv->icon_theme != NULL) {
                g_signal_connect (widget->priv->icon_theme,
                                  "changed",
                                  G_CALLBACK (on_icon_theme_changed),
                                  widget);
        }

        load_icons (widget);
}

static void
gdm_user_chooser_widget_init (GdmUserChooserWidget *widget)
{
        widget->priv = GDM_USER_CHOOSER_WIDGET_GET_PRIVATE (widget);

        gdm_chooser_widget_set_separator_position (GDM_CHOOSER_WIDGET (widget),
                                                   GDM_CHOOSER_WIDGET_POSITION_BOTTOM);
        gdm_chooser_widget_set_in_use_message (GDM_CHOOSER_WIDGET (widget),
                                               _("Currently logged in"));

        setup_icons (widget);
}

static void
gdm_user_chooser_widget_finalize (GObject *object)
{
        GdmUserChooserWidget *widget;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GDM_IS_USER_CHOOSER_WIDGET (object));

        widget = GDM_USER_CHOOSER_WIDGET (object);

        g_return_if_fail (widget->priv != NULL);

        G_OBJECT_CLASS (gdm_user_chooser_widget_parent_class)->finalize (object);
}

GtkWidget *
gdm_user_chooser_widget_new (void)
{
        GObject *object;

        object = g_object_new (GDM_TYPE_USER_CHOOSER_WIDGET,
                               NULL);

        return GTK_WIDGET (object);
}
