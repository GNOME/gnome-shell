#include "nd-daemon.h"

#include "nd-notifications.h"

struct _NdDaemon {
  GObject parent;

  GMainLoop *loop;

  NdNotifications *notifications_skeleton;
  NdNotifications *notifications_proxy;

  GHashTable *sender_map;

  int rv;
};

G_DEFINE_TYPE (NdDaemon, nd_daemon, G_TYPE_OBJECT)

static void
nd_daemon_fail (NdDaemon   *daemon,
                const char *message)
{
  g_printerr ("%s\n", message);

  daemon->rv = 1;
  g_main_loop_quit (daemon->loop);
}

static const char *
nd_daemon_lookup_sender (NdDaemon *daemon,
                         guint     id)
{
  const char *sender;

  sender = g_hash_table_lookup (daemon->sender_map, GUINT_TO_POINTER (id));

  if (sender == NULL)
    g_warning ("No sender for notification with ID %u", id);
  return sender;
}

static void
on_action_invoked (NdNotifications *proxy,
                   guint            id,
                   const char      *action,
                   gpointer         user_data)
{
  NdDaemon *daemon = ND_DAEMON (user_data);
  GDBusInterfaceSkeleton *skeleton;
  GDBusConnection *connection;
  g_autoptr (GVariant) signal_params = NULL;
  const char *sender, *object_path;

  sender = nd_daemon_lookup_sender (daemon, id);
  if (sender == NULL)
    return;

  skeleton = G_DBUS_INTERFACE_SKELETON (daemon->notifications_skeleton);

  connection = g_dbus_interface_skeleton_get_connection (skeleton);
  object_path = g_dbus_interface_skeleton_get_object_path (skeleton);
  signal_params = g_variant_ref_sink (g_variant_new ("(us)", id, action));

  g_dbus_connection_emit_signal (connection, sender, object_path,
                                 "org.freedesktop.Notifications",
                                 "ActionInvoked", signal_params,
                                 NULL);
}

static void
on_notification_closed (NdNotifications *proxy,
                        guint            id,
                        guint            reason,
                        gpointer         user_data)
{
  NdDaemon *daemon = ND_DAEMON (user_data);
  GDBusInterfaceSkeleton *skeleton;
  GDBusConnection *connection;
  g_autoptr (GVariant) signal_params = NULL;
  const char *sender, *object_path;

  sender = nd_daemon_lookup_sender (daemon, id);
  if (sender == NULL)
    return;

  skeleton = G_DBUS_INTERFACE_SKELETON (daemon->notifications_skeleton);

  connection = g_dbus_interface_skeleton_get_connection (skeleton);
  object_path = g_dbus_interface_skeleton_get_object_path (skeleton);
  signal_params = g_variant_ref_sink (g_variant_new ("(uu)", id, reason));

  g_dbus_connection_emit_signal (connection, sender, object_path,
                                 "org.freedesktop.Notifications",
                                 "NotificationClosed", signal_params,
                                 NULL);

  g_hash_table_remove (daemon->sender_map, GUINT_TO_POINTER (id));
}

static gboolean
handle_get_server_information (NdNotifications       *skeleton,
                               GDBusMethodInvocation *invocation,
                               gpointer               user_data)
{
  NdDaemon *daemon = ND_DAEMON (user_data);
  NdNotifications *proxy = daemon->notifications_proxy;
  g_autoptr (GError) error = NULL;
  g_autofree char *name = NULL;
  g_autofree char *vendor = NULL;
  g_autofree char *version = NULL;
  g_autofree char *spec = NULL;
  gboolean success;

  success = nd_notifications_call_get_server_information_sync (proxy,
                                                               &name,
                                                               &vendor,
                                                               &version,
                                                               &spec,
                                                               NULL,
                                                               &error);

  if (success)
    {
      nd_notifications_complete_get_server_information (skeleton,
                                                        invocation,
                                                        name,
                                                        vendor,
                                                        version,
                                                        spec);
    } else {
      g_dbus_method_invocation_return_gerror (invocation, error);
    }

  return TRUE;
}

static gboolean
handle_get_capabilities (NdNotifications       *skeleton,
                         GDBusMethodInvocation *invocation,
                         gpointer               user_data)
{
  NdDaemon *daemon = ND_DAEMON (user_data);
  NdNotifications *proxy = daemon->notifications_proxy;
  g_autoptr (GError) error = NULL;
  g_auto (GStrv) caps = NULL;
  gboolean success;

  success = nd_notifications_call_get_capabilities_sync (proxy,
                                                         &caps,
                                                         NULL,
                                                         &error);

  if (success)
    {
      nd_notifications_complete_get_capabilities (skeleton,
                                                  invocation,
                                                  (const char * const *)caps);
    }
  else
    {
      g_dbus_method_invocation_return_gerror (invocation, error);
    }

  return TRUE;
}

