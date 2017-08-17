/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright 2014 Red Hat, Inc.
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#ifndef META_INPUT_SETTINGS_PRIVATE_H
#define META_INPUT_SETTINGS_PRIVATE_H

#include "display-private.h"
#include "meta-monitor-manager-private.h"

#include <clutter/clutter.h>

#ifdef HAVE_LIBWACOM
#include <libwacom/libwacom.h>
#endif

#define META_TYPE_INPUT_SETTINGS (meta_input_settings_get_type ())
G_DECLARE_DERIVABLE_TYPE (MetaInputSettings, meta_input_settings,
                          META, INPUT_SETTINGS, GObject)

struct _MetaInputSettingsClass
{
  GObjectClass parent_class;

  void (* set_send_events)   (MetaInputSettings        *settings,
                              ClutterInputDevice       *device,
                              GDesktopDeviceSendEvents  mode);
  void (* set_matrix)        (MetaInputSettings  *settings,
                              ClutterInputDevice *device,
                              gfloat              matrix[6]);
  void (* set_speed)         (MetaInputSettings  *settings,
                              ClutterInputDevice *device,
                              gdouble             speed);
  void (* set_left_handed)   (MetaInputSettings  *settings,
                              ClutterInputDevice *device,
                              gboolean            enabled);
  void (* set_tap_enabled)   (MetaInputSettings  *settings,
                              ClutterInputDevice *device,
                              gboolean            enabled);
  void (* set_tap_and_drag_enabled) (MetaInputSettings  *settings,
                                     ClutterInputDevice *device,
                                     gboolean            enabled);
  void (* set_disable_while_typing) (MetaInputSettings  *settings,
                                     ClutterInputDevice *device,
                                     gboolean            enabled);
  void (* set_invert_scroll) (MetaInputSettings  *settings,
                              ClutterInputDevice *device,
                              gboolean            inverted);
  void (* set_edge_scroll)   (MetaInputSettings  *settings,
                              ClutterInputDevice *device,
                              gboolean            enabled);
  void (* set_two_finger_scroll) (MetaInputSettings  *settings,
                                  ClutterInputDevice *device,
                                  gboolean            enabled);
  void (* set_scroll_button) (MetaInputSettings  *settings,
                              ClutterInputDevice *device,
                              guint               button);

  void (* set_click_method)  (MetaInputSettings            *settings,
                              ClutterInputDevice           *device,
                              GDesktopTouchpadClickMethod   mode);

  void (* set_keyboard_repeat) (MetaInputSettings *settings,
                                gboolean           repeat,
                                guint              delay,
                                guint              interval);

  void (* set_tablet_mapping)        (MetaInputSettings      *settings,
                                      ClutterInputDevice     *device,
                                      GDesktopTabletMapping   mapping);
  void (* set_tablet_keep_aspect)    (MetaInputSettings      *settings,
                                      ClutterInputDevice     *device,
                                      MetaLogicalMonitor     *logical_monitor,
                                      gboolean                keep_aspect);
  void (* set_tablet_area)           (MetaInputSettings      *settings,
                                      ClutterInputDevice     *device,
                                      gdouble                 padding_left,
                                      gdouble                 padding_right,
                                      gdouble                 padding_top,
                                      gdouble                 padding_bottom);

  void (* set_mouse_accel_profile) (MetaInputSettings          *settings,
                                    ClutterInputDevice         *device,
                                    GDesktopPointerAccelProfile profile);
  void (* set_trackball_accel_profile) (MetaInputSettings          *settings,
                                        ClutterInputDevice         *device,
                                        GDesktopPointerAccelProfile profile);

  void (* set_stylus_pressure) (MetaInputSettings            *settings,
                                ClutterInputDevice           *device,
                                ClutterInputDeviceTool       *tool,
                                const gint32                  curve[4]);
  void (* set_stylus_button_map) (MetaInputSettings          *settings,
                                  ClutterInputDevice         *device,
                                  ClutterInputDeviceTool     *tool,
                                  GDesktopStylusButtonAction  primary,
                                  GDesktopStylusButtonAction  secondary);
  gboolean (* has_two_finger_scroll) (MetaInputSettings  *settings,
                                      ClutterInputDevice *device);
};

GSettings *           meta_input_settings_get_tablet_settings (MetaInputSettings  *settings,
                                                               ClutterInputDevice *device);
MetaLogicalMonitor *  meta_input_settings_get_tablet_logical_monitor (MetaInputSettings  *settings,
                                                                      ClutterInputDevice *device);

GDesktopTabletMapping meta_input_settings_get_tablet_mapping (MetaInputSettings  *settings,
                                                              ClutterInputDevice *device);

gboolean                   meta_input_settings_is_pad_button_grabbed     (MetaInputSettings  *input_settings,
                                                                          ClutterInputDevice *pad,
                                                                          guint               button);

gboolean                   meta_input_settings_handle_pad_event          (MetaInputSettings    *input_settings,
                                                                          const ClutterEvent   *event);
gchar *                    meta_input_settings_get_pad_action_label      (MetaInputSettings  *input_settings,
                                                                          ClutterInputDevice *pad,
                                                                          MetaPadActionType   action,
                                                                          guint               number);

#ifdef HAVE_LIBWACOM
WacomDevice * meta_input_settings_get_tablet_wacom_device (MetaInputSettings *settings,
                                                           ClutterInputDevice *device);
#endif

gboolean meta_input_device_is_trackball (ClutterInputDevice *device);

#endif /* META_INPUT_SETTINGS_PRIVATE_H */
