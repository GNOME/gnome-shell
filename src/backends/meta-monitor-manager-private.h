/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * \file screen-private.h  Handling of monitor configuration
 *
 * Managing multiple monitors
 * This file contains structures and functions that handle
 * multiple monitors, including reading the current configuration
 * and available hardware, and applying it.
 *
 * This interface is private to mutter, API users should look
 * at MetaScreen instead.
 */

/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2003 Rob Adams
 * Copyright (C) 2004-2006 Elijah Newren
 * Copyright (C) 2013 Red Hat Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef META_MONITOR_PRIVATE_H
#define META_MONITOR_PRIVATE_H

#include <cogl/cogl.h>
#include <libgnome-desktop/gnome-pnp-ids.h>

#include "display-private.h"
#include <meta/screen.h>
#include "stack-tracker.h"
#include <meta/meta-monitor-manager.h>

#include "meta-display-config-shared.h"
#include "meta-dbus-display-config.h"
#include "meta-cursor.h"

typedef struct _MetaMonitorConfigClass    MetaMonitorConfigClass;
typedef struct _MetaMonitorConfig         MetaMonitorConfig;

typedef struct _MetaCRTC MetaCRTC;
typedef struct _MetaOutput MetaOutput;
typedef struct _MetaMonitorMode MetaMonitorMode;
typedef struct _MetaMonitorInfo MetaMonitorInfo;
typedef struct _MetaCRTCInfo MetaCRTCInfo;
typedef struct _MetaOutputInfo MetaOutputInfo;
typedef struct _MetaTileInfo MetaTileInfo;

typedef enum {
  META_MONITOR_TRANSFORM_NORMAL,
  META_MONITOR_TRANSFORM_90,
  META_MONITOR_TRANSFORM_180,
  META_MONITOR_TRANSFORM_270,
  META_MONITOR_TRANSFORM_FLIPPED,
  META_MONITOR_TRANSFORM_FLIPPED_90,
  META_MONITOR_TRANSFORM_FLIPPED_180,
  META_MONITOR_TRANSFORM_FLIPPED_270,
} MetaMonitorTransform;

