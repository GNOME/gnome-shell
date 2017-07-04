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

#ifndef META_OUTPUT_H
#define META_OUTPUT_H

#include <glib-object.h>

#include "backends/meta-monitor-manager-private.h"

struct _MetaTileInfo
{
  guint32 group_id;
  guint32 flags;
  guint32 max_h_tiles;
  guint32 max_v_tiles;
  guint32 loc_h_tile;
  guint32 loc_v_tile;
  guint32 tile_w;
  guint32 tile_h;
};

/* This matches the values in drm_mode.h */
typedef enum
{
  META_CONNECTOR_TYPE_Unknown = 0,
  META_CONNECTOR_TYPE_VGA = 1,
  META_CONNECTOR_TYPE_DVII = 2,
  META_CONNECTOR_TYPE_DVID = 3,
  META_CONNECTOR_TYPE_DVIA = 4,
  META_CONNECTOR_TYPE_Composite = 5,
  META_CONNECTOR_TYPE_SVIDEO = 6,
  META_CONNECTOR_TYPE_LVDS = 7,
  META_CONNECTOR_TYPE_Component = 8,
  META_CONNECTOR_TYPE_9PinDIN = 9,
  META_CONNECTOR_TYPE_DisplayPort = 10,
  META_CONNECTOR_TYPE_HDMIA = 11,
  META_CONNECTOR_TYPE_HDMIB = 12,
  META_CONNECTOR_TYPE_TV = 13,
  META_CONNECTOR_TYPE_eDP = 14,
  META_CONNECTOR_TYPE_VIRTUAL = 15,
  META_CONNECTOR_TYPE_DSI = 16,
} MetaConnectorType;

struct _MetaOutput
{
  GObject parent;

  MetaMonitorManager *monitor_manager;

  /* The CRTC driving this output, NULL if the output is not enabled */
  MetaCrtc *crtc;

  /* The low-level ID of this output, used to apply back configuration */
  glong winsys_id;
  char *name;
  char *vendor;
  char *product;
  char *serial;
  int width_mm;
  int height_mm;
  CoglSubpixelOrder subpixel_order;

  MetaConnectorType connector_type;

  MetaCrtcMode *preferred_mode;
  MetaCrtcMode **modes;
  unsigned int n_modes;

  MetaCrtc **possible_crtcs;
  unsigned int n_possible_crtcs;

  MetaOutput **possible_clones;
  unsigned int n_possible_clones;

  int backlight;
  int backlight_min;
  int backlight_max;

  /* Used when changing configuration */
  gboolean is_dirty;

  gboolean is_primary;
  gboolean is_presentation;

  gboolean is_underscanning;
  gboolean supports_underscanning;

  gpointer driver_private;
  GDestroyNotify driver_notify;

  /*
   * Get a new preferred mode on hotplug events, to handle dynamic guest
   * resizing.
   */
  gboolean hotplug_mode_update;
  gint suggested_x;
  gint suggested_y;

  MetaTileInfo tile_info;
};

#define META_TYPE_OUTPUT (meta_output_get_type ())
G_DECLARE_FINAL_TYPE (MetaOutput, meta_output, META, OUTPUT, GObject)

MetaMonitorManager * meta_output_get_monitor_manager (MetaOutput *output);

#endif /* META_OUTPUT_H */
