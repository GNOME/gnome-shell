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
#include "backends/meta-pointer-constraint.h"
#include "backends/meta-renderer.h"
#include "core/util-private.h"

#define DEFAULT_XKB_RULES_FILE "evdev"
#define DEFAULT_XKB_MODEL "pc105+inet"

#define META_TYPE_BACKEND             (meta_backend_get_type ())
#define META_BACKEND(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_BACKEND, MetaBackend))
#define META_BACKEND_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_BACKEND, MetaBackendClass))
#define META_IS_BACKEND(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_BACKEND))
#define META_IS_BACKEND_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_BACKEND))
#define META_BACKEND_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_BACKEND, MetaBackendClass))

struct _MetaBackend
{
  GObject parent;

  GHashTable *device_monitors;
  gint current_device_id;

  MetaPointerConstraint *client_pointer_constraint;
};

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

  gboolean (* grab_device) (MetaBackend *backend,
                            int          device_id,
                            uint32_t     timestamp);
  gboolean (* ungrab_device) (MetaBackend *backend,
                              int          device_id,
                              uint32_t     timestamp);

  void (* warp_pointer) (MetaBackend *backend,
                         int          x,
                         int          y);

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
};

void meta_init_backend (MetaBackendType backend_type);

ClutterBackend * meta_backend_get_clutter_backend (MetaBackend *backend);

MetaIdleMonitor * meta_backend_get_idle_monitor (MetaBackend *backend,
                                                 int          device_id);
MetaMonitorManager * meta_backend_get_monitor_manager (MetaBackend *backend);
MetaCursorRenderer * meta_backend_get_cursor_renderer (MetaBackend *backend);
MetaRenderer * meta_backend_get_renderer (MetaBackend *backend);

gboolean meta_backend_grab_device (MetaBackend *backend,
                                   int          device_id,
                                   uint32_t     timestamp);
gboolean meta_backend_ungrab_device (MetaBackend *backend,
                                     int          device_id,
                                     uint32_t     timestamp);

void meta_backend_warp_pointer (MetaBackend *backend,
                                int          x,
                                int          y);

struct xkb_keymap * meta_backend_get_keymap (MetaBackend *backend);

void meta_backend_update_last_device (MetaBackend *backend,
                                      int          device_id);

gboolean meta_backend_get_relative_motion_deltas (MetaBackend *backend,
                                                  const        ClutterEvent *event,
                                                  double       *dx,
                                                  double       *dy,
                                                  double       *dx_unaccel,
                                                  double       *dy_unaccel);

void meta_backend_set_client_pointer_constraint (MetaBackend *backend,
                                                 MetaPointerConstraint *constraint);

ClutterBackend * meta_backend_get_clutter_backend (MetaBackend *backend);

#endif /* META_BACKEND_PRIVATE_H */
