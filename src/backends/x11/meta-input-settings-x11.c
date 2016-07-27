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

#include "meta-backend-x11.h"
#include "meta-input-settings-x11.h"

#include <string.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <X11/extensions/XInput2.h>
#include <X11/XKBlib.h>

#include <meta/errors.h>

G_DEFINE_TYPE (MetaInputSettingsX11, meta_input_settings_x11, META_TYPE_INPUT_SETTINGS)

enum {
  SCROLL_METHOD_FIELD_2FG,
  SCROLL_METHOD_FIELD_EDGE,
  SCROLL_METHOD_FIELD_BUTTON,
  SCROLL_METHOD_NUM_FIELDS
};

static void *
get_property (ClutterInputDevice *device,
              const gchar        *property,
              Atom                type,
              int                 format,
              gulong              nitems)
{
  MetaBackend *backend = meta_get_backend ();
  Display *xdisplay = meta_backend_x11_get_xdisplay (META_BACKEND_X11 (backend));
  gulong nitems_ret, bytes_after_ret;
  int rc, device_id, format_ret;
  Atom property_atom, type_ret;
  guchar *data_ret = NULL;

  property_atom = XInternAtom (xdisplay, property, True);
  if (!property_atom)
    return NULL;

  device_id = clutter_input_device_get_device_id (device);

  rc = XIGetProperty (xdisplay, device_id, property_atom,
                      0, 10, False, type, &type_ret, &format_ret,
                      &nitems_ret, &bytes_after_ret, &data_ret);
  if (rc == Success && type_ret == type && format_ret == format && nitems_ret >= nitems)
    {
      if (nitems_ret > nitems)
        g_warning ("Property '%s' for device '%s' returned %lu items, expected %lu",
                   property, clutter_input_device_get_device_name (device), nitems_ret, nitems);
      return data_ret;
    }

  meta_XFree (data_ret);
  return NULL;
}

static void
change_property (ClutterInputDevice *device,
                 const gchar        *property,
                 Atom                type,
                 int                 format,
                 void               *data,
                 gulong              nitems)
{
  MetaBackend *backend = meta_get_backend ();
  Display *xdisplay = meta_backend_x11_get_xdisplay (META_BACKEND_X11 (backend));
  int device_id;
  Atom property_atom;
  guchar *data_ret;

  property_atom = XInternAtom (xdisplay, property, True);
  if (!property_atom)
    return;

  device_id = clutter_input_device_get_device_id (device);

  data_ret = get_property (device, property, type, format, nitems);
  if (!data_ret)
    return;

  XIChangeProperty (xdisplay, device_id, property_atom, type,
                    format, XIPropModeReplace, data, nitems);
  meta_XFree (data_ret);
}

static void
meta_input_settings_x11_set_send_events (MetaInputSettings        *settings,
                                         ClutterInputDevice       *device,
                                         GDesktopDeviceSendEvents  mode)
{
  guchar values[2] = { 0 }; /* disabled, disabled-on-external-mouse */
  guchar *available;

  available = get_property (device, "libinput Send Events Modes Available",
                            XA_INTEGER, 8, 2);
  if (!available)
    return;

  switch (mode)
    {
    case G_DESKTOP_DEVICE_SEND_EVENTS_DISABLED:
      values[0] = 1;
      break;
    case G_DESKTOP_DEVICE_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE:
      values[1] = 1;
      break;
    default:
      break;
    }

  if ((values[0] && !available[0]) || (values[1] && !available[1]))
    g_warning ("Device '%s' does not support sendevents mode %d\n",
               clutter_input_device_get_device_name (device), mode);
  else
    change_property (device, "libinput Send Events Mode Enabled",
                     XA_INTEGER, 8, &values, 2);

  meta_XFree (available);
}

static void
meta_input_settings_x11_set_matrix (MetaInputSettings  *settings,
                                    ClutterInputDevice *device,
                                    gfloat              matrix[6])
{
  MetaBackend *backend = meta_get_backend ();
  Display *xdisplay = meta_backend_x11_get_xdisplay (META_BACKEND_X11 (backend));
  gfloat full_matrix[9] = { matrix[0], matrix[1], matrix[2],
                            matrix[3], matrix[4], matrix[5],
                            0, 0, 1 };

  change_property (device, "Coordinate Transformation Matrix",
                   XInternAtom (xdisplay, "FLOAT", False),
                   32, &full_matrix, 9);
}

