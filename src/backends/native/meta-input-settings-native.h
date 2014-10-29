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

#ifndef META_INPUT_SETTINGS_NATIVE_H
#define META_INPUT_SETTINGS_NATIVE_H

#include "meta-input-settings-private.h"

#define META_TYPE_INPUT_SETTINGS_NATIVE             (meta_input_settings_native_get_type ())
#define META_INPUT_SETTINGS_NATIVE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), META_TYPE_INPUT_SETTINGS_NATIVE, MetaInputSettingsNative))
#define META_INPUT_SETTINGS_NATIVE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass),  META_TYPE_INPUT_SETTINGS_NATIVE, MetaInputSettingsNativeClass))
#define META_IS_INPUT_SETTINGS_NATIVE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), META_TYPE_INPUT_SETTINGS_NATIVE))
#define META_IS_INPUT_SETTINGS_NATIVE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass),  META_TYPE_INPUT_SETTINGS_NATIVE))
#define META_INPUT_SETTINGS_NATIVE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj),  META_TYPE_INPUT_SETTINGS_NATIVE, MetaInputSettingsNativeClass))

typedef struct _MetaInputSettingsNative MetaInputSettingsNative;
typedef struct _MetaInputSettingsNativeClass MetaInputSettingsNativeClass;

struct _MetaInputSettingsNative
{
  MetaInputSettings parent_instance;
};

struct _MetaInputSettingsNativeClass
{
  MetaInputSettingsClass parent_class;
};

GType meta_input_settings_native_get_type (void) G_GNUC_CONST;

#endif /* META_INPUT_SETTINGS_NATIVE_H */
