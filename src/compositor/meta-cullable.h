/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2013 Red Hat
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
 *
 * Written by:
 *     Owen Taylor <otaylor@redhat.com>
 *     Ray Strode <rstrode@redhat.com>
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#ifndef __META_CULLABLE_H__
#define __META_CULLABLE_H__

#include <clutter/clutter.h>

G_BEGIN_DECLS

#define META_TYPE_CULLABLE             (meta_cullable_get_type ())
#define META_CULLABLE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_CULLABLE, MetaCullable))
#define META_IS_CULLABLE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_CULLABLE))
#define META_CULLABLE_GET_IFACE(obj)   (G_TYPE_INSTANCE_GET_INTERFACE ((obj),  META_TYPE_CULLABLE, MetaCullableInterface))

typedef struct _MetaCullable MetaCullable;
typedef struct _MetaCullableInterface MetaCullableInterface;

struct _MetaCullableInterface
{
  GTypeInterface g_iface;

  void (* cull_out)      (MetaCullable   *cullable,
                          cairo_region_t *unobscured_region,
                          cairo_region_t *clip_region);
  void (* reset_culling) (MetaCullable  *cullable);
};

GType meta_cullable_get_type (void);

void meta_cullable_cull_out (MetaCullable   *cullable,
                             cairo_region_t *unobscured_region,
                             cairo_region_t *clip_region);
void meta_cullable_reset_culling (MetaCullable *cullable);

/* Utility methods for implementations */
void meta_cullable_cull_out_children (MetaCullable   *cullable,
                                      cairo_region_t *unobscured_region,
                                      cairo_region_t *clip_region);
void meta_cullable_reset_culling_children (MetaCullable *cullable);

G_END_DECLS

#endif /* __META_CULLABLE_H__ */

