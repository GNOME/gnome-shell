/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __ST_SHADOW_TEXTURE__
#define __ST_SHADOW_TEXTURE__

#include <clutter/clutter.h>

G_BEGIN_DECLS

typedef struct _StShadowTexture      StShadowTexture;
typedef struct _StShadowTextureClass StShadowTextureClass;

#define ST_TYPE_SHADOW_TEXTURE            (st_shadow_texture_get_type ())
#define ST_SHADOW_TEXTURE(object)         (G_TYPE_CHECK_INSTANCE_CAST((object),ST_TYPE_SHADOW_TEXTURE, StShadowTexture))
#define ST_IS_SHADOW_TEXTURE(object)      (G_TYPE_CHECK_INSTANCE_TYPE((object),ST_TYPE_SHADOW_TEXTURE))
#define ST_SHADOW_TEXTURE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),    ST_TYPE_SHADOW_TEXTURE, StShadowTextureClass))
#define ST_IS_SHADOW_TEXTURE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),    ST_TYPE_SHADOW_TEXTURE))
#define ST_SHADOW_TEXTURE_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), ST_TYPE_SHADOW_TEXTURE, StShadowTextureClass))

GType st_shadow_texture_get_type (void) G_GNUC_CONST;

ClutterActor *st_shadow_texture_new (ClutterActor *actor,
                                     ClutterColor *color,
                                     gdouble       blur_radius);

void st_shadow_texture_adjust_allocation (StShadowTexture *shadow,
                                          ClutterActorBox *allocation);

G_END_DECLS

#endif /* __ST_SHADOW_TEXTURE__ */
