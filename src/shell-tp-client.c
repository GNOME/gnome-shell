/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "shell-tp-client.h"

#include <string.h>

#include <telepathy-glib/telepathy-glib.h>

G_DEFINE_TYPE(ShellTpClient, shell_tp_client, TP_TYPE_BASE_CLIENT)

struct _ShellTpClientPrivate
{
  ShellTpClientObserveChannelsImpl observe_impl;
  gpointer user_data_obs;
  GDestroyNotify destroy_obs;

  ShellTpClientApproveChannelsImpl approve_channels_impl;
  gpointer user_data_approve_channels;
  GDestroyNotify destroy_approve_channels;

  ShellTpClientHandleChannelsImpl handle_channels_impl;
  gpointer user_data_handle_channels;
  GDestroyNotify destroy_handle_channels;
};

/**
 * ShellTpClientObserveChannelsImpl:
 * @client: a #ShellTpClient instance
 * @account: a #TpAccount having %TP_ACCOUNT_FEATURE_CORE prepared if possible
 * @connection: a #TpConnection having %TP_CONNECTION_FEATURE_CORE prepared
 * if possible
 * @channels: (element-type TelepathyGLib.Channel): a #GList of #TpChannel,
 *  all having %TP_CHANNEL_FEATURE_CORE prepared if possible
 * @dispatch_operation: (nullable): a #TpChannelDispatchOperation or %NULL;
 *  the dispatch_operation is not guaranteed to be prepared
 * @requests: (element-type TelepathyGLib.ChannelRequest): a #GList of
 *  #TpChannelRequest, all having their object-path defined but are not
 *  guaranteed to be prepared.
 * @context: a #TpObserveChannelsContext representing the context of this
 *  D-Bus call
 *
 * Signature of the implementation of the ObserveChannels method.
 */

/**
 * ShellTpClientApproveChannelsImpl:
 * @client: a #ShellTpClient instance
 * @account: a #TpAccount having %TP_ACCOUNT_FEATURE_CORE prepared if possible
 * @connection: a #TpConnection having %TP_CONNECTION_FEATURE_CORE prepared
 * if possible
 * @channels: (element-type TelepathyGLib.Channel): a #GList of #TpChannel,
 *  all having %TP_CHANNEL_FEATURE_CORE prepared if possible
 * @dispatch_operation: (nullable): a #TpChannelDispatchOperation or %NULL;
 *  the dispatch_operation is not guaranteed to be prepared
 * @context: a #TpAddDispatchOperationContext representing the context of this
 *  D-Bus call
 *
 * Signature of the implementation of the AddDispatchOperation method.
 */

/**
 * ShellTpClientHandleChannelsImpl:
 * @client: a #ShellTpClient instance
 * @account: a #TpAccount having %TP_ACCOUNT_FEATURE_CORE prepared if possible
 * @connection: a #TpConnection having %TP_CONNECTION_FEATURE_CORE prepared
 * if possible
 * @channels: (element-type TelepathyGLib.Channel): a #GList of #TpChannel,
 *  all having %TP_CHANNEL_FEATURE_CORE prepared if possible
 * @requests_satisfied: (element-type TelepathyGLib.ChannelRequest): a #GList of
 * #TpChannelRequest having their object-path defined but are not guaranteed
 * to be prepared.
 * @user_action_time: the time at which user action occurred, or one of the
 *  special values %TP_USER_ACTION_TIME_NOT_USER_ACTION or
 *  %TP_USER_ACTION_TIME_CURRENT_TIME
 *  (see #TpAccountChannelRequest:user-action-time for details)
 * @context: a #TpHandleChannelsContext representing the context of this
 *  D-Bus call
 *
 * Signature of the implementation of the HandleChannels method.
 */

