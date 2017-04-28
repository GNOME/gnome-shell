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
 */

#include "config.h"

#include "backends/meta-monitor.h"

#include "backends/meta-backend-private.h"
#include "backends/meta-monitor-manager-private.h"
#include "backends/meta-settings-private.h"

typedef struct _MetaMonitorMode
{
  MetaMonitorModeSpec spec;
  MetaMonitorCrtcMode *crtc_modes;
} MetaMonitorMode;

typedef struct _MetaMonitorModeTiled
{
  MetaMonitorMode parent;

  gboolean is_tiled;
} MetaMonitorModeTiled;

typedef struct _MetaMonitorPrivate
{
  GList *outputs;
  GList *modes;

  MetaMonitorMode *preferred_mode;
  MetaMonitorMode *current_mode;

  MetaMonitorSpec *spec;

  /*
   * The primary or first output for this monitor, 0 if we can't figure out.
   * It can be matched to a winsys_id of a MetaOutput.
   *
   * This is used as an opaque token on reconfiguration when switching from
   * clone to extened, to decide on what output the windows should go next
   * (it's an attempt to keep windows on the same monitor, and preferably on
   * the primary one).
   */
  long winsys_id;
} MetaMonitorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (MetaMonitor, meta_monitor, G_TYPE_OBJECT)

struct _MetaMonitorNormal
{
  MetaMonitor parent;
};

G_DEFINE_TYPE (MetaMonitorNormal, meta_monitor_normal, META_TYPE_MONITOR)

struct _MetaMonitorTiled
{
  MetaMonitor parent;

  uint32_t tile_group_id;

  MetaOutput *main_output;
};

G_DEFINE_TYPE (MetaMonitorTiled, meta_monitor_tiled, META_TYPE_MONITOR)

static void
meta_monitor_mode_free (MetaMonitorMode *mode);

MetaMonitorSpec *
meta_monitor_spec_clone (MetaMonitorSpec *monitor_spec)
{
  MetaMonitorSpec *new_monitor_spec;

  new_monitor_spec = g_new0 (MetaMonitorSpec, 1);
  *new_monitor_spec = (MetaMonitorSpec) {
    .connector = g_strdup (monitor_spec->connector),
    .vendor = g_strdup (monitor_spec->vendor),
    .product = g_strdup (monitor_spec->product),
    .serial = g_strdup (monitor_spec->serial),
  };

  return new_monitor_spec;
}

gboolean
meta_monitor_spec_equals (MetaMonitorSpec *monitor_spec,
                          MetaMonitorSpec *other_monitor_spec)
{
  return (g_str_equal (monitor_spec->connector, other_monitor_spec->connector) &&
          g_str_equal (monitor_spec->vendor, other_monitor_spec->vendor) &&
          g_str_equal (monitor_spec->product, other_monitor_spec->product) &&
          g_str_equal (monitor_spec->serial, other_monitor_spec->serial));
}

int
meta_monitor_spec_compare (MetaMonitorSpec *monitor_spec_a,
                           MetaMonitorSpec *monitor_spec_b)
{
  int ret;

  ret = strcmp (monitor_spec_a->connector, monitor_spec_b->connector);
  if (ret != 0)
    return ret;

  ret = strcmp (monitor_spec_a->vendor, monitor_spec_b->vendor);
  if (ret != 0)
    return ret;

  ret = strcmp (monitor_spec_a->product, monitor_spec_b->product);
  if (ret != 0)
    return ret;

  return strcmp (monitor_spec_a->serial, monitor_spec_b->serial);
}

void
meta_monitor_spec_free (MetaMonitorSpec *monitor_spec)
{
  g_free (monitor_spec->connector);
  g_free (monitor_spec->vendor);
  g_free (monitor_spec->product);
  g_free (monitor_spec->serial);
  g_free (monitor_spec);
}

static void
meta_monitor_generate_spec (MetaMonitor *monitor)
{
  MetaMonitorPrivate *priv = meta_monitor_get_instance_private (monitor);
  MetaOutput *output = meta_monitor_get_main_output (monitor);
  MetaMonitorSpec *monitor_spec;

  monitor_spec = g_new0 (MetaMonitorSpec, 1);
  *monitor_spec = (MetaMonitorSpec) {
    .connector = g_strdup (output->name),
    .vendor = g_strdup (output->vendor),
    .product = g_strdup (output->product),
    .serial = g_strdup (output->serial),
  };

  priv->spec = monitor_spec;
}

