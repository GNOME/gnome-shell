/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2017 Red Hat
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

#include "config.h"

#include "backends/meta-gpu.h"

#include "backends/meta-output.h"

enum
{
  PROP_0,

  PROP_MONITOR_MANAGER,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

typedef struct _MetaGpuPrivate
{
  MetaMonitorManager *monitor_manager;

  GList *outputs;
  GList *crtcs;
  GList *modes;
} MetaGpuPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaGpu, meta_gpu, G_TYPE_OBJECT)

gboolean
meta_gpu_has_hotplug_mode_update (MetaGpu *gpu)
{
  MetaGpuPrivate *priv = meta_gpu_get_instance_private (gpu);
  GList *l;

  for (l = priv->outputs; l; l = l->next)
    {
      MetaOutput *output = l->data;

      if (output->hotplug_mode_update)
        return TRUE;
    }

  return FALSE;
}

gboolean
meta_gpu_read_current (MetaGpu  *gpu,
                       GError  **error)
{
  MetaGpuPrivate *priv = meta_gpu_get_instance_private (gpu);
  gboolean ret;
  GList *old_outputs;
  GList *old_crtcs;
  GList *old_modes;

  /* TODO: Get rid of this when objects incref:s what they need instead */
  old_outputs = priv->outputs;
  old_crtcs = priv->crtcs;
  old_modes = priv->modes;

  ret = META_GPU_GET_CLASS (gpu)->read_current (gpu, error);

  g_list_free_full (old_outputs, g_object_unref);
  g_list_free_full (old_modes, g_object_unref);
  g_list_free_full (old_crtcs, g_object_unref);

  return ret;
}

MetaMonitorManager *
meta_gpu_get_monitor_manager (MetaGpu *gpu)
{
  MetaGpuPrivate *priv = meta_gpu_get_instance_private (gpu);

  return priv->monitor_manager;
}

GList *
meta_gpu_get_outputs (MetaGpu *gpu)
{
  MetaGpuPrivate *priv = meta_gpu_get_instance_private (gpu);

  return priv->outputs;
}

GList *
meta_gpu_get_crtcs (MetaGpu *gpu)
{
  MetaGpuPrivate *priv = meta_gpu_get_instance_private (gpu);

  return priv->crtcs;
}

GList *
meta_gpu_get_modes (MetaGpu *gpu)
{
  MetaGpuPrivate *priv = meta_gpu_get_instance_private (gpu);

  return priv->modes;
}

void
meta_gpu_take_outputs (MetaGpu *gpu,
                       GList   *outputs)
{
  MetaGpuPrivate *priv = meta_gpu_get_instance_private (gpu);

  priv->outputs = outputs;
}

void
meta_gpu_take_crtcs (MetaGpu *gpu,
                    GList   *crtcs)
{
  MetaGpuPrivate *priv = meta_gpu_get_instance_private (gpu);

  priv->crtcs = crtcs;
}

void
meta_gpu_take_modes (MetaGpu *gpu,
                     GList   *modes)
{
  MetaGpuPrivate *priv = meta_gpu_get_instance_private (gpu);

  priv->modes = modes;
}

static void
meta_gpu_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  MetaGpu *gpu = META_GPU (object);
  MetaGpuPrivate *priv = meta_gpu_get_instance_private (gpu);

  switch (prop_id)
    {
    case PROP_MONITOR_MANAGER:
      priv->monitor_manager = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
meta_gpu_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  MetaGpu *gpu = META_GPU (object);
  MetaGpuPrivate *priv = meta_gpu_get_instance_private (gpu);

  switch (prop_id)
    {
    case PROP_MONITOR_MANAGER:
      g_value_set_object (value, priv->monitor_manager);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
meta_gpu_finalize (GObject *object)
{
  MetaGpu *gpu = META_GPU (object);
  MetaGpuPrivate *priv = meta_gpu_get_instance_private (gpu);

  g_list_free_full (priv->outputs, g_object_unref);
  g_list_free_full (priv->modes, g_object_unref);
  g_list_free_full (priv->crtcs, g_object_unref);

  G_OBJECT_CLASS (meta_gpu_parent_class)->finalize (object);
}

static void
meta_gpu_init (MetaGpu *gpu)
{
}

static void
meta_gpu_class_init (MetaGpuClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = meta_gpu_set_property;
  object_class->get_property = meta_gpu_get_property;
  object_class->finalize = meta_gpu_finalize;

  obj_props[PROP_MONITOR_MANAGER] =
    g_param_spec_object ("monitor-manager",
                         "monitor-manager",
                         "MetaMonitorManager",
                         META_TYPE_MONITOR_MANAGER,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, PROP_LAST, obj_props);
}