static gboolean
handle_notify (NdNotifications       *skeleton,
               GDBusMethodInvocation *invocation,
	       const char            *app_name,
	       guint                  replaces_id,
	       const char            *app_icon,
	       const char            *summary,
	       const char            *body,
	       const char *const     *actions,
	       GVariant              *hints,
	       int                    expire_timeout,
               gpointer               user_data)
{
  NdDaemon *daemon = ND_DAEMON (user_data);
  NdNotifications *proxy = daemon->notifications_proxy;
  guint id = 0;
  g_autoptr (GError) error = NULL;
  gboolean success;

  success = nd_notifications_call_notify_sync (proxy,
                                               app_name,
                                               replaces_id,
                                               app_icon,
                                               summary,
                                               body,
                                               actions,
                                               hints,
                                               expire_timeout,
                                               &id,
                                               NULL,
                                               &error);

  if (success)
    {
      const char *sender = g_dbus_method_invocation_get_sender (invocation);

      g_hash_table_insert (daemon->sender_map,
                           GUINT_TO_POINTER (id), g_strdup (sender));

      nd_notifications_complete_notify (skeleton, invocation, id);
    }
  else
    {
      g_dbus_method_invocation_return_gerror (invocation, error);
    }

  return TRUE;
}

static gboolean
handle_close_notification (NdNotifications       *skeleton,
                           GDBusMethodInvocation *invocation,
                           guint                  id,
                           gpointer               user_data)
{
  NdDaemon *daemon = ND_DAEMON (user_data);
  NdNotifications *proxy = daemon->notifications_proxy;
  g_autoptr (GError) error = NULL;
  gboolean success;

  success = nd_notifications_call_close_notification_sync (proxy,
                                                           id,
                                                           NULL,
                                                           &error);

  if (success)
    nd_notifications_complete_close_notification (skeleton, invocation);
  else
    g_dbus_method_invocation_return_gerror (invocation, error);

  return TRUE;
}

static void
nd_daemon_bus_acquired (GDBusConnection *connection,
                        const char      *name G_GNUC_UNUSED,
                        gpointer         user_data)
{
  NdDaemon *daemon = ND_DAEMON (user_data);
  GDBusInterfaceSkeleton *skeleton;
  g_autofree char *name_owner = NULL;

  daemon->notifications_proxy =
    nd_notifications_proxy_new_sync (connection,
                                     G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                     "org.gnome.Shell.FdoNotifications",
                                     "/org/gnome/Shell/FdoNotifications",
                                     NULL, NULL);

  g_object_get (G_OBJECT (daemon->notifications_proxy),
                "g-name-owner", &name_owner,
                NULL);

  if (name_owner == NULL)
    {
      nd_daemon_fail (daemon,
                      "Failed to connect to GNOME Shell notification server");
      return;
    }

  g_signal_connect (daemon->notifications_proxy,
                    "action-invoked",
                    G_CALLBACK (on_action_invoked),
                    daemon);
  g_signal_connect (daemon->notifications_proxy,
                    "notification-closed",
                    G_CALLBACK (on_notification_closed),
                    daemon);

  skeleton = G_DBUS_INTERFACE_SKELETON (daemon->notifications_skeleton);

  g_signal_connect (skeleton,
                    "handle-get-server-information",
                    G_CALLBACK (handle_get_server_information),
                    daemon);
  g_signal_connect (skeleton,
                    "handle-get-capabilities",
                    G_CALLBACK (handle_get_capabilities),
                    daemon);
  g_signal_connect (skeleton,
                    "handle-notify",
                    G_CALLBACK (handle_notify),
                    daemon);
  g_signal_connect (skeleton,
                    "handle-close-notification",
                    G_CALLBACK (handle_close_notification),
                    daemon);

  if (!g_dbus_interface_skeleton_export (skeleton,
                                         connection,
                                         "/org/freedesktop/Notifications",
                                         NULL))
    {
      nd_daemon_fail (daemon, "Failed to export interface");
      return;
    }
}

static void
nd_daemon_name_lost (GDBusConnection *connection,
                     const char      *name G_GNUC_UNUSED,
                     gpointer         user_data)
{
  NdDaemon *daemon = ND_DAEMON (user_data);

  g_main_loop_quit (daemon->loop);
}

static void
nd_daemon_dispose (GObject *object)
{
  NdDaemon *daemon = ND_DAEMON (object);

  g_clear_pointer (&daemon->loop, g_main_loop_unref);
  g_clear_pointer (&daemon->sender_map, g_hash_table_unref);

  g_clear_object (&daemon->notifications_proxy);
  g_clear_object (&daemon->notifications_skeleton);

  G_OBJECT_CLASS (nd_daemon_parent_class)->dispose (object);
}

static void
nd_daemon_class_init (NdDaemonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = nd_daemon_dispose;
}

static void
nd_daemon_init (NdDaemon *self)
{
  self->loop = g_main_loop_new (NULL, FALSE);
  self->notifications_skeleton = nd_notifications_skeleton_new ();
  self->sender_map = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                            NULL, g_free);
}

NdDaemon *
nd_daemon_new (void)
{
  return g_object_new (ND_TYPE_DAEMON, NULL);
}

int
nd_daemon_run (NdDaemon *daemon)
{
  guint id = 0;

  g_return_val_if_fail (ND_IS_DAEMON (daemon), 1);

  id = g_bus_own_name (G_BUS_TYPE_SESSION,
                       "org.freedesktop.Notifications",
                       G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT,
                       nd_daemon_bus_acquired,
                       NULL,
                       nd_daemon_name_lost,
                       daemon,
                       NULL);

  g_main_loop_run (daemon->loop);

  g_bus_unown_name (id);

  return daemon->rv;
}
