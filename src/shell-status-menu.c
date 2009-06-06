/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/* Adapted from gdm/gui/user-switch-applet/applet.c */
/*
 *
 * Copyright (C) 2004-2005 James M. Cape <jcape@ignore-your.tv>.
 * Copyright (C) 2008,2009      Red Hat, Inc.
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

#include "shell-status-menu.h"

#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <dbus/dbus-glib.h>

#define GDMUSER_I_KNOW_THIS_IS_UNSTABLE
#include <gdmuser/gdm-user-manager.h>

#include "shell-global.h"

#define LOCKDOWN_DIR    "/desktop/gnome/lockdown"
#define LOCKDOWN_KEY    LOCKDOWN_DIR "/disable_user_switching"

struct _ShellStatusMenuPrivate {
  GConfClient    *client;
  GdmUserManager *manager;
  GdmUser        *user;

  ClutterTexture *user_icon;
  BigBox         *name_box;
  ClutterText    *name;

  GtkWidget      *menu;
  GtkWidget      *account_item;
  GtkWidget      *control_panel_item;
  GtkWidget      *lock_screen_item;
  GtkWidget      *login_screen_item;
  GtkWidget      *quit_session_item;

  guint           client_notify_lockdown_id;

  guint           current_status_state;

  guint           user_icon_changed_id;
  guint           user_notify_id;

  gboolean        has_other_users;

  GtkIconSize     icon_size;
  guint           pixel_size;
};

enum {
  PROP_0
};

G_DEFINE_TYPE(ShellStatusMenu, shell_status_menu, BIG_TYPE_BOX);

/* Signals */
enum
{
  DEACTIVATED,
  LAST_SIGNAL
};

static guint shell_status_menu_signals [LAST_SIGNAL] = { 0 };

static void
reset_icon (ShellStatusMenu *status)
{
  ShellStatusMenuPrivate *priv = status->priv;
  GdkPixbuf *pixbuf;

  if (priv->user == NULL)
    return;

  if (priv->user_icon != NULL)
    {
      pixbuf = gdm_user_render_icon (priv->user, 24);

      if (pixbuf == NULL)
        return;

      shell_clutter_texture_set_from_pixbuf (priv->user_icon, pixbuf);

      g_object_unref (pixbuf);
    }
}

static void
update_name_text (ShellStatusMenu *status)
{
  ShellStatusMenuPrivate *priv = status->priv;
  char      *markup;

  markup = g_markup_printf_escaped("<b>%s</b>",
                                   gdm_user_get_real_name (GDM_USER (priv->user)));
  clutter_text_set_markup (priv->name, markup);
  g_free (markup);
}

static void
on_user_icon_changed (GdmUser         *user,
                      ShellStatusMenu *status)
{
  g_debug ("User icon changed");
  reset_icon (status);
}

static void
user_notify_display_name_cb (GObject       *object,
                             GParamSpec    *pspec,
                             ShellStatusMenu *status)
{
  update_name_text (status);
}

static void
setup_current_user (ShellStatusMenu *status)
{
  ShellStatusMenuPrivate *priv = status->priv;
  const char *name;

  priv->user = gdm_user_manager_get_user_by_uid (priv->manager, getuid ());
  if (priv->user != NULL)
    {
      g_object_ref (priv->user);
      name = gdm_user_get_real_name (priv->user);
    }
  else
    {
      name = _("Unknown");
    }

  update_name_text (status);

  if (priv->user != NULL)
    {
      reset_icon (status);

      priv->user_icon_changed_id =
        g_signal_connect (priv->user,
                          "icon-changed",
                          G_CALLBACK (on_user_icon_changed),
                          status);
      priv->user_notify_id =
        g_signal_connect (priv->user,
                          "notify::display-name",
                          G_CALLBACK (user_notify_display_name_cb),
                          status);
    }
}

