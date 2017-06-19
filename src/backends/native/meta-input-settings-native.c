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
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"

#include <clutter/evdev/clutter-evdev.h>
#include <linux/input-event-codes.h>
#include <libinput.h>

#include "meta-backend-native.h"
#include "meta-input-settings-native.h"
#include "backends/meta-logical-monitor.h"

G_DEFINE_TYPE (MetaInputSettingsNative, meta_input_settings_native, META_TYPE_INPUT_SETTINGS)

static void
meta_input_settings_native_set_send_events (MetaInputSettings        *settings,
                                            ClutterInputDevice       *device,
                                            GDesktopDeviceSendEvents  mode)
{
  enum libinput_config_send_events_mode libinput_mode;
  struct libinput_device *libinput_device;

  switch (mode)
    {
    case G_DESKTOP_DEVICE_SEND_EVENTS_DISABLED:
      libinput_mode = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
      break;
    case G_DESKTOP_DEVICE_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE:
      libinput_mode = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
      break;
    case G_DESKTOP_DEVICE_SEND_EVENTS_ENABLED:
      libinput_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
      break;
    default:
      g_assert_not_reached ();
    }

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);
  if (!libinput_device)
    return;
  libinput_device_config_send_events_set_mode (libinput_device, libinput_mode);
}

static void
meta_input_settings_native_set_matrix (MetaInputSettings  *settings,
                                       ClutterInputDevice *device,
                                       gfloat              matrix[6])
{
  cairo_matrix_t dev_matrix;

  cairo_matrix_init (&dev_matrix, matrix[0], matrix[3], matrix[1],
                     matrix[4], matrix[2], matrix[5]);
  g_object_set (device, "device-matrix", &dev_matrix, NULL);
}

static void
meta_input_settings_native_set_speed (MetaInputSettings  *settings,
                                      ClutterInputDevice *device,
                                      gdouble             speed)
{
  struct libinput_device *libinput_device;

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);
  if (!libinput_device)
    return;
  libinput_device_config_accel_set_speed (libinput_device,
                                          CLAMP (speed, -1, 1));
}

static void
meta_input_settings_native_set_left_handed (MetaInputSettings  *settings,
                                            ClutterInputDevice *device,
                                            gboolean            enabled)
{
  struct libinput_device *libinput_device;

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);
  if (!libinput_device)
    return;

  if (libinput_device_config_left_handed_is_available (libinput_device))
    libinput_device_config_left_handed_set (libinput_device, enabled);
}

static void
meta_input_settings_native_set_tap_enabled (MetaInputSettings  *settings,
                                            ClutterInputDevice *device,
                                            gboolean            enabled)
{
  struct libinput_device *libinput_device;

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);
  if (!libinput_device)
    return;

  if (libinput_device_config_tap_get_finger_count (libinput_device) > 0)
    libinput_device_config_tap_set_enabled (libinput_device,
                                            enabled ?
                                            LIBINPUT_CONFIG_TAP_ENABLED :
                                            LIBINPUT_CONFIG_TAP_DISABLED);
}

static void
meta_input_settings_native_set_disable_while_typing (MetaInputSettings  *settings,
                                                     ClutterInputDevice *device,
                                                     gboolean            enabled)
{
  struct libinput_device *libinput_device;

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);

  if (!libinput_device)
    return;

  if (libinput_device_config_dwt_is_available (libinput_device))
    libinput_device_config_dwt_set_enabled (libinput_device,
                                            enabled ?
                                            LIBINPUT_CONFIG_DWT_ENABLED :
                                            LIBINPUT_CONFIG_DWT_DISABLED);
}

static void
meta_input_settings_native_set_invert_scroll (MetaInputSettings  *settings,
                                              ClutterInputDevice *device,
                                              gboolean            inverted)
{
  struct libinput_device *libinput_device;

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);
  if (!libinput_device)
    return;

  if (libinput_device_config_scroll_has_natural_scroll (libinput_device))
    libinput_device_config_scroll_set_natural_scroll_enabled (libinput_device,
                                                              inverted);
}

static gboolean
device_set_scroll_method (struct libinput_device             *libinput_device,
                          enum libinput_config_scroll_method  method)
{
  enum libinput_config_status status =
    libinput_device_config_scroll_set_method (libinput_device, method);
  return status == LIBINPUT_CONFIG_STATUS_SUCCESS;
}

