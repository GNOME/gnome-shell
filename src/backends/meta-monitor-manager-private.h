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

#ifndef META_MONITOR_MANAGER_PRIVATE_H
#define META_MONITOR_MANAGER_PRIVATE_H

#include <cogl/cogl.h>
#include <libgnome-desktop/gnome-pnp-ids.h>
#include <libupower-glib/upower.h>

#include "display-private.h"
#include <meta/screen.h>
#include "stack-tracker.h"
#include <meta/meta-monitor-manager.h>

#include "meta-display-config-shared.h"
#include "meta-dbus-display-config.h"
#include "meta-cursor.h"

typedef struct _MetaMonitorConfig MetaMonitorConfig;
typedef struct _MetaMonitorConfigManager MetaMonitorConfigManager;
typedef struct _MetaMonitorConfigStore MetaMonitorConfigStore;
typedef struct _MetaMonitorsConfig MetaMonitorsConfig;

typedef struct _MetaMonitor MetaMonitor;
typedef struct _MetaMonitorNormal MetaMonitorNormal;
typedef struct _MetaMonitorTiled MetaMonitorTiled;
typedef struct _MetaMonitorSpec MetaMonitorSpec;
typedef struct _MetaLogicalMonitor MetaLogicalMonitor;

typedef struct _MetaMonitorMode MetaMonitorMode;

typedef struct _MetaCrtc MetaCrtc;
typedef struct _MetaOutput MetaOutput;
typedef struct _MetaCrtcMode MetaCrtcMode;
typedef struct _MetaCrtcInfo MetaCrtcInfo;
typedef struct _MetaOutputInfo MetaOutputInfo;
typedef struct _MetaTileInfo MetaTileInfo;

typedef enum _MetaMonitorManagerCapability
{
  META_MONITOR_MANAGER_CAPABILITY_NONE = 0,
  META_MONITOR_MANAGER_CAPABILITY_MIRRORING = (1 << 0),
  META_MONITOR_MANAGER_CAPABILITY_LAYOUT_MODE = (1 << 1),
  META_MONITOR_MANAGER_CAPABILITY_GLOBAL_SCALE_REQUIRED = (1 << 2)
} MetaMonitorManagerCapability;

/* Equivalent to the 'method' enum in org.gnome.Mutter.DisplayConfig */
typedef enum _MetaMonitorsConfigMethod
{
  META_MONITORS_CONFIG_METHOD_VERIFY = 0,
  META_MONITORS_CONFIG_METHOD_TEMPORARY = 1,
  META_MONITORS_CONFIG_METHOD_PERSISTENT = 2
} MetaMonitorsConfigMethod;

/* Equivalent to the 'layout-mode' enum in org.gnome.Mutter.DisplayConfig */
typedef enum _MetaLogicalMonitorLayoutMode
{
  META_LOGICAL_MONITOR_LAYOUT_MODE_LOGICAL = 1,
  META_LOGICAL_MONITOR_LAYOUT_MODE_PHYSICAL = 2
} MetaLogicalMonitorLayoutMode;

typedef enum _MetaMonitorManagerDeriveFlag
{
  META_MONITOR_MANAGER_DERIVE_FLAG_NONE = 0,
  META_MONITOR_MANAGER_DERIVE_FLAG_CONFIGURED_SCALE = (1 << 0)
} MetaMonitorManagerDeriveFlag;

