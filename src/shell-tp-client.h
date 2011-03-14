#ifndef __SHELL_TP_CLIENT_H__
#define __SHELL_TP_CLIENT_H__

#include <dbus/dbus-glib.h>
#include <glib-object.h>

#include <telepathy-glib/base-client.h>

G_BEGIN_DECLS

typedef struct _ShellTpClient ShellTpClient;
typedef struct _ShellTpClientClass ShellTpClientClass;
typedef struct _ShellTpClientPrivate ShellTpClientPrivate;

struct _ShellTpClientClass {
    /*<private>*/
    TpBaseClientClass parent_class;
};

struct _ShellTpClient {
    /*<private>*/
    TpBaseClient parent;
    ShellTpClientPrivate *priv;
};

GType shell_tp_client_get_type (void);

#define SHELL_TYPE_TP_CLIENT \
  (shell_tp_client_get_type ())
#define SHELL_TP_CLIENT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SHELL_TYPE_TP_CLIENT, \
                               ShellTpClient))
#define SHELL_TP_CLIENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SHELL_TYPE_TP_CLIENT, \
                            ShellTpClientClass))
#define SHELL_IS_TP_CLIENT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SHELL_TYPE_TP_CLIENT))
#define SHELL_IS_TP_CLIENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SHELL_TYPE_TP_CLIENT))
#define SHELL_TP_CLIENT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SHELL_TYPE_TP_CLIENT, \
                              ShellTpClientClass))

typedef void (*ShellTpClientObserveChannelsImpl) (ShellTpClient *client,
                                                  TpAccount *account,
                                                  TpConnection *connection,
                                                  GList *channels,
                                                  TpChannelDispatchOperation *dispatch_operation,
                                                  GList *requests,
                                                  TpObserveChannelsContext *context,
                                                  gpointer user_data);

ShellTpClient * shell_tp_client_new (TpDBusDaemon *dbus);

void shell_tp_client_set_observe_channels_func (ShellTpClient *self,
                                                ShellTpClientObserveChannelsImpl observe_impl,
                                                gpointer user_data,
                                                GDestroyNotify destroy);

G_END_DECLS
#endif /* __SHELL_TP_CLIENT_H__ */