static gboolean
device_set_click_method (struct libinput_device            *libinput_device,
                         enum libinput_config_click_method  method)
{
  enum libinput_config_status status =
    libinput_device_config_click_set_method (libinput_device, method);
  return status == LIBINPUT_CONFIG_STATUS_SUCCESS;
}

static void
meta_input_settings_native_set_edge_scroll (MetaInputSettings            *settings,
                                            ClutterInputDevice           *device,
                                            gboolean                      edge_scrolling_enabled)
{
  struct libinput_device *libinput_device;
  enum libinput_config_scroll_method current, method;

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);

  method = edge_scrolling_enabled ? LIBINPUT_CONFIG_SCROLL_EDGE : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
  current = libinput_device_config_scroll_get_method (libinput_device);
  current &= ~LIBINPUT_CONFIG_SCROLL_EDGE;

  device_set_scroll_method (libinput_device, current | method);
}

static void
meta_input_settings_native_set_two_finger_scroll (MetaInputSettings            *settings,
                                                  ClutterInputDevice           *device,
                                                  gboolean                      two_finger_scroll_enabled)
{
  struct libinput_device *libinput_device;
  enum libinput_config_scroll_method current, method;

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);

  method = two_finger_scroll_enabled ? LIBINPUT_CONFIG_SCROLL_2FG : LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
  current = libinput_device_config_scroll_get_method (libinput_device);
  current &= ~LIBINPUT_CONFIG_SCROLL_2FG;

  device_set_scroll_method (libinput_device, current | method);
}

static gboolean
meta_input_settings_native_has_two_finger_scroll (MetaInputSettings  *settings,
                                                  ClutterInputDevice *device)
{
  struct libinput_device *libinput_device;

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);
  if (!libinput_device)
    return FALSE;

  return libinput_device_config_scroll_get_methods (libinput_device) & LIBINPUT_CONFIG_SCROLL_2FG;
}

static void
meta_input_settings_native_set_scroll_button (MetaInputSettings  *settings,
                                              ClutterInputDevice *device,
                                              guint               button)
{
  struct libinput_device *libinput_device;

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);
  if (!libinput_device)
    return;

  if (!device_set_scroll_method (libinput_device,
                                 LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN))
    return;

  libinput_device_config_scroll_set_button (libinput_device, button);
}

static void
meta_input_settings_native_set_click_method (MetaInputSettings           *settings,
                                             ClutterInputDevice          *device,
                                             GDesktopTouchpadClickMethod  mode)
{
  enum libinput_config_click_method click_method = 0;
  struct libinput_device *libinput_device;

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);
  if (!libinput_device)
    return;

  switch (mode)
    {
    case G_DESKTOP_TOUCHPAD_CLICK_METHOD_DEFAULT:
      click_method = libinput_device_config_click_get_default_method (libinput_device);
      break;
    case G_DESKTOP_TOUCHPAD_CLICK_METHOD_NONE:
      click_method = LIBINPUT_CONFIG_CLICK_METHOD_NONE;
      break;
    case G_DESKTOP_TOUCHPAD_CLICK_METHOD_AREAS:
      click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
      break;
    case G_DESKTOP_TOUCHPAD_CLICK_METHOD_FINGERS:
      click_method = LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER;
      break;
    default:
      g_assert_not_reached ();
      return;
  }

  device_set_click_method (libinput_device, click_method);
}

static void
meta_input_settings_native_set_keyboard_repeat (MetaInputSettings *settings,
                                                gboolean           enabled,
                                                guint              delay,
                                                guint              interval)
{
  ClutterDeviceManager *manager = clutter_device_manager_get_default ();

  clutter_evdev_set_keyboard_repeat (manager, enabled, delay, interval);
}

static void
set_device_accel_profile (ClutterInputDevice         *device,
                          GDesktopPointerAccelProfile profile)
{
  struct libinput_device *libinput_device;
  enum libinput_config_accel_profile libinput_profile;
  uint32_t profiles;

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);

  switch (profile)
    {
    case G_DESKTOP_POINTER_ACCEL_PROFILE_FLAT:
      libinput_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
      break;
    case G_DESKTOP_POINTER_ACCEL_PROFILE_ADAPTIVE:
      libinput_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
      break;
    default:
      g_warn_if_reached ();
    case G_DESKTOP_POINTER_ACCEL_PROFILE_DEFAULT:
      libinput_profile =
        libinput_device_config_accel_get_default_profile (libinput_device);
    }

  profiles = libinput_device_config_accel_get_profiles (libinput_device);
  if ((profiles & libinput_profile) == 0)
    {
      libinput_profile =
        libinput_device_config_accel_get_default_profile (libinput_device);
    }

  libinput_device_config_accel_set_profile (libinput_device,
                                            libinput_profile);
}

