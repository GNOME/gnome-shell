/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2015 Red Hat
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
 *     Jonas Ådahl <jadahl@gmail.com>
 */

#ifndef META_BARRIER_NATIVE_H
#define META_BARRIER_NATIVE_H

#include "backends/meta-barrier-private.h"

G_BEGIN_DECLS

#define META_TYPE_BARRIER_IMPL_NATIVE            (meta_barrier_impl_native_get_type ())
#define META_BARRIER_IMPL_NATIVE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_BARRIER_IMPL_NATIVE, MetaBarrierImplNative))
#define META_BARRIER_IMPL_NATIVE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_BARRIER_IMPL_NATIVE, MetaBarrierImplNativeClass))
#define META_IS_BARRIER_IMPL_NATIVE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_BARRIER_IMPL_NATIVE))
#define META_IS_BARRIER_IMPL_NATIVE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_BARRIER_IMPL_NATIVE))
#define META_BARRIER_IMPL_NATIVE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_BARRIER_IMPL_NATIVE, MetaBarrierImplNativeClass))

typedef struct _MetaBarrierImplNative        MetaBarrierImplNative;
typedef struct _MetaBarrierImplNativeClass   MetaBarrierImplNativeClass;
typedef struct _MetaBarrierImplNativePrivate MetaBarrierImplNativePrivate;

typedef struct _MetaBarrierManagerNative     MetaBarrierManagerNative;

struct _MetaBarrierImplNative
{
  MetaBarrierImpl parent;
};

struct _MetaBarrierImplNativeClass
{
  MetaBarrierImplClass parent_class;
};

GType meta_barrier_impl_native_get_type (void) G_GNUC_CONST;

MetaBarrierImpl *meta_barrier_impl_native_new (MetaBarrier *barrier);

MetaBarrierManagerNative *meta_barrier_manager_native_new (void);
void meta_barrier_manager_native_process (MetaBarrierManagerNative *manager,
                                          ClutterInputDevice       *device,
                                          guint32                   time,
                                          float                    *x,
                                          float                    *y);

G_END_DECLS

#endif /* META_BARRIER_NATIVE_H */
