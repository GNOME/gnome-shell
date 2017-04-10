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

/**
 * SECTION:input-settings
 * @title: MetaInputSettings
 * @short_description: Mutter input device configuration
 */

#include "config.h"

#include <string.h>

#include "meta-backend-private.h"
#include "meta-input-settings-private.h"
#include "backends/meta-logical-monitor.h"
#include "backends/meta-monitor.h"

#include <glib/gi18n-lib.h>
#include <meta/util.h>

static GQuark quark_tool_settings = 0;

typedef struct _MetaInputSettingsPrivate MetaInputSettingsPrivate;
typedef struct _DeviceMappingInfo DeviceMappingInfo;

struct _DeviceMappingInfo
{
  MetaInputSettings *input_settings;
  ClutterInputDevice *device;
  GSettings *settings;
#ifdef HAVE_LIBWACOM
  WacomDevice *wacom_device;
#endif
};

struct _MetaInputSettingsPrivate
{
  ClutterDeviceManager *device_manager;
  MetaMonitorManager *monitor_manager;
  guint monitors_changed_id;

  GSettings *mouse_settings;
  GSettings *touchpad_settings;
  GSettings *trackball_settings;
  GSettings *keyboard_settings;
  GSettings *gsd_settings;

  GHashTable *mappable_devices;

  ClutterVirtualInputDevice *virtual_pad_keyboard;

#ifdef HAVE_LIBWACOM
  WacomDeviceDatabase *wacom_db;
#endif

  GHashTable *two_finger_devices;
};

typedef void (*ConfigBoolFunc)   (MetaInputSettings  *input_settings,
                                  ClutterInputDevice *device,
                                  gboolean            setting);
typedef void (*ConfigDoubleFunc) (MetaInputSettings  *input_settings,
                                  ClutterInputDevice *device,
                                  gdouble             value);
typedef void (*ConfigUintFunc)   (MetaInputSettings  *input_settings,
                                  ClutterInputDevice *device,
                                  guint               value);

G_DEFINE_TYPE_WITH_PRIVATE (MetaInputSettings, meta_input_settings, G_TYPE_OBJECT)

static GSList *
meta_input_settings_get_devices (MetaInputSettings      *settings,
                                 ClutterInputDeviceType  type)
{
  MetaInputSettingsPrivate *priv;
  const GSList *devices;
  GSList *list = NULL;

  priv = meta_input_settings_get_instance_private (settings);
  devices = clutter_device_manager_peek_devices (priv->device_manager);

  while (devices)
    {
      ClutterInputDevice *device = devices->data;

      if (clutter_input_device_get_device_type (device) == type &&
          clutter_input_device_get_device_mode (device) != CLUTTER_INPUT_MODE_MASTER)
        list = g_slist_prepend (list, device);

      devices = devices->next;
    }

  return list;
}

static void
meta_input_settings_dispose (GObject *object)
{
  MetaInputSettings *settings = META_INPUT_SETTINGS (object);
  MetaInputSettingsPrivate *priv = meta_input_settings_get_instance_private (settings);

  g_clear_object (&priv->virtual_pad_keyboard);

  g_clear_object (&priv->mouse_settings);
  g_clear_object (&priv->touchpad_settings);
  g_clear_object (&priv->trackball_settings);
  g_clear_object (&priv->keyboard_settings);
  g_clear_object (&priv->gsd_settings);
  g_clear_pointer (&priv->mappable_devices, g_hash_table_unref);

  if (priv->monitors_changed_id && priv->monitor_manager)
    {
      g_signal_handler_disconnect (priv->monitor_manager,
                                   priv->monitors_changed_id);
      priv->monitors_changed_id = 0;
    }

  g_clear_object (&priv->monitor_manager);

#ifdef HAVE_LIBWACOM
  if (priv->wacom_db)
    libwacom_database_destroy (priv->wacom_db);
#endif

  g_clear_pointer (&priv->two_finger_devices, g_hash_table_destroy);

  G_OBJECT_CLASS (meta_input_settings_parent_class)->dispose (object);
}

static void
settings_device_set_bool_setting (MetaInputSettings  *input_settings,
                                  ClutterInputDevice *device,
                                  ConfigBoolFunc      func,
                                  gboolean            enabled)
{
  func (input_settings, device, enabled);
}

static void
settings_set_bool_setting (MetaInputSettings      *input_settings,
                           ClutterInputDeviceType  type,
                           ConfigBoolFunc          func,
                           gboolean                enabled)
{
  GSList *devices, *d;

  devices = meta_input_settings_get_devices (input_settings, type);

  for (d = devices; d; d = d->next)
    settings_device_set_bool_setting (input_settings, d->data, func, enabled);

  g_slist_free (devices);
}

static void
settings_device_set_double_setting (MetaInputSettings  *input_settings,
                                    ClutterInputDevice *device,
                                    ConfigDoubleFunc    func,
                                    gdouble             value)
{
  func (input_settings, device, value);
}

static void
settings_set_double_setting (MetaInputSettings      *input_settings,
                             ClutterInputDeviceType  type,
                             ConfigDoubleFunc        func,
                             gdouble                 value)
{
  GSList *devices, *d;

  devices = meta_input_settings_get_devices (input_settings, type);

  for (d = devices; d; d = d->next)
    settings_device_set_double_setting (input_settings, d->data, func, value);

  g_slist_free (devices);
}

static void
settings_device_set_uint_setting (MetaInputSettings  *input_settings,
                                  ClutterInputDevice *device,
                                  ConfigUintFunc      func,
                                  guint               value)
{
  (func) (input_settings, device, value);
}

static void
settings_set_uint_setting (MetaInputSettings      *input_settings,
                           ClutterInputDeviceType  type,
                           ConfigUintFunc          func,
                           guint                   value)
{
  GSList *devices, *d;

  devices = meta_input_settings_get_devices (input_settings, type);

  for (d = devices; d; d = d->next)
    settings_device_set_uint_setting (input_settings, d->data, func, value);

  g_slist_free (devices);
}

static void
update_touchpad_left_handed (MetaInputSettings  *input_settings,
                             ClutterInputDevice *device)
{
  MetaInputSettingsClass *input_settings_class;
  GDesktopTouchpadHandedness handedness;
  MetaInputSettingsPrivate *priv;
  gboolean enabled = FALSE;

  if (device &&
      clutter_input_device_get_device_type (device) != CLUTTER_TOUCHPAD_DEVICE)
    return;

  priv = meta_input_settings_get_instance_private (input_settings);
  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  handedness = g_settings_get_enum (priv->touchpad_settings, "left-handed");

  switch (handedness)
    {
    case G_DESKTOP_TOUCHPAD_HANDEDNESS_RIGHT:
      enabled = FALSE;
      break;
    case G_DESKTOP_TOUCHPAD_HANDEDNESS_LEFT:
      enabled = TRUE;
      break;
    case G_DESKTOP_TOUCHPAD_HANDEDNESS_MOUSE:
      enabled = g_settings_get_boolean (priv->mouse_settings, "left-handed");
      break;
    default:
      g_assert_not_reached ();
    }

  if (device)
    {
      settings_device_set_bool_setting (input_settings, device,
                                        input_settings_class->set_left_handed,
                                        enabled);
    }
  else
    {
      settings_set_bool_setting (input_settings, CLUTTER_TOUCHPAD_DEVICE,
                                 input_settings_class->set_left_handed,
                                 enabled);
    }
}