static gboolean
has_udev_property (ClutterInputDevice *device,
                   const char         *property)
{
  struct libinput_device *libinput_device;
  struct udev_device *udev_device;
  struct udev_device *parent_udev_device;

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);
  if (!libinput_device)
    return FALSE;

  udev_device = libinput_device_get_udev_device (libinput_device);

  if (!udev_device)
    return FALSE;

  if (NULL != udev_device_get_property_value (udev_device, property))
    {
      udev_device_unref (udev_device);
      return TRUE;
    }

  parent_udev_device = udev_device_get_parent (udev_device);
  udev_device_unref (udev_device);

  if (!parent_udev_device)
    return FALSE;

  if (NULL != udev_device_get_property_value (parent_udev_device, property))
    return TRUE;

  return FALSE;
}

static gboolean
is_mouse_device (ClutterInputDevice *device)
{
  return (has_udev_property (device, "ID_INPUT_MOUSE") &&
          !has_udev_property (device, "ID_INPUT_POINTINGSTICK"));
}

static gboolean
is_trackball_device (ClutterInputDevice *device)
{
  return meta_input_device_is_trackball (device);
}

static void
meta_input_settings_native_set_mouse_accel_profile (MetaInputSettings          *settings,
                                                    ClutterInputDevice         *device,
                                                    GDesktopPointerAccelProfile profile)
{
  if (!is_mouse_device (device))
    return;

  set_device_accel_profile (device, profile);
}

static void
meta_input_settings_native_set_trackball_accel_profile (MetaInputSettings          *settings,
                                                        ClutterInputDevice         *device,
                                                        GDesktopPointerAccelProfile profile)
{
  if (!is_trackball_device (device))
    return;

  set_device_accel_profile (device, profile);
}

static void
meta_input_settings_native_set_tablet_mapping (MetaInputSettings     *settings,
                                               ClutterInputDevice    *device,
                                               GDesktopTabletMapping  mapping)
{
  ClutterInputDeviceMapping dev_mapping;

  if (mapping == G_DESKTOP_TABLET_MAPPING_ABSOLUTE)
    dev_mapping = CLUTTER_INPUT_DEVICE_MAPPING_ABSOLUTE;
  else if (mapping == G_DESKTOP_TABLET_MAPPING_RELATIVE)
    dev_mapping = CLUTTER_INPUT_DEVICE_MAPPING_RELATIVE;
  else
    return;

  clutter_input_device_set_mapping_mode (device, dev_mapping);
}

static void
meta_input_settings_native_set_tablet_keep_aspect (MetaInputSettings  *settings,
                                                   ClutterInputDevice *device,
                                                   MetaLogicalMonitor *logical_monitor,
                                                   gboolean            keep_aspect)
{
  double aspect_ratio = 0;

  if (keep_aspect)
    {
      int width, height;

      if (logical_monitor)
        {
          width = logical_monitor->rect.width;
          height = logical_monitor->rect.height;
        }
      else
        {
          MetaMonitorManager *monitor_manager;
          MetaBackend *backend;

          backend = meta_get_backend ();
          monitor_manager = meta_backend_get_monitor_manager (backend);
	  meta_monitor_manager_get_screen_size (monitor_manager,
						&width,
						&height);
        }

      aspect_ratio = (double) width / height;
    }

  g_object_set (device, "output-aspect-ratio", aspect_ratio, NULL);
}

static void
meta_input_settings_native_set_tablet_area (MetaInputSettings  *settings,
                                            ClutterInputDevice *device,
                                            gdouble             padding_left,
                                            gdouble             padding_right,
                                            gdouble             padding_top,
                                            gdouble             padding_bottom)
{
  struct libinput_device *libinput_device;
  gfloat scale_x;
  gfloat scale_y;
  gfloat offset_x;
  gfloat offset_y;

  scale_x = 1. / (1. - (padding_left + padding_right));
  scale_y = 1. / (1. - (padding_top + padding_bottom));
  offset_x = -padding_left * scale_x;
  offset_y = -padding_top * scale_y;

  gfloat matrix[6] = { scale_x, 0., offset_x,
                       0., scale_y, offset_y };

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);
  if (!libinput_device ||
      !libinput_device_config_calibration_has_matrix (libinput_device))
    return;

  libinput_device_config_calibration_set_matrix (libinput_device, matrix);
}