static void
maybe_lock_screen (ShellStatusMenu *status)
{
  char *args[3];
  GError *err;
  GdkScreen *screen;
  gboolean use_gscreensaver = TRUE;
  gboolean res;

  g_debug ("Attempting to lock screen");

  args[0] = g_find_program_in_path ("gnome-screensaver-command");
  if (args[0] == NULL)
    {
      args[0] = g_find_program_in_path ("xscreensaver-command");
      use_gscreensaver = FALSE;
    }

  if (args[0] == NULL)
    return;

  if (use_gscreensaver)
    args[1] = "--lock";
  else
    args[1] = "-lock";
  args[2] = NULL;

  screen = gdk_screen_get_default ();

  err = NULL;
  res = gdk_spawn_on_screen (screen, g_get_home_dir (), args, NULL, 0, NULL,
      NULL, NULL, &err);
  if (!res)
    {
      g_warning (_("Can't lock screen: %s"), err->message);
      g_error_free (err);
    }

  if (use_gscreensaver)
    args[1] = "--throttle";
  else
    args[1] = "-throttle";

  err = NULL;
  res = gdk_spawn_on_screen (screen, g_get_home_dir (), args, NULL,
      (G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_STDOUT_TO_DEV_NULL), NULL, NULL,
      NULL, &err);
  if (!res)
    {
      g_warning (_("Can't temporarily set screensaver to blank screen: %s"),
          err->message);
      g_error_free (err);
    }

  g_free (args[0]);
}

static void
on_lock_screen_activate (GtkMenuItem   *item,
                         ShellStatusMenu *status)
{
  maybe_lock_screen (status);
}

static void
do_switch (ShellStatusMenu *status,
           GdmUser       *user)
{
  ShellStatusMenuPrivate *priv = status->priv;
  guint num_sessions;

  g_debug ("Do user switch");

  if (user == NULL)
    {
      gdm_user_manager_goto_login_session (priv->manager);
      goto out;
    }

  num_sessions = gdm_user_get_num_sessions (user);
  if (num_sessions > 0)
    gdm_user_manager_activate_user_session (priv->manager, user);
  else
    gdm_user_manager_goto_login_session (priv->manager);
out:
  maybe_lock_screen (status);
}

static void
on_login_screen_activate (GtkMenuItem   *item,
                          ShellStatusMenu *status)
{
  GdmUser *user;

  user = NULL;

  do_switch (status, user);
}

static void
spawn_external (ShellStatusMenu *status, const char *program)
{
  char *args[2];
  GError *error;
  GdkScreen *screen;
  gboolean res;

  args[0] = g_find_program_in_path (program);
  if (args[0] == NULL)
    return;
  args[1] = NULL;

  screen = gdk_screen_get_default ();

  error = NULL;
  res = gdk_spawn_on_screen (screen, g_get_home_dir (), args, NULL, 0, NULL,
      NULL, NULL, &error);
  if (!res)
    {
      g_warning ("Failed to exec %s: %s", program, error->message);
      g_clear_error (&error);
    }

  g_free (args[0]);

}

static void
on_control_panel_activate (GtkMenuItem   *item,
                           ShellStatusMenu *status)
{
  spawn_external (status, "gnome-control-center");
}

static void
on_account_activate (GtkMenuItem   *item,
                     ShellStatusMenu *status)
{
  spawn_external (status, "gnome-about-me");
}


static void
on_quit_session_activate (GtkMenuItem   *item,
                          ShellStatusMenu *status)
{
  char      *args[3];
  GError    *error;
  GdkScreen *screen;
  gboolean   res;

  args[0] = g_find_program_in_path ("gnome-session-save");
  if (args[0] == NULL)
    return;

  args[1] = "--logout-dialog";
  args[2] = NULL;

  screen = gdk_screen_get_default ();

  error = NULL;
  res = gdk_spawn_on_screen (screen, g_get_home_dir (), args, NULL, 0, NULL,
      NULL, NULL, &error);
  if (!res)
    {
      g_warning (_("Can't logout: %s"), error->message);
      g_error_free (error);
    }

  g_free (args[0]);
}

static void
update_switch_user (ShellStatusMenu *status)
{
  ShellStatusMenuPrivate *priv = status->priv;
  GSList *users;

  users = gdm_user_manager_list_users (priv->manager);
  priv->has_other_users = FALSE;
  if (users != NULL)
    priv->has_other_users = g_slist_length (users) > 1;
  g_slist_free (users);

  if (priv->has_other_users)
    gtk_widget_show (priv->login_screen_item);
  else
    gtk_widget_hide (priv->login_screen_item);
}

static void
on_manager_user_added (GdmUserManager *manager,
                       GdmUser        *user,
                       ShellStatusMenu *status)
{
  update_switch_user (status);
}

static void
on_manager_user_removed (GdmUserManager *manager,
                         GdmUser        *user,
                         ShellStatusMenu *status)
{
  update_switch_user (status);
}

static void
on_manager_users_loaded (GdmUserManager *manager,
                         ShellStatusMenu *status)
{
  update_switch_user (status);
}

