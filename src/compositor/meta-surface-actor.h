/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#ifndef META_SURFACE_ACTOR_PRIVATE_H
#define META_SURFACE_ACTOR_PRIVATE_H

#include <config.h>

#include <meta/meta-shaped-texture.h>
#include <meta/window.h>

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

  void     (* process_damage)    (MetaSurfaceActor *actor,
                                  int x, int y, int width, int height);
  void     (* pre_paint)         (MetaSurfaceActor *actor);
  gboolean (* is_argb32)         (MetaSurfaceActor *actor);
  gboolean (* is_visible)        (MetaSurfaceActor *actor);

  gboolean (* should_unredirect) (MetaSurfaceActor *actor);
  void     (* set_unredirected)  (MetaSurfaceActor *actor,
                                  gboolean          unredirected);
  gboolean (* is_unredirected)   (MetaSurfaceActor *actor);

  MetaWindow *(* get_window)      (MetaSurfaceActor *actor);
};

struct _MetaSurfaceActor
{
  ClutterActor            parent;

  MetaSurfaceActorPrivate *priv;
};

GType meta_surface_actor_get_type (void);

cairo_surface_t *meta_surface_actor_get_image (MetaSurfaceActor      *self,
                                               cairo_rectangle_int_t *clip);

MetaShapedTexture *meta_surface_actor_get_texture (MetaSurfaceActor *self);
MetaWindow        *meta_surface_actor_get_window  (MetaSurfaceActor *self);

gboolean meta_surface_actor_is_obscured (MetaSurfaceActor *self);
gboolean meta_surface_actor_get_unobscured_bounds (MetaSurfaceActor      *self,
                                                   cairo_rectangle_int_t *unobscured_bounds);

void meta_surface_actor_set_input_region (MetaSurfaceActor *self,
                                          cairo_region_t   *region);
void meta_surface_actor_set_opaque_region (MetaSurfaceActor *self,
                                           cairo_region_t   *region);

void meta_surface_actor_update_area (MetaSurfaceActor *actor,
                                     int x, int y, int width, int height);

void meta_surface_actor_process_damage (MetaSurfaceActor *actor,
                                        int x, int y, int width, int height);
void meta_surface_actor_pre_paint (MetaSurfaceActor *actor);
gboolean meta_surface_actor_is_argb32 (MetaSurfaceActor *actor);
gboolean meta_surface_actor_is_visible (MetaSurfaceActor *actor);

void meta_surface_actor_set_frozen (MetaSurfaceActor *actor,
                                    gboolean          frozen);

gboolean meta_surface_actor_should_unredirect (MetaSurfaceActor *actor);
void meta_surface_actor_set_unredirected (MetaSurfaceActor *actor,
                                          gboolean          unredirected);
gboolean meta_surface_actor_is_unredirected (MetaSurfaceActor *actor);

G_END_DECLS

#endif /* META_SURFACE_ACTOR_PRIVATE_H */
