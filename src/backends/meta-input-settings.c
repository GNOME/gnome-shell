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

#include <meta/util.h>

typedef struct _MetaInputSettingsPrivate MetaInputSettingsPrivate;

struct _MetaInputSettingsPrivate
{
  ClutterDeviceManager *device_manager;
  GSettings *mouse_settings;
  GSettings *touchpad_settings;
  GSettings *trackball_settings;
  GSettings *keyboard_settings;
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
      g_assert (clutter_input_device_get_device_type (device) == CLUTTER_TOUCHPAD_DEVICE);
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

  priv = meta_input_settings_get_instance_private (input_settings);
  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  enabled = g_settings_get_boolean (priv->mouse_settings, "left-handed");

  if (device)
    {
      g_assert (clutter_input_device_get_device_type (device) == CLUTTER_POINTER_DEVICE);
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
update_device_speed (MetaInputSettings      *input_settings,
                     GSettings              *settings,
                     ClutterInputDevice     *device,
                     ClutterInputDeviceType  type)
{
  MetaInputSettingsClass *input_settings_class;
  gdouble speed;

  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  speed = g_settings_get_double (settings, "speed");

  if (device)
    settings_device_set_double_setting (input_settings, device,
                                        input_settings_class->set_speed,
                                        speed);
  else
    settings_set_double_setting (input_settings, type,
                                 input_settings_class->set_speed,
                                 speed);
}

static void
update_device_natural_scroll (MetaInputSettings      *input_settings,
                              GSettings              *settings,
                              ClutterInputDevice     *device,
                              ClutterInputDeviceType  type)
{
  MetaInputSettingsClass *input_settings_class;
  gboolean enabled;

  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  enabled = g_settings_get_boolean (settings, "natural-scroll");

  if (device)
    {
      settings_device_set_bool_setting (input_settings, device,
                                        input_settings_class->set_invert_scroll,
                                        enabled);
    }
  else
    {
      settings_set_bool_setting (input_settings, type,
                                 input_settings_class->set_invert_scroll,
                                 enabled);
    }
}

static void
update_touchpad_tap_enabled (MetaInputSettings  *input_settings,
                             ClutterInputDevice *device)
{
  MetaInputSettingsClass *input_settings_class;
  MetaInputSettingsPrivate *priv;
  gboolean enabled;

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
update_touchpad_scroll_method (MetaInputSettings *input_settings,
                               ClutterInputDevice *device)
{
  MetaInputSettingsClass *input_settings_class;
  GDesktopTouchpadScrollMethod method;
  MetaInputSettingsPrivate *priv;

  priv = meta_input_settings_get_instance_private (input_settings);
  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  method = g_settings_get_enum (priv->touchpad_settings, "scroll-method");

  if (device)
    {
      settings_device_set_uint_setting (input_settings, device,
                                        input_settings_class->set_scroll_method,
                                        method);
    }
  else
    {
      settings_set_uint_setting (input_settings, CLUTTER_TOUCHPAD_DEVICE,
                                 (ConfigUintFunc) input_settings_class->set_scroll_method,
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

  priv = meta_input_settings_get_instance_private (input_settings);
  input_settings_class = META_INPUT_SETTINGS_GET_CLASS (input_settings);
  button = g_settings_get_uint (priv->trackball_settings, "scroll-wheel-emulation-button");

  if (device && device_is_trackball (device))
    {
      input_settings_class->set_scroll_button (input_settings, device, button);
    }
  else if (!device)
    {
      MetaInputSettingsPrivate *priv;
      const GSList *devices;

      priv = meta_input_settings_get_instance_private (input_settings);
      devices = clutter_device_manager_peek_devices (priv->device_manager);

      while (devices)
        {
          ClutterInputDevice *device = devices->data;

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
        update_device_speed (input_settings, settings, NULL,
                             CLUTTER_POINTER_DEVICE);
      else if (strcmp (key, "natural-scroll") == 0)
        update_device_natural_scroll (input_settings, settings,
                                      NULL, CLUTTER_POINTER_DEVICE);
    }
  else if (settings == priv->touchpad_settings)
    {
      if (strcmp (key, "left-handed") == 0)
        update_touchpad_left_handed (input_settings, NULL);
      else if (strcmp (key, "speed") == 0)
        update_device_speed (input_settings, settings, NULL,
                             CLUTTER_TOUCHPAD_DEVICE);
      else if (strcmp (key, "natural-scroll") == 0)
        update_device_natural_scroll (input_settings, settings,
                                      NULL, CLUTTER_TOUCHPAD_DEVICE);
      else if (strcmp (key, "tap-to-click") == 0)
        update_touchpad_tap_enabled (input_settings, NULL);
      else if (strcmp (key, "send-events") == 0)
        update_touchpad_send_events (input_settings, NULL);
      else if (strcmp (key, "scroll-method") == 0)
        update_touchpad_scroll_method (input_settings, NULL);
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
meta_input_settings_device_added (ClutterDeviceManager *device_manager,
                                  ClutterInputDevice   *device,
                                  MetaInputSettings    *input_settings)
{
  ClutterInputDeviceType type;
  MetaInputSettingsPrivate *priv;

  if (clutter_input_device_get_device_mode (device) == CLUTTER_INPUT_MODE_MASTER)
    return;

  priv = meta_input_settings_get_instance_private (input_settings);
  type = clutter_input_device_get_device_type (device);

  if (type == CLUTTER_POINTER_DEVICE)
    {
      update_mouse_left_handed (input_settings, device);
      update_device_speed (input_settings, priv->mouse_settings, device, type);

      if (device_is_trackball (device))
        update_trackball_scroll_button (input_settings, device);
    }
  else if (type == CLUTTER_TOUCHPAD_DEVICE)
    {
      update_touchpad_left_handed (input_settings, device);
      update_touchpad_tap_enabled (input_settings, device);
      update_touchpad_scroll_method (input_settings, device);
      update_touchpad_send_events (input_settings, device);

      update_device_speed (input_settings, priv->touchpad_settings,
                           device, type);
      update_device_natural_scroll (input_settings, priv->touchpad_settings,
                                    device, type);
    }
}

static void
meta_input_settings_device_removed (ClutterDeviceManager *device_manager,
                                    ClutterInputDevice   *device,
                                    MetaInputSettings    *input_settings)
{
}

static void
meta_input_settings_constructed (GObject *object)
{
  MetaInputSettings *input_settings = META_INPUT_SETTINGS (object);
  MetaInputSettingsPrivate *priv;

  priv = meta_input_settings_get_instance_private (input_settings);

  update_mouse_left_handed (input_settings, NULL);

  update_touchpad_left_handed (input_settings, NULL);
  update_touchpad_tap_enabled (input_settings, NULL);
  update_touchpad_send_events (input_settings, NULL);

  update_device_natural_scroll (input_settings, priv->touchpad_settings,
                                NULL, CLUTTER_TOUCHPAD_DEVICE);
  update_device_speed (input_settings, priv->touchpad_settings, NULL,
                       CLUTTER_TOUCHPAD_DEVICE);
  update_device_speed (input_settings, priv->mouse_settings, NULL,
                       CLUTTER_POINTER_DEVICE);

  update_keyboard_repeat (input_settings);
}

static void
meta_input_settings_class_init (MetaInputSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = meta_input_settings_dispose;
  object_class->constructed = meta_input_settings_constructed;
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
}

MetaInputSettings *
meta_input_settings_create (void)
{
  return NULL;
}