GList *
meta_monitor_get_outputs (MetaMonitor *monitor)
{
  MetaMonitorPrivate *priv = meta_monitor_get_instance_private (monitor);

  return priv->outputs;
}

MetaOutput *
meta_monitor_get_main_output (MetaMonitor *monitor)
{
  return META_MONITOR_GET_CLASS (monitor)->get_main_output (monitor);
}

gboolean
meta_monitor_is_active (MetaMonitor *monitor)
{
  MetaOutput *output;

  output = meta_monitor_get_main_output (monitor);

  return output->crtc && output->crtc->current_mode;
}

gboolean
meta_monitor_is_primary (MetaMonitor *monitor)
{
  MetaOutput *output;

  output = meta_monitor_get_main_output (monitor);

  return output->is_primary;
}

gboolean
meta_monitor_supports_underscanning (MetaMonitor *monitor)
{
  MetaOutput *output;

  output = meta_monitor_get_main_output (monitor);

  return output->supports_underscanning;
}

gboolean
meta_monitor_is_underscanning (MetaMonitor *monitor)
{
  MetaOutput *output;

  output = meta_monitor_get_main_output (monitor);

  return output->is_underscanning;
}

gboolean
meta_monitor_is_laptop_panel (MetaMonitor *monitor)
{
  MetaOutput *output;

  output = meta_monitor_get_main_output (monitor);

  switch (output->connector_type)
    {
    case META_CONNECTOR_TYPE_eDP:
    case META_CONNECTOR_TYPE_LVDS:
    case META_CONNECTOR_TYPE_DSI:
      return TRUE;
    default:
      return FALSE;
    }
}

void
meta_monitor_get_current_resolution (MetaMonitor *monitor,
                                     int         *width,
                                     int         *height)
{
  MetaMonitorMode *mode = meta_monitor_get_current_mode (monitor);

  *width = mode->spec.width;
  *height = mode->spec.height;
}

void
meta_monitor_derive_dimensions (MetaMonitor   *monitor,
                                int           *width,
                                int           *height)
{
  META_MONITOR_GET_CLASS (monitor)->derive_dimensions (monitor, width, height);
}

void
meta_monitor_get_physical_dimensions (MetaMonitor *monitor,
                                      int         *width_mm,
                                      int         *height_mm)
{
  MetaOutput *output;

  output = meta_monitor_get_main_output (monitor);
  *width_mm = output->width_mm;
  *height_mm = output->height_mm;
}

CoglSubpixelOrder
meta_monitor_get_subpixel_order (MetaMonitor *monitor)
{
  MetaOutput *output;

  output = meta_monitor_get_main_output (monitor);
  return output->subpixel_order;
}

const char *
meta_monitor_get_connector (MetaMonitor *monitor)
{
  MetaOutput *output;

  output = meta_monitor_get_main_output (monitor);
  return output->name;
}

const char *
meta_monitor_get_vendor (MetaMonitor *monitor)
{
  MetaOutput *output;

  output = meta_monitor_get_main_output (monitor);
  return output->vendor;
}

const char *
meta_monitor_get_product (MetaMonitor *monitor)
{
  MetaOutput *output;

  output = meta_monitor_get_main_output (monitor);
  return output->product;
}

const char *
meta_monitor_get_serial (MetaMonitor *monitor)
{
  MetaOutput *output;

  output = meta_monitor_get_main_output (monitor);
  return output->serial;
}

MetaConnectorType
meta_monitor_get_connector_type (MetaMonitor *monitor)
{
  MetaOutput *output;

  output = meta_monitor_get_main_output (monitor);
  return output->connector_type;
}

static void
meta_monitor_finalize (GObject *object)
{
  MetaMonitor *monitor = META_MONITOR (object);
  MetaMonitorPrivate *priv = meta_monitor_get_instance_private (monitor);

  g_list_free_full (priv->modes, (GDestroyNotify) meta_monitor_mode_free);
  g_clear_pointer (&priv->outputs, g_list_free);
  meta_monitor_spec_free (priv->spec);
}

