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

#include "config.h"

#include "meta-wayland-outputs.h"

#include "meta-wayland-private.h"
#include "meta-monitor-manager-private.h"

#include <string.h>

enum {
  OUTPUT_DESTROYED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (MetaWaylandOutput, meta_wayland_output, G_TYPE_OBJECT)

static void
output_resource_destroy (struct wl_resource *res)
{
  MetaWaylandOutput *wayland_output;

  wayland_output = wl_resource_get_user_data (res);
  if (!wayland_output)
    return;

  wayland_output->resources = g_list_remove (wayland_output->resources, res);
}

static void
bind_output (struct wl_client *client,
             void *data,
             guint32 version,
             guint32 id)
{
  MetaWaylandOutput *wayland_output = data;
  MetaMonitorInfo *monitor_info = wayland_output->monitor_info;
  struct wl_resource *resource;
  guint mode_flags;
  MetaOutput *output = monitor_info->outputs[0];

  resource = wl_resource_create (client, &wl_output_interface, version, id);
  wayland_output->resources = g_list_prepend (wayland_output->resources, resource);

  wl_resource_set_user_data (resource, wayland_output);
  wl_resource_set_destructor (resource, output_resource_destroy);

  meta_verbose ("Binding monitor %p/%s (%u, %u, %u, %u) x %f\n",
                monitor_info, output->name,
                monitor_info->rect.x, monitor_info->rect.y,
                monitor_info->rect.width, monitor_info->rect.height,
                monitor_info->refresh_rate);

  wl_output_send_geometry (resource,
                           (int)monitor_info->rect.x,
                           (int)monitor_info->rect.y,
                           monitor_info->width_mm,
                           monitor_info->height_mm,
                           /* Cogl values reflect XRandR values,
                              and so does wayland */
                           output->subpixel_order,
                           output->vendor,
                           output->product,
                           output->crtc->transform);

  g_assert (output->crtc->current_mode != NULL);

  mode_flags = WL_OUTPUT_MODE_CURRENT;
  if (output->crtc->current_mode == output->preferred_mode)
    mode_flags |= WL_OUTPUT_MODE_PREFERRED;

  wl_output_send_mode (resource,
                       mode_flags,
                       (int)monitor_info->rect.width,
                       (int)monitor_info->rect.height,
                       (int)monitor_info->refresh_rate);

  if (version >= WL_OUTPUT_SCALE_SINCE_VERSION)
    wl_output_send_scale (resource, output->scale);

  if (version >= WL_OUTPUT_DONE_SINCE_VERSION)
    wl_output_send_done (resource);
}

static void
wayland_output_destroy_notify (gpointer data)
{
  MetaWaylandOutput *wayland_output = data;

  g_signal_emit (wayland_output, signals[OUTPUT_DESTROYED], 0);
  g_object_unref (wayland_output);
}

static inline enum wl_output_transform
wl_output_transform_from_meta_monitor_transform (MetaMonitorTransform transform)
{
  /* The enums are the same. */
  return (enum wl_output_transform) transform;
}

static void
wayland_output_update_for_output (MetaWaylandOutput *wayland_output,
                                  MetaMonitorInfo *monitor_info)
{
  GList *iter;
  guint mode_flags;
  MetaOutput *output = monitor_info->outputs[0];
  enum wl_output_transform wl_transform = wl_output_transform_from_meta_monitor_transform (output->crtc->transform);

  mode_flags = WL_OUTPUT_MODE_CURRENT;
  if (output->crtc->current_mode == output->preferred_mode)
    mode_flags |= WL_OUTPUT_MODE_PREFERRED;

  for (iter = wayland_output->resources; iter; iter = iter->next)
    {
      struct wl_resource *resource = iter->data;

      if (wayland_output->x != monitor_info->rect.x ||
          wayland_output->y != monitor_info->rect.y ||
          wayland_output->transform != wl_transform)
        {
          wl_output_send_geometry (resource,
                                   (int)monitor_info->rect.x,
                                   (int)monitor_info->rect.y,
                                   monitor_info->width_mm,
                                   monitor_info->height_mm,
                                   output->subpixel_order,
                                   output->vendor,
                                   output->product,
                                   wl_transform);
        }

      wl_output_send_mode (resource,
                           mode_flags,
                           (int)monitor_info->rect.width,
                           (int)monitor_info->rect.height,
                           (int)monitor_info->refresh_rate);
    }

  /* It's very important that we change the output pointer here, as
     the old structure is about to be freed by MetaMonitorManager */
  wayland_output->monitor_info = monitor_info;
  wayland_output->x = monitor_info->rect.x;
  wayland_output->y = monitor_info->rect.y;
  wayland_output->transform = wl_transform;
}

static MetaWaylandOutput *
meta_wayland_output_new (MetaWaylandCompositor *compositor)
{
  MetaWaylandOutput *wayland_output;

  wayland_output = g_object_new (META_TYPE_WAYLAND_OUTPUT, NULL);
  wayland_output->global = wl_global_create (compositor->wayland_display,
                                             &wl_output_interface,
                                             META_WL_OUTPUT_VERSION,
                                             wayland_output, bind_output);

  return wayland_output;
}

static GHashTable *
meta_wayland_compositor_update_outputs (MetaWaylandCompositor *compositor,
                                        MetaMonitorManager    *monitors)
{
  unsigned int i;
  GHashTable *new_table;
  MetaMonitorInfo *monitor_infos;
  unsigned int n_monitor_infos;

  monitor_infos = meta_monitor_manager_get_monitor_infos (monitors, &n_monitor_infos);
  new_table = g_hash_table_new_full (NULL, NULL, NULL, wayland_output_destroy_notify);

  for (i = 0; i < n_monitor_infos; i++)
    {
      MetaMonitorInfo *info = &monitor_infos[i];
      MetaWaylandOutput *wayland_output;

      if (info->winsys_id == 0)
        continue;
      wayland_output = g_hash_table_lookup (compositor->outputs, GSIZE_TO_POINTER (info->winsys_id));

      if (wayland_output)
        {
          g_hash_table_steal (compositor->outputs, GSIZE_TO_POINTER (info->winsys_id));
        }
      else
        wayland_output = meta_wayland_output_new (compositor);

      wayland_output_update_for_output (wayland_output, info);
      g_hash_table_insert (new_table, GSIZE_TO_POINTER (info->winsys_id), wayland_output);
    }

  g_hash_table_destroy (compositor->outputs);
  return new_table;
}

static void
on_monitors_changed (MetaMonitorManager    *monitors,
                     MetaWaylandCompositor *compositor)
{
  compositor->outputs = meta_wayland_compositor_update_outputs (compositor, monitors);
}

static void
meta_wayland_output_init (MetaWaylandOutput *wayland_output)
{
}

static void
meta_wayland_output_finalize (GObject *object)
{
  MetaWaylandOutput *wayland_output = META_WAYLAND_OUTPUT (object);
  GList *l;

  wl_global_destroy (wayland_output->global);

  /* Make sure the wl_output destructor doesn't try to access MetaWaylandOutput
   * after we have freed it.
   */
  for (l = wayland_output->resources; l; l = l->next)
    {
      struct wl_resource *output_resource = l->data;

      wl_resource_set_user_data (output_resource, NULL);
    }

  g_list_free (wayland_output->resources);

  G_OBJECT_CLASS (meta_wayland_output_parent_class)->finalize (object);
}

static void
meta_wayland_output_class_init (MetaWaylandOutputClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_wayland_output_finalize;

  signals[OUTPUT_DESTROYED] = g_signal_new ("output-destroyed",
                                            G_TYPE_FROM_CLASS (object_class),
                                            G_SIGNAL_RUN_LAST,
                                            0,
                                            NULL, NULL, NULL,
                                            G_TYPE_NONE, 0);
}

void
meta_wayland_outputs_init (MetaWaylandCompositor *compositor)
{
  MetaMonitorManager *monitors;

  monitors = meta_monitor_manager_get ();
  g_signal_connect (monitors, "monitors-changed",
                    G_CALLBACK (on_monitors_changed), compositor);

  compositor->outputs = g_hash_table_new_full (NULL, NULL, NULL, wayland_output_destroy_notify);
  compositor->outputs = meta_wayland_compositor_update_outputs (compositor, monitors);
}
