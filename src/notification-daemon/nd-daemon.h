#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define ND_TYPE_DAEMON (nd_daemon_get_type ())
G_DECLARE_FINAL_TYPE (NdDaemon, nd_daemon, ND, DAEMON, GObject)

NdDaemon *nd_daemon_new (void);
int       nd_daemon_run (NdDaemon *daemon);

G_END_DECLS
