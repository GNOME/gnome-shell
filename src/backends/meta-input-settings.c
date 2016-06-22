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
#include "x11/meta-input-settings-x11.h"

#ifdef HAVE_NATIVE_BACKEND
#include "native/meta-backend-native.h"
#include "native/meta-input-settings-native.h"
#endif

#include <meta/util.h>

static GQuark quark_tool_settings = 0;

typedef struct _MetaInputSettingsPrivate MetaInputSettingsPrivate;
typedef struct _DeviceMappingInfo DeviceMappingInfo;
typedef struct _ToolSettings ToolSettings;

struct _DeviceMappingInfo
{
  MetaInputSettings *input_settings;
  ClutterInputDevice *device;
  GSettings *settings;
#ifdef HAVE_LIBWACOM
  WacomDevice *wacom_device;
#endif
};

struct _ToolSettings
{
  GSettings *settings;
  ClutterInputDeviceTool *tool;
  GDesktopStylusButtonAction button_action;
  GDesktopStylusButtonAction secondary_button_action;
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

  GHashTable *mappable_devices;

#ifdef HAVE_LIBWACOM
  WacomDeviceDatabase *wacom_db;
#endif
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

  g_clear_object (&priv->mouse_settings);
  g_clear_object (&priv->touchpad_settings);
  g_clear_object (&priv->trackball_settings);
  g_clear_object (&priv->keyboard_settings);
  g_clear_pointer (&priv->mappable_devices, g_hash_table_unref);

  if (priv->monitors_changed_id && priv->monitor_manager)
    {
      g_signal_handler_disconnect (priv->monitor_manager,
                                   priv->monitors_changed_id);
      priv->monitors_changed_id = 0;
    }

  g_clear_object (&priv->monitor_manager);

  if (priv->wacom_db)
    libwacom_database_destroy (priv->wacom_db);

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
  MetaInputSettingsPrivate *priv;

  if (device &&
      clutter_input_device_get_device_type (device) != CLUTTER_TOUCHPAD_DEVICE)
    return;

  priv = meta_input_settings_get_instance_private (input_settings);
  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  edge_scroll_enabled = g_settings_get_boolean (priv->touchpad_settings, "edge-scrolling-enabled");

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

static gboolean
device_is_trackball (ClutterInputDevice *device)
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

  if (device && !device_is_trackball (device))
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

          if (device_is_trackball (device))
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

  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  input_settings_class->set_keyboard_repeat (input_settings,
                                             repeat, delay, interval);
}

static MetaOutput *
meta_input_settings_find_output (MetaInputSettings  *input_settings,
                                 GSettings          *settings,
                                 ClutterInputDevice *device)
{
  MetaInputSettingsPrivate *priv;
  guint n_values, n_outputs, i;
  MetaOutput *outputs;
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

  outputs = meta_monitor_manager_get_outputs (priv->monitor_manager,
                                              &n_outputs);
  for (i = 0; i < n_outputs; i++)
    {
      if (g_strcmp0 (outputs[i].vendor, edid[0]) == 0 &&
          g_strcmp0 (outputs[i].product, edid[1]) == 0 &&
          g_strcmp0 (outputs[i].serial, edid[2]) == 0)
        return &outputs[i];
    }

  return NULL;
}

static void
update_tablet_keep_aspect (MetaInputSettings  *input_settings,
                           GSettings          *settings,
                           ClutterInputDevice *device)
{
  MetaInputSettingsClass *input_settings_class;
  MetaOutput *output = NULL;
  gboolean keep_aspect;

  if (clutter_input_device_get_device_type (device) != CLUTTER_TABLET_DEVICE)
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
      output = meta_input_settings_find_output (input_settings, settings, device);
    }
  else
    {
      keep_aspect = FALSE;
    }

  input_settings_class->set_tablet_keep_aspect (input_settings, device,
                                                output, keep_aspect);
}

