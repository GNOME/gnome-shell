/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef META_SURFACE_ACTOR_PRIVATE_H
#define META_SURFACE_ACTOR_PRIVATE_H

#include <config.h>

#include <meta/meta-shaped-texture.h>
#include "meta-wayland-private.h"

G_BEGIN_DECLS

#define META_TYPE_SURFACE_ACTOR            (meta_surface_actor_get_type())
#define META_SURFACE_ACTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_SURFACE_ACTOR, MetaSurfaceActor))
#define META_SURFACE_ACTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), META_TYPE_SURFACE_ACTOR, MetaSurfaceActorClass))
#define META_IS_SURFACE_ACTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_SURFACE_ACTOR))
#define META_IS_SURFACE_ACTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), META_TYPE_SURFACE_ACTOR))
#define META_SURFACE_ACTOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), META_TYPE_SURFACE_ACTOR, MetaSurfaceActorClass))

typedef struct _MetaSurfaceActor        MetaSurfaceActor;
typedef struct _MetaSurfaceActorClass   MetaSurfaceActorClass;
typedef struct _MetaSurfaceActorPrivate MetaSurfaceActorPrivate;

struct _MetaSurfaceActorClass
{
  /*< private >*/
  ClutterActorClass parent_class;
};

struct _MetaSurfaceActor
{
  ClutterActor            parent;

  MetaSurfaceActorPrivate *priv;
};

GType meta_surface_actor_get_type (void);

MetaSurfaceActor *meta_surface_actor_new (void);

cairo_surface_t *meta_surface_actor_get_image (MetaSurfaceActor      *self,
                                               cairo_rectangle_int_t *clip);

MetaShapedTexture *meta_surface_actor_get_texture (MetaSurfaceActor *self);

gboolean meta_surface_actor_damage_all (MetaSurfaceActor *self,
                                        cairo_region_t   *unobscured_region);

gboolean meta_surface_actor_damage_area (MetaSurfaceActor *self,
                                         int               x,
                                         int               y,
                                         int               width,
                                         int               height,
                                         cairo_region_t   *unobscured_region);

void meta_surface_actor_set_texture (MetaSurfaceActor *self,
                                     CoglTexture      *texture);
void meta_surface_actor_attach_wayland_buffer (MetaSurfaceActor  *self,
                                               MetaWaylandBuffer *buffer);
void meta_surface_actor_set_input_region (MetaSurfaceActor *self,
                                          cairo_region_t   *region);
void meta_surface_actor_set_opaque_region (MetaSurfaceActor *self,
                                           cairo_region_t   *region);

G_END_DECLS

#endif /* META_SURFACE_ACTOR_PRIVATE_H */