static void
meta_monitor_init (MetaMonitor *monitor)
{
}

static void
meta_monitor_class_init (MetaMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_monitor_finalize;
}

static void
meta_monitor_normal_generate_modes (MetaMonitorNormal *monitor_normal)
{
  MetaMonitor *monitor = META_MONITOR (monitor_normal);
  MetaMonitorPrivate *monitor_priv =
    meta_monitor_get_instance_private (monitor);
  MetaOutput *output;
  unsigned int i;

  output = meta_monitor_get_main_output (monitor);
  for (i = 0; i < output->n_modes; i++)
    {
      MetaCrtcMode *crtc_mode = output->modes[i];
      MetaMonitorMode *mode;

      mode = g_new0 (MetaMonitorMode, 1);
      mode->spec = (MetaMonitorModeSpec) {
        .width = crtc_mode->width,
        .height = crtc_mode->height,
        .refresh_rate = crtc_mode->refresh_rate
      },
      mode->crtc_modes = g_new (MetaMonitorCrtcMode, 1);
      mode->crtc_modes[0] = (MetaMonitorCrtcMode) {
        .output = output,
        .crtc_mode = crtc_mode
      };

      if (crtc_mode == output->preferred_mode)
        monitor_priv->preferred_mode = mode;
      if (output->crtc && crtc_mode == output->crtc->current_mode)
        monitor_priv->current_mode = mode;

      monitor_priv->modes = g_list_append (monitor_priv->modes, mode);
    }
}

MetaMonitorNormal *
meta_monitor_normal_new (MetaOutput *output)
{
  MetaMonitorNormal *monitor_normal;
  MetaMonitor *monitor;
  MetaMonitorPrivate *monitor_priv;

  monitor_normal = g_object_new (META_TYPE_MONITOR_NORMAL, NULL);
  monitor = META_MONITOR (monitor_normal);
  monitor_priv = meta_monitor_get_instance_private (monitor);

  monitor_priv->outputs = g_list_append (NULL, output);
  monitor_priv->winsys_id = output->winsys_id;

  meta_monitor_normal_generate_modes (monitor_normal);
  meta_monitor_generate_spec (monitor);

  return monitor_normal;
}

static MetaOutput *
meta_monitor_normal_get_main_output (MetaMonitor *monitor)
{
  MetaMonitorPrivate *monitor_priv =
    meta_monitor_get_instance_private (monitor);

  return monitor_priv->outputs->data;
}

static void
meta_monitor_normal_derive_dimensions (MetaMonitor   *monitor,
                                       int           *width,
                                       int           *height)
{
  MetaOutput *output;

  output = meta_monitor_get_main_output (monitor);
  *width = output->crtc->rect.width;
  *height = output->crtc->rect.height;
}

static void
meta_monitor_normal_calculate_crtc_pos (MetaMonitor         *monitor,
                                        MetaMonitorMode     *monitor_mode,
                                        MetaOutput          *output,
                                        MetaMonitorTransform crtc_transform,
                                        int                 *out_x,
                                        int                 *out_y)
{
  *out_x = 0;
  *out_y = 0;
}

static void
meta_monitor_normal_init (MetaMonitorNormal *monitor)
{
}

static void
meta_monitor_normal_class_init (MetaMonitorNormalClass *klass)
{
  MetaMonitorClass *monitor_class = META_MONITOR_CLASS (klass);

  monitor_class->get_main_output = meta_monitor_normal_get_main_output;
  monitor_class->derive_dimensions = meta_monitor_normal_derive_dimensions;
  monitor_class->calculate_crtc_pos = meta_monitor_normal_calculate_crtc_pos;
}

uint32_t
meta_monitor_tiled_get_tile_group_id (MetaMonitorTiled *monitor_tiled)
{
  return monitor_tiled->tile_group_id;
}

gboolean
meta_monitor_get_suggested_position (MetaMonitor *monitor,
                                     int         *x,
                                     int         *y)
{
  MetaOutput *main_output;

  main_output = meta_monitor_get_main_output (monitor);
  if (main_output->suggested_x < 0 && main_output->suggested_y < 0)
    return FALSE;

  *x = main_output->suggested_x;
  *y = main_output->suggested_y;

  return TRUE;
}

