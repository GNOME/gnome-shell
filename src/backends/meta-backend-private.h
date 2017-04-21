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


#ifndef META_BACKEND_PRIVATE_H
#define META_BACKEND_PRIVATE_H

#include <glib-object.h>

#include <xkbcommon/xkbcommon.h>

#include <meta/meta-backend.h>
#include <meta/meta-idle-monitor.h>
#include "meta-cursor-renderer.h"
#include "meta-monitor-manager-private.h"
#include "meta-input-settings-private.h"
#include "backends/meta-egl.h"
#include "backends/meta-pointer-constraint.h"
#include "backends/meta-renderer.h"
#include "backends/meta-settings-private.h"
#include "core/util-private.h"

#define DEFAULT_XKB_RULES_FILE "evdev"
#define DEFAULT_XKB_MODEL "pc105+inet"

#define META_TYPE_BACKEND (meta_backend_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaBackend, meta_backend, META, BACKEND, GObject)

struct _MetaBackendClass
{
  GObjectClass parent_class;

  ClutterBackend * (* create_clutter_backend) (MetaBackend *backend);

  void (* post_init) (MetaBackend *backend);

  MetaIdleMonitor * (* create_idle_monitor) (MetaBackend *backend,
                                             int          device_id);
  MetaMonitorManager * (* create_monitor_manager) (MetaBackend *backend);
  MetaCursorRenderer * (* create_cursor_renderer) (MetaBackend *backend);
  MetaRenderer * (* create_renderer) (MetaBackend *backend);
  MetaInputSettings * (* create_input_settings) (MetaBackend *backend);

  gboolean (* grab_device) (MetaBackend *backend,
                            int          device_id,
                            uint32_t     timestamp);
  gboolean (* ungrab_device) (MetaBackend *backend,
                              int          device_id,
                              uint32_t     timestamp);

  void (* warp_pointer) (MetaBackend *backend,
                         int          x,
                         int          y);

  MetaLogicalMonitor * (* get_current_logical_monitor) (MetaBackend *backend);

  void (* set_keymap) (MetaBackend *backend,
                       const char  *layouts,
                       const char  *variants,
                       const char  *options);

  struct xkb_keymap * (* get_keymap) (MetaBackend *backend);

  void (* lock_layout_group) (MetaBackend *backend,
                              guint        idx);

  void (* update_screen_size) (MetaBackend *backend, int width, int height);
  void (* select_stage_events) (MetaBackend *backend);

  gboolean (* get_relative_motion_deltas) (MetaBackend *backend,
                                           const        ClutterEvent *event,
                                           double       *dx,
                                           double       *dy,
                                           double       *dx_unaccel,
                                           double       *dy_unaccel);
  void (* set_numlock) (MetaBackend *backend,
                        gboolean     numlock_state);

};

void meta_init_backend (GType backend_gtype);

void meta_backend_x11_display_opened (MetaBackend *backend);

ClutterBackend * meta_backend_get_clutter_backend (MetaBackend *backend);

MetaIdleMonitor * meta_backend_get_idle_monitor (MetaBackend *backend,
                                                 int          device_id);
void meta_backend_foreach_device_monitor (MetaBackend *backend,
                                          GFunc        func,
                                          gpointer     user_data);

MetaMonitorManager * meta_backend_get_monitor_manager (MetaBackend *backend);
MetaCursorTracker * meta_backend_get_cursor_tracker (MetaBackend *backend);
MetaCursorRenderer * meta_backend_get_cursor_renderer (MetaBackend *backend);
MetaRenderer * meta_backend_get_renderer (MetaBackend *backend);
MetaEgl * meta_backend_get_egl (MetaBackend *backend);
MetaSettings * meta_backend_get_settings (MetaBackend *backend);

gboolean meta_backend_grab_device (MetaBackend *backend,
                                   int          device_id,
                                   uint32_t     timestamp);
gboolean meta_backend_ungrab_device (MetaBackend *backend,
                                     int          device_id,
                                     uint32_t     timestamp);

void meta_backend_warp_pointer (MetaBackend *backend,
                                int          x,
                                int          y);

MetaLogicalMonitor * meta_backend_get_current_logical_monitor (MetaBackend *backend);

struct xkb_keymap * meta_backend_get_keymap (MetaBackend *backend);

void meta_backend_update_last_device (MetaBackend *backend,
                                      int          device_id);

gboolean meta_backend_get_relative_motion_deltas (MetaBackend *backend,
                                                  const        ClutterEvent *event,
                                                  double       *dx,
                                                  double       *dy,
                                                  double       *dx_unaccel,
                                                  double       *dy_unaccel);

MetaPointerConstraint * meta_backend_get_client_pointer_constraint (MetaBackend *backend);
void meta_backend_set_client_pointer_constraint (MetaBackend *backend,
                                                 MetaPointerConstraint *constraint);

ClutterBackend * meta_backend_get_clutter_backend (MetaBackend *backend);

void meta_backend_monitors_changed (MetaBackend *backend);

gboolean meta_is_stage_views_enabled (void);

gboolean meta_is_stage_views_scaled (void);

MetaInputSettings *meta_backend_get_input_settings (MetaBackend *backend);

void meta_backend_notify_keymap_changed (MetaBackend *backend);

void meta_backend_notify_keymap_layout_group_changed (MetaBackend *backend,
                                                      unsigned int locked_group);

void meta_backend_notify_ui_scaling_factor_changed (MetaBackend *backend);

#endif /* META_BACKEND_PRIVATE_H */