static void
meta_input_settings_native_set_stylus_pressure (MetaInputSettings      *settings,
                                                ClutterInputDevice     *device,
                                                ClutterInputDeviceTool *tool,
                                                const gint              curve[4])
{
  gdouble pressure_curve[4];

  pressure_curve[0] = (gdouble) curve[0] / 100;
  pressure_curve[1] = (gdouble) curve[1] / 100;
  pressure_curve[2] = (gdouble) curve[2] / 100;
  pressure_curve[3] = (gdouble) curve[3] / 100;

  clutter_evdev_input_device_tool_set_pressure_curve (tool, pressure_curve);
}

static guint
action_to_evcode (GDesktopStylusButtonAction action)
{
  switch (action)
    {
    case G_DESKTOP_STYLUS_BUTTON_ACTION_MIDDLE:
      return BTN_STYLUS;
    case G_DESKTOP_STYLUS_BUTTON_ACTION_RIGHT:
      return BTN_STYLUS2;
    case G_DESKTOP_STYLUS_BUTTON_ACTION_BACK:
      return BTN_BACK;
    case G_DESKTOP_STYLUS_BUTTON_ACTION_FORWARD:
      return BTN_FORWARD;
    case G_DESKTOP_STYLUS_BUTTON_ACTION_DEFAULT:
    default:
      return 0;
    }
}

static void
meta_input_settings_native_set_stylus_button_map (MetaInputSettings          *settings,
                                                  ClutterInputDevice         *device,
                                                  ClutterInputDeviceTool     *tool,
                                                  GDesktopStylusButtonAction  primary,
                                                  GDesktopStylusButtonAction  secondary)
{
  clutter_evdev_input_device_tool_set_button_code (tool, CLUTTER_BUTTON_MIDDLE,
                                                   action_to_evcode (primary));
  clutter_evdev_input_device_tool_set_button_code (tool, CLUTTER_BUTTON_SECONDARY,
                                                   action_to_evcode (secondary));
}

static void
meta_input_settings_native_class_init (MetaInputSettingsNativeClass *klass)
{
  MetaInputSettingsClass *input_settings_class = META_INPUT_SETTINGS_CLASS (klass);

  input_settings_class->set_send_events = meta_input_settings_native_set_send_events;
  input_settings_class->set_matrix = meta_input_settings_native_set_matrix;
  input_settings_class->set_speed = meta_input_settings_native_set_speed;
  input_settings_class->set_left_handed = meta_input_settings_native_set_left_handed;
  input_settings_class->set_tap_enabled = meta_input_settings_native_set_tap_enabled;
  input_settings_class->set_invert_scroll = meta_input_settings_native_set_invert_scroll;
  input_settings_class->set_edge_scroll = meta_input_settings_native_set_edge_scroll;
  input_settings_class->set_two_finger_scroll = meta_input_settings_native_set_two_finger_scroll;
  input_settings_class->set_scroll_button = meta_input_settings_native_set_scroll_button;
  input_settings_class->set_click_method = meta_input_settings_native_set_click_method;
  input_settings_class->set_keyboard_repeat = meta_input_settings_native_set_keyboard_repeat;
  input_settings_class->set_disable_while_typing = meta_input_settings_native_set_disable_while_typing;

  input_settings_class->set_tablet_mapping = meta_input_settings_native_set_tablet_mapping;
  input_settings_class->set_tablet_keep_aspect = meta_input_settings_native_set_tablet_keep_aspect;
  input_settings_class->set_tablet_area = meta_input_settings_native_set_tablet_area;

  input_settings_class->set_mouse_accel_profile = meta_input_settings_native_set_mouse_accel_profile;
  input_settings_class->set_trackball_accel_profile = meta_input_settings_native_set_trackball_accel_profile;

  input_settings_class->set_stylus_pressure = meta_input_settings_native_set_stylus_pressure;
  input_settings_class->set_stylus_button_map = meta_input_settings_native_set_stylus_button_map;

  input_settings_class->has_two_finger_scroll = meta_input_settings_native_has_two_finger_scroll;
}

static void
meta_input_settings_native_init (MetaInputSettingsNative *settings)
{
}