static void
add_tiled_monitor_outputs (MetaMonitorManager *monitor_manager,
                           MetaMonitorTiled   *monitor_tiled)
{
  MetaMonitorPrivate *monitor_priv =
    meta_monitor_get_instance_private (META_MONITOR (monitor_tiled));
  unsigned int i;

  for (i = 0; i < monitor_manager->n_outputs; i++)
    {
      MetaOutput *output = &monitor_manager->outputs[i];

      if (output->tile_info.group_id != monitor_tiled->tile_group_id)
        continue;

      g_warn_if_fail (output->subpixel_order ==
                      monitor_tiled->main_output->subpixel_order);

      monitor_priv->outputs = g_list_append (monitor_priv->outputs, output);
    }
}

static void
calculate_tile_coordinate (MetaMonitor         *monitor,
                           MetaOutput          *output,
                           MetaMonitorTransform crtc_transform,
                           int                 *out_x,
                           int                 *out_y)
{
  MetaMonitorPrivate *monitor_priv =
    meta_monitor_get_instance_private (monitor);
  GList *l;
  int x = 0;
  int y = 0;

  for (l = monitor_priv->outputs; l; l = l->next)
    {
      MetaOutput *other_output = l->data;

      switch (crtc_transform)
        {
        case META_MONITOR_TRANSFORM_NORMAL:
        case META_MONITOR_TRANSFORM_FLIPPED:
          if (other_output->tile_info.loc_v_tile == output->tile_info.loc_v_tile &&
              other_output->tile_info.loc_h_tile < output->tile_info.loc_h_tile)
            x += other_output->tile_info.tile_w;
          if (other_output->tile_info.loc_h_tile == output->tile_info.loc_h_tile &&
              other_output->tile_info.loc_v_tile < output->tile_info.loc_v_tile)
            y += other_output->tile_info.tile_h;
          break;
        case META_MONITOR_TRANSFORM_180:
        case META_MONITOR_TRANSFORM_FLIPPED_180:
          if (other_output->tile_info.loc_v_tile == output->tile_info.loc_v_tile &&
              other_output->tile_info.loc_h_tile > output->tile_info.loc_h_tile)
            x += other_output->tile_info.tile_w;
          if (other_output->tile_info.loc_h_tile == output->tile_info.loc_h_tile &&
              other_output->tile_info.loc_v_tile > output->tile_info.loc_v_tile)
            y += other_output->tile_info.tile_h;
          break;
        case META_MONITOR_TRANSFORM_270:
        case META_MONITOR_TRANSFORM_FLIPPED_270:
          if (other_output->tile_info.loc_v_tile == output->tile_info.loc_v_tile &&
              other_output->tile_info.loc_h_tile < output->tile_info.loc_h_tile)
            y += other_output->tile_info.tile_w;
          if (other_output->tile_info.loc_h_tile == output->tile_info.loc_h_tile &&
              other_output->tile_info.loc_v_tile < output->tile_info.loc_v_tile)
            x += other_output->tile_info.tile_h;
          break;
        case META_MONITOR_TRANSFORM_90:
        case META_MONITOR_TRANSFORM_FLIPPED_90:
          if (other_output->tile_info.loc_v_tile == output->tile_info.loc_v_tile &&
              other_output->tile_info.loc_h_tile > output->tile_info.loc_h_tile)
            y += other_output->tile_info.tile_w;
          if (other_output->tile_info.loc_h_tile == output->tile_info.loc_h_tile &&
              other_output->tile_info.loc_v_tile > output->tile_info.loc_v_tile)
            x += other_output->tile_info.tile_h;
          break;
        }
    }

  *out_x = x;
  *out_y = y;
}

static void
meta_monitor_tiled_calculate_tiled_size (MetaMonitor *monitor,
                                         int         *out_width,
                                         int         *out_height)
{
  MetaMonitorPrivate *monitor_priv =
    meta_monitor_get_instance_private (monitor);
  GList *l;
  int width;
  int height;

  width = 0;
  height = 0;
  for (l = monitor_priv->outputs; l; l = l->next)
    {
      MetaOutput *output = l->data;

      if (output->tile_info.loc_v_tile == 0)
        width += output->tile_info.tile_w;

      if (output->tile_info.loc_h_tile == 0)
        height += output->tile_info.tile_h;
    }

  *out_width = width;
  *out_height = height;
}

