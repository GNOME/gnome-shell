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
#include "meta-monitor-manager.h"

typedef struct {
  MetaOutput               *output;
  struct wl_global         *global;
  int                       x, y;
  enum wl_output_transform  transform;

  GList                    *resources;
} MetaWaylandOutput;

static void
output_resource_destroy (struct wl_resource *res)
{
  MetaWaylandOutput *wayland_output;

  wayland_output = wl_resource_get_user_data (res);
  wayland_output->resources = g_list_remove (wayland_output->resources, res);
}

static void
bind_output (struct wl_client *client,
             void *data,
             guint32 version,
             guint32 id)
{
  MetaWaylandOutput *wayland_output = data;
  MetaOutput *output = wayland_output->output;
  struct wl_resource *resource;
  guint mode_flags;

  resource = wl_resource_create (client, &wl_output_interface,
				 MIN (META_WL_OUTPUT_VERSION, version), id);
  wayland_output->resources = g_list_prepend (wayland_output->resources, resource);

  wl_resource_set_user_data (resource, wayland_output);
  wl_resource_set_destructor (resource, output_resource_destroy);

  meta_verbose ("Binding output %p/%s (%u, %u, %u, %u) x %f\n",
                output, output->name,
                output->crtc->rect.x, output->crtc->rect.y,
                output->crtc->rect.width, output->crtc->rect.height,
                output->crtc->current_mode->refresh_rate);

  wl_resource_post_event (resource,
                          WL_OUTPUT_GEOMETRY,
                          (int)output->crtc->rect.x,
                          (int)output->crtc->rect.y,
                          output->width_mm,
                          output->height_mm,
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

  wl_resource_post_event (resource,
                          WL_OUTPUT_MODE,
                          mode_flags,
                          (int)output->crtc->current_mode->width,
                          (int)output->crtc->current_mode->height,
                          (int)output->crtc->current_mode->refresh_rate);

  if (version >= META_WL_OUTPUT_HAS_DONE)
    wl_resource_post_event (resource,
                            WL_OUTPUT_DONE);
}

static void
wayland_output_destroy_notify (gpointer data)
{
  MetaWaylandOutput *wayland_output = data;
  GList *resources;

  /* Make sure the destructors don't mess with the list */
  resources = wayland_output->resources;
  wayland_output->resources = NULL;

  wl_global_destroy (wayland_output->global);
  g_list_free (resources);

  g_slice_free (MetaWaylandOutput, wayland_output);
}

static void
wayland_output_update_for_output (MetaWaylandOutput *wayland_output,
                                  MetaOutput        *output)
{
  GList *iter;
  guint mode_flags;

  g_assert (output->crtc->current_mode != NULL);

  mode_flags = WL_OUTPUT_MODE_CURRENT;
  if (output->crtc->current_mode == output->preferred_mode)
    mode_flags |= WL_OUTPUT_MODE_PREFERRED;

  for (iter = wayland_output->resources; iter; iter = iter->next)
    {
      struct wl_resource *resource = iter->data;

      if (wayland_output->x != output->crtc->rect.x ||
          wayland_output->y != output->crtc->rect.y ||
          wayland_output->transform != output->crtc->transform)
        {
            wl_resource_post_event (resource,
                                    WL_OUTPUT_GEOMETRY,
                                    (int)output->crtc->rect.x,
                                    (int)output->crtc->rect.y,
                                    output->width_mm,
                                    output->height_mm,
                                    output->subpixel_order,
                                    output->vendor,
                                    output->product,
                                    output->crtc->transform);
        }

      wl_resource_post_event (resource,
                              WL_OUTPUT_MODE,
                              mode_flags,
                              (int)output->crtc->current_mode->width,
                              (int)output->crtc->current_mode->height,
                              (int)output->crtc->current_mode->refresh_rate);
    }

  /* It's very important that we change the output pointer here, as
     the old structure is about to be freed by MetaMonitorManager */
  wayland_output->output = output;
  wayland_output->x = output->crtc->rect.x;
  wayland_output->y = output->crtc->rect.y;
  wayland_output->transform = output->crtc->transform;
}

static GHashTable *
meta_wayland_compositor_update_outputs (MetaWaylandCompositor *compositor,
                                        MetaMonitorManager    *monitors)
{
  MetaOutput *outputs;
  unsigned int i, n_outputs;
  GHashTable *new_table;

  outputs = meta_monitor_manager_get_outputs (monitors, &n_outputs);
  new_table = g_hash_table_new_full (NULL, NULL, NULL, wayland_output_destroy_notify);

  for (i = 0; i < n_outputs; i++)
    {
      MetaOutput *output = &outputs[i];
      MetaWaylandOutput *wayland_output;

      /* wayland does not expose disabled outputs */
      if (output->crtc == NULL)
        {
          g_hash_table_remove (compositor->outputs, GSIZE_TO_POINTER (output->output_id));
          continue;
        }

      wayland_output = g_hash_table_lookup (compositor->outputs, GSIZE_TO_POINTER (output->output_id));

      if (wayland_output)
        {
          g_hash_table_steal (compositor->outputs, GSIZE_TO_POINTER (output->output_id));
        }
      else
        {
          wayland_output = g_slice_new0 (MetaWaylandOutput);
          wayland_output->global = wl_global_create (compositor->wayland_display,
                                                     &wl_output_interface,
						     META_WL_OUTPUT_VERSION,
                                                     wayland_output, bind_output);
        }

      wayland_output_update_for_output (wayland_output, output);
      g_hash_table_insert (new_table, GSIZE_TO_POINTER (output->output_id), wayland_output);
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
