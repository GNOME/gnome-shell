#include "shell-tp-client.h"

#include <string.h>

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

/* Telepathy utility functions */

/**
 * ShellGetTpContactCb:
 * @connection: The connection
 * @contacts: (element-type TelepathyGLib.Contact): List of contacts
 * @failed: Array of failed contacts
 */

static void
shell_global_get_tp_contacts_cb (TpConnection *self,
                                 guint n_contacts,
                                 TpContact * const *contacts,
                                 guint n_failed,
                                 const TpHandle *failed,
                                 const GError *error,
                                 gpointer user_data,
                                 GObject *weak_object)
{
  int i;
  GList *contact_list = NULL;
  for (i = 0; i < n_contacts; i++) {
      contact_list = g_list_append(contact_list, contacts[i]);
  }

  TpHandle *failed_list = g_new0 (TpHandle, n_failed + 1);
  memcpy(failed_list, failed, n_failed);

  ((ShellGetTpContactCb)user_data)(self, contact_list, failed_list);
}

/**
 * shell_get_tp_contacts:
 * @self: A connection, which must be ready
 * @n_handles: Number of handles in handles
 * @handles: (array length=n_handles) (element-type uint): Array of handles
 * @n_features: Number of features in features
 * @features: (array length=n_features) (allow-none) (element-type uint):
 *  Array of features
 * @callback: (scope async): User callback to run when the contacts are ready
 *
 * Wrap tp_connection_get_contacts_by_handle so we can transform the array
 * into a null-terminated one, which gjs can handle.
 * We send the original callback to tp_connection_get_contacts_by_handle as
 * user_data, and we have our own function as callback, which does the
 * transforming.
 */
void
shell_get_tp_contacts (TpConnection *self,
                       guint n_handles,
                       const TpHandle *handles,
                       guint n_features,
                       const TpContactFeature *features,
                       ShellGetTpContactCb callback)
{
  tp_connection_get_contacts_by_handle(self, n_handles, handles,
                                       n_features, features,
                                       shell_global_get_tp_contacts_cb,
                                       callback, NULL, NULL);
}

static void
shell_global_get_self_contact_features_cb (TpConnection *connection,
                                           guint n_contacts,
                                           TpContact * const *contacts,
                                           const GError *error,
                                           gpointer user_data,
                                           GObject *weak_object)
{
  if (error != NULL) {
    g_print ("Failed to upgrade self contact: %s", error->message);
    return;
  }
  ((ShellGetSelfContactFeaturesCb)user_data)(connection, *contacts);
}

/**
 * shell_get_self_contact_features:
 * @self: A connection, which must be ready
 * @n_features: Number of features in features
 * @features: (array length=n_features) (allow-none) (element-type uint):
 *  Array of features
 * @callback: (scope async): User callback to run when the contact is ready
 *
 * Wrap tp_connection_upgrade_contacts due to the lack of support for
 * proper arrays arguments in GJS.
 */
void
shell_get_self_contact_features (TpConnection *self,
                                 guint n_features,
                                 const TpContactFeature *features,
                                 ShellGetSelfContactFeaturesCb callback)
{
  TpContact *self_contact = tp_connection_get_self_contact (self);

  tp_connection_upgrade_contacts (self, 1, &self_contact,
                                  n_features, features,
                                  shell_global_get_self_contact_features_cb,
                                  callback, NULL, NULL);
}

/**
 * shell_get_contact_events:
 * @log_manager: A #TplLogManager
 * @account: A #TpAccount
 * @entity: A #TplEntity
 * @num_events: The number of events to retrieve
 * @callback: (scope async): User callback to run when the contact is ready
 *
 * Wrap tpl_log_manager_get_filtered_events_async because gjs cannot support
 * multiple callbacks in the same function call.
 */
void
shell_get_contact_events (TplLogManager *log_manager,
                          TpAccount *account,
                          TplEntity *entity,
                          guint num_events,
                          GAsyncReadyCallback callback)
{
  tpl_log_manager_get_filtered_events_async (log_manager,
                                             account,
                                             entity,
                                             TPL_EVENT_MASK_TEXT,
                                             num_events,
                                             NULL, NULL,
                                             callback, NULL);
}