static gboolean
is_monitor_mode_assigned (MetaMonitor     *monitor,
                          MetaMonitorMode *mode)
{
  MetaMonitorPrivate *priv = meta_monitor_get_instance_private (monitor);
  GList *l;
  int i;

  for (l = priv->outputs, i = 0; l; l = l->next, i++)
    {
      MetaOutput *output = l->data;
      MetaMonitorCrtcMode *monitor_crtc_mode = &mode->crtc_modes[i];

      if (monitor_crtc_mode->crtc_mode &&
          (!output->crtc ||
           output->crtc->current_mode != monitor_crtc_mode->crtc_mode))
        return FALSE;
      else if (!monitor_crtc_mode->crtc_mode && output->crtc)
        return FALSE;
    }

  return TRUE;
}

static MetaMonitorMode *
create_tiled_monitor_mode (MetaMonitorTiled *monitor_tiled)
{
  MetaMonitor *monitor = META_MONITOR (monitor_tiled);
  MetaMonitorPrivate *monitor_priv =
    meta_monitor_get_instance_private (monitor);
  MetaMonitorModeTiled *mode;
  GList *l;
  int i;

  mode = g_new0 (MetaMonitorModeTiled, 1);
  mode->is_tiled = TRUE;
  meta_monitor_tiled_calculate_tiled_size (monitor,
                                           &mode->parent.spec.width,
                                           &mode->parent.spec.height);
  mode->parent.crtc_modes = g_new0 (MetaMonitorCrtcMode,
                                    g_list_length (monitor_priv->outputs));
  for (l = monitor_priv->outputs, i = 0; l; l = l->next, i++)
    {
      MetaOutput *output = l->data;
      MetaCrtcMode *preferred_crtc_mode = output->preferred_mode;

      mode->parent.crtc_modes[i] = (MetaMonitorCrtcMode) {
        .output = output,
        .crtc_mode = preferred_crtc_mode
      };

      g_warn_if_fail (mode->parent.spec.refresh_rate == 0.0f ||
                      (mode->parent.spec.refresh_rate ==
                       preferred_crtc_mode->refresh_rate));

      mode->parent.spec.refresh_rate = preferred_crtc_mode->refresh_rate;
    }

  return &mode->parent;
}

static MetaMonitorMode *
create_untiled_monitor_mode (MetaMonitorTiled *monitor_tiled,
                             MetaOutput       *main_output,
                             MetaCrtcMode     *crtc_mode)
{
  MetaMonitor *monitor = META_MONITOR (monitor_tiled);
  MetaMonitorPrivate *monitor_priv =
    meta_monitor_get_instance_private (monitor);
  MetaMonitorModeTiled *mode;
  GList *l;
  int i;

  /*
   * Assume modes with a resolution identical to the tile sizes are tiled
   * modes.
   */
  if (crtc_mode->width == (int) main_output->tile_info.tile_w &&
      crtc_mode->height == (int) main_output->tile_info.tile_h)
    return NULL;

  mode = g_new0 (MetaMonitorModeTiled, 1);

  mode->is_tiled = FALSE;
  mode->parent.spec = (MetaMonitorModeSpec) {
    .width = crtc_mode->width,
    .height = crtc_mode->height,
    .refresh_rate = crtc_mode->refresh_rate
  };
  mode->parent.crtc_modes = g_new0 (MetaMonitorCrtcMode,
                             g_list_length (monitor_priv->outputs));

  for (l = monitor_priv->outputs, i = 0; l; l = l->next, i++)
    {
      MetaOutput *output = l->data;

      if (output == main_output)
        {
          mode->parent.crtc_modes[i] = (MetaMonitorCrtcMode) {
            .output = output,
            .crtc_mode = crtc_mode
          };
        }
      else
        {
          mode->parent.crtc_modes[i] = (MetaMonitorCrtcMode) {
            .output = output,
            .crtc_mode = NULL
          };
        }
    }

  return &mode->parent;
}

