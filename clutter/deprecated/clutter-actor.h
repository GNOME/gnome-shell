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

CLUTTER_DEPRECATED_FOR(clutter_actor_remove_child() and clutter_actor_add_child())
void            clutter_actor_reparent          (ClutterActor          *self,
                                                 ClutterActor          *new_parent);

CLUTTER_DEPRECATED_FOR(clutter_actor_add_child)
void            clutter_actor_set_parent        (ClutterActor          *self,
                                                 ClutterActor          *parent);

CLUTTER_DEPRECATED_FOR(clutter_actor_remove_child)
void            clutter_actor_unparent          (ClutterActor          *self);

CLUTTER_DEPRECATED_FOR(clutter_actor_set_child_above_sibling)
void            clutter_actor_raise             (ClutterActor          *self,
                                                 ClutterActor          *below);

CLUTTER_DEPRECATED_FOR(clutter_actor_set_child_below_sibling)
void            clutter_actor_lower             (ClutterActor          *self,
                                                 ClutterActor          *above);

CLUTTER_DEPRECATED_FOR(clutter_actor_set_child_above_sibling() with NULL sibling)
void            clutter_actor_raise_top         (ClutterActor          *self);

CLUTTER_DEPRECATED_FOR(clutter_actor_set_child_below_sibling() with NULL sibling)
void            clutter_actor_lower_bottom      (ClutterActor          *self);

G_END_DECLS

#endif /* __CLUTTER_ACTOR_DEPRECATED_H__ */
