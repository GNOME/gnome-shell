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

#include "config.h"

#include "backends/meta-settings-private.h"

#include <gio/gio.h>

#include "backends/meta-backend-private.h"
#include "backends/meta-logical-monitor.h"
#include "backends/meta-monitor-manager-private.h"
#include "ui/theme-private.h"

#ifndef XWAYLAND_GRAB_DEFAULT_ACCESS_RULES
# warning "XWAYLAND_GRAB_DEFAULT_ACCESS_RULES is not set"
# define  XWAYLAND_GRAB_DEFAULT_ACCESS_RULES ""
#endif

enum
{
  UI_SCALING_FACTOR_CHANGED,
  GLOBAL_SCALING_FACTOR_CHANGED,
  FONT_DPI_CHANGED,
  EXPERIMENTAL_FEATURES_CHANGED,

  N_SIGNALS
};

static guint signals[N_SIGNALS];

struct _MetaSettings
{
  GObject parent;

  MetaBackend *backend;

  GSettings *interface_settings;
  GSettings *mutter_settings;
  GSettings *wayland_settings;

  int ui_scaling_factor;
  int global_scaling_factor;

  int font_dpi;

  MetaExperimentalFeature experimental_features;
  gboolean experimental_features_overridden;

  gboolean xwayland_allow_grabs;
  GPtrArray *xwayland_grab_whitelist_patterns;
  GPtrArray *xwayland_grab_blacklist_patterns;
};

G_DEFINE_TYPE (MetaSettings, meta_settings, G_TYPE_OBJECT)

static int
calculate_ui_scaling_factor (MetaSettings *settings)
{
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (settings->backend);
  MetaLogicalMonitor *primary_logical_monitor;

  primary_logical_monitor =
    meta_monitor_manager_get_primary_logical_monitor (monitor_manager);
  if (!primary_logical_monitor)
    return 1;

  return (int) meta_logical_monitor_get_scale (primary_logical_monitor);
}

