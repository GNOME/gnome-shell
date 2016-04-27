#ifndef __CLUTTER_EFFECT_PRIVATE_H__
#define __CLUTTER_EFFECT_PRIVATE_H__

#include <clutter/clutter-effect.h>

G_BEGIN_DECLS

gboolean        _clutter_effect_pre_paint               (ClutterEffect           *effect);
void            _clutter_effect_post_paint              (ClutterEffect           *effect);
gboolean        _clutter_effect_get_paint_volume        (ClutterEffect           *effect,
                                                         ClutterPaintVolume      *volume);
void            _clutter_effect_paint                   (ClutterEffect           *effect,
                                                         ClutterEffectPaintFlags  flags);
void            _clutter_effect_pick                    (ClutterEffect           *effect,
                                                         ClutterEffectPaintFlags  flags);

G_END_DECLS

#endif /* __CLUTTER_EFFECT_PRIVATE_H__ */