static void
meta_input_settings_x11_set_speed (MetaInputSettings  *settings,
                                   ClutterInputDevice *device,
                                   gdouble             speed)
{
  MetaBackend *backend = meta_get_backend ();
  Display *xdisplay = meta_backend_x11_get_xdisplay (META_BACKEND_X11 (backend));
  gfloat value = speed;

  change_property (device, "libinput Accel Speed",
                   XInternAtom (xdisplay, "FLOAT", False),
                   32, &value, 1);
}

static void
meta_input_settings_x11_set_left_handed (MetaInputSettings  *settings,
                                         ClutterInputDevice *device,
                                         gboolean            enabled)
{
  guchar value = (enabled) ? 1 : 0;

  change_property (device, "libinput Left Handed Enabled",
                   XA_INTEGER, 8, &value, 1);
}

static void
meta_input_settings_x11_set_tap_enabled (MetaInputSettings  *settings,
                                         ClutterInputDevice *device,
                                         gboolean            enabled)
{
  guchar value = (enabled) ? 1 : 0;

  change_property (device, "libinput Tapping Enabled",
                   XA_INTEGER, 8, &value, 1);
}

static void
meta_input_settings_x11_set_invert_scroll (MetaInputSettings  *settings,
                                           ClutterInputDevice *device,
                                           gboolean            inverted)
{
  guchar value = (inverted) ? 1 : 0;

  change_property (device, "libinput Natural Scrolling Enabled",
                   XA_INTEGER, 8, &value, 1);
}

static void
meta_input_settings_x11_set_edge_scroll (MetaInputSettings            *settings,
                                         ClutterInputDevice           *device,
                                         gboolean                      edge_scroll_enabled)
{
  guchar values[SCROLL_METHOD_NUM_FIELDS] = { 0 }; /* 2fg, edge, button. The last value is unused */
  guchar *defaults;
  guchar *available;

  available = get_property (device, "libinput Scroll Methods Available",
                            XA_INTEGER, 8, SCROLL_METHOD_NUM_FIELDS);
  defaults = get_property (device, "libinput Scroll Method Enabled",
                           XA_INTEGER, 8, SCROLL_METHOD_NUM_FIELDS);
  if (!available || !defaults)
    goto out;

  memcpy (values, defaults, SCROLL_METHOD_NUM_FIELDS);

  /* Don't set edge scrolling if two-finger scrolling is enabled and available */
  if (available[SCROLL_METHOD_FIELD_EDGE] &&
      !(available[SCROLL_METHOD_FIELD_2FG] && values[SCROLL_METHOD_FIELD_2FG]))
    {
      values[1] = !!edge_scroll_enabled;
      change_property (device, "libinput Scroll Method Enabled",
                       XA_INTEGER, 8, &values, SCROLL_METHOD_NUM_FIELDS);
    }

out:
  if (available)
    meta_XFree (available);
  if (defaults)
    meta_XFree (defaults);
}

static void
meta_input_settings_x11_set_two_finger_scroll (MetaInputSettings            *settings,
                                               ClutterInputDevice           *device,
                                               gboolean                      two_finger_scroll_enabled)
{
  guchar values[SCROLL_METHOD_NUM_FIELDS] = { 0 }; /* 2fg, edge, button. The last value is unused */
  guchar *defaults;
  guchar *available;
  gboolean changed;

  available = get_property (device, "libinput Scroll Methods Available",
                            XA_INTEGER, 8, SCROLL_METHOD_NUM_FIELDS);
  defaults = get_property (device, "libinput Scroll Method Enabled",
                           XA_INTEGER, 8, SCROLL_METHOD_NUM_FIELDS);
  if (!available || !defaults)
    goto out;

  memcpy (values, defaults, SCROLL_METHOD_NUM_FIELDS);
  changed = FALSE;

  if (available[SCROLL_METHOD_FIELD_2FG])
    {
      values[SCROLL_METHOD_FIELD_2FG] = !!two_finger_scroll_enabled;
      changed = TRUE;
    }

  /* Disable edge scrolling when two-finger scrolling is enabled */
  if (values[SCROLL_METHOD_FIELD_2FG] && values[SCROLL_METHOD_FIELD_EDGE])
    {
      values[SCROLL_METHOD_FIELD_EDGE] = 0;
      changed = TRUE;
    }

  if (changed)
    {
      change_property (device, "libinput Scroll Method Enabled",
                       XA_INTEGER, 8, &values, SCROLL_METHOD_NUM_FIELDS);
    }

out:
  if (available)
    meta_XFree (available);
  if (defaults)
    meta_XFree (defaults);
}

static void
meta_input_settings_x11_set_scroll_button (MetaInputSettings  *settings,
                                           ClutterInputDevice *device,
                                           guint               button)
{
  change_property (device, "libinput Button Scrolling Button",
                   XA_INTEGER, 32, &button, 1);
}