static void
menu_style_set_cb (GtkWidget *menu, GtkStyle *old_style,
                   ShellStatusMenu *status)
{
  ShellStatusMenuPrivate *priv = status->priv;
  GtkSettings *settings;
  int width;
  int height;

  priv->icon_size = gtk_icon_size_from_name ("panel-menu");
  if (priv->icon_size == GTK_ICON_SIZE_INVALID)
    priv->icon_size = gtk_icon_size_register ("panel-menu", 24, 24);

  if (gtk_widget_has_screen (menu))
    settings = gtk_settings_get_for_screen (gtk_widget_get_screen (menu));
  else
    settings = gtk_settings_get_default ();

  if (!gtk_icon_size_lookup_for_settings (settings, priv->icon_size, &width,
      &height))
    priv->pixel_size = -1;
  else
    priv->pixel_size = MAX(width, height);
}

static void
menuitem_style_set_cb (GtkWidget     *menuitem,
                       GtkStyle      *old_style,
                       ShellStatusMenu *status)
{
  GtkWidget *image;
  const char *icon_name;
  ShellStatusMenuPrivate *priv = status->priv;

  if (menuitem == priv->login_screen_item)
    icon_name = "system-users";
  else if (menuitem == priv->lock_screen_item)
    icon_name = "system-lock-screen";
  else if (menuitem == priv->quit_session_item)
    icon_name = "system-log-out";
  else if (menuitem == priv->account_item)
    icon_name = "user-info";
  else if (menuitem == priv->control_panel_item)
    icon_name = "preferences-desktop";
  else
    icon_name = GTK_STOCK_MISSING_IMAGE;

  image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (menuitem));
  gtk_image_set_pixel_size (GTK_IMAGE (image), priv->pixel_size);
  gtk_image_set_from_icon_name (GTK_IMAGE (image), icon_name, priv->icon_size);
}

static void
on_deactivate (GtkMenuShell *menushell, gpointer user_data)
{
  ShellStatusMenu *status = SHELL_STATUS_MENU (user_data);
  g_signal_emit (G_OBJECT (status), shell_status_menu_signals[DEACTIVATED], 0);
}

static void
create_sub_menu (ShellStatusMenu *status)
{
  ShellStatusMenuPrivate *priv = status->priv;
  GtkWidget *item;

  priv->menu = gtk_menu_new ();
  g_signal_connect (priv->menu, "style-set", G_CALLBACK (menu_style_set_cb),
      status);

  g_signal_connect (priv->manager, "users-loaded",
      G_CALLBACK (on_manager_users_loaded), status);
  g_signal_connect (priv->manager, "user-added",
      G_CALLBACK (on_manager_user_added), status);
  g_signal_connect (priv->manager, "user-removed",
      G_CALLBACK (on_manager_user_removed), status);

  priv->account_item = gtk_image_menu_item_new_with_label (_("Account Information..."));
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (priv->account_item),
      gtk_image_new ());
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), priv->account_item);
  g_signal_connect (priv->account_item, "style-set",
      G_CALLBACK (menuitem_style_set_cb), status);
  g_signal_connect (priv->account_item, "activate",
      G_CALLBACK (on_account_activate), status);
  gtk_widget_show (priv->account_item);

  priv->control_panel_item = gtk_image_menu_item_new_with_label (_("System Preferences..."));
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (priv->control_panel_item),
      gtk_image_new ());
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), priv->control_panel_item);
  g_signal_connect (priv->control_panel_item, "style-set",
      G_CALLBACK (menuitem_style_set_cb), status);
  g_signal_connect (priv->control_panel_item, "activate",
      G_CALLBACK (on_control_panel_activate), status);
  gtk_widget_show (priv->control_panel_item);

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), item);
  gtk_widget_show (item);

  priv->lock_screen_item
      = gtk_image_menu_item_new_with_label (_("Lock Screen"));
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (priv->lock_screen_item),
      gtk_image_new ());
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), priv->lock_screen_item);
  g_signal_connect (priv->lock_screen_item, "style-set",
      G_CALLBACK (menuitem_style_set_cb), status);
  g_signal_connect (priv->lock_screen_item, "activate",
      G_CALLBACK (on_lock_screen_activate), status);
  gtk_widget_show (priv->lock_screen_item);

  priv->login_screen_item = gtk_image_menu_item_new_with_label (_("Switch User"));
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (priv->login_screen_item),
      gtk_image_new ());
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), priv->login_screen_item);
  g_signal_connect (priv->login_screen_item, "style-set",
      G_CALLBACK (menuitem_style_set_cb), status);
  g_signal_connect (priv->login_screen_item, "activate",
      G_CALLBACK (on_login_screen_activate), status);
  /* Only show switch user if there are other users */

  priv->quit_session_item = gtk_image_menu_item_new_with_label (_("Quit..."));
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (priv->quit_session_item),
      gtk_image_new ());
  gtk_menu_shell_append (GTK_MENU_SHELL (priv->menu), priv->quit_session_item);
  g_signal_connect (priv->quit_session_item, "style-set",
      G_CALLBACK (menuitem_style_set_cb), status);
  g_signal_connect (priv->quit_session_item, "activate",
      G_CALLBACK (on_quit_session_activate), status);
  gtk_widget_show (priv->quit_session_item);

  g_signal_connect (G_OBJECT (priv->menu), "deactivate",
      G_CALLBACK (on_deactivate), status);
}

