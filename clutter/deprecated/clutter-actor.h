#ifndef __CLUTTER_ACTOR_DEPRECATED_H__
#define __CLUTTER_ACTOR_DEPRECATED_H__

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

CLUTTER_DEPRECATED
void            clutter_actor_set_geometry      (ClutterActor          *self,
                                                 const ClutterGeometry *geometry);

CLUTTER_DEPRECATED_FOR(clutter_actor_get_allocation_geometry)
void            clutter_actor_get_geometry      (ClutterActor          *self,
                                                 ClutterGeometry       *geometry);
CLUTTER_DEPRECATED
guint32         clutter_actor_get_gid           (ClutterActor          *self);

CLUTTER_DEPRECATED
ClutterActor *  clutter_get_actor_by_gid        (guint32                id_);

G_END_DECLS

#endif /* __CLUTTER_ACTOR_DEPRECATED_H__ */
