#ifndef __CLUTTER_TIMEOUT_POOL_H__
#define __CLUTTER_TIMEOUT_POOL_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _ClutterTimeoutPool    ClutterTimeoutPool;

ClutterTimeoutPool *clutter_timeout_pool_new    (gint                priority);
guint               clutter_timeout_pool_add    (ClutterTimeoutPool *pool,
                                                 guint               interval,
                                                 GSourceFunc         func,
                                                 gpointer            data,
                                                 GDestroyNotify      notify);
void                clutter_timeout_pool_remove (ClutterTimeoutPool *pool,
                                                 guint               id);

G_END_DECLS

#endif /* __CLUTTER_TIMEOUT_POOL_H__ */
