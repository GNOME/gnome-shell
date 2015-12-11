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

#include <clutter/clutter.h>

#define META_TYPE_INPUT_SETTINGS             (meta_input_settings_get_type ())
#define META_INPUT_SETTINGS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_INPUT_SETTINGS, MetaInputSettings))
#define META_INPUT_SETTINGS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_INPUT_SETTINGS, MetaInputSettingsClass))
#define META_IS_INPUT_SETTINGS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_INPUT_SETTINGS))
#define META_IS_INPUT_SETTINGS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_INPUT_SETTINGS))
#define META_INPUT_SETTINGS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_INPUT_SETTINGS, MetaInputSettingsClass))

typedef struct _MetaInputSettings MetaInputSettings;
typedef struct _MetaInputSettingsClass MetaInputSettingsClass;

struct _MetaInputSettings
{
  GObject parent_instance;
};

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
  void (* set_invert_scroll) (MetaInputSettings  *settings,
                              ClutterInputDevice *device,
                              gboolean            inverted);
  void (* set_edge_scroll)   (MetaInputSettings  *settings,
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
};

GType meta_input_settings_get_type (void) G_GNUC_CONST;

MetaInputSettings * meta_input_settings_create (void);

#endif /* META_INPUT_SETTINGS_PRIVATE_H */