static gboolean
update_ui_scaling_factor (MetaSettings *settings)
{
  int ui_scaling_factor;

  if (meta_is_stage_views_scaled ())
    ui_scaling_factor = 1;
  else
    ui_scaling_factor = calculate_ui_scaling_factor (settings);

  if (settings->ui_scaling_factor != ui_scaling_factor)
    {
      settings->ui_scaling_factor = ui_scaling_factor;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

void
meta_settings_update_ui_scaling_factor (MetaSettings *settings)
{
  if (update_ui_scaling_factor (settings))
    g_signal_emit (settings, signals[UI_SCALING_FACTOR_CHANGED], 0);
}

int
meta_settings_get_ui_scaling_factor (MetaSettings *settings)
{
  g_assert (settings->ui_scaling_factor != 0);

  return settings->ui_scaling_factor;
}

static gboolean
update_global_scaling_factor (MetaSettings *settings)
{
  int global_scaling_factor;

  global_scaling_factor =
    (int) g_settings_get_uint (settings->interface_settings,
                               "scaling-factor");

  if (settings->global_scaling_factor != global_scaling_factor)
    {
      settings->global_scaling_factor = global_scaling_factor;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

gboolean
meta_settings_get_global_scaling_factor (MetaSettings *settings,
                                         int          *out_scaling_factor)
{
  if (settings->global_scaling_factor == 0)
    return FALSE;

  *out_scaling_factor = settings->global_scaling_factor;
  return TRUE;
}

static gboolean
update_font_dpi (MetaSettings *settings)
{
  double text_scaling_factor;
  /* Number of logical pixels on an inch when unscaled */
  const double dots_per_inch = 96;
  /* Being based on Xft, API users expect the DPI to be 1/1024th of an inch. */
  const double xft_factor = 1024;
  int font_dpi;

  text_scaling_factor = g_settings_get_double (settings->interface_settings,
                                               "text-scaling-factor");
  font_dpi = (int) (text_scaling_factor *
                    dots_per_inch *
                    xft_factor *
                    settings->ui_scaling_factor);

  if (font_dpi != settings->font_dpi)
    {
      settings->font_dpi = font_dpi;

      g_object_set (clutter_settings_get_default (),
                    "font-dpi", font_dpi,
                    NULL);

      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static void
meta_settings_update_font_dpi (MetaSettings *settings)
{
  if (update_font_dpi (settings))
    g_signal_emit (settings, signals[FONT_DPI_CHANGED], 0);
}

int
meta_settings_get_font_dpi (MetaSettings *settings)
{
  g_assert (settings->font_dpi != 0);

  return settings->font_dpi;
}

static void
interface_settings_changed (GSettings    *interface_settings,
                            const char   *key,
                            MetaSettings *settings)
{
  if (g_str_equal (key, "scaling-factor"))
    {
      if (update_global_scaling_factor (settings))
        g_signal_emit (settings, signals[GLOBAL_SCALING_FACTOR_CHANGED], 0);
    }
  else if (g_str_equal (key, "text-scaling-factor"))
    {
      meta_settings_update_font_dpi (settings);
    }
}

gboolean
meta_settings_is_experimental_feature_enabled (MetaSettings           *settings,
                                               MetaExperimentalFeature feature)
{
  return !!(settings->experimental_features & feature);
}

void
meta_settings_override_experimental_features (MetaSettings *settings)
{
  settings->experimental_features = META_EXPERIMENTAL_FEATURE_NONE;
  settings->experimental_features_overridden = TRUE;
}

void
meta_settings_enable_experimental_feature (MetaSettings           *settings,
                                           MetaExperimentalFeature feature)
{
  g_assert (settings->experimental_features_overridden);

  settings->experimental_features |= feature;
}

static gboolean
experimental_features_handler (GVariant *features_variant,
                               gpointer *result,
                               gpointer  data)
{
  MetaSettings *settings = data;
  GVariantIter features_iter;
  char *feature;
  MetaExperimentalFeature features = META_EXPERIMENTAL_FEATURE_NONE;

  if (settings->experimental_features_overridden)
    {
      *result = GINT_TO_POINTER (FALSE);
      return TRUE;
    }

  g_variant_iter_init (&features_iter, features_variant);
  while (g_variant_iter_loop (&features_iter, "s", &feature))
    {
      /* So far no experimental features defined. */
      if (g_str_equal (feature, "scale-monitor-framebuffer"))
        features |= META_EXPERIMENTAL_FEATURE_SCALE_MONITOR_FRAMEBUFFER;
      else if (g_str_equal (feature, "screen-cast"))
        features |= META_EXPERIMENTAL_FEATURE_SCREEN_CAST;
      else if (g_str_equal (feature, "remote-desktop"))
        features |= META_EXPERIMENTAL_FEATURE_REMOTE_DESKTOP;
      else
        g_info ("Unknown experimental feature '%s'\n", feature);
    }

  if (features != settings->experimental_features)
    {
      settings->experimental_features = features;
      *result = GINT_TO_POINTER (TRUE);
    }
  else
    {
      *result = GINT_TO_POINTER (FALSE);
    }

  return TRUE;
}

static gboolean
update_experimental_features (MetaSettings *settings)
{
  return GPOINTER_TO_INT (g_settings_get_mapped (settings->mutter_settings,
                                                 "experimental-features",
                                                 experimental_features_handler,
                                                 settings));
}

static void
mutter_settings_changed (GSettings    *mutter_settings,
                         gchar        *key,
                         MetaSettings *settings)
{
  MetaExperimentalFeature old_experimental_features;

  if (!g_str_equal (key, "experimental-features"))
    return;

  old_experimental_features = settings->experimental_features;
  if (update_experimental_features (settings))
    g_signal_emit (settings, signals[EXPERIMENTAL_FEATURES_CHANGED], 0,
                   (unsigned int) old_experimental_features);
}

static void
xwayland_grab_list_add_item (MetaSettings *settings,
                             char         *item)
{
  /* If first character is '!', it's a blacklisted item */
  if (item[0] != '!')
    g_ptr_array_add (settings->xwayland_grab_whitelist_patterns,
                     g_pattern_spec_new (item));
  else if (item[1] != 0)
    g_ptr_array_add (settings->xwayland_grab_blacklist_patterns,
                     g_pattern_spec_new (&item[1]));
}

static gboolean
xwayland_grab_access_rules_handler (GVariant *variant,
                                    gpointer *result,
                                    gpointer  data)
{
  MetaSettings *settings = data;
  GVariantIter iter;
  char *item;

  /* Create a GPatternSpec for each element */
  g_variant_iter_init (&iter, variant);
  while (g_variant_iter_loop (&iter, "s", &item))
    xwayland_grab_list_add_item (settings, item);

  *result = GINT_TO_POINTER (TRUE);

  return TRUE;
}

static void
update_xwayland_grab_access_rules (MetaSettings *settings)
{
  gchar **system_defaults;
  int i;

  /* Free previous patterns and create new arrays */
  g_clear_pointer (&settings->xwayland_grab_whitelist_patterns,
                   g_ptr_array_unref);
  settings->xwayland_grab_whitelist_patterns =
    g_ptr_array_new_with_free_func ((GDestroyNotify) g_pattern_spec_free);

  g_clear_pointer (&settings->xwayland_grab_blacklist_patterns,
                   g_ptr_array_unref);
  settings->xwayland_grab_blacklist_patterns =
    g_ptr_array_new_with_free_func ((GDestroyNotify) g_pattern_spec_free);

  /* Add system defaults values */
  system_defaults = g_strsplit (XWAYLAND_GRAB_DEFAULT_ACCESS_RULES, ",", -1);
  for (i = 0; system_defaults[i]; i++)
    xwayland_grab_list_add_item (settings, system_defaults[i]);
  g_strfreev (system_defaults);

  /* Then add gsettings values */
  g_settings_get_mapped (settings->wayland_settings,
                         "xwayland-grab-access-rules",
                         xwayland_grab_access_rules_handler,
                         settings);
}

static void
update_xwayland_allow_grabs (MetaSettings *settings)
{
  settings->xwayland_allow_grabs =
    g_settings_get_boolean (settings->wayland_settings,
                            "xwayland-allow-grabs");
}

static void
wayland_settings_changed (GSettings    *wayland_settings,
                          gchar        *key,
                          MetaSettings *settings)
{

  if (g_str_equal (key, "xwayland-allow-grabs"))
    {
      update_xwayland_allow_grabs (settings);
    }
  else if (g_str_equal (key, "xwayland-grab-access-rules"))
    {
      update_xwayland_grab_access_rules (settings);
    }
}

void
meta_settings_get_xwayland_grab_patterns (MetaSettings  *settings,
                                          GPtrArray    **whitelist_patterns,
                                          GPtrArray    **blacklist_patterns)
{
  *whitelist_patterns = settings->xwayland_grab_whitelist_patterns;
  *blacklist_patterns = settings->xwayland_grab_blacklist_patterns;
}

gboolean
 meta_settings_are_xwayland_grabs_allowed (MetaSettings *settings)
{
  return (settings->xwayland_allow_grabs);
}

MetaSettings *
meta_settings_new (MetaBackend *backend)
{
  MetaSettings *settings;

  settings = g_object_new (META_TYPE_SETTINGS, NULL);
  settings->backend = backend;

  return settings;
}

static void
meta_settings_dispose (GObject *object)
{
  MetaSettings *settings = META_SETTINGS (object);

  g_clear_object (&settings->mutter_settings);
  g_clear_object (&settings->interface_settings);
  g_clear_object (&settings->wayland_settings);
  g_clear_pointer (&settings->xwayland_grab_whitelist_patterns,
                   g_ptr_array_unref);
  g_clear_pointer (&settings->xwayland_grab_blacklist_patterns,
                   g_ptr_array_unref);

  G_OBJECT_CLASS (meta_settings_parent_class)->dispose (object);
}

static void
meta_settings_init (MetaSettings *settings)
{
  settings->interface_settings = g_settings_new ("org.gnome.desktop.interface");
  g_signal_connect (settings->interface_settings, "changed",
                    G_CALLBACK (interface_settings_changed),
                    settings);
  settings->mutter_settings = g_settings_new ("org.gnome.mutter");
  g_signal_connect (settings->mutter_settings, "changed",
                    G_CALLBACK (mutter_settings_changed),
                    settings);
  settings->wayland_settings = g_settings_new ("org.gnome.mutter.wayland");
  g_signal_connect (settings->wayland_settings, "changed",
                    G_CALLBACK (wayland_settings_changed),
                    settings);

  /* Chain up inter-dependent settings. */
  g_signal_connect (settings, "global-scaling-factor-changed",
                    G_CALLBACK (meta_settings_update_ui_scaling_factor), NULL);
  g_signal_connect (settings, "ui-scaling-factor-changed",
                    G_CALLBACK (meta_settings_update_font_dpi), NULL);

  update_global_scaling_factor (settings);
  update_experimental_features (settings);
  update_xwayland_grab_access_rules (settings);
  update_xwayland_allow_grabs (settings);
}

static void
on_monitors_changed (MetaMonitorManager *monitor_manager,
                     MetaSettings       *settings)
{
  meta_settings_update_ui_scaling_factor (settings);
}

void
meta_settings_post_init (MetaSettings *settings)
{
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (settings->backend);

  update_ui_scaling_factor (settings);
  update_font_dpi (settings);

  g_signal_connect_object (monitor_manager, "monitors-changed-internal",
                           G_CALLBACK (on_monitors_changed),
                           settings, G_CONNECT_AFTER);
}

static void
meta_settings_class_init (MetaSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = meta_settings_dispose;

  signals[UI_SCALING_FACTOR_CHANGED] =
    g_signal_new ("ui-scaling-factor-changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[GLOBAL_SCALING_FACTOR_CHANGED] =
    g_signal_new ("global-scaling-factor-changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[FONT_DPI_CHANGED] =
    g_signal_new ("font-dpi-changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[EXPERIMENTAL_FEATURES_CHANGED] =
    g_signal_new ("experimental-features-changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_UINT);
}
