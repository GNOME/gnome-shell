#ifndef __CLUTTER_OFFSCREEN_EFFECT_PRIVATE_H__
#define __CLUTTER_OFFSCREEN_EFFECT_PRIVATE_H__

#include <clutter/clutter-offscreen-effect.h>

G_BEGIN_DECLS

gboolean        _clutter_offscreen_effect_get_target_size       (ClutterOffscreenEffect *effect,
                                                                 gfloat                 *width,
                                                                 gfloat                 *height);

G_END_DECLS

#endif /* __CLUTTER_OFFSCREEN_EFFECT_PRIVATE_H__ */
