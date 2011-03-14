#include "shell-tp-client.h"

#include <telepathy-glib/telepathy-glib.h>

G_DEFINE_TYPE(ShellTpClient, shell_tp_client, TP_TYPE_BASE_CLIENT)

struct _ShellTpClientPrivate
{
  ShellTpClientObserveChannelsImpl observe_impl;
  gpointer user_data_obs;
  GDestroyNotify destroy_obs;
};

/**
 * ShellTpClientObserveChannelsImpl:
 * @client: a #ShellTpClient instance
 * @account: a #TpAccount having %TP_ACCOUNT_FEATURE_CORE prepared if possible
 * @connection: a #TpConnection having %TP_CONNECTION_FEATURE_CORE prepared
 * if possible
 * @channels: (element-type TelepathyGLib.Channel): a #GList of #TpChannel,
 *  all having %TP_CHANNEL_FEATURE_CORE prepared if possible
 * @dispatch_operation: (allow-none): a #TpChannelDispatchOperation or %NULL;
 *  the dispatch_operation is not guaranteed to be prepared
 * @requests: (element-type TelepathyGLib.ChannelRequest): a #GList of
 *  #TpChannelRequest, all having their object-path defined but are not
 *  guaranteed to be prepared.
 * @context: a #TpObserveChannelsContext representing the context of this
 *  D-Bus call
 *
 * Signature of the implementation of the ObserveChannels method.
 */

static void
shell_tp_client_init (ShellTpClient *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, SHELL_TYPE_TP_CLIENT,
      ShellTpClientPrivate);

  /* Observer */

  /* We only care about single-user text-based chats */
  tp_base_client_take_observer_filter (TP_BASE_CLIENT (self),
      tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_TEXT,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          TP_HANDLE_TYPE_CONTACT,
        NULL));
}

static void
observe_channels (TpBaseClient *client,
                  TpAccount *account,
                  TpConnection *connection,
                  GList *channels,
                  TpChannelDispatchOperation *dispatch_operation,
                  GList *requests,
                  TpObserveChannelsContext *context)
{
  ShellTpClient *self = (ShellTpClient *) client;

  g_assert (self->priv->observe_impl != NULL);

  self->priv->observe_impl (self, account, connection, channels,
      dispatch_operation, requests, context, self->priv->user_data_obs);
}

static void
shell_tp_client_dispose (GObject *object)
{
  ShellTpClient *self = SHELL_TP_CLIENT (object);
  void (*dispose) (GObject *) =
    G_OBJECT_CLASS (shell_tp_client_parent_class)->dispose;

  if (self->priv->destroy_obs != NULL)
    {
      self->priv->destroy_obs (self->priv->user_data_obs);
      self->priv->destroy_obs = NULL;
    }

  if (dispose != NULL)
    dispose (object);
}

static void
shell_tp_client_class_init (ShellTpClientClass *cls)
{
  GObjectClass *object_class = G_OBJECT_CLASS (cls);
  TpBaseClientClass *base_clt_cls = TP_BASE_CLIENT_CLASS (cls);

  g_type_class_add_private (cls, sizeof (ShellTpClientPrivate));

  object_class->dispose = shell_tp_client_dispose;

  base_clt_cls->observe_channels = observe_channels;
}

/**
 * shell_tp_client_new:
 * @dbus: a #TpDBusDaemon object, may not be %NULL
 *
 * Returns: a new #ShellTpClient
 */
ShellTpClient *
shell_tp_client_new (TpDBusDaemon *dbus)
{
  return g_object_new (SHELL_TYPE_TP_CLIENT,
      "dbus-daemon", dbus,
      "name", "GnomeShell",
      "uniquify-name", TRUE,
      NULL);
}

void
shell_tp_client_set_observe_channels_func (ShellTpClient *self,
                                           ShellTpClientObserveChannelsImpl observe_impl,
                                           gpointer user_data,
                                           GDestroyNotify destroy)
{
  g_assert (self->priv->observe_impl == NULL);

  self->priv->observe_impl = observe_impl;
  self->priv->user_data_obs = user_data;
  self->priv->destroy_obs = destroy;
}
