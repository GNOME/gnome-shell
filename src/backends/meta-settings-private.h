/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2017 Red Hat
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
 */

#ifndef META_SETTINGS_PRIVATE_H
#define META_SETTINGS_PRIVATE_H

#include <glib-object.h>

#include "meta/meta-settings.h"
#include "meta/types.h"

typedef enum _MetaExperimentalFeature
{
  META_EXPERIMENTAL_FEATURE_NONE = 0,
  META_EXPERIMENTAL_FEATURE_SCALE_MONITOR_FRAMEBUFFER = (1 << 0),
  META_EXPERIMENTAL_FEATURE_MONITOR_CONFIG_MANAGER = (1 << 1)
} MetaExperimentalFeature;

#define META_TYPE_SETTINGS (meta_settings_get_type ())
G_DECLARE_FINAL_TYPE (MetaSettings, meta_settings,
                      META, SETTINGS, GObject)

MetaSettings * meta_settings_new (MetaBackend *backend);

void meta_settings_post_init (MetaSettings *settings);

void meta_settings_update_ui_scaling_factor (MetaSettings *settings);

gboolean meta_settings_get_global_scaling_factor (MetaSettings *settings,
                                                  int          *scaing_factor);

gboolean meta_settings_is_experimental_feature_enabled (MetaSettings           *settings,
                                                        MetaExperimentalFeature feature);

MetaExperimentalFeature meta_settings_get_experimental_features (MetaSettings *settings);

void meta_settings_override_experimental_features (MetaSettings *settings);

void meta_settings_enable_experimental_feature (MetaSettings           *settings,
                                                MetaExperimentalFeature feature);

#endif /* META_SETTINGS_PRIVATE_H */
