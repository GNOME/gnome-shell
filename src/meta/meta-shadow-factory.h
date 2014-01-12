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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __META_SHADOW_FACTORY_H__
#define __META_SHADOW_FACTORY_H__

#include <glib-object.h>

/**
 * MetaShadowParams:
 * @radius: the radius (gaussian standard deviation) of the shadow
 * @top_fade: if >= 0, the shadow doesn't extend above the top
 *  of the shape, and fades out over the given number of pixels
 * @x_offset: horizontal offset of the shadow with respect to the
 *  shape being shadowed, in pixels
 * @y_offset: vertical offset of the shadow with respect to the
 *  shape being shadowed, in pixels
 * @opacity: opacity of the shadow, from 0 to 255
 *
 * The #MetaShadowParams structure holds information about how to draw
 * a particular style of shadow.
 */

typedef struct _MetaShadowParams MetaShadowParams;

struct _MetaShadowParams
{
  int radius;
  int top_fade;
  int x_offset;
  int y_offset;
  guint8 opacity;
};

#define META_TYPE_SHADOW_FACTORY            (meta_shadow_factory_get_type ())
#define META_SHADOW_FACTORY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_SHADOW_FACTORY, MetaShadowFactory))
#define META_SHADOW_FACTORY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_SHADOW_FACTORY, MetaShadowFactoryClass))
#define META_IS_SHADOW_FACTORY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_SHADOW_FACTORY))
#define META_IS_SHADOW_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_SHADOW_FACTORY))
#define META_SHADOW_FACTORY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_SHADOW_FACTORY, MetaShadowFactoryClass))

/**
 * MetaShadowFactory:
 *
 * #MetaShadowFactory is used to create window shadows. It caches shadows internally
 * so that multiple shadows created for the same shape with the same radius will
 * share the same MetaShadow.
 */
typedef struct _MetaShadowFactory      MetaShadowFactory;
typedef struct _MetaShadowFactoryClass MetaShadowFactoryClass;

MetaShadowFactory *meta_shadow_factory_get_default (void);

GType meta_shadow_factory_get_type (void);

void meta_shadow_factory_set_params (MetaShadowFactory *factory,
                                     const char        *class_name,
                                     gboolean           focused,
                                     MetaShadowParams  *params);
void meta_shadow_factory_get_params (MetaShadowFactory *factory,
                                     const char        *class_name,
                                     gboolean           focused,
                                     MetaShadowParams  *params);

#endif /* __META_SHADOW_FACTORY_H__ */
