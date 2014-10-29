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

#ifndef META_INPUT_SETTINGS_X11_H
#define META_INPUT_SETTINGS_X11_H

#include "meta-input-settings-private.h"

#define META_TYPE_INPUT_SETTINGS_X11             (meta_input_settings_x11_get_type ())
#define META_INPUT_SETTINGS_X11(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_INPUT_SETTINGS_X11, MetaInputSettingsX11))
#define META_INPUT_SETTINGS_X11_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_INPUT_SETTINGS_X11, MetaInputSettingsX11Class))
#define META_IS_INPUT_SETTINGS_X11(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_INPUT_SETTINGS_X11))
#define META_IS_INPUT_SETTINGS_X11_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_INPUT_SETTINGS_X11))
#define META_INPUT_SETTINGS_X11_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_INPUT_SETTINGS_X11, MetaInputSettingsX11Class))

typedef struct _MetaInputSettingsX11 MetaInputSettingsX11;
typedef struct _MetaInputSettingsX11Class MetaInputSettingsX11Class;

struct _MetaInputSettingsX11
{
  MetaInputSettings parent_instance;
};

struct _MetaInputSettingsX11Class
{
  MetaInputSettingsClass parent_class;
};

GType meta_input_settings_x11_get_type (void) G_GNUC_CONST;

#endif /* META_INPUT_SETTINGS_X11_H */