static void
update_mouse_left_handed (MetaInputSettings  *input_settings,
                          ClutterInputDevice *device)
{
  MetaInputSettingsClass *input_settings_class;
  MetaInputSettingsPrivate *priv;
  gboolean enabled;

  if (device &&
      clutter_input_device_get_device_type (device) != CLUTTER_POINTER_DEVICE)
    return;

  priv = meta_input_settings_get_instance_private (input_settings);
  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  enabled = g_settings_get_boolean (priv->mouse_settings, "left-handed");

  if (device)
    {
      settings_device_set_bool_setting (input_settings, device,
                                        input_settings_class->set_left_handed,
                                        enabled);
    }
  else
    {
      GDesktopTouchpadHandedness touchpad_handedness;

      settings_set_bool_setting (input_settings, CLUTTER_POINTER_DEVICE,
                                 input_settings_class->set_left_handed,
                                 enabled);

      touchpad_handedness = g_settings_get_enum (priv->touchpad_settings,
                                                 "left-handed");

      /* Also update touchpads if they're following mouse settings */
      if (touchpad_handedness == G_DESKTOP_TOUCHPAD_HANDEDNESS_MOUSE)
        update_touchpad_left_handed (input_settings, NULL);
    }
}

static void
do_update_pointer_accel_profile (MetaInputSettings          *input_settings,
                                 GSettings                  *settings,
                                 ClutterInputDevice         *device,
                                 GDesktopPointerAccelProfile profile)
{
  MetaInputSettingsPrivate *priv =
    meta_input_settings_get_instance_private (input_settings);
  MetaInputSettingsClass *input_settings_class =
    META_INPUT_SETTINGS_GET_CLASS (input_settings);

  if (settings == priv->mouse_settings)
    input_settings_class->set_mouse_accel_profile (input_settings,
                                                   device,
                                                   profile);
  else if (settings == priv->trackball_settings)
    input_settings_class->set_trackball_accel_profile (input_settings,
                                                       device,
                                                       profile);
}

static void
update_pointer_accel_profile (MetaInputSettings  *input_settings,
                              GSettings          *settings,
                              ClutterInputDevice *device)
{
  GDesktopPointerAccelProfile profile;

  profile = g_settings_get_enum (settings, "accel-profile");

  if (device)
    {
      do_update_pointer_accel_profile (input_settings, settings,
                                       device, profile);
    }
  else
    {
      MetaInputSettingsPrivate *priv =
        meta_input_settings_get_instance_private (input_settings);
      const GSList *devices;
      const GSList *l;

      devices = clutter_device_manager_peek_devices (priv->device_manager);
      for (l = devices; l; l = l->next)
        {
          device = l->data;

          if (clutter_input_device_get_device_mode (device) ==
              CLUTTER_INPUT_MODE_MASTER)
            continue;

          do_update_pointer_accel_profile (input_settings, settings,
                                           device, profile);
        }
    }
}

static GSettings *
get_settings_for_device_type (MetaInputSettings      *input_settings,
                              ClutterInputDeviceType  type)
{
  MetaInputSettingsPrivate *priv;
  priv = meta_input_settings_get_instance_private (input_settings);
  switch (type)
    {
    case CLUTTER_POINTER_DEVICE:
      return priv->mouse_settings;
    case CLUTTER_TOUCHPAD_DEVICE:
      return priv->touchpad_settings;
    default:
      return NULL;
    }
}

static void
update_device_speed (MetaInputSettings      *input_settings,
                     ClutterInputDevice     *device)
{
  GSettings *settings;
  ConfigDoubleFunc func;
  const gchar *key = "speed";

  func = META_INPUT_SETTINGS_GET_CLASS (input_settings)->set_speed;

  if (device)
    {
      settings = get_settings_for_device_type (input_settings,
                                               clutter_input_device_get_device_type (device));
      if (!settings)
        return;

      settings_device_set_double_setting (input_settings, device, func,
                                          g_settings_get_double (settings, key));
    }
  else
    {
      settings = get_settings_for_device_type (input_settings, CLUTTER_POINTER_DEVICE);
      settings_set_double_setting (input_settings, CLUTTER_POINTER_DEVICE, func,
                                   g_settings_get_double (settings, key));
      settings = get_settings_for_device_type (input_settings, CLUTTER_TOUCHPAD_DEVICE);
      settings_set_double_setting (input_settings, CLUTTER_TOUCHPAD_DEVICE, func,
                                   g_settings_get_double (settings, key));
    }
}

static void
update_device_natural_scroll (MetaInputSettings      *input_settings,
                              ClutterInputDevice     *device)
{
  GSettings *settings;
  ConfigBoolFunc func;
  const gchar *key = "natural-scroll";

  func = META_INPUT_SETTINGS_GET_CLASS (input_settings)->set_invert_scroll;

  if (device)
    {
      settings = get_settings_for_device_type (input_settings,
                                               clutter_input_device_get_device_type (device));
      if (!settings)
        return;

      settings_device_set_bool_setting (input_settings, device, func,
                                        g_settings_get_boolean (settings, key));
    }
  else
    {
      settings = get_settings_for_device_type (input_settings, CLUTTER_POINTER_DEVICE);
      settings_set_bool_setting (input_settings, CLUTTER_POINTER_DEVICE, func,
                                 g_settings_get_boolean (settings, key));
      settings = get_settings_for_device_type (input_settings, CLUTTER_TOUCHPAD_DEVICE);
      settings_set_bool_setting (input_settings, CLUTTER_TOUCHPAD_DEVICE, func,
                                 g_settings_get_boolean (settings, key));
    }
}

static void
update_touchpad_tap_enabled (MetaInputSettings  *input_settings,
                             ClutterInputDevice *device)
{
  MetaInputSettingsClass *input_settings_class;
  MetaInputSettingsPrivate *priv;
  gboolean enabled;

  if (device &&
      clutter_input_device_get_device_type (device) != CLUTTER_TOUCHPAD_DEVICE)
    return;

  priv = meta_input_settings_get_instance_private (input_settings);
  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  enabled = g_settings_get_boolean (priv->touchpad_settings, "tap-to-click");

  if (device)
    {
      settings_device_set_bool_setting (input_settings, device,
                                        input_settings_class->set_tap_enabled,
                                        enabled);
    }
  else
    {
      settings_set_bool_setting (input_settings, CLUTTER_TOUCHPAD_DEVICE,
                                 input_settings_class->set_tap_enabled,
                                 enabled);
    }
}

static void
update_touchpad_edge_scroll (MetaInputSettings *input_settings,
                             ClutterInputDevice *device)
{
  MetaInputSettingsClass *input_settings_class;
  gboolean edge_scroll_enabled;
  gboolean two_finger_scroll_enabled;
  gboolean two_finger_scroll_available;
  MetaInputSettingsPrivate *priv;

  if (device &&
      clutter_input_device_get_device_type (device) != CLUTTER_TOUCHPAD_DEVICE)
    return;

  priv = meta_input_settings_get_instance_private (input_settings);
  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  edge_scroll_enabled = g_settings_get_boolean (priv->touchpad_settings, "edge-scrolling-enabled");
  two_finger_scroll_enabled = g_settings_get_boolean (priv->touchpad_settings, "two-finger-scrolling-enabled");
  two_finger_scroll_available = g_hash_table_size (priv->two_finger_devices) > 0;

  /* If both are enabled we prefer two finger. */
  if (edge_scroll_enabled && two_finger_scroll_enabled && two_finger_scroll_available)
    edge_scroll_enabled = FALSE;

  if (device)
    {
      settings_device_set_bool_setting (input_settings, device,
                                        input_settings_class->set_edge_scroll,
                                        edge_scroll_enabled);
    }
  else
    {
      settings_set_bool_setting (input_settings, CLUTTER_TOUCHPAD_DEVICE,
                                 (ConfigBoolFunc) input_settings_class->set_edge_scroll,
                                 edge_scroll_enabled);
    }
}

