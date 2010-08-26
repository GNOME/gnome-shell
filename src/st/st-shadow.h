/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __ST_SHADOW__
#define __ST_SHADOW__

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define ST_TYPE_SHADOW              (st_shadow_get_type ())

typedef struct _StShadow StShadow;

/**
 * StShadow:
 * @color: shadow's color
 * @xoffset: horizontal offset - positive values mean placement to the right,
 *           negative values placement to the left of the element.
 * @yoffset: vertical offset - positive values mean placement below, negative
 *           values placement above the element.
 * @blur: shadow's blur radius - a value of 0.0 will result in a hard shadow.
 * @spread: shadow's spread radius - grow the shadow without enlarging the
 *           blur.
 *
 * Attributes of the -st-shadow property.
 */
struct _StShadow {
    ClutterColor color;
    gdouble      xoffset;
    gdouble      yoffset;
    gdouble      blur;
    gdouble      spread;
};

GType     st_shadow_get_type (void) G_GNUC_CONST;

StShadow *st_shadow_new      (ClutterColor   *color,
                              gdouble         xoffset,
                              gdouble         yoffset,
                              gdouble         blur,
                              gdouble         spread);
StShadow *st_shadow_copy     (const StShadow *shadow);
void      st_shadow_free     (StShadow       *shadow);

gboolean  st_shadow_equal    (StShadow       *shadow,
                              StShadow       *other);

void      st_shadow_get_box  (StShadow              *shadow,
                              const ClutterActorBox *actor_box,
                              ClutterActorBox       *shadow_box);

G_END_DECLS

#endif /* __ST_SHADOW__ */