static void
update_device_display (MetaInputSettings  *input_settings,
                       GSettings          *settings,
                       ClutterInputDevice *device)
{
  MetaInputSettingsClass *input_settings_class;
  MetaInputSettingsPrivate *priv;
  gfloat matrix[6] = { 1, 0, 0, 0, 1, 0 };
  MetaOutput *output;

  if (clutter_input_device_get_device_type (device) != CLUTTER_TABLET_DEVICE &&
      clutter_input_device_get_device_type (device) != CLUTTER_TOUCHSCREEN_DEVICE)
    return;

  priv = meta_input_settings_get_instance_private (input_settings);
  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);

  /* If mapping is relative, the device can move on all displays */
  if (clutter_input_device_get_device_type (device) != CLUTTER_TABLET_DEVICE ||
      clutter_input_device_get_mapping_mode (device) ==
      CLUTTER_INPUT_DEVICE_MAPPING_ABSOLUTE)
    output = meta_input_settings_find_output (input_settings, settings, device);
  else
    output = NULL;

  if (output)
    meta_monitor_manager_get_monitor_matrix (priv->monitor_manager,
                                             output, matrix);

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

  if (clutter_input_device_get_device_type (device) != CLUTTER_TABLET_DEVICE)
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

  if (clutter_input_device_get_device_type (device) != CLUTTER_TABLET_DEVICE)
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

  if (clutter_input_device_get_device_type (device) != CLUTTER_TABLET_DEVICE)
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
      else if (strcmp (key, "click-method") == 0)
        update_touchpad_click_method (input_settings, NULL);
    }
  else if (settings == priv->trackball_settings)
    {
      if (strcmp (key, "scroll-wheel-emulation-button") == 0)
        update_trackball_scroll_button (input_settings, NULL);
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

static void
tool_settings_changed_cb (GSettings    *settings,
                          const gchar  *key,
                          ToolSettings *tool_settings)
{
  if (strcmp (key, "button-action") == 0)
    tool_settings->button_action = g_settings_get_enum (settings, "button-action");
  else if (strcmp (key, "secondary-button-action") == 0)
    tool_settings->secondary_button_action = g_settings_get_enum (settings, "secondary-button-action");
}

static ToolSettings *
tool_settings_new (ClutterInputDeviceTool *tool,
                   const gchar            *schema_path)
{
  ToolSettings *tool_settings;

  tool_settings = g_new0 (ToolSettings, 1);
  tool_settings->tool = tool;
  tool_settings->settings =
    g_settings_new_with_path ("org.gnome.desktop.peripherals.tablet.stylus",
                              schema_path);

  g_signal_connect (tool_settings->settings, "changed",
                    G_CALLBACK (tool_settings_changed_cb), tool_settings);

  /* Initialize values */
  tool_settings->button_action =
    g_settings_get_enum (tool_settings->settings, "button-action");
  tool_settings->secondary_button_action =
    g_settings_get_enum (tool_settings->settings, "secondary-button-action");

  return tool_settings;
}

static void
tool_settings_free (ToolSettings *tool_settings)
{
  g_object_unref (tool_settings->settings);
  g_free (tool_settings);
}

static ToolSettings *
lookup_tool_settings (ClutterInputDeviceTool *tool,
                      ClutterInputDevice     *device)
{
  ToolSettings *tool_settings;
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

  tool_settings = tool_settings_new (tool, path);
  g_object_set_qdata_full (G_OBJECT (tool), quark_tool_settings, tool_settings,
                           (GDestroyNotify) tool_settings_free);
  g_free (path);

  return tool_settings;
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
  update_mouse_left_handed (input_settings, device);
  update_device_speed (input_settings, device);
  update_device_natural_scroll (input_settings, device);

  update_touchpad_left_handed (input_settings, device);
  update_device_speed (input_settings, device);
  update_device_natural_scroll (input_settings, device);
  update_touchpad_tap_enabled (input_settings, device);
  update_touchpad_send_events (input_settings, device);
  update_touchpad_edge_scroll (input_settings, device);
  update_touchpad_click_method (input_settings, device);

  update_trackball_scroll_button (input_settings, device);
}

static void
meta_input_settings_device_added (ClutterDeviceManager *device_manager,
                                  ClutterInputDevice   *device,
                                  MetaInputSettings    *input_settings)
{
  if (clutter_input_device_get_device_mode (device) == CLUTTER_INPUT_MODE_MASTER)
    return;

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

  priv->mappable_devices =
    g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) device_mapping_info_free);

  priv->monitor_manager = g_object_ref (meta_monitor_manager_get ());
  g_signal_connect (priv->monitor_manager, "monitors-changed",
                    G_CALLBACK (monitors_changed_cb), settings);

  priv->wacom_db = libwacom_database_new ();
  if (!priv->wacom_db)
    {
      g_warning ("Could not create database of Wacom devices, "
                 "expect tablets to misbehave");
    }
}

MetaInputSettings *
meta_input_settings_create (void)
{
#ifdef HAVE_NATIVE_BACKEND
  MetaBackend *backend;

  backend = meta_get_backend ();

  if (META_IS_BACKEND_NATIVE (backend))
    return g_object_new (META_TYPE_INPUT_SETTINGS_NATIVE, NULL);
#endif
  if (!meta_is_wayland_compositor ())
    return g_object_new (META_TYPE_INPUT_SETTINGS_X11, NULL);

  return NULL;
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

GDesktopStylusButtonAction
meta_input_settings_get_stylus_button_action (MetaInputSettings      *input_settings,
                                              ClutterInputDeviceTool *tool,
                                              ClutterInputDevice     *current_tablet,
                                              guint                   button)
{
  ToolSettings *tool_settings;

  g_return_val_if_fail (META_IS_INPUT_SETTINGS (input_settings),
                        G_DESKTOP_STYLUS_BUTTON_ACTION_DEFAULT);
  g_return_val_if_fail (CLUTTER_IS_INPUT_DEVICE_TOOL (tool),
                        G_DESKTOP_STYLUS_BUTTON_ACTION_DEFAULT);

  tool_settings = lookup_tool_settings (tool, current_tablet);

  if (button == 2)
    return tool_settings->button_action;
  else if (button == 3)
    return tool_settings->secondary_button_action;
  else
    return G_DESKTOP_STYLUS_BUTTON_ACTION_DEFAULT;
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