static void
update_touchpad_two_finger_scroll (MetaInputSettings *input_settings,
                                   ClutterInputDevice *device)
{
  MetaInputSettingsClass *input_settings_class;
  gboolean two_finger_scroll_enabled;
  MetaInputSettingsPrivate *priv;

  if (device &&
      clutter_input_device_get_device_type (device) != CLUTTER_TOUCHPAD_DEVICE)
    return;

  priv = meta_input_settings_get_instance_private (input_settings);
  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  two_finger_scroll_enabled = g_settings_get_boolean (priv->touchpad_settings, "two-finger-scrolling-enabled");

  /* Disable edge since they can't both be set. */
  if (two_finger_scroll_enabled)
    update_touchpad_edge_scroll (input_settings, device);

  if (device)
    {
      settings_device_set_bool_setting (input_settings, device,
                                        input_settings_class->set_two_finger_scroll,
                                        two_finger_scroll_enabled);
    }
  else
    {
      settings_set_bool_setting (input_settings, CLUTTER_TOUCHPAD_DEVICE,
                                 (ConfigBoolFunc) input_settings_class->set_two_finger_scroll,
                                 two_finger_scroll_enabled);
    }

  /* Edge might have been disabled because two finger was on. */
  if (!two_finger_scroll_enabled)
    update_touchpad_edge_scroll (input_settings, device);
}

static void
update_touchpad_click_method (MetaInputSettings *input_settings,
                              ClutterInputDevice *device)
{
  MetaInputSettingsClass *input_settings_class;
  GDesktopTouchpadClickMethod method;
  MetaInputSettingsPrivate *priv;

  if (device &&
      clutter_input_device_get_device_type (device) != CLUTTER_TOUCHPAD_DEVICE)
    return;

  priv = meta_input_settings_get_instance_private (input_settings);
  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  method = g_settings_get_enum (priv->touchpad_settings, "click-method");

  if (device)
    {
      settings_device_set_uint_setting (input_settings, device,
                                        input_settings_class->set_click_method,
                                        method);
    }
  else
    {
      settings_set_uint_setting (input_settings, CLUTTER_TOUCHPAD_DEVICE,
                                 (ConfigUintFunc) input_settings_class->set_click_method,
                                 method);
    }
}

static void
update_touchpad_send_events (MetaInputSettings  *input_settings,
                             ClutterInputDevice *device)
{
  MetaInputSettingsClass *input_settings_class;
  MetaInputSettingsPrivate *priv;
  GDesktopDeviceSendEvents mode;

  if (device &&
      clutter_input_device_get_device_type (device) != CLUTTER_TOUCHPAD_DEVICE)
    return;

  priv = meta_input_settings_get_instance_private (input_settings);
  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  mode = g_settings_get_enum (priv->touchpad_settings, "send-events");

  if (device)
    {
      settings_device_set_uint_setting (input_settings, device,
                                        input_settings_class->set_send_events,
                                        mode);
    }
  else
    {
      settings_set_uint_setting (input_settings, CLUTTER_TOUCHPAD_DEVICE,
                                 input_settings_class->set_send_events,
                                 mode);
    }
}

gboolean
meta_input_device_is_trackball (ClutterInputDevice *device)
{
  gboolean is_trackball;
  char *name;

  if (clutter_input_device_get_device_mode (device) == CLUTTER_INPUT_MODE_MASTER)
    return FALSE;

  name = g_ascii_strdown (clutter_input_device_get_device_name (device), -1);
  is_trackball = strstr (name, "trackball") != NULL;
  g_free (name);

  return is_trackball;
}

static void
update_trackball_scroll_button (MetaInputSettings  *input_settings,
                                ClutterInputDevice *device)
{
  MetaInputSettingsClass *input_settings_class;
  MetaInputSettingsPrivate *priv;
  guint button;

  if (device && !meta_input_device_is_trackball (device))
    return;

  priv = meta_input_settings_get_instance_private (input_settings);
  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  /* This key is 'i' in the schema but it also specifies a minimum
   * range of 0 so the cast here is safe. */
  button = (guint) g_settings_get_int (priv->trackball_settings, "scroll-wheel-emulation-button");

  if (device)
    {
      input_settings_class->set_scroll_button (input_settings, device, button);
    }
  else if (!device)
    {
      const GSList *devices;

      devices = clutter_device_manager_peek_devices (priv->device_manager);

      while (devices)
        {
          device = devices->data;

          if (meta_input_device_is_trackball (device))
            input_settings_class->set_scroll_button (input_settings, device, button);

          devices = devices->next;
        }
    }
}

static void
update_keyboard_repeat (MetaInputSettings *input_settings)
{
  MetaInputSettingsClass *input_settings_class;
  MetaInputSettingsPrivate *priv;
  guint delay, interval;
  gboolean repeat;

  priv = meta_input_settings_get_instance_private (input_settings);
  repeat = g_settings_get_boolean (priv->keyboard_settings, "repeat");
  delay = g_settings_get_uint (priv->keyboard_settings, "delay");
  interval = g_settings_get_uint (priv->keyboard_settings, "repeat-interval");

  delay = MAX (1, delay);
  interval = MAX (1, interval);

  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  input_settings_class->set_keyboard_repeat (input_settings,
                                             repeat, delay, interval);
}

static gboolean
logical_monitor_has_monitor (MetaMonitorManager *monitor_manager,
                             MetaLogicalMonitor *logical_monitor,
                             const char         *vendor,
                             const char         *product,
                             const char         *serial)
{
  GList *monitors;
  GList *l;

  monitors = meta_monitor_manager_get_monitors (monitor_manager);
  for (l = monitors; l; l = l->next)
    {
      MetaMonitor *monitor = l->data;

      if (g_strcmp0 (meta_monitor_get_vendor (monitor), vendor) == 0 &&
          g_strcmp0 (meta_monitor_get_product (monitor), product) == 0 &&
          g_strcmp0 (meta_monitor_get_serial (monitor), serial) == 0)
        return TRUE;
    }

  return FALSE;
}

static MetaLogicalMonitor *
meta_input_settings_find_logical_monitor (MetaInputSettings  *input_settings,
                                          GSettings          *settings,
                                          ClutterInputDevice *device)
{
  MetaInputSettingsPrivate *priv;
  MetaMonitorManager *monitor_manager;
  guint n_values;
  GList *logical_monitors;
  GList *l;
  gchar **edid;

  priv = meta_input_settings_get_instance_private (input_settings);
  edid = g_settings_get_strv (settings, "display");
  n_values = g_strv_length (edid);

  if (n_values != 3)
    {
      g_warning ("EDID configuration for device '%s' "
                 "is incorrect, must have 3 values",
                 clutter_input_device_get_device_name (device));
      return NULL;
    }

  if (!*edid[0] && !*edid[1] && !*edid[2])
    return NULL;

  monitor_manager = priv->monitor_manager;
  logical_monitors =
    meta_monitor_manager_get_logical_monitors (monitor_manager);
  for (l = logical_monitors; l; l = l->next)
    {
      MetaLogicalMonitor *logical_monitor = l->data;

      if (logical_monitor_has_monitor (monitor_manager,
                                       logical_monitor,
                                       edid[0],
                                       edid[1],
                                       edid[2]))
        return logical_monitor;
    }

  return NULL;
}