typedef enum
{
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

struct _MetaOutput
{
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

  /* The low-level bits used to build the high-level info
     in MetaLogicalMonitor

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

struct _MetaCrtc
{
  glong crtc_id;
  MetaRectangle rect;
  MetaCrtcMode *current_mode;
  MetaMonitorTransform transform;
  unsigned int all_transforms;

  /* Only used to build the logical configuration
     from the HW one
  */
  MetaLogicalMonitor *logical_monitor;

  /* Used when changing configuration */
  gboolean is_dirty;

  /* Used by cursor renderer backend */
  void *cursor_renderer_private;

  gpointer driver_private;
  GDestroyNotify driver_notify;
};

struct _MetaCrtcMode
{
  /* The low-level ID of this mode, used to apply back configuration */
  glong mode_id;
  char *name;

  int width;
  int height;
  float refresh_rate;
  guint32 flags;

  gpointer driver_private;
  GDestroyNotify driver_notify;
};

/*
 * MetaCrtcInfo:
 * This represents the writable part of a CRTC, as deserialized from DBus
 * or built by MetaMonitorConfig
 *
 * Note: differently from the other structures in this file, MetaCrtcInfo
 * is handled by pointer. This is to accomodate the usage in MetaMonitorConfig
 */
struct _MetaCrtcInfo
{
  MetaCrtc                 *crtc;
  MetaCrtcMode             *mode;
  int                       x;
  int                       y;
  MetaMonitorTransform      transform;
  GPtrArray                *outputs;
};

/*
 * MetaOutputInfo:
 * this is the same as MetaCrtcInfo, but for outputs
 */
struct _MetaOutputInfo
{
  MetaOutput  *output;
  gboolean     is_primary;
  gboolean     is_presentation;
  gboolean     is_underscanning;
};

typedef enum _MetaMonitorConfigSystem
{
  META_MONITOR_CONFIG_SYSTEM_LEGACY,
  META_MONITOR_CONFIG_SYSTEM_MANAGER
} MetaMonitorConfigSystem;

#define META_TYPE_MONITOR_MANAGER            (meta_monitor_manager_get_type ())
#define META_MONITOR_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_MONITOR_MANAGER, MetaMonitorManager))
#define META_MONITOR_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_MONITOR_MANAGER, MetaMonitorManagerClass))
#define META_IS_MONITOR_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_MONITOR_MANAGER))
#define META_IS_MONITOR_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_MONITOR_MANAGER))
#define META_MONITOR_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_MONITOR_MANAGER, MetaMonitorManagerClass))

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MetaMonitorManager, g_object_unref)

struct _MetaMonitorManager
{
  MetaDBusDisplayConfigSkeleton parent_instance;

  /* XXX: this structure is very badly
     packed, but I like the logical organization
     of fields */

  gboolean in_init;
  unsigned int serial;

  MetaPowerSave power_save_mode;

  MetaLogicalMonitorLayoutMode layout_mode;

  int screen_width;
  int screen_height;

  /* Outputs refer to physical screens,
     CRTCs refer to stuff that can drive outputs
     (like encoders, but less tied to the HW),
     while logical_monitors refer to logical ones.
  */
  MetaOutput *outputs;
  unsigned int n_outputs;

  MetaCrtcMode *modes;
  unsigned int n_modes;

  MetaCrtc *crtcs;
  unsigned int n_crtcs;

  GList *monitors;

  GList *logical_monitors;
  MetaLogicalMonitor *primary_logical_monitor;

  int dbus_name_id;

  MetaMonitorConfigSystem pending_persistent_system;
  int persistent_timeout_id;

  MetaMonitorConfig *legacy_config;

  MetaMonitorConfigManager *config_manager;

  GnomePnpIds *pnp_ids;
  UpClient *up_client;

  gulong experimental_features_changed_handler_id;
};

struct _MetaMonitorManagerClass
{
  MetaDBusDisplayConfigSkeletonClass parent_class;

  void (*read_current) (MetaMonitorManager *);

  char* (*get_edid_file) (MetaMonitorManager *,
                          MetaOutput         *);
  GBytes* (*read_edid) (MetaMonitorManager *,
                        MetaOutput         *);

  gboolean (*is_lid_closed) (MetaMonitorManager *);

  void (*ensure_initial_config) (MetaMonitorManager *);

  gboolean (*apply_monitors_config) (MetaMonitorManager      *,
                                     MetaMonitorsConfig      *,
                                     MetaMonitorsConfigMethod ,
                                     GError                 **);

