/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * MetaShadowFactory:
 *
 * Create and cache shadow textures for arbitrary window shapes
 *
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __META_SHADOW_FACTORY_H__
#define __META_SHADOW_FACTORY_H__

#include <clutter/clutter.h>
#include "meta-window-shape.h"

#define META_TYPE_SHADOW_FACTORY            (meta_shadow_factory_get_type ())
#define META_SHADOW_FACTORY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_SHADOW_FACTORY, MetaShadowFactory))
#define META_SHADOW_FACTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_SHADOW_FACTORY, MetaShadowFactoryClass))
#define META_IS_SHADOW_FACTORY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_SHADOW_FACTORY))
#define META_IS_SHADOW_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_SHADOW_FACTORY))
#define META_SHADOW_FACTORY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_SHADOW_FACTORY, MetaShadowFactoryClass))

/**
 * MetaShadow:
 * #MetaShadow holds a shadow texture along with information about how to
 * apply that texture to draw a window texture. (E.g., it knows how big the
 * unscaled borders are on each side of the shadow texture.)
 */
typedef struct _MetaShadow MetaShadow;

MetaShadow *meta_shadow_ref         (MetaShadow            *shadow);
void        meta_shadow_unref       (MetaShadow            *shadow);
CoglHandle  meta_shadow_get_texture (MetaShadow            *shadow);
void        meta_shadow_paint       (MetaShadow            *shadow,
                                     int                    window_x,
                                     int                    window_y,
                                     int                    window_width,
                                     int                    window_height,
                                     guint8                 opacity);
void        meta_shadow_get_bounds  (MetaShadow            *shadow,
                                     int                    window_x,
                                     int                    window_y,
                                     int                    window_width,
                                     int                    window_height,
                                     cairo_rectangle_int_t *bounds);

/**
 * MetaShadowFactory:
 * #MetaShadowFactory is used to create window shadows. It caches shadows internally
 * so that multiple shadows created for the same shape with the same radius will
 * share the same MetaShadow.
 */
typedef struct _MetaShadowFactory      MetaShadowFactory;
typedef struct _MetaShadowFactoryClass MetaShadowFactoryClass;

MetaShadowFactory *meta_shadow_factory_get_default (void);

GType meta_shadow_factory_get_type (void);

MetaShadowFactory *meta_shadow_factory_new        (void);
MetaShadow *       meta_shadow_factory_get_shadow (MetaShadowFactory *factory,
                                                   MetaWindowShape   *shape,
                                                   int                width,
                                                   int                height,
                                                   int                radius,
                                                   int                top_fade);

#endif /* __META_SHADOW_FACTORY_H__ */
