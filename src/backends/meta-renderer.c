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

typedef struct _MetaRendererPrivate
{
  GList *views;
} MetaRendererPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaRenderer, meta_renderer, G_TYPE_OBJECT)

CoglRenderer *
meta_renderer_create_cogl_renderer (MetaRenderer *renderer)
{
  return META_RENDERER_GET_CLASS (renderer)->create_cogl_renderer (renderer);
}

static MetaRendererView *
meta_renderer_create_view (MetaRenderer    *renderer,
                           MetaMonitorInfo *monitor_info)
{
  return META_RENDERER_GET_CLASS (renderer)->create_view (renderer,
                                                          monitor_info);
}

void
meta_renderer_rebuild_views (MetaRenderer *renderer)
{
  MetaRendererPrivate *priv = meta_renderer_get_instance_private (renderer);
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaMonitorInfo *monitor_infos;
  unsigned int num_monitor_infos;
  unsigned int i;

  g_list_free_full (priv->views, g_object_unref);
  priv->views = NULL;

  monitor_infos = meta_monitor_manager_get_monitor_infos (monitor_manager,
                                                          &num_monitor_infos);

  for (i = 0; i < num_monitor_infos; i++)
    {
      MetaRendererView *view;

      view = meta_renderer_create_view (renderer, &monitor_infos[i]);
      priv->views = g_list_append (priv->views, view);
    }
}

void
meta_renderer_set_legacy_view (MetaRenderer     *renderer,
                               MetaRendererView *legacy_view)
{
  MetaRendererPrivate *priv = meta_renderer_get_instance_private (renderer);

  g_assert (!priv->views);

  priv->views = g_list_append (priv->views, legacy_view);
}

GList *
meta_renderer_get_views (MetaRenderer *renderer)
{
  MetaRendererPrivate *priv = meta_renderer_get_instance_private (renderer);

  return priv->views;
}

static void
meta_renderer_finalize (GObject *object)
{
  MetaRenderer *renderer = META_RENDERER (object);
  MetaRendererPrivate *priv = meta_renderer_get_instance_private (renderer);

  g_list_free_full (priv->views, g_object_unref);
  priv->views = NULL;
}

static void
meta_renderer_init (MetaRenderer *renderer)
{
}

static void
meta_renderer_class_init (MetaRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_renderer_finalize;
}