static void
shell_status_menu_init (ShellStatusMenu *status)
{
  ShellStatusMenuPrivate *priv;

  status->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE (status, SHELL_TYPE_STATUS_MENU,
                                                     ShellStatusMenuPrivate);

  g_object_set (G_OBJECT (status),
                "orientation", BIG_BOX_ORIENTATION_HORIZONTAL,
                NULL);
  priv->client = gconf_client_get_default ();

  priv->user_icon = CLUTTER_TEXTURE (clutter_texture_new ());
  big_box_append (BIG_BOX (status), CLUTTER_ACTOR (status->priv->user_icon), 0);

  priv->name_box = BIG_BOX (big_box_new (BIG_BOX_ORIENTATION_VERTICAL));
  g_object_set (G_OBJECT (priv->name_box), "y-align", BIG_BOX_ALIGNMENT_CENTER, NULL);
  big_box_append (BIG_BOX (status), CLUTTER_ACTOR (priv->name_box), BIG_BOX_PACK_EXPAND);
  priv->name = CLUTTER_TEXT (clutter_text_new ());
  big_box_append (BIG_BOX (priv->name_box), CLUTTER_ACTOR (priv->name), BIG_BOX_PACK_EXPAND);

  priv->manager = gdm_user_manager_ref_default ();
  setup_current_user (status);

  create_sub_menu (status);
}

static void
shell_status_menu_finalize (GObject *object)
{
  ShellStatusMenu *status = SHELL_STATUS_MENU (object);
  ShellStatusMenuPrivate *priv = status->priv;

  gconf_client_notify_remove (priv->client, priv->client_notify_lockdown_id);

  g_signal_handler_disconnect (priv->user, priv->user_notify_id);
  g_signal_handler_disconnect (priv->user, priv->user_icon_changed_id);

  if (priv->user != NULL) {
    g_object_unref (priv->user);
  }
  g_object_unref (priv->client);
  g_object_unref (priv->manager);

  G_OBJECT_CLASS (shell_status_menu_parent_class)->finalize (object);
}

static void
shell_status_menu_class_init (ShellStatusMenuClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ShellStatusMenuPrivate));

  gobject_class->finalize = shell_status_menu_finalize;

  shell_status_menu_signals[DEACTIVATED] =
    g_signal_new ("deactivated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ShellStatusMenuClass, deactivated),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

ShellStatusMenu *
shell_status_menu_new (void)
{
  return g_object_new (SHELL_TYPE_STATUS_MENU, NULL);
}

static void
position_menu (GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer user_data)
{
  ShellStatusMenu *status = SHELL_STATUS_MENU (user_data);
  float src_x, src_y;

  clutter_actor_get_transformed_position (CLUTTER_ACTOR (status), &src_x, &src_y);

  *x = (gint)(0.5 + src_x);
  *y = (gint)(0.5 + src_y);
}

void
shell_status_menu_toggle (ShellStatusMenu *status, ClutterEvent *event)
{
  ShellStatusMenuPrivate *priv = status->priv;

  if (GTK_WIDGET_VISIBLE (GTK_WIDGET (priv->menu)))
    {
      gtk_widget_hide (GTK_WIDGET (priv->menu));
    }
  else
    {
      gtk_widget_show (GTK_WIDGET (priv->menu));
      gtk_menu_popup (GTK_MENU (priv->menu), NULL, NULL, position_menu,
          status, 1, event->button.time);
    }
}
