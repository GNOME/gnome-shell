/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014-2015 Red Hat
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
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 *     Jonas Ådahl <jadahl@gmail.com>
 */

#ifndef META_BARRIER_PRIVATE_H
#define META_BARRIER_PRIVATE_H

#include "core/meta-border.h"

G_BEGIN_DECLS

#define META_TYPE_BARRIER_IMPL            (meta_barrier_impl_get_type ())
#define META_BARRIER_IMPL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_BARRIER_IMPL, MetaBarrierImpl))
#define META_BARRIER_IMPL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_BARRIER_IMPL, MetaBarrierImplClass))
#define META_IS_BARRIER_IMPL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_BARRIER_IMPL))
#define META_IS_BARRIER_IMPL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_BARRIER_IMPL))
#define META_BARRIER_IMPL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_BARRIER_IMPL, MetaBarrierImplClass))

typedef struct _MetaBarrierImpl        MetaBarrierImpl;
typedef struct _MetaBarrierImplClass   MetaBarrierImplClass;

struct _MetaBarrierImpl
{
  GObject parent;
};

struct _MetaBarrierImplClass
{
  GObjectClass parent_class;

  gboolean (*is_active) (MetaBarrierImpl *barrier);
  void (*release) (MetaBarrierImpl  *barrier,
                   MetaBarrierEvent *event);
  void (*destroy) (MetaBarrierImpl *barrier);
};

GType meta_barrier_impl_get_type (void) G_GNUC_CONST;

void _meta_barrier_emit_hit_signal (MetaBarrier      *barrier,
                                    MetaBarrierEvent *event);
void _meta_barrier_emit_left_signal (MetaBarrier      *barrier,
                                     MetaBarrierEvent *event);

void meta_barrier_event_unref (MetaBarrierEvent *event);

G_END_DECLS

struct _MetaBarrierPrivate
{
  MetaDisplay *display;
  MetaBorder border;
  MetaBarrierImpl *impl;
};

#endif /* META_BARRIER_PRIVATE_H */