static void
update_tablet_keep_aspect (MetaInputSettings  *input_settings,
                           GSettings          *settings,
                           ClutterInputDevice *device)
{
  MetaInputSettingsClass *input_settings_class;
  MetaLogicalMonitor *logical_monitor = NULL;
  gboolean keep_aspect;

  if (clutter_input_device_get_device_type (device) != CLUTTER_TABLET_DEVICE &&
      clutter_input_device_get_device_type (device) != CLUTTER_PEN_DEVICE &&
      clutter_input_device_get_device_type (device) != CLUTTER_ERASER_DEVICE)
    return;

#ifdef HAVE_LIBWACOM
  {
    WacomDevice *wacom_device;

    wacom_device = meta_input_settings_get_tablet_wacom_device (input_settings,
                                                                device);
    /* Keep aspect only makes sense in external tablets */
    if (wacom_device &&
        libwacom_get_integration_flags (wacom_device) != WACOM_DEVICE_INTEGRATED_NONE)
      return;
  }
#endif

  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);

  if (clutter_input_device_get_mapping_mode (device) ==
      CLUTTER_INPUT_DEVICE_MAPPING_ABSOLUTE)
    {
      keep_aspect = g_settings_get_boolean (settings, "keep-aspect");
      logical_monitor = meta_input_settings_find_logical_monitor (input_settings,
                                                                  settings,
                                                                  device);
    }
  else
    {
      keep_aspect = FALSE;
    }

  input_settings_class->set_tablet_keep_aspect (input_settings, device,
                                                logical_monitor, keep_aspect);
}

static void
update_device_display (MetaInputSettings  *input_settings,
                       GSettings          *settings,
                       ClutterInputDevice *device)
{
  MetaInputSettingsClass *input_settings_class;
  MetaInputSettingsPrivate *priv;
  gfloat matrix[6] = { 1, 0, 0, 0, 1, 0 };
  MetaLogicalMonitor *logical_monitor;

  if (clutter_input_device_get_device_type (device) != CLUTTER_TABLET_DEVICE &&
      clutter_input_device_get_device_type (device) != CLUTTER_PEN_DEVICE &&
      clutter_input_device_get_device_type (device) != CLUTTER_ERASER_DEVICE &&
      clutter_input_device_get_device_type (device) != CLUTTER_TOUCHSCREEN_DEVICE)
    return;

  priv = meta_input_settings_get_instance_private (input_settings);
  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);

  /* If mapping is relative, the device can move on all displays */
  if (clutter_input_device_get_device_type (device) != CLUTTER_TABLET_DEVICE ||
      clutter_input_device_get_mapping_mode (device) ==
      CLUTTER_INPUT_DEVICE_MAPPING_ABSOLUTE)
    logical_monitor = meta_input_settings_find_logical_monitor (input_settings,
                                                                settings,
                                                                device);
  else
    logical_monitor = NULL;

  if (logical_monitor)
    meta_monitor_manager_get_monitor_matrix (priv->monitor_manager,
                                             logical_monitor, matrix);

  input_settings_class->set_matrix (input_settings, device, matrix);

  /* Ensure the keep-aspect mapping is updated */
  update_tablet_keep_aspect (input_settings, settings, device);
}

static void
update_tablet_mapping (MetaInputSettings  *input_settings,
                       GSettings          *settings,
                       ClutterInputDevice *device)
{
  MetaInputSettingsClass *input_settings_class;
  GDesktopTabletMapping mapping;

  if (clutter_input_device_get_device_type (device) != CLUTTER_TABLET_DEVICE &&
      clutter_input_device_get_device_type (device) != CLUTTER_PEN_DEVICE &&
      clutter_input_device_get_device_type (device) != CLUTTER_ERASER_DEVICE)
    return;

#ifdef HAVE_LIBWACOM
  {
    WacomDevice *wacom_device;

    wacom_device = meta_input_settings_get_tablet_wacom_device (input_settings,
                                                                device);
    /* Tablet mapping only makes sense on external tablets */
    if (wacom_device &&
        (libwacom_get_integration_flags (wacom_device) != WACOM_DEVICE_INTEGRATED_NONE))
      return;
  }
#endif

  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  mapping = g_settings_get_enum (settings, "mapping");

  settings_device_set_uint_setting (input_settings, device,
                                    input_settings_class->set_tablet_mapping,
                                    mapping);

  /* Relative mapping disables keep-aspect/display */
  update_tablet_keep_aspect (input_settings, settings, device);
  update_device_display (input_settings, settings, device);
}

static void
update_tablet_area (MetaInputSettings  *input_settings,
                    GSettings          *settings,
                    ClutterInputDevice *device)
{
  MetaInputSettingsClass *input_settings_class;
  GVariant *variant;
  const gdouble *area;
  gsize n_elems;

  if (clutter_input_device_get_device_type (device) != CLUTTER_TABLET_DEVICE &&
      clutter_input_device_get_device_type (device) != CLUTTER_PEN_DEVICE &&
      clutter_input_device_get_device_type (device) != CLUTTER_ERASER_DEVICE)
    return;

#ifdef HAVE_LIBWACOM
  {
    WacomDevice *wacom_device;

    wacom_device = meta_input_settings_get_tablet_wacom_device (input_settings,
                                                                device);
    /* Tablet area only makes sense on system/display integrated tablets */
    if (wacom_device &&
        (libwacom_get_integration_flags (wacom_device) &
         (WACOM_DEVICE_INTEGRATED_SYSTEM | WACOM_DEVICE_INTEGRATED_DISPLAY)) == 0)
      return;
  }
#endif

  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  variant = g_settings_get_value (settings, "area");

  area = g_variant_get_fixed_array (variant, &n_elems, sizeof (gdouble));
  if (n_elems == 4)
    {
      input_settings_class->set_tablet_area (input_settings, device,
                                             area[0], area[1],
                                             area[2], area[3]);
    }

  g_variant_unref (variant);
}

static void
update_tablet_left_handed (MetaInputSettings  *input_settings,
                           GSettings          *settings,
                           ClutterInputDevice *device)
{
  MetaInputSettingsClass *input_settings_class;
  gboolean enabled;

  if (clutter_input_device_get_device_type (device) != CLUTTER_TABLET_DEVICE &&
      clutter_input_device_get_device_type (device) != CLUTTER_PEN_DEVICE &&
      clutter_input_device_get_device_type (device) != CLUTTER_ERASER_DEVICE &&
      clutter_input_device_get_device_type (device) != CLUTTER_PAD_DEVICE)
    return;

#ifdef HAVE_LIBWACOM
  {
    WacomDevice *wacom_device;

    wacom_device = meta_input_settings_get_tablet_wacom_device (input_settings,
                                                                device);
    /* Left handed mode only makes sense on external tablets */
    if (wacom_device &&
        (libwacom_get_integration_flags (wacom_device) != WACOM_DEVICE_INTEGRATED_NONE))
      return;
  }
#endif

  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  enabled = g_settings_get_boolean (settings, "left-handed");

  settings_device_set_bool_setting (input_settings, device,
                                    input_settings_class->set_left_handed,
                                    enabled);
}