  void (*apply_configuration) (MetaMonitorManager  *,
                               MetaCrtcInfo       **,
                               unsigned int         ,
                               MetaOutputInfo     **,
                               unsigned int);

  void (*set_power_save_mode) (MetaMonitorManager *,
                               MetaPowerSave);

  void (*change_backlight) (MetaMonitorManager *,
                            MetaOutput         *,
                            int);

  void (*get_crtc_gamma) (MetaMonitorManager  *,
                          MetaCrtc            *,
                          gsize               *,
                          unsigned short     **,
                          unsigned short     **,
                          unsigned short     **);
  void (*set_crtc_gamma) (MetaMonitorManager *,
                          MetaCrtc           *,
                          gsize               ,
                          unsigned short     *,
                          unsigned short     *,
                          unsigned short     *);

  void (*tiled_monitor_added) (MetaMonitorManager *,
                               MetaMonitor        *);

  void (*tiled_monitor_removed) (MetaMonitorManager *,
                                 MetaMonitor        *);

  gboolean (*is_transform_handled) (MetaMonitorManager  *,
                                    MetaCrtc            *,
                                    MetaMonitorTransform);

  float (*calculate_monitor_mode_scale) (MetaMonitorManager *,
                                         MetaMonitor        *,
                                         MetaMonitorMode    *);

  void (*get_supported_scales) (MetaMonitorManager          *,
                                MetaLogicalMonitorLayoutMode ,
                                float                      **,
                                int                         *);

  MetaMonitorManagerCapability (*get_capabilities) (MetaMonitorManager *);

  gboolean (*get_max_screen_size) (MetaMonitorManager *,
                                   int                *,
                                   int                *);

  MetaLogicalMonitorLayoutMode (*get_default_layout_mode) (MetaMonitorManager *);
};

gboolean            meta_is_monitor_config_manager_enabled (void);

void                meta_monitor_manager_rebuild (MetaMonitorManager *manager,
                                                  MetaMonitorsConfig *config);
void                meta_monitor_manager_rebuild_derived (MetaMonitorManager          *manager,
                                                          MetaMonitorManagerDeriveFlag flags);

int                 meta_monitor_manager_get_num_logical_monitors (MetaMonitorManager *manager);

GList *             meta_monitor_manager_get_logical_monitors (MetaMonitorManager *manager);

MetaLogicalMonitor *meta_monitor_manager_get_logical_monitor_from_number (MetaMonitorManager *manager,
                                                                          int                 number);

MetaLogicalMonitor *meta_monitor_manager_get_primary_logical_monitor (MetaMonitorManager *manager);

MetaLogicalMonitor *meta_monitor_manager_get_logical_monitor_at (MetaMonitorManager *manager,
                                                                 float               x,
                                                                 float               y);

MetaLogicalMonitor *meta_monitor_manager_get_logical_monitor_from_rect (MetaMonitorManager *manager,
                                                                        MetaRectangle      *rect);

MetaLogicalMonitor *meta_monitor_manager_get_logical_monitor_neighbor (MetaMonitorManager *manager,
                                                                       MetaLogicalMonitor *logical_monitor,
                                                                       MetaScreenDirection direction);

MetaMonitor *       meta_monitor_manager_get_primary_monitor (MetaMonitorManager *manager);

MetaMonitor *       meta_monitor_manager_get_laptop_panel (MetaMonitorManager *manager);

MetaMonitor *       meta_monitor_manager_get_monitor_from_spec (MetaMonitorManager *manager,
                                                                MetaMonitorSpec    *monitor_spec);

GList *             meta_monitor_manager_get_monitors      (MetaMonitorManager *manager);

MetaOutput         *meta_monitor_manager_get_outputs       (MetaMonitorManager *manager,
							    unsigned int       *n_outputs);

