/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#pragma once

#include <glib-object.h>
#include <glib.h>
#include <NetworkManager.h>
#include <nm-secret-agent-old.h>

G_BEGIN_DECLS

typedef enum {
  SHELL_NETWORK_AGENT_CONFIRMED,
  SHELL_NETWORK_AGENT_USER_CANCELED,
  SHELL_NETWORK_AGENT_INTERNAL_ERROR
} ShellNetworkAgentResponse;

typedef struct _ShellNetworkAgent         ShellNetworkAgent;

#define SHELL_TYPE_NETWORK_AGENT                  (shell_network_agent_get_type ())

G_DEFINE_AUTOPTR_CLEANUP_FUNC (NMSecretAgentOld, g_object_unref)

G_DECLARE_FINAL_TYPE (ShellNetworkAgent,
                      shell_network_agent,
                      SHELL, NETWORK_AGENT,
                      NMSecretAgentOld)

void               shell_network_agent_add_vpn_secret (ShellNetworkAgent *self,
                                                       gchar             *request_id,
                                                       gchar             *setting_key,
                                                       gchar             *setting_value);
void               shell_network_agent_set_password (ShellNetworkAgent *self,
                                                     gchar             *request_id,
                                                     gchar             *setting_key,
                                                     gchar             *setting_value);
void               shell_network_agent_respond      (ShellNetworkAgent *self,
                                                     gchar             *request_id,
                                                     ShellNetworkAgentResponse response);

void               shell_network_agent_search_vpn_plugin (ShellNetworkAgent   *self,
                                                          const char          *service,
                                                          GAsyncReadyCallback  callback,
                                                          gpointer             user_data);
NMVpnPluginInfo   *shell_network_agent_search_vpn_plugin_finish (ShellNetworkAgent  *self,
                                                                 GAsyncResult       *result,
                                                                 GError            **error);

/* If these are kept in sync with nm-applet, secrets will be shared */
#define SHELL_KEYRING_UUID_TAG "connection-uuid"
#define SHELL_KEYRING_SN_TAG "setting-name"
#define SHELL_KEYRING_SK_TAG "setting-key"

G_END_DECLS