static void
meta_input_settings_changed_cb (GSettings  *settings,
                                const char *key,
                                gpointer    user_data)
{
  MetaInputSettings *input_settings = META_INPUT_SETTINGS (user_data);
  MetaInputSettingsPrivate *priv = meta_input_settings_get_instance_private (input_settings);

  if (settings == priv->mouse_settings)
    {
      if (strcmp (key, "left-handed") == 0)
        update_mouse_left_handed (input_settings, NULL);
      else if (strcmp (key, "speed") == 0)
        update_device_speed (input_settings, NULL);
      else if (strcmp (key, "natural-scroll") == 0)
        update_device_natural_scroll (input_settings, NULL);
      else if (strcmp (key, "accel-profile") == 0)
        update_pointer_accel_profile (input_settings, settings, NULL);
    }
  else if (settings == priv->touchpad_settings)
    {
      if (strcmp (key, "left-handed") == 0)
        update_touchpad_left_handed (input_settings, NULL);
      else if (strcmp (key, "speed") == 0)
        update_device_speed (input_settings, NULL);
      else if (strcmp (key, "natural-scroll") == 0)
        update_device_natural_scroll (input_settings, NULL);
      else if (strcmp (key, "tap-to-click") == 0)
        update_touchpad_tap_enabled (input_settings, NULL);
      else if (strcmp (key, "send-events") == 0)
        update_touchpad_send_events (input_settings, NULL);
      else if (strcmp (key, "edge-scrolling-enabled") == 0)
        update_touchpad_edge_scroll (input_settings, NULL);
      else if (strcmp (key, "two-finger-scrolling-enabled") == 0)
        update_touchpad_two_finger_scroll (input_settings, NULL);
      else if (strcmp (key, "click-method") == 0)
        update_touchpad_click_method (input_settings, NULL);
    }
  else if (settings == priv->trackball_settings)
    {
      if (strcmp (key, "scroll-wheel-emulation-button") == 0)
        update_trackball_scroll_button (input_settings, NULL);
      else if (strcmp (key, "accel-profile") == 0)
        update_pointer_accel_profile (input_settings, settings, NULL);
    }
  else if (settings == priv->keyboard_settings)
    {
      if (strcmp (key, "repeat") == 0 ||
          strcmp (key, "repeat-interval") == 0 ||
          strcmp (key, "delay") == 0)
        update_keyboard_repeat (input_settings);
    }
}

static void
mapped_device_changed_cb (GSettings         *settings,
                          const gchar       *key,
                          DeviceMappingInfo *info)
{
  if (strcmp (key, "display") == 0)
    update_device_display (info->input_settings, settings, info->device);
  else if (strcmp (key, "mapping") == 0)
    update_tablet_mapping (info->input_settings, settings, info->device);
  else if (strcmp (key, "area") == 0)
    update_tablet_area (info->input_settings, settings, info->device);
  else if (strcmp (key, "keep-aspect") == 0)
    update_tablet_keep_aspect (info->input_settings, settings, info->device);
  else if (strcmp (key, "left-handed") == 0)
    update_tablet_left_handed (info->input_settings, settings, info->device);
}

static void
apply_mappable_device_settings (MetaInputSettings *input_settings,
                                DeviceMappingInfo *info)
{
  update_device_display (input_settings, info->settings, info->device);

  if (clutter_input_device_get_device_type (info->device) == CLUTTER_TABLET_DEVICE ||
      clutter_input_device_get_device_type (info->device) == CLUTTER_PAD_DEVICE)
    {
      update_tablet_mapping (input_settings, info->settings, info->device);
      update_tablet_area (input_settings, info->settings, info->device);
      update_tablet_keep_aspect (input_settings, info->settings, info->device);
      update_tablet_left_handed (input_settings, info->settings, info->device);
    }
}

static GSettings *
lookup_device_settings (ClutterInputDevice *device)
{
  const gchar *group, *schema, *vendor, *product;
  ClutterInputDeviceType type;
  GSettings *settings;
  gchar *path;

  type = clutter_input_device_get_device_type (device);

  if (type == CLUTTER_TOUCHSCREEN_DEVICE)
    {
      group = "touchscreens";
      schema = "org.gnome.desktop.peripherals.touchscreen";
    }
  else if (type == CLUTTER_TABLET_DEVICE ||
           type == CLUTTER_PEN_DEVICE ||
           type == CLUTTER_ERASER_DEVICE ||
           type == CLUTTER_CURSOR_DEVICE ||
           type == CLUTTER_PAD_DEVICE)
    {
      group = "tablets";
      schema = "org.gnome.desktop.peripherals.tablet";
    }
  else
    return NULL;

  vendor = clutter_input_device_get_vendor_id (device);
  product = clutter_input_device_get_product_id (device);
  path = g_strdup_printf ("/org/gnome/desktop/peripherals/%s/%s:%s/",
                          group, vendor, product);

  settings = g_settings_new_with_path (schema, path);
  g_free (path);

  return settings;
}

static GSettings *
lookup_tool_settings (ClutterInputDeviceTool *tool,
                      ClutterInputDevice     *device)
{
  GSettings *tool_settings;
  guint64 serial;
  gchar *path;

  tool_settings = g_object_get_qdata (G_OBJECT (tool), quark_tool_settings);
  if (tool_settings)
    return tool_settings;

  serial = clutter_input_device_tool_get_serial (tool);

  if (serial == 0)
    {
      path = g_strdup_printf ("/org/gnome/desktop/peripherals/stylus/default-%s:%s/",
                              clutter_input_device_get_vendor_id (device),
                              clutter_input_device_get_product_id (device));
    }
  else
    {
      path = g_strdup_printf ("/org/gnome/desktop/peripherals/stylus/%lx/", serial);
    }

  tool_settings =
    g_settings_new_with_path ("org.gnome.desktop.peripherals.tablet.stylus",
                              path);
  g_object_set_qdata_full (G_OBJECT (tool), quark_tool_settings, tool_settings,
                           (GDestroyNotify) g_object_unref);
  g_free (path);

  return tool_settings;
}

static GSettings *
lookup_pad_button_settings (ClutterInputDevice *device,
                            guint               button)
{
  const gchar *vendor, *product;
  GSettings *settings;
  gchar *path;

  vendor = clutter_input_device_get_vendor_id (device);
  product = clutter_input_device_get_product_id (device);
  path = g_strdup_printf ("/org/gnome/desktop/peripherals/tablets/%s:%s/button%c/",
                          vendor, product, 'A' + button);
  settings = g_settings_new_with_path ("org.gnome.desktop.peripherals.tablet.pad-button",
                                       path);
  g_free (path);

  return settings;
}

static void
monitors_changed_cb (MetaMonitorManager *monitor_manager,
                     MetaInputSettings  *input_settings)
{
  MetaInputSettingsPrivate *priv;
  ClutterInputDevice *device;
  DeviceMappingInfo *info;
  GHashTableIter iter;

  priv = meta_input_settings_get_instance_private (input_settings);
  g_hash_table_iter_init (&iter, priv->mappable_devices);

  while (g_hash_table_iter_next (&iter, (gpointer *) &device,
                                 (gpointer *) &info))
    update_device_display (input_settings, info->settings, device);
}

static void
device_mapping_info_free (DeviceMappingInfo *info)
{
#ifdef HAVE_LIBWACOM
  if (info->wacom_device)
    libwacom_destroy (info->wacom_device);
#endif
  g_object_unref (info->settings);
  g_slice_free (DeviceMappingInfo, info);
}

static gboolean
check_add_mappable_device (MetaInputSettings  *input_settings,
                           ClutterInputDevice *device)
{
  MetaInputSettingsPrivate *priv;
  DeviceMappingInfo *info;
  GSettings *settings;

  settings = lookup_device_settings (device);

  if (!settings)
    return FALSE;

  priv = meta_input_settings_get_instance_private (input_settings);

  info = g_slice_new0 (DeviceMappingInfo);
  info->input_settings = input_settings;
  info->device = device;
  info->settings = settings;

#ifdef HAVE_LIBWACOM
  if (clutter_input_device_get_device_type (device) == CLUTTER_TABLET_DEVICE ||
      clutter_input_device_get_device_type (device) == CLUTTER_PAD_DEVICE)
    {
      WacomError *error = libwacom_error_new ();

      info->wacom_device = libwacom_new_from_path (priv->wacom_db,
                                                   clutter_input_device_get_device_node (device),
                                                   WFALLBACK_NONE, error);
      if (!info->wacom_device)
        {
          g_warning ("Could not get tablet information for '%s': %s",
                     clutter_input_device_get_device_name (device),
                     libwacom_error_get_message (error));
        }

      libwacom_error_free (&error);
    }
#endif

  g_signal_connect (settings, "changed",
                    G_CALLBACK (mapped_device_changed_cb), info);

  g_hash_table_insert (priv->mappable_devices, device, info);

  apply_mappable_device_settings (input_settings, info);

  return TRUE;
}