static void
meta_input_settings_x11_set_click_method (MetaInputSettings           *settings,
                                          ClutterInputDevice          *device,
                                          GDesktopTouchpadClickMethod  mode)
{
  guchar values[2] = { 0 }; /* buttonareas, clickfinger */
  guchar *defaults, *available;

  available = get_property (device, "libinput Click Methods Available",
                            XA_INTEGER, 8, 2);
  if (!available)
    return;

  switch (mode)
    {
    case G_DESKTOP_TOUCHPAD_CLICK_METHOD_DEFAULT:
      defaults = get_property (device, "libinput Click Method Enabled Default",
                               XA_INTEGER, 8, 2);
      if (!defaults)
        break;
      memcpy (values, defaults, 2);
      meta_XFree (defaults);
      break;
    case G_DESKTOP_TOUCHPAD_CLICK_METHOD_NONE:
      break;
    case G_DESKTOP_TOUCHPAD_CLICK_METHOD_AREAS:
      values[0] = 1;
      break;
    case G_DESKTOP_TOUCHPAD_CLICK_METHOD_FINGERS:
      values[1] = 1;
      break;
    default:
      g_assert_not_reached ();
      return;
  }

  if ((values[0] && !available[0]) || (values[1] && !available[1]))
    g_warning ("Device '%s' does not support click method %d\n",
               clutter_input_device_get_device_name (device), mode);
  else
    change_property (device, "libinput Click Method Enabled",
                     XA_INTEGER, 8, &values, 2);

  meta_XFree(available);
}

static void
meta_input_settings_x11_set_keyboard_repeat (MetaInputSettings *settings,
                                             gboolean           enabled,
                                             guint              delay,
                                             guint              interval)
{
  MetaBackend *backend = meta_get_backend ();
  Display *xdisplay = meta_backend_x11_get_xdisplay (META_BACKEND_X11 (backend));

  if (enabled)
    {
      XAutoRepeatOn (xdisplay);
      XkbSetAutoRepeatRate (xdisplay, XkbUseCoreKbd, delay, interval);
    }
  else
    {
      XAutoRepeatOff (xdisplay);
    }
}

static void
meta_input_settings_x11_set_tablet_mapping (MetaInputSettings     *settings,
                                            ClutterInputDevice    *device,
                                            GDesktopTabletMapping  mapping)
{
}

static void
meta_input_settings_x11_set_tablet_keep_aspect (MetaInputSettings  *settings,
                                                ClutterInputDevice *device,
                                                MetaOutput         *output,
                                                gboolean            keep_aspect)
{
}

static void
meta_input_settings_x11_set_tablet_area (MetaInputSettings  *settings,
                                         ClutterInputDevice *device,
                                         gdouble             padding_left,
                                         gdouble             padding_right,
                                         gdouble             padding_top,
                                         gdouble             padding_bottom)
{
}

static void
meta_input_settings_x11_class_init (MetaInputSettingsX11Class *klass)
{
  MetaInputSettingsClass *input_settings_class = META_INPUT_SETTINGS_CLASS (klass);

  input_settings_class->set_send_events = meta_input_settings_x11_set_send_events;
  input_settings_class->set_matrix = meta_input_settings_x11_set_matrix;
  input_settings_class->set_speed = meta_input_settings_x11_set_speed;
  input_settings_class->set_left_handed = meta_input_settings_x11_set_left_handed;
  input_settings_class->set_tap_enabled = meta_input_settings_x11_set_tap_enabled;
  input_settings_class->set_invert_scroll = meta_input_settings_x11_set_invert_scroll;
  input_settings_class->set_edge_scroll = meta_input_settings_x11_set_edge_scroll;
  input_settings_class->set_two_finger_scroll = meta_input_settings_x11_set_two_finger_scroll;
  input_settings_class->set_scroll_button = meta_input_settings_x11_set_scroll_button;
  input_settings_class->set_click_method = meta_input_settings_x11_set_click_method;
  input_settings_class->set_keyboard_repeat = meta_input_settings_x11_set_keyboard_repeat;

  input_settings_class->set_tablet_mapping = meta_input_settings_x11_set_tablet_mapping;
  input_settings_class->set_tablet_keep_aspect = meta_input_settings_x11_set_tablet_keep_aspect;
  input_settings_class->set_tablet_area = meta_input_settings_x11_set_tablet_area;
}

static void
meta_input_settings_x11_init (MetaInputSettingsX11 *settings)
{
}
