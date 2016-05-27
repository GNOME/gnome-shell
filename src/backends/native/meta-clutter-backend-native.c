/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2016 Red Hat
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

#include "config.h"

#include <glib-object.h>

#include "backends/meta-backend-private.h"
#include "backends/meta-renderer.h"
#include "backends/native/meta-clutter-backend-native.h"
#include "backends/native/meta-stage-native.h"
#include "clutter/clutter.h"
#include "meta/meta-backend.h"

struct _MetaClutterBackendNative
{
  ClutterBackendEglNative parent;

  MetaStageNative *stage_native;
};

G_DEFINE_TYPE (MetaClutterBackendNative, meta_clutter_backend_native,
               CLUTTER_TYPE_BACKEND_EGL_NATIVE)

MetaStageNative *
meta_clutter_backend_native_get_stage_native (ClutterBackend *backend)
{
  MetaClutterBackendNative *clutter_backend_native =
    META_CLUTTER_BACKEND_NATIVE (backend);

  return clutter_backend_native->stage_native;
}

static CoglRenderer *
meta_clutter_backend_native_get_renderer (ClutterBackend  *clutter_backend,
                                          GError         **error)
{
  MetaBackend *backend = meta_get_backend ();
  MetaRenderer *renderer = meta_backend_get_renderer (backend);

  return meta_renderer_create_cogl_renderer (renderer);
}

static ClutterStageWindow *
meta_clutter_backend_native_create_stage (ClutterBackend  *backend,
                                          ClutterStage    *wrapper,
                                          GError         **error)
{
  MetaClutterBackendNative *clutter_backend_native =
    META_CLUTTER_BACKEND_NATIVE (backend);

  g_assert (!clutter_backend_native->stage_native);

  clutter_backend_native->stage_native = g_object_new (META_TYPE_STAGE_NATIVE,
                                                       "backend", backend,
                                                       "wrapper", wrapper,
                                                       NULL);
  return CLUTTER_STAGE_WINDOW (clutter_backend_native->stage_native);
}

static void
meta_clutter_backend_native_init (MetaClutterBackendNative *clutter_backend_nativen)
{
}

static void
meta_clutter_backend_native_class_init (MetaClutterBackendNativeClass *klass)
{
  ClutterBackendClass *clutter_backend_class = CLUTTER_BACKEND_CLASS (klass);

  clutter_backend_class->get_renderer = meta_clutter_backend_native_get_renderer;
  clutter_backend_class->create_stage = meta_clutter_backend_native_create_stage;
}