void                meta_monitor_manager_get_resources     (MetaMonitorManager  *manager,
                                                            MetaCrtcMode       **modes,
                                                            unsigned int        *n_modes,
                                                            MetaCrtc           **crtcs,
                                                            unsigned int        *n_crtcs,
                                                            MetaOutput         **outputs,
                                                            unsigned int        *n_outputs);

void                meta_monitor_manager_get_screen_size   (MetaMonitorManager *manager,
                                                            int                *width,
                                                            int                *height);

void                meta_monitor_manager_apply_configuration (MetaMonitorManager  *manager,
                                                              MetaCrtcInfo       **crtcs,
                                                              unsigned int         n_crtcs,
                                                              MetaOutputInfo     **outputs,
                                                              unsigned int         n_outputs);

void                meta_monitor_manager_confirm_configuration (MetaMonitorManager *manager,
                                                                gboolean            ok);

void               meta_output_parse_edid (MetaOutput *output,
                                           GBytes     *edid);
gboolean           meta_output_is_laptop  (MetaOutput *output);

void               meta_crtc_info_free   (MetaCrtcInfo   *info);
void               meta_output_info_free (MetaOutputInfo *info);

gboolean           meta_monitor_manager_has_hotplug_mode_update (MetaMonitorManager *manager);
void               meta_monitor_manager_read_current_state (MetaMonitorManager *manager);
void               meta_monitor_manager_on_hotplug (MetaMonitorManager *manager);

gboolean           meta_monitor_manager_get_monitor_matrix (MetaMonitorManager *manager,
                                                            MetaLogicalMonitor *logical_monitor,
                                                            gfloat              matrix[6]);

void               meta_monitor_manager_tiled_monitor_added (MetaMonitorManager *manager,
                                                             MetaMonitor        *monitor);
void               meta_monitor_manager_tiled_monitor_removed (MetaMonitorManager *manager,
                                                               MetaMonitor        *monitor);

gboolean           meta_monitor_manager_is_transform_handled (MetaMonitorManager  *manager,
                                                              MetaCrtc            *crtc,
                                                              MetaMonitorTransform transform);

MetaMonitorsConfig * meta_monitor_manager_ensure_configured (MetaMonitorManager *manager);

void               meta_monitor_manager_update_logical_state (MetaMonitorManager *manager,
                                                              MetaMonitorsConfig *config);
void               meta_monitor_manager_update_logical_state_derived (MetaMonitorManager          *manager,
                                                                      MetaMonitorManagerDeriveFlag flags);

gboolean           meta_monitor_manager_is_lid_closed (MetaMonitorManager *manager);

void               meta_monitor_manager_lid_is_closed_changed (MetaMonitorManager *manager);

gboolean           meta_monitor_manager_is_headless (MetaMonitorManager *manager);

float              meta_monitor_manager_calculate_monitor_mode_scale (MetaMonitorManager *manager,
                                                                      MetaMonitor        *monitor,
                                                                      MetaMonitorMode    *monitor_mode);

gboolean           meta_monitor_manager_is_scale_supported (MetaMonitorManager          *manager,
                                                            MetaLogicalMonitorLayoutMode layout_mode,
                                                            float                        scale);

MetaMonitorManagerCapability
                   meta_monitor_manager_get_capabilities (MetaMonitorManager *manager);

gboolean           meta_monitor_manager_get_max_screen_size (MetaMonitorManager *manager,
                                                             int                *max_width,
                                                             int                *max_height);

MetaLogicalMonitorLayoutMode
                   meta_monitor_manager_get_default_layout_mode (MetaMonitorManager *manager);

void meta_monitor_manager_clear_output (MetaOutput *output);
void meta_monitor_manager_clear_mode (MetaCrtcMode *mode);
void meta_monitor_manager_clear_crtc (MetaCrtc *crtc);

/* Returns true if transform causes width and height to be inverted
   This is true for the odd transforms in the enum */
static inline gboolean
meta_monitor_transform_is_rotated (MetaMonitorTransform transform)
{
  return (transform % 2);
}

#endif /* META_MONITOR_MANAGER_PRIVATE_H */
