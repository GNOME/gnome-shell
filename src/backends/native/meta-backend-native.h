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

#ifndef META_BACKEND_NATIVE_H
#define META_BACKEND_NATIVE_H

#include "backends/meta-backend-private.h"
#include "backends/native/meta-clutter-backend-native.h"
#include "backends/native/meta-launcher.h"

#define META_TYPE_BACKEND_NATIVE (meta_backend_native_get_type ())
G_DECLARE_FINAL_TYPE (MetaBackendNative, meta_backend_native,
                      META, BACKEND_NATIVE, MetaBackend)

gboolean meta_activate_vt (int vt, GError **error);

void meta_backend_native_pause (MetaBackendNative *backend_native);

void meta_backend_native_resume (MetaBackendNative *backend_native);

MetaLauncher * meta_backend_native_get_launcher (MetaBackendNative *native);

#endif /* META_BACKEND_NATIVE_H */