static void
apply_device_settings (MetaInputSettings  *input_settings,
                       ClutterInputDevice *device)
{
  MetaInputSettingsPrivate *priv =
    meta_input_settings_get_instance_private (input_settings);

  update_device_speed (input_settings, device);
  update_device_natural_scroll (input_settings, device);

  update_mouse_left_handed (input_settings, device);
  update_pointer_accel_profile (input_settings,
                                priv->mouse_settings,
                                device);

  update_touchpad_left_handed (input_settings, device);
  update_touchpad_tap_enabled (input_settings, device);
  update_touchpad_send_events (input_settings, device);
  update_touchpad_two_finger_scroll (input_settings, device);
  update_touchpad_edge_scroll (input_settings, device);
  update_touchpad_click_method (input_settings, device);

  update_trackball_scroll_button (input_settings, device);
  update_pointer_accel_profile (input_settings,
                                priv->trackball_settings,
                                device);
}

static void
update_stylus_pressure (MetaInputSettings      *input_settings,
                        ClutterInputDevice     *device,
                        ClutterInputDeviceTool *tool)
{
  MetaInputSettingsClass *input_settings_class;
  GSettings *tool_settings;
  const gint32 *curve;
  GVariant *variant;
  gsize n_elems;

  if (clutter_input_device_get_device_type (device) != CLUTTER_TABLET_DEVICE &&
      clutter_input_device_get_device_type (device) != CLUTTER_PEN_DEVICE &&
      clutter_input_device_get_device_type (device) != CLUTTER_ERASER_DEVICE)
    return;

  if (!tool)
    return;

  tool_settings = lookup_tool_settings (tool, device);

  if (clutter_input_device_tool_get_tool_type (tool) ==
      CLUTTER_INPUT_DEVICE_TOOL_ERASER)
    variant = g_settings_get_value (tool_settings, "eraser-pressure-curve");
  else
    variant = g_settings_get_value (tool_settings, "pressure-curve");

  curve = g_variant_get_fixed_array (variant, &n_elems, sizeof (gint32));
  if (n_elems != 4)
    return;

  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  input_settings_class->set_stylus_pressure (input_settings, device, tool, curve);
}

static void
update_stylus_buttonmap (MetaInputSettings      *input_settings,
                         ClutterInputDevice     *device,
                         ClutterInputDeviceTool *tool)
{
  MetaInputSettingsClass *input_settings_class;
  GDesktopStylusButtonAction primary, secondary;
  GSettings *tool_settings;

  if (clutter_input_device_get_device_type (device) != CLUTTER_TABLET_DEVICE &&
      clutter_input_device_get_device_type (device) != CLUTTER_PEN_DEVICE &&
      clutter_input_device_get_device_type (device) != CLUTTER_ERASER_DEVICE)
    return;

  if (!tool)
    return;

  tool_settings = lookup_tool_settings (tool, device);

  primary = g_settings_get_enum (tool_settings, "button-action");
  secondary = g_settings_get_enum (tool_settings, "secondary-button-action");

  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  input_settings_class->set_stylus_button_map (input_settings, device, tool,
                                               primary, secondary);
}

static void
apply_stylus_settings (MetaInputSettings      *input_settings,
                       ClutterInputDevice     *device,
                       ClutterInputDeviceTool *tool)
{
  update_stylus_pressure (input_settings, device, tool);
  update_stylus_buttonmap (input_settings, device, tool);
}

static void
evaluate_two_finger_scrolling (MetaInputSettings  *input_settings,
                               ClutterInputDevice *device)
{
  MetaInputSettingsClass *klass;
  MetaInputSettingsPrivate *priv;

  if (clutter_input_device_get_device_type (device) != CLUTTER_TOUCHPAD_DEVICE)
    return;

  klass = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  priv = meta_input_settings_get_instance_private (input_settings);

  if (klass->has_two_finger_scroll (input_settings, device))
    g_hash_table_add (priv->two_finger_devices, device);
}

static void
meta_input_settings_device_added (ClutterDeviceManager *device_manager,
                                  ClutterInputDevice   *device,
                                  MetaInputSettings    *input_settings)
{
  if (clutter_input_device_get_device_mode (device) == CLUTTER_INPUT_MODE_MASTER)
    return;

  evaluate_two_finger_scrolling (input_settings, device);

  apply_device_settings (input_settings, device);
  check_add_mappable_device (input_settings, device);
}

static void
meta_input_settings_device_removed (ClutterDeviceManager *device_manager,
                                    ClutterInputDevice   *device,
                                    MetaInputSettings    *input_settings)
{
  MetaInputSettingsPrivate *priv;

  priv = meta_input_settings_get_instance_private (input_settings);
  g_hash_table_remove (priv->mappable_devices, device);

  if (g_hash_table_remove (priv->two_finger_devices, device) &&
      g_hash_table_size (priv->two_finger_devices) == 0)
    apply_device_settings (input_settings, NULL);
}

static void
meta_input_settings_tool_changed (ClutterDeviceManager   *device_manager,
                                  ClutterInputDevice     *device,
                                  ClutterInputDeviceTool *tool,
                                  MetaInputSettings      *input_settings)
{
  if (!tool)
    return;

  apply_stylus_settings (input_settings, device, tool);
}

static void
check_mappable_devices (MetaInputSettings *input_settings)
{
  MetaInputSettingsPrivate *priv;
  const GSList *devices, *l;

  priv = meta_input_settings_get_instance_private (input_settings);
  devices = clutter_device_manager_peek_devices (priv->device_manager);

  for (l = devices; l; l = l->next)
    {
      ClutterInputDevice *device = l->data;

      if (clutter_input_device_get_device_mode (device) == CLUTTER_INPUT_MODE_MASTER)
        continue;

      check_add_mappable_device (input_settings, device);
    }
}

static void
meta_input_settings_constructed (GObject *object)
{
  MetaInputSettings *input_settings = META_INPUT_SETTINGS (object);
  GSList *devices, *d;

  devices = meta_input_settings_get_devices (input_settings, CLUTTER_TOUCHPAD_DEVICE);
  for (d = devices; d; d = d->next)
    evaluate_two_finger_scrolling (input_settings, d->data);

  g_slist_free (devices);

  apply_device_settings (input_settings, NULL);
  update_keyboard_repeat (input_settings);
  check_mappable_devices (input_settings);
}

static void
meta_input_settings_class_init (MetaInputSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = meta_input_settings_dispose;
  object_class->constructed = meta_input_settings_constructed;

  quark_tool_settings =
    g_quark_from_static_string ("meta-input-settings-tool-settings");
}

