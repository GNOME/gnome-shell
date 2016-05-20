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
#include "backends/x11/meta-clutter-backend-x11.h"
#include "backends/x11/meta-stage-x11-nested.h"
#include "clutter/clutter.h"
#include "clutter/clutter-mutter.h"
#include "meta/meta-backend.h"

struct _MetaClutterBackendX11
{
  ClutterBackendX11 parent;
};

G_DEFINE_TYPE (MetaClutterBackendX11, meta_clutter_backend_x11,
               CLUTTER_TYPE_BACKEND_X11)

static CoglRenderer *
meta_clutter_backend_x11_get_renderer (ClutterBackend  *clutter_backend,
                                       GError         **error)
{
  MetaBackend *backend = meta_get_backend ();
  MetaRenderer *renderer = meta_backend_get_renderer (backend);

  return meta_renderer_create_cogl_renderer (renderer);
}

static ClutterStageWindow *
meta_clutter_backend_x11_create_stage (ClutterBackend  *backend,
                                       ClutterStage    *wrapper,
                                       GError         **error)
{
  ClutterEventTranslator *translator;
  ClutterStageWindow *stage;
  GType stage_type;

  if (meta_is_wayland_compositor ())
    stage_type = META_TYPE_STAGE_X11_NESTED;
  else
    stage_type  = CLUTTER_TYPE_STAGE_X11;

  stage = g_object_new (stage_type,
			"backend", backend,
			"wrapper", wrapper,
			NULL);

  /* the X11 stage does event translation */
  translator = CLUTTER_EVENT_TRANSLATOR (stage);
  _clutter_backend_add_event_translator (backend, translator);

  return stage;
}

static void
meta_clutter_backend_x11_init (MetaClutterBackendX11 *clutter_backend_x11)
{
}

static void
meta_clutter_backend_x11_class_init (MetaClutterBackendX11Class *klass)
{
  ClutterBackendClass *clutter_backend_class = CLUTTER_BACKEND_CLASS (klass);

  clutter_backend_class->get_renderer = meta_clutter_backend_x11_get_renderer;
  clutter_backend_class->create_stage = meta_clutter_backend_x11_create_stage;
}