static void
shell_tp_client_init (ShellTpClient *self)
{
  GHashTable *filter;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, SHELL_TYPE_TP_CLIENT,
      ShellTpClientPrivate);

  /* We only care about single-user text-based chats */
  filter = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
        TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
        TP_HANDLE_TYPE_CONTACT,
      NULL);

  /* Observer */
  tp_base_client_set_observer_recover (TP_BASE_CLIENT (self), TRUE);

  tp_base_client_add_observer_filter (TP_BASE_CLIENT (self), filter);

  /* Approver */
  tp_base_client_add_approver_filter (TP_BASE_CLIENT (self), filter);

  /* Approve room invitations. We don't handle or observe room channels so
   * just register this filter for the approver. */
  tp_base_client_take_approver_filter (TP_BASE_CLIENT (self), tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
        TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
        TP_HANDLE_TYPE_ROOM,
      NULL));

  /* Approve calls. We let Empathy handle the call itself. */
  tp_base_client_take_approver_filter (TP_BASE_CLIENT (self),
      tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING, TP_IFACE_CHANNEL_TYPE_CALL,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT, TP_HANDLE_TYPE_CONTACT,
        NULL));

  /* Approve file transfers. We let Empathy handle the transfer itself. */
  tp_base_client_take_approver_filter (TP_BASE_CLIENT (self),
      tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT, TP_HANDLE_TYPE_CONTACT,
        NULL));

  /* Handler */
  tp_base_client_add_handler_filter (TP_BASE_CLIENT (self), filter);

  g_hash_table_unref (filter);
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
add_dispatch_operation (TpBaseClient *client,
    TpAccount *account,
    TpConnection *connection,
    GList *channels,
    TpChannelDispatchOperation *dispatch_operation,
    TpAddDispatchOperationContext *context)
{
  ShellTpClient *self = (ShellTpClient *) client;

  g_assert (self->priv->approve_channels_impl != NULL);

  self->priv->approve_channels_impl (self, account, connection, channels,
      dispatch_operation, context, self->priv->user_data_approve_channels);
}

static void
handle_channels (TpBaseClient *client,
    TpAccount *account,
    TpConnection *connection,
    GList *channels,
    GList *requests_satisfied,
    gint64 user_action_time,
    TpHandleChannelsContext *context)
{
  ShellTpClient *self = (ShellTpClient *) client;

  g_assert (self->priv->handle_channels_impl != NULL);

  self->priv->handle_channels_impl (self, account, connection, channels,
      requests_satisfied, user_action_time, context,
      self->priv->user_data_handle_channels);
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
      self->priv->user_data_obs = NULL;
    }

  if (self->priv->destroy_approve_channels != NULL)
    {
      self->priv->destroy_approve_channels (self->priv->user_data_approve_channels);
      self->priv->destroy_approve_channels = NULL;
      self->priv->user_data_approve_channels = NULL;
    }

  if (self->priv->destroy_handle_channels != NULL)
    {
      self->priv->destroy_handle_channels (self->priv->user_data_handle_channels);
      self->priv->destroy_handle_channels = NULL;
      self->priv->user_data_handle_channels = NULL;
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
  base_clt_cls->add_dispatch_operation = add_dispatch_operation;
  base_clt_cls->handle_channels = handle_channels;
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

void
shell_tp_client_set_approve_channels_func (ShellTpClient *self,
    ShellTpClientApproveChannelsImpl approve_channels_impl,
    gpointer user_data,
    GDestroyNotify destroy)
{
  g_assert (self->priv->approve_channels_impl == NULL);

  self->priv->approve_channels_impl = approve_channels_impl;
  self->priv->user_data_approve_channels = user_data;
  self->priv->destroy_approve_channels = destroy;
}

void
shell_tp_client_set_handle_channels_func (ShellTpClient *self,
    ShellTpClientHandleChannelsImpl handle_channels_impl,
    gpointer user_data,
    GDestroyNotify destroy)
{
  g_assert (self->priv->handle_channels_impl == NULL);

  self->priv->handle_channels_impl = handle_channels_impl;
  self->priv->user_data_handle_channels = user_data;
  self->priv->destroy_handle_channels = destroy;
}
