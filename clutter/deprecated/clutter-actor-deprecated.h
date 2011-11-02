#ifndef __CLUTTER_ACTOR_DEPRECATED_H__
#define __CLUTTER_ACTOR_DEPRECATED_H__

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

CLUTTER_DEPRECATED
guint32         clutter_actor_get_gid           (ClutterActor *self);

CLUTTER_DEPRECATED
ClutterActor *  clutter_get_actor_by_gid        (guint32       id_);


G_END_DECLS

#endif /* __CLUTTER_ACTOR_DEPRECATED_H__ */
