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
#include <libinput.h>

#include "meta-input-settings-native.h"

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
  libinput_device_config_send_events_set_mode (libinput_device, libinput_mode);
}

static void
meta_input_settings_native_set_matrix (MetaInputSettings  *settings,
                                       ClutterInputDevice *device,
                                       gfloat              matrix[6])
{
  struct libinput_device *libinput_device;

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);

  if (libinput_device_config_calibration_has_matrix (libinput_device) > 0)
    libinput_device_config_calibration_set_matrix (libinput_device, matrix);
}

static void
meta_input_settings_native_set_speed (MetaInputSettings  *settings,
                                      ClutterInputDevice *device,
                                      gdouble             speed)
{
  struct libinput_device *libinput_device;

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);
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

  if (libinput_device_config_tap_get_finger_count (libinput_device) > 0)
    libinput_device_config_tap_set_enabled (libinput_device,
                                            enabled ?
                                            LIBINPUT_CONFIG_TAP_ENABLED :
                                            LIBINPUT_CONFIG_TAP_DISABLED);
}

static void
meta_input_settings_native_set_invert_scroll (MetaInputSettings  *settings,
                                              ClutterInputDevice *device,
                                              gboolean            inverted)
{
  struct libinput_device *libinput_device;

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);

  if (libinput_device_config_scroll_has_natural_scroll (libinput_device))
    libinput_device_config_scroll_set_natural_scroll_enabled (libinput_device,
                                                              inverted);
}

static gboolean
device_set_scroll_method (struct libinput_device             *libinput_device,
                          enum libinput_config_scroll_method  method)
{
  enum libinput_config_scroll_method supported;

  supported = libinput_device_config_scroll_get_methods (libinput_device);

  if (method & supported)
    libinput_device_config_scroll_set_method (libinput_device, method);

  return (method & supported) != 0;
}

static gboolean
device_set_click_method (struct libinput_device            *libinput_device,
                         enum libinput_config_click_method  method)
{
  enum libinput_config_click_method supported;

  supported = libinput_device_config_click_get_methods (libinput_device);

  if (method & supported)
    libinput_device_config_click_set_method (libinput_device, method);

  return (method & supported) != 0;
}

static void
meta_input_settings_native_set_edge_scroll (MetaInputSettings            *settings,
                                            ClutterInputDevice           *device,
                                            gboolean                      edge_scrolling_enabled)
{
  enum libinput_config_scroll_method scroll_method = 0;
  struct libinput_device *libinput_device;
  enum libinput_config_scroll_method supported;

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);
  supported = libinput_device_config_scroll_get_methods (libinput_device);

  if (supported & LIBINPUT_CONFIG_SCROLL_2FG)
    {
      scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
    }
  else if (supported & LIBINPUT_CONFIG_SCROLL_EDGE &&
           edge_scrolling_enabled)
    {
      scroll_method = LIBINPUT_CONFIG_SCROLL_EDGE;
    }
  else
    {
      scroll_method = LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
    }

  device_set_scroll_method (libinput_device, scroll_method);
}

static void
meta_input_settings_native_set_scroll_button (MetaInputSettings  *settings,
                                              ClutterInputDevice *device,
                                              guint               button)
{
  struct libinput_device *libinput_device;

  libinput_device = clutter_evdev_input_device_get_libinput_device (device);

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
  input_settings_class->set_scroll_button = meta_input_settings_native_set_scroll_button;
  input_settings_class->set_click_method = meta_input_settings_native_set_click_method;
  input_settings_class->set_keyboard_repeat = meta_input_settings_native_set_keyboard_repeat;
}

static void
meta_input_settings_native_init (MetaInputSettingsNative *settings)
{
}