static void
meta_input_settings_init (MetaInputSettings *settings)
{
  MetaInputSettingsPrivate *priv;

  priv = meta_input_settings_get_instance_private (settings);
  priv->device_manager = clutter_device_manager_get_default ();
  g_signal_connect (priv->device_manager, "device-added",
                    G_CALLBACK (meta_input_settings_device_added), settings);
  g_signal_connect (priv->device_manager, "device-removed",
                    G_CALLBACK (meta_input_settings_device_removed), settings);
  g_signal_connect (priv->device_manager, "tool-changed",
                    G_CALLBACK (meta_input_settings_tool_changed), settings);

  priv->mouse_settings = g_settings_new ("org.gnome.desktop.peripherals.mouse");
  g_signal_connect (priv->mouse_settings, "changed",
                    G_CALLBACK (meta_input_settings_changed_cb), settings);

  priv->touchpad_settings = g_settings_new ("org.gnome.desktop.peripherals.touchpad");
  g_signal_connect (priv->touchpad_settings, "changed",
                    G_CALLBACK (meta_input_settings_changed_cb), settings);

  priv->trackball_settings = g_settings_new ("org.gnome.desktop.peripherals.trackball");
  g_signal_connect (priv->trackball_settings, "changed",
                    G_CALLBACK (meta_input_settings_changed_cb), settings);

  priv->keyboard_settings = g_settings_new ("org.gnome.desktop.peripherals.keyboard");
  g_signal_connect (priv->keyboard_settings, "changed",
                    G_CALLBACK (meta_input_settings_changed_cb), settings);

  priv->gsd_settings = g_settings_new ("org.gnome.settings-daemon.peripherals.mouse");

  g_settings_bind (priv->gsd_settings, "double-click",
                   clutter_settings_get_default(), "double-click-time",
                   G_SETTINGS_BIND_GET);

  priv->mappable_devices =
    g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) device_mapping_info_free);

  priv->monitor_manager = g_object_ref (meta_monitor_manager_get ());
  g_signal_connect (priv->monitor_manager, "monitors-changed",
                    G_CALLBACK (monitors_changed_cb), settings);

#ifdef HAVE_LIBWACOM
  priv->wacom_db = libwacom_database_new ();
  if (!priv->wacom_db)
    {
      g_warning ("Could not create database of Wacom devices, "
                 "expect tablets to misbehave");
    }
#endif

  priv->two_finger_devices = g_hash_table_new (NULL, NULL);
}

GSettings *
meta_input_settings_get_tablet_settings (MetaInputSettings  *settings,
                                         ClutterInputDevice *device)
{
  MetaInputSettingsPrivate *priv;
  DeviceMappingInfo *info;

  g_return_val_if_fail (META_IS_INPUT_SETTINGS (settings), NULL);
  g_return_val_if_fail (CLUTTER_IS_INPUT_DEVICE (device), NULL);

  priv = meta_input_settings_get_instance_private (settings);
  info = g_hash_table_lookup (priv->mappable_devices, device);

  return info ? g_object_ref (info->settings) : NULL;
}

MetaLogicalMonitor *
meta_input_settings_get_tablet_logical_monitor (MetaInputSettings  *settings,
                                                ClutterInputDevice *device)
{
  MetaInputSettingsPrivate *priv;
  DeviceMappingInfo *info;

  g_return_val_if_fail (META_IS_INPUT_SETTINGS (settings), NULL);
  g_return_val_if_fail (CLUTTER_IS_INPUT_DEVICE (device), NULL);

  priv = meta_input_settings_get_instance_private (settings);
  info = g_hash_table_lookup (priv->mappable_devices, device);
  if (!info)
    return NULL;

  return meta_input_settings_find_logical_monitor (settings,
                                                   info->settings,
                                                   device);
}

GDesktopTabletMapping
meta_input_settings_get_tablet_mapping (MetaInputSettings  *settings,
                                        ClutterInputDevice *device)
{
  MetaInputSettingsPrivate *priv;
  DeviceMappingInfo *info;

  g_return_val_if_fail (META_IS_INPUT_SETTINGS (settings),
                        G_DESKTOP_TABLET_MAPPING_ABSOLUTE);
  g_return_val_if_fail (CLUTTER_IS_INPUT_DEVICE (device),
                        G_DESKTOP_TABLET_MAPPING_ABSOLUTE);

  priv = meta_input_settings_get_instance_private (settings);
  info = g_hash_table_lookup (priv->mappable_devices, device);
  g_return_val_if_fail (info != NULL, G_DESKTOP_TABLET_MAPPING_ABSOLUTE);

  return g_settings_get_enum (info->settings, "mapping");
}

static GDesktopPadButtonAction
meta_input_settings_get_pad_button_action (MetaInputSettings   *input_settings,
                                           ClutterInputDevice  *pad,
                                           guint                button)
{
  GDesktopPadButtonAction action;
  GSettings *settings;

  g_return_val_if_fail (META_IS_INPUT_SETTINGS (input_settings),
                        G_DESKTOP_PAD_BUTTON_ACTION_NONE);
  g_return_val_if_fail (CLUTTER_IS_INPUT_DEVICE (pad),
                        G_DESKTOP_PAD_BUTTON_ACTION_NONE);

  settings = lookup_pad_button_settings (pad, button);
  action = g_settings_get_enum (settings, "action");
  g_object_unref (settings);

  return action;
}

#ifdef HAVE_LIBWACOM
WacomDevice *
meta_input_settings_get_tablet_wacom_device (MetaInputSettings *settings,
                                             ClutterInputDevice *device)
{
  MetaInputSettingsPrivate *priv;
  DeviceMappingInfo *info;

  g_return_val_if_fail (META_IS_INPUT_SETTINGS (settings), NULL);
  g_return_val_if_fail (CLUTTER_IS_INPUT_DEVICE (device), NULL);

  priv = meta_input_settings_get_instance_private (settings);
  info = g_hash_table_lookup (priv->mappable_devices, device);
  g_return_val_if_fail (info != NULL, NULL);

  return info->wacom_device;
}
#endif /* HAVE_LIBWACOM */

static gboolean
cycle_logical_monitors (MetaInputSettings   *settings,
                        MetaLogicalMonitor  *current_logical_monitor,
                        MetaLogicalMonitor **next_logical_monitor)
{
  MetaInputSettingsPrivate *priv =
    meta_input_settings_get_instance_private (settings);
  MetaMonitorManager *monitor_manager = priv->monitor_manager;
  GList *logical_monitors;

  /* We cycle between:
   * - the span of all monitors (current_output = NULL)
   * - each monitor individually.
   */

  logical_monitors =
    meta_monitor_manager_get_logical_monitors (monitor_manager);

  if (!current_logical_monitor)
    {
      *next_logical_monitor = logical_monitors->data;
    }
  else
    {
      GList *l;

      l = g_list_find (logical_monitors, current_logical_monitor);
      if (l->next)
        *next_logical_monitor = l->next->data;
      else
        *next_logical_monitor = logical_monitors->data;
    }

  return TRUE;
}

static void
meta_input_settings_cycle_tablet_output (MetaInputSettings  *input_settings,
                                         ClutterInputDevice *device)
{
  MetaInputSettingsPrivate *priv;
  DeviceMappingInfo *info;
  MetaLogicalMonitor *logical_monitor;
  const gchar *edid[4] = { 0 }, *pretty_name = NULL;

  g_return_if_fail (META_IS_INPUT_SETTINGS (input_settings));
  g_return_if_fail (CLUTTER_IS_INPUT_DEVICE (device));
  g_return_if_fail (clutter_input_device_get_device_type (device) == CLUTTER_TABLET_DEVICE ||
                    clutter_input_device_get_device_type (device) == CLUTTER_PAD_DEVICE);

  priv = meta_input_settings_get_instance_private (input_settings);
  info = g_hash_table_lookup (priv->mappable_devices, device);
  g_return_if_fail (info != NULL);

#ifdef HAVE_LIBWACOM
  if (info->wacom_device)
    {
      /* Output rotation only makes sense on external tablets */
      if (libwacom_get_integration_flags (info->wacom_device) != WACOM_DEVICE_INTEGRATED_NONE)
        return;

      pretty_name = libwacom_get_name (info->wacom_device);
    }
#endif

  logical_monitor = meta_input_settings_find_logical_monitor (input_settings,
                                                              info->settings,
                                                              device);
  if (!cycle_logical_monitors (input_settings,
                               logical_monitor,
                               &logical_monitor))
    return;

  if (logical_monitor)
    {
      MetaMonitor *monitor;

      /* Pick an arbitrary monitor in the logical monitor to represent it. */
      monitor = meta_logical_monitor_get_monitors (logical_monitor)->data;
      edid[0] = meta_monitor_get_vendor (monitor);
      edid[1] = meta_monitor_get_product (monitor);
      edid[2] = meta_monitor_get_serial (monitor);
    }
  else
    {
      edid[0] = "";
      edid[1] = "";
      edid[2] = "";
    }
  g_settings_set_strv (info->settings, "display", edid);

  meta_display_show_tablet_mapping_notification (meta_get_display (),
                                                 device, pretty_name);
}

