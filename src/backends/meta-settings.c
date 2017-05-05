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

  int ui_scaling_factor;
  int global_scaling_factor;

  int font_dpi;

  MetaExperimentalFeature experimental_features;
  gboolean experimental_features_overridden;
};

G_DEFINE_TYPE (MetaSettings, meta_settings, G_TYPE_OBJECT)

static int
calculate_ui_scaling_factor (MetaSettings *settings)
{
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (settings->backend);
  GList *logical_monitors;
  GList *l;
  int max_scale = 1;

  logical_monitors =
    meta_monitor_manager_get_logical_monitors (monitor_manager);
  for (l = logical_monitors; l; l = l->next)
    {
      MetaLogicalMonitor *logical_monitor = l->data;

      max_scale = MAX (meta_logical_monitor_get_scale (logical_monitor),
                       max_scale);
    }

  return max_scale;
}

static int
get_xsettings_scaling_factor (void)
{
  GdkScreen *screen;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_INT);

  screen = gdk_screen_get_default ();
  if (gdk_screen_get_setting (screen, "gdk-window-scaling-factor", &value))
    return g_value_get_int (&value);
  else
    return 1;
}

static gboolean
update_ui_scaling_factor (MetaSettings *settings)
{
  int ui_scaling_factor;

  if (meta_is_stage_views_scaled ())
    {
      ui_scaling_factor = 1;
    }
  else
    {
      if (meta_is_monitor_config_manager_enabled ())
        ui_scaling_factor = calculate_ui_scaling_factor (settings);
      else
        ui_scaling_factor = get_xsettings_scaling_factor ();
    }

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

static void
xft_dpi_changed (GtkSettings  *gtk_settings,
                 GParamSpec   *pspec,
                 MetaSettings *settings)
{
  /* This only matters when we rely on XSettings. */
  if (meta_is_monitor_config_manager_enabled ())
    return;

  meta_settings_update_ui_scaling_factor (settings);
}

static void
x11_display_opened (MetaBackend  *backend,
                    MetaSettings *settings)
{
  /*
   * gdk-window-scaling-factor is not exported to gtk-settings
   * because it is handled inside gdk, so we use gtk-xft-dpi instead
   * which also changes when the scale factor changes.
   *
   * TODO: Only rely on our own configured scale when we only have
   * MetaMonitorConfigManager.
   */
  g_signal_connect (gtk_settings_get_default (), "notify::gtk-xft-dpi",
                    G_CALLBACK (xft_dpi_changed), settings);
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
      else if (g_str_equal (feature, "monitor-config-manager"))
        features |= META_EXPERIMENTAL_FEATURE_MONITOR_CONFIG_MANAGER;
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

MetaSettings *
meta_settings_new (MetaBackend *backend)
{
  MetaSettings *settings;

  settings = g_object_new (META_TYPE_SETTINGS, NULL);
  settings->backend = backend;

  g_signal_connect (backend, "x11-display-opened",
                    G_CALLBACK (x11_display_opened),
                    settings);

  return settings;
}

static void
meta_settings_dispose (GObject *object)
{
  MetaSettings *settings = META_SETTINGS (object);

  g_clear_object (&settings->mutter_settings);
  g_clear_object (&settings->interface_settings);

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

  /* Chain up inter-dependent settings. */
  g_signal_connect (settings, "global-scaling-factor-changed",
                    G_CALLBACK (meta_settings_update_ui_scaling_factor), NULL);
  g_signal_connect (settings, "ui-scaling-factor-changed",
                    G_CALLBACK (meta_settings_update_font_dpi), NULL);

  update_global_scaling_factor (settings);
  update_experimental_features (settings);
}

void
meta_settings_post_init (MetaSettings *settings)
{
  update_ui_scaling_factor (settings);
  update_font_dpi (settings);
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