static void
meta_monitor_tiled_generate_modes (MetaMonitorTiled *monitor_tiled)
{
  MetaMonitor *monitor = META_MONITOR (monitor_tiled);
  MetaMonitorPrivate *monitor_priv =
    meta_monitor_get_instance_private (monitor);
  MetaMonitorMode *mode;
  MetaOutput *main_output;
  unsigned int i;

  mode = create_tiled_monitor_mode (monitor_tiled);
  monitor_priv->modes = g_list_append (monitor_priv->modes, mode);

  monitor_priv->preferred_mode = mode;

  if (is_monitor_mode_assigned (monitor, mode))
    monitor_priv->current_mode = mode;

  main_output = meta_monitor_get_main_output (monitor);
  for (i = 0; i < main_output->n_modes; i++)
    {
      MetaCrtcMode *crtc_mode = main_output->modes[i];

      mode = create_untiled_monitor_mode (monitor_tiled,
                                          main_output,
                                          crtc_mode);
      if (mode)
        {
          monitor_priv->modes = g_list_append (monitor_priv->modes, mode);

          if (is_monitor_mode_assigned (monitor, mode))
            {
              g_assert (!monitor_priv->current_mode);
              monitor_priv->current_mode = mode;
            }
        }
    }
}

MetaMonitorTiled *
meta_monitor_tiled_new (MetaMonitorManager *monitor_manager,
                        MetaOutput         *output)
{
  MetaMonitorTiled *monitor_tiled;
  MetaMonitor *monitor;
  MetaMonitorPrivate *monitor_priv;

  monitor_tiled = g_object_new (META_TYPE_MONITOR_TILED, NULL);
  monitor = META_MONITOR (monitor_tiled);
  monitor_priv = meta_monitor_get_instance_private (monitor);

  monitor_tiled->tile_group_id = output->tile_info.group_id;
  monitor_priv->winsys_id = output->winsys_id;

  monitor_tiled->main_output = output;
  add_tiled_monitor_outputs (monitor_manager, monitor_tiled);

  meta_monitor_manager_tiled_monitor_added (monitor_manager,
                                            META_MONITOR (monitor_tiled));

  meta_monitor_tiled_generate_modes (monitor_tiled);
  meta_monitor_generate_spec (monitor);

  return monitor_tiled;
}

static MetaOutput *
meta_monitor_tiled_get_main_output (MetaMonitor *monitor)
{
  MetaMonitorTiled *monitor_tiled = META_MONITOR_TILED (monitor);

  return monitor_tiled->main_output;
}

static void
meta_monitor_tiled_derive_dimensions (MetaMonitor   *monitor,
                                      int           *out_width,
                                      int           *out_height)
{
  MetaMonitorPrivate *monitor_priv =
    meta_monitor_get_instance_private (monitor);
  GList *l;
  int min_x, min_y, max_x, max_y;

  min_x = INT_MAX;
  min_y = INT_MAX;
  max_x = 0;
  max_y = 0;
  for (l = monitor_priv->outputs; l; l = l->next)
    {
      MetaOutput *output = l->data;

      if (!output->crtc)
        continue;

      min_x = MIN (output->crtc->rect.x, min_x);
      min_y = MIN (output->crtc->rect.y, min_y);
      max_x = MAX (output->crtc->rect.x + output->crtc->rect.width, max_x);
      max_y = MAX (output->crtc->rect.y + output->crtc->rect.height, max_y);
    }

  *out_width = max_x - min_x;
  *out_height = max_y - min_y;
}

static void
meta_monitor_tiled_calculate_crtc_pos (MetaMonitor         *monitor,
                                       MetaMonitorMode     *monitor_mode,
                                       MetaOutput          *output,
                                       MetaMonitorTransform crtc_transform,
                                       int                 *out_x,
                                       int                 *out_y)
{
  MetaMonitorModeTiled *mode_tiled = (MetaMonitorModeTiled *) monitor_mode;

  if (mode_tiled->is_tiled)
    {
      calculate_tile_coordinate (monitor, output, crtc_transform,
                                 out_x, out_y);
    }
  else
    {
      *out_x = 0;
      *out_y = 0;
    }
}

