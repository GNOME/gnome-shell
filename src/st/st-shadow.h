/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-shadow.h: Boxed type holding for -st-shadow attributes
 *
 * Copyright 2009, 2010 Florian Müllner
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define ST_TYPE_SHADOW              (st_shadow_get_type ())
#define ST_TYPE_SHADOW_HELPER       (st_shadow_get_type ())

typedef struct _StShadow StShadow;
typedef struct _StShadowHelper StShadowHelper;

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
 * A type representing -st-shadow attributes
 *
 * #StShadow is a boxed type for storing attributes of the -st-shadow
 * property, modelled liberally after the CSS3 box-shadow property.
 * See http://www.css3.info/preview/box-shadow/
 */
struct _StShadow {
    CoglColor color;
    gdouble      xoffset;
    gdouble      yoffset;
    gdouble      blur;
    gdouble      spread;
    gboolean     inset;
};

GType     st_shadow_get_type (void) G_GNUC_CONST;

StShadow *st_shadow_new      (CoglColor      *color,
                              gdouble         xoffset,
                              gdouble         yoffset,
                              gdouble         blur,
                              gdouble         spread,
                              gboolean        inset);
StShadow *st_shadow_ref      (StShadow       *shadow);
void      st_shadow_unref    (StShadow       *shadow);

gboolean  st_shadow_equal    (StShadow       *shadow,
                              StShadow       *other);

void      st_shadow_get_box  (StShadow              *shadow,
                              const ClutterActorBox *actor_box,
                              ClutterActorBox       *shadow_box);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (StShadow, st_shadow_unref)


GType     st_shadow_helper_get_type (void) G_GNUC_CONST;

StShadowHelper *st_shadow_helper_new  (StShadow       *shadow);

StShadowHelper *st_shadow_helper_copy (StShadowHelper *helper);
void            st_shadow_helper_free (StShadowHelper *helper);

void            st_shadow_helper_update (StShadowHelper      *helper,
                                         ClutterActor        *source,
                                         ClutterPaintContext *paint_context);

void            st_shadow_helper_paint (StShadowHelper   *helper,
                                        ClutterPaintNode *node,
                                        ClutterActorBox  *actor_box,
                                        uint8_t           paint_opacity);

G_END_DECLS