/* This matches the values in drm_mode.h */
typedef enum {
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

struct _MetaTileInfo {
  guint32 group_id;
  guint32 flags;
  guint32 max_h_tiles;
  guint32 max_v_tiles;
  guint32 loc_h_tile;
  guint32 loc_v_tile;
  guint32 tile_w;
  guint32 tile_h;
};

struct _MetaOutput
{
  /* The CRTC driving this output, NULL if the output is not enabled */
  MetaCRTC *crtc;
  /* The low-level ID of this output, used to apply back configuration */
  glong winsys_id;
  char *name;
  char *vendor;
  char *product;
  char *serial;
  int width_mm;
  int height_mm;
  CoglSubpixelOrder subpixel_order;
  int scale;

  MetaConnectorType connector_type;

  MetaMonitorMode *preferred_mode;
  MetaMonitorMode **modes;
  unsigned int n_modes;

  MetaCRTC **possible_crtcs;
  unsigned int n_possible_crtcs;

  MetaOutput **possible_clones;
  unsigned int n_possible_clones;

  int backlight;
  int backlight_min;
  int backlight_max;

  /* Used when changing configuration */
  gboolean is_dirty;

  /* The low-level bits used to build the high-level info
     in MetaMonitorInfo

     XXX: flags maybe?
     There is a lot of code that uses MonitorInfo->is_primary,
     but nobody uses MetaOutput yet
  */
  gboolean is_primary;
  gboolean is_presentation;
  gboolean is_underscanning;
  gboolean supports_underscanning;

  gpointer driver_private;
  GDestroyNotify driver_notify;

  /* get a new preferred mode on hotplug events, to handle dynamic guest resizing */
  gboolean hotplug_mode_update;
  gint suggested_x;
  gint suggested_y;

  MetaTileInfo tile_info;
};

struct _MetaCRTC
{
  glong crtc_id;
  MetaRectangle rect;
  MetaMonitorMode *current_mode;
  MetaMonitorTransform transform;
  unsigned int all_transforms;

  /* Only used to build the logical configuration
     from the HW one
  */
  MetaMonitorInfo *logical_monitor;

  /* Used when changing configuration */
  gboolean is_dirty;

  /* Used by cursor renderer backend */
  void *cursor_renderer_private;

  gpointer driver_private;
  GDestroyNotify driver_notify;
};

struct _MetaMonitorMode
{
  /* The low-level ID of this mode, used to apply back configuration */
  glong mode_id;
  char *name;

  int width;
  int height;
  float refresh_rate;

  gpointer driver_private;
  GDestroyNotify driver_notify;
};

#define META_MAX_OUTPUTS_PER_MONITOR 4
/**
 * MetaMonitorInfo:
 *
 * A structure with high-level information about monitors.
 * This corresponds to a subset of the compositor coordinate space.
 * Clones are only reported once, irrespective of the way
 * they're implemented (two CRTCs configured for the same
 * coordinates or one CRTCs driving two outputs). Inactive CRTCs
 * are ignored, and so are disabled outputs.
 */
struct _MetaMonitorInfo
{
  int number;
  int xinerama_index;
  MetaRectangle rect;
  /* for tiled monitors these are calculated, from untiled just copied */
  float refresh_rate;
  int width_mm;
  int height_mm;
  gboolean is_primary;
  gboolean is_presentation; /* XXX: not yet used */
  gboolean in_fullscreen;
  int scale;

  /* The primary or first output for this monitor, 0 if we can't figure out.
     It can be matched to a winsys_id of a MetaOutput.

     This is used as an opaque token on reconfiguration when switching from
     clone to extened, to decide on what output the windows should go next
     (it's an attempt to keep windows on the same monitor, and preferably on
     the primary one).
  */
  glong winsys_id;

  guint32 tile_group_id;

  int monitor_winsys_xid;
  int n_outputs;
  MetaOutput *outputs[META_MAX_OUTPUTS_PER_MONITOR];
};

/*
 * MetaCRTCInfo:
 * This represents the writable part of a CRTC, as deserialized from DBus
 * or built by MetaMonitorConfig
 *
 * Note: differently from the other structures in this file, MetaCRTCInfo
 * is handled by pointer. This is to accomodate the usage in MetaMonitorConfig
 */
struct _MetaCRTCInfo {
  MetaCRTC                 *crtc;
  MetaMonitorMode          *mode;
  int                       x;
  int                       y;
  MetaMonitorTransform      transform;
  GPtrArray                *outputs;
};

/*
 * MetaOutputInfo:
 * this is the same as MetaCRTCInfo, but for outputs
 */
struct _MetaOutputInfo {
  MetaOutput  *output;
  gboolean     is_primary;
  gboolean     is_presentation;
  gboolean     is_underscanning;
};

#define META_TYPE_MONITOR_MANAGER            (meta_monitor_manager_get_type ())
#define META_MONITOR_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_MONITOR_MANAGER, MetaMonitorManager))
#define META_MONITOR_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_MONITOR_MANAGER, MetaMonitorManagerClass))
#define META_IS_MONITOR_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_MONITOR_MANAGER))
#define META_IS_MONITOR_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_MONITOR_MANAGER))
#define META_MONITOR_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_MONITOR_MANAGER, MetaMonitorManagerClass))

struct _MetaMonitorManager
{
  MetaDBusDisplayConfigSkeleton parent_instance;

  /* XXX: this structure is very badly
     packed, but I like the logical organization
     of fields */

  gboolean in_init;
  unsigned int serial;

  MetaPowerSave power_save_mode;

  int max_screen_width;
  int max_screen_height;
  int screen_width;
  int screen_height;

  /* Outputs refer to physical screens,
     CRTCs refer to stuff that can drive outputs
     (like encoders, but less tied to the HW),
     while monitor_infos refer to logical ones.
  */
  MetaOutput *outputs;
  unsigned int n_outputs;

  MetaMonitorMode *modes;
  unsigned int n_modes;

  MetaCRTC *crtcs;
  unsigned int n_crtcs;

  MetaMonitorInfo *monitor_infos;
  unsigned int n_monitor_infos;
  int primary_monitor_index;

  int dbus_name_id;

  int persistent_timeout_id;
  MetaMonitorConfig *config;

  GnomePnpIds *pnp_ids;
};

struct _MetaMonitorManagerClass
{
  MetaDBusDisplayConfigSkeletonClass parent_class;

  void (*read_current) (MetaMonitorManager *);

  char* (*get_edid_file) (MetaMonitorManager *,
                          MetaOutput         *);
  GBytes* (*read_edid) (MetaMonitorManager *,
                        MetaOutput         *);

  void (*apply_configuration) (MetaMonitorManager  *,
                               MetaCRTCInfo       **,
                               unsigned int         ,
                               MetaOutputInfo     **,
                               unsigned int);

  void (*set_power_save_mode) (MetaMonitorManager *,
                               MetaPowerSave);

  void (*change_backlight) (MetaMonitorManager *,
                            MetaOutput         *,
                            int);

  void (*get_crtc_gamma) (MetaMonitorManager  *,
                          MetaCRTC            *,
                          gsize               *,
                          unsigned short     **,
                          unsigned short     **,
                          unsigned short     **);
  void (*set_crtc_gamma) (MetaMonitorManager *,
                          MetaCRTC           *,
                          gsize               ,
                          unsigned short     *,
                          unsigned short     *,
                          unsigned short     *);

  void (*add_monitor) (MetaMonitorManager *,
                       MetaMonitorInfo *);

  void (*delete_monitor) (MetaMonitorManager *,
                          int monitor_winsys_xid);

};

void                meta_monitor_manager_rebuild_derived   (MetaMonitorManager *manager);

MetaMonitorInfo    *meta_monitor_manager_get_monitor_infos (MetaMonitorManager *manager,
							    unsigned int       *n_infos);

MetaOutput         *meta_monitor_manager_get_outputs       (MetaMonitorManager *manager,
							    unsigned int       *n_outputs);

void                meta_monitor_manager_get_resources     (MetaMonitorManager  *manager,
                                                            MetaMonitorMode    **modes,
                                                            unsigned int        *n_modes,
                                                            MetaCRTC           **crtcs,
                                                            unsigned int        *n_crtcs,
                                                            MetaOutput         **outputs,
                                                            unsigned int        *n_outputs);

int                 meta_monitor_manager_get_primary_index (MetaMonitorManager *manager);

void                meta_monitor_manager_get_screen_size   (MetaMonitorManager *manager,
                                                            int                *width,
                                                            int                *height);

void                meta_monitor_manager_get_screen_limits (MetaMonitorManager *manager,
                                                            int                *width,
                                                            int                *height);

void                meta_monitor_manager_apply_configuration (MetaMonitorManager  *manager,
                                                              MetaCRTCInfo       **crtcs,
                                                              unsigned int         n_crtcs,
                                                              MetaOutputInfo     **outputs,
                                                              unsigned int         n_outputs);

void                meta_monitor_manager_confirm_configuration (MetaMonitorManager *manager,
                                                                gboolean            ok);

void               meta_output_parse_edid (MetaOutput *output,
                                           GBytes     *edid);

void               meta_crtc_info_free   (MetaCRTCInfo   *info);
void               meta_output_info_free (MetaOutputInfo *info);

gboolean           meta_monitor_manager_has_hotplug_mode_update (MetaMonitorManager *manager);
void               meta_monitor_manager_read_current_config (MetaMonitorManager *manager);
void               meta_monitor_manager_on_hotplug (MetaMonitorManager *manager);

gboolean           meta_monitor_manager_get_monitor_matrix (MetaMonitorManager *manager,
                                                            MetaOutput         *output,
                                                            gfloat              matrix[6]);

gint               meta_monitor_manager_get_monitor_at_point (MetaMonitorManager *manager,
                                                              gfloat              x,
                                                              gfloat              y);

/* Returns true if transform causes width and height to be inverted
   This is true for the odd transforms in the enum */
static inline gboolean
meta_monitor_transform_is_rotated (MetaMonitorTransform transform)
{
  return (transform % 2);
}

#endif