static void
meta_monitor_tiled_finalize (GObject *object)
{
  MetaMonitorTiled *monitor_tiled = META_MONITOR_TILED (object);
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);

  meta_monitor_manager_tiled_monitor_removed (monitor_manager,
                                              META_MONITOR (monitor_tiled));
}

static void
meta_monitor_tiled_init (MetaMonitorTiled *monitor)
{
}

static void
meta_monitor_tiled_class_init (MetaMonitorTiledClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MetaMonitorClass *monitor_class = META_MONITOR_CLASS (klass);

  object_class->finalize = meta_monitor_tiled_finalize;

  monitor_class->get_main_output = meta_monitor_tiled_get_main_output;
  monitor_class->derive_dimensions = meta_monitor_tiled_derive_dimensions;
  monitor_class->calculate_crtc_pos = meta_monitor_tiled_calculate_crtc_pos;
}

static void
meta_monitor_mode_free (MetaMonitorMode *monitor_mode)
{
  g_free (monitor_mode->crtc_modes);
  g_free (monitor_mode);
}

MetaMonitorSpec *
meta_monitor_get_spec (MetaMonitor *monitor)
{
  MetaMonitorPrivate *priv = meta_monitor_get_instance_private (monitor);

  return priv->spec;
}

MetaLogicalMonitor *
meta_monitor_get_logical_monitor (MetaMonitor *monitor)
{
  MetaOutput *output = meta_monitor_get_main_output (monitor);

  if (output->crtc)
    return output->crtc->logical_monitor;
  else
    return NULL;
}

static gboolean
meta_monitor_mode_spec_equals (MetaMonitorModeSpec *monitor_mode_spec,
                               MetaMonitorModeSpec *other_monitor_mode_spec)
{
  return (monitor_mode_spec->width == other_monitor_mode_spec->width &&
          monitor_mode_spec->height == other_monitor_mode_spec->height &&
          (monitor_mode_spec->refresh_rate ==
           other_monitor_mode_spec->refresh_rate));
}

MetaMonitorMode *
meta_monitor_get_mode_from_spec (MetaMonitor         *monitor,
                                 MetaMonitorModeSpec *monitor_mode_spec)
{
  MetaMonitorPrivate *priv = meta_monitor_get_instance_private (monitor);
  GList *l;

  for (l = priv->modes; l; l = l->next)
    {
      MetaMonitorMode *monitor_mode = l->data;

      if (meta_monitor_mode_spec_equals (monitor_mode_spec,
                                         &monitor_mode->spec))
        return monitor_mode;
    }

  return NULL;
}

MetaMonitorMode *
meta_monitor_get_preferred_mode (MetaMonitor *monitor)
{
  MetaMonitorPrivate *priv = meta_monitor_get_instance_private (monitor);

  return priv->preferred_mode;
}

MetaMonitorMode *
meta_monitor_get_current_mode (MetaMonitor *monitor)
{
  MetaMonitorPrivate *priv = meta_monitor_get_instance_private (monitor);

  return priv->current_mode;
}

void
meta_monitor_derive_current_mode (MetaMonitor *monitor)
{
  MetaMonitorPrivate *priv = meta_monitor_get_instance_private (monitor);
  MetaMonitorMode *current_mode = NULL;
  GList *l;

  for (l = priv->modes; l; l = l->next)
    {
      MetaMonitorMode *mode = l->data;

      if (is_monitor_mode_assigned (monitor, mode))
        {
          current_mode = mode;
          break;
        }
    }

  priv->current_mode = current_mode;
}

void
meta_monitor_set_current_mode (MetaMonitor     *monitor,
                               MetaMonitorMode *mode)
{
  MetaMonitorPrivate *priv = meta_monitor_get_instance_private (monitor);

  priv->current_mode = mode;
}

GList *
meta_monitor_get_modes (MetaMonitor *monitor)
{
  MetaMonitorPrivate *priv = meta_monitor_get_instance_private (monitor);

  return priv->modes;
}