static void
emulate_modifiers (ClutterVirtualInputDevice *device,
                   ClutterModifierType        mods,
                   ClutterKeyState            state)
{
  guint i;
  struct {
    ClutterModifierType mod;
    guint keyval;
  } mod_map[] = {
    { CLUTTER_SHIFT_MASK, CLUTTER_KEY_Shift_L },
    { CLUTTER_CONTROL_MASK, CLUTTER_KEY_Control_L },
    { CLUTTER_MOD1_MASK, CLUTTER_KEY_Meta_L }
  };

  for (i = 0; i < G_N_ELEMENTS (mod_map); i++)
    {
      if ((mods & mod_map[i].mod) == 0)
        continue;

      clutter_virtual_input_device_notify_keyval (device,
                                                  clutter_get_current_event_time (),
                                                  mod_map[i].keyval, state);
    }
}

static void
meta_input_settings_emulate_keybinding (MetaInputSettings  *input_settings,
                                        ClutterInputDevice *pad,
                                        guint               button,
                                        gboolean            is_press)
{
  MetaInputSettingsPrivate *priv;
  ClutterKeyState state;
  GSettings *settings;
  guint key, mods;
  gchar *accel;

  priv = meta_input_settings_get_instance_private (input_settings);
  settings = lookup_pad_button_settings (pad, button);
  accel = g_settings_get_string (settings, "keybinding");
  g_object_unref (settings);

  /* FIXME: This is appalling */
  gtk_accelerator_parse (accel, &key, &mods);
  g_free (accel);

  if (!priv->virtual_pad_keyboard)
    {
      ClutterDeviceManager *manager = clutter_device_manager_get_default ();

      priv->virtual_pad_keyboard =
        clutter_device_manager_create_virtual_device (manager,
                                                      CLUTTER_KEYBOARD_DEVICE);
    }

  state = is_press ? CLUTTER_KEY_STATE_PRESSED : CLUTTER_KEY_STATE_RELEASED;

  if (is_press)
    emulate_modifiers (priv->virtual_pad_keyboard, mods, state);

  clutter_virtual_input_device_notify_keyval (priv->virtual_pad_keyboard,
                                              clutter_get_current_event_time (),
                                              key, state);
  if (!is_press)
    emulate_modifiers (priv->virtual_pad_keyboard, mods, state);
}

gboolean
meta_input_settings_is_pad_button_grabbed (MetaInputSettings  *input_settings,
                                           ClutterInputDevice *pad,
                                           guint               button)
{
  g_return_val_if_fail (META_IS_INPUT_SETTINGS (input_settings), FALSE);
  g_return_val_if_fail (CLUTTER_IS_INPUT_DEVICE (pad), FALSE);
  g_return_val_if_fail (clutter_input_device_get_device_type (pad) ==
                        CLUTTER_PAD_DEVICE, FALSE);

  return (meta_input_settings_get_pad_button_action (input_settings, pad, button) !=
          G_DESKTOP_PAD_BUTTON_ACTION_NONE);
}

gboolean
meta_input_settings_handle_pad_button (MetaInputSettings           *input_settings,
                                       const ClutterPadButtonEvent *event)
{
  GDesktopPadButtonAction action;
  ClutterInputDevice *pad;
  gint button, group, mode;
  gboolean is_press;

  g_return_val_if_fail (META_IS_INPUT_SETTINGS (input_settings), FALSE);
  g_return_val_if_fail (event->type == CLUTTER_PAD_BUTTON_PRESS ||
                        event->type == CLUTTER_PAD_BUTTON_RELEASE, FALSE);

  pad = clutter_event_get_source_device ((ClutterEvent *) event);
  button = event->button;
  mode = event->mode;
  group = clutter_input_device_get_mode_switch_button_group (pad, button);
  is_press = event->type == CLUTTER_PAD_BUTTON_PRESS;

  if (is_press && group >= 0)
    {
      guint n_modes = clutter_input_device_get_group_n_modes (pad, group);
      const gchar *pretty_name = NULL;
#ifdef HAVE_LIBWACOM
      MetaInputSettingsPrivate *priv;
      DeviceMappingInfo *info;

      priv = meta_input_settings_get_instance_private (input_settings);
      info = g_hash_table_lookup (priv->mappable_devices, pad);

      if (info && info->wacom_device)
        pretty_name = libwacom_get_name (info->wacom_device);
#endif
      meta_display_notify_pad_group_switch (meta_get_display (), pad,
                                            pretty_name, group, mode, n_modes);
    }

  action = meta_input_settings_get_pad_button_action (input_settings, pad, button);

  switch (action)
    {
    case G_DESKTOP_PAD_BUTTON_ACTION_SWITCH_MONITOR:
      if (is_press)
        meta_input_settings_cycle_tablet_output (input_settings, pad);
      return TRUE;
    case G_DESKTOP_PAD_BUTTON_ACTION_HELP:
      if (is_press)
        meta_display_request_pad_osd (meta_get_display (), pad, FALSE);
      return TRUE;
    case G_DESKTOP_PAD_BUTTON_ACTION_KEYBINDING:
      meta_input_settings_emulate_keybinding (input_settings, pad,
                                              button, is_press);
      return TRUE;
    case G_DESKTOP_PAD_BUTTON_ACTION_NONE:
    default:
      return FALSE;
    }
}

gchar *
meta_input_settings_get_pad_button_action_label (MetaInputSettings  *input_settings,
                                                 ClutterInputDevice *pad,
                                                 guint               button)
{
  GDesktopPadButtonAction action;
  gint group;

  g_return_val_if_fail (META_IS_INPUT_SETTINGS (input_settings), NULL);
  g_return_val_if_fail (CLUTTER_IS_INPUT_DEVICE (pad), NULL);
  g_return_val_if_fail (clutter_input_device_get_device_type (pad) ==
                        CLUTTER_PAD_DEVICE, NULL);

  group = clutter_input_device_get_mode_switch_button_group (pad, button);

  if (group >= 0)
    {
      /* TRANSLATORS: This string refers to a button that switches between
       * different modes.
       */
      return g_strdup_printf (_("Mode Switch (Group %d)"), group);
    }

  action = meta_input_settings_get_pad_button_action (input_settings, pad, button);

  switch (action)
    {
    case G_DESKTOP_PAD_BUTTON_ACTION_KEYBINDING:
      {
        GSettings *settings;
        gchar *accel;

        settings = lookup_pad_button_settings (pad, button);
        accel = g_settings_get_string (settings, "keybinding");
        g_object_unref (settings);

        return accel;
      }
    case G_DESKTOP_PAD_BUTTON_ACTION_SWITCH_MONITOR:
      /* TRANSLATORS: This string refers to an action, cycles drawing tablets'
       * mapping through the available outputs.
       */
      return g_strdup (_("Switch monitor"));
    case G_DESKTOP_PAD_BUTTON_ACTION_HELP:
      return g_strdup (_("Show on-screen help"));
    case G_DESKTOP_PAD_BUTTON_ACTION_NONE:
    default:
      return NULL;
    }
}
