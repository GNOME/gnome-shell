/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Red Hat
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
 */


#ifndef META_BACKEND_PRIVATE_H
#define META_BACKEND_PRIVATE_H

#include <glib-object.h>

#include "meta-backend.h"

#define META_TYPE_BACKEND             (meta_backend_get_type ())
#define META_BACKEND(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_BACKEND, MetaBackend))
#define META_BACKEND_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_BACKEND, MetaBackendClass))
#define META_IS_BACKEND(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_BACKEND))
#define META_IS_BACKEND_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_BACKEND))
#define META_BACKEND_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_BACKEND, MetaBackendClass))

struct _MetaBackend
{
  GObject parent;

  MetaIdleMonitor *device_monitors[256];
  int device_id_max;
};

struct _MetaBackendClass
{
  GObjectClass parent_class;

  MetaIdleMonitor * (* create_idle_monitor) (MetaBackend *backend,
                                             int          device_id);
};

#endif /* META_BACKEND_PRIVATE_H */