void
meta_monitor_calculate_crtc_pos (MetaMonitor         *monitor,
                                 MetaMonitorMode     *monitor_mode,
                                 MetaOutput          *output,
                                 MetaMonitorTransform crtc_transform,
                                 int                 *out_x,
                                 int                 *out_y)
{
  META_MONITOR_GET_CLASS (monitor)->calculate_crtc_pos (monitor,
                                                        monitor_mode,
                                                        output,
                                                        crtc_transform,
                                                        out_x,
                                                        out_y);
}

/* The minimum resolution at which we turn on a window-scale of 2 */
#define HIDPI_LIMIT 192

/*
 * The minimum screen height at which we turn on a window-scale of 2;
 * below this there just isn't enough vertical real estate for GNOME
 * apps to work, and it's better to just be tiny
 */
#define HIDPI_MIN_HEIGHT 1200

/* From http://en.wikipedia.org/wiki/4K_resolution#Resolutions_of_common_formats */
#define SMALLEST_4K_WIDTH 3656

static int
calculate_scale (MetaMonitor     *monitor,
                 MetaMonitorMode *monitor_mode)
{
  int resolution_width, resolution_height;
  int width_mm, height_mm;
  int scale;

  scale = 1;

  meta_monitor_mode_get_resolution (monitor_mode,
                                    &resolution_width,
                                    &resolution_height);

  if (resolution_height < HIDPI_MIN_HEIGHT)
    goto out;

  /* 4K TV */
  switch (meta_monitor_get_connector_type (monitor))
    {
    case META_CONNECTOR_TYPE_HDMIA:
    case META_CONNECTOR_TYPE_HDMIB:
      if (resolution_width >= SMALLEST_4K_WIDTH)
        goto out;
      break;
    default:
      break;
    }

  meta_monitor_get_physical_dimensions (monitor, &width_mm, &height_mm);

  /*
   * Somebody encoded the aspect ratio (16/9 or 16/10) instead of the physical
   * size.
   */
  if ((width_mm == 160 && height_mm == 90) ||
      (width_mm == 160 && height_mm == 100) ||
      (width_mm == 16 && height_mm == 9) ||
      (width_mm == 16 && height_mm == 10))
    goto out;

  if (width_mm > 0 && height_mm > 0)
    {
      double dpi_x, dpi_y;

      dpi_x = (double) resolution_width / (width_mm / 25.4);
      dpi_y = (double) resolution_height / (height_mm / 25.4);

      /*
       * We don't completely trust these values so both must be high, and never
       * pick higher ratio than 2 automatically.
       */
      if (dpi_x > HIDPI_LIMIT && dpi_y > HIDPI_LIMIT)
        scale = 2;
    }

out:
  return scale;
}

int
meta_monitor_calculate_mode_scale (MetaMonitor     *monitor,
                                   MetaMonitorMode *monitor_mode)
{
  MetaBackend *backend = meta_get_backend ();
  MetaSettings *settings = meta_backend_get_settings (backend);
  int global_scaling_factor;

  if (meta_settings_get_global_scaling_factor (settings,
                                               &global_scaling_factor))
    return global_scaling_factor;

  return calculate_scale (monitor, monitor_mode);
}

MetaMonitorModeSpec *
meta_monitor_mode_get_spec (MetaMonitorMode *monitor_mode)
{
  return &monitor_mode->spec;
}

void
meta_monitor_mode_get_resolution (MetaMonitorMode *monitor_mode,
                                  int             *width,
                                  int             *height)
{
  *width = monitor_mode->spec.width;
  *height = monitor_mode->spec.height;
}

float
meta_monitor_mode_get_refresh_rate (MetaMonitorMode *monitor_mode)
{
  return monitor_mode->spec.refresh_rate;
}

gboolean
meta_monitor_mode_foreach_crtc (MetaMonitor        *monitor,
                                MetaMonitorMode    *mode,
                                MetaMonitorModeFunc func,
                                gpointer            user_data,
                                GError            **error)
{
  MetaMonitorPrivate *monitor_priv =
    meta_monitor_get_instance_private (monitor);
  GList *l;
  int i;

  for (l = monitor_priv->outputs, i = 0; l; l = l->next, i++)
    {
      MetaMonitorCrtcMode *monitor_crtc_mode = &mode->crtc_modes[i];

      if (!func (monitor, mode, monitor_crtc_mode, user_data, error))
        return FALSE;
    }

  return TRUE;
}
