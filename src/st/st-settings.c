/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-settings.c: Global settings
 *
 * Copyright 2019 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <gio/gio.h>

#include "st-private.h"
#include "st-settings.h"

#define KEY_ENABLE_ANIMATIONS "enable-animations"
#define KEY_PRIMARY_PASTE     "gtk-enable-primary-paste"
#define KEY_DRAG_THRESHOLD    "drag-threshold"
#define KEY_FONT_NAME         "font-name"
#define KEY_GTK_THEME         "gtk-theme"
#define KEY_GTK_ICON_THEME    "icon-theme"
#define KEY_MAGNIFIER_ACTIVE "screen-magnifier-enabled"

enum {
  PROP_0,
  PROP_ENABLE_ANIMATIONS,
  PROP_PRIMARY_PASTE,
  PROP_DRAG_THRESHOLD,
  PROP_FONT_NAME,
  PROP_GTK_THEME,
  PROP_GTK_ICON_THEME,
  PROP_MAGNIFIER_ACTIVE,
  PROP_SLOW_DOWN_FACTOR,
  N_PROPS
};

GParamSpec *props[N_PROPS] = { 0 };

struct _StSettings
{
  GObject parent_object;
  GSettings *interface_settings;
  GSettings *mouse_settings;
  GSettings *a11y_settings;

  gchar *font_name;
  gchar *gtk_theme;
  gchar *gtk_icon_theme;
  int inhibit_animations_count;
  gboolean enable_animations;
  gboolean primary_paste;
  gboolean magnifier_active;
  gint drag_threshold;
  double slow_down_factor;
};

G_DEFINE_TYPE (StSettings, st_settings, G_TYPE_OBJECT)

#define EPSILON (1e-10)

static void
st_settings_set_slow_down_factor (StSettings *settings,
                                  double      factor)
{
  if (fabs (settings->slow_down_factor - factor) < EPSILON)
    return;

  settings->slow_down_factor = factor;
  g_object_notify_by_pspec (G_OBJECT (settings), props[PROP_SLOW_DOWN_FACTOR]);
}

static gboolean
get_enable_animations (StSettings *settings)
{
  if (settings->inhibit_animations_count > 0)
    return FALSE;
  else
    return settings->enable_animations;
}

void
st_settings_inhibit_animations (StSettings *settings)
{
  gboolean enable_animations;

  enable_animations = get_enable_animations (settings);
  settings->inhibit_animations_count++;

  if (enable_animations != get_enable_animations (settings))
    g_object_notify_by_pspec (G_OBJECT (settings),
                              props[PROP_ENABLE_ANIMATIONS]);
}

void
st_settings_uninhibit_animations (StSettings *settings)
{
  gboolean enable_animations;

  enable_animations = get_enable_animations (settings);
  settings->inhibit_animations_count--;

  if (enable_animations != get_enable_animations (settings))
    g_object_notify_by_pspec (G_OBJECT (settings),
                              props[PROP_ENABLE_ANIMATIONS]);
}

static void
st_settings_finalize (GObject *object)
{
  StSettings *settings = ST_SETTINGS (object);

  g_object_unref (settings->interface_settings);
  g_object_unref (settings->mouse_settings);
  g_object_unref (settings->a11y_settings);
  g_free (settings->font_name);
  g_free (settings->gtk_theme);
  g_free (settings->gtk_icon_theme);

  G_OBJECT_CLASS (st_settings_parent_class)->finalize (object);
}

static void
st_settings_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  StSettings *settings = ST_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_SLOW_DOWN_FACTOR:
      st_settings_set_slow_down_factor (settings, g_value_get_double (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
st_settings_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  StSettings *settings = ST_SETTINGS (object);

  switch (prop_id)
    {
    case PROP_ENABLE_ANIMATIONS:
      g_value_set_boolean (value, get_enable_animations (settings));
      break;
    case PROP_PRIMARY_PASTE:
      g_value_set_boolean (value, settings->primary_paste);
      break;
    case PROP_DRAG_THRESHOLD:
      g_value_set_int (value, settings->drag_threshold);
      break;
    case PROP_FONT_NAME:
      g_value_set_string (value, settings->font_name);
      break;
    case PROP_GTK_THEME:
      g_value_set_string (value, settings->gtk_theme);
      break;
    case PROP_GTK_ICON_THEME:
      g_value_set_string (value, settings->gtk_icon_theme);
      break;
    case PROP_MAGNIFIER_ACTIVE:
      g_value_set_boolean (value, settings->magnifier_active);
      break;
    case PROP_SLOW_DOWN_FACTOR:
      g_value_set_double (value, settings->slow_down_factor);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
st_settings_class_init (StSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = st_settings_finalize;
  object_class->set_property = st_settings_set_property;
  object_class->get_property = st_settings_get_property;

  props[PROP_ENABLE_ANIMATIONS] = g_param_spec_boolean ("enable-animations",
                                                        "Enable animations",
                                                        "Enable animations",
                                                        TRUE,
                                                        ST_PARAM_READABLE);
  props[PROP_PRIMARY_PASTE] = g_param_spec_boolean ("primary-paste",
                                                    "Primary paste",
                                                    "Primary paste",
                                                    TRUE,
                                                    ST_PARAM_READABLE);
  props[PROP_DRAG_THRESHOLD] = g_param_spec_int ("drag-threshold",
                                                 "Drag threshold",
                                                 "Drag threshold",
                                                 0, G_MAXINT, 8,
                                                 ST_PARAM_READABLE);
  props[PROP_FONT_NAME] = g_param_spec_string ("font-name",
                                               "font name",
                                               "font name",
                                               "",
                                               ST_PARAM_READABLE);
  props[PROP_GTK_THEME] = g_param_spec_string ("gtk-theme",
                                               "GTK+ Theme",
                                               "GTK+ Theme",
                                               "",
                                               ST_PARAM_READABLE);
  props[PROP_GTK_ICON_THEME] = g_param_spec_string ("gtk-icon-theme",
                                                    "GTK+ Icon Theme",
                                                    "GTK+ Icon Theme",
                                                    "",
                                                    ST_PARAM_READABLE);
  props[PROP_MAGNIFIER_ACTIVE] = g_param_spec_boolean("magnifier-active",
                                                      "Magnifier is active",
                                                      "Weather the a11y magnifier is active",
                                                      FALSE,
                                                      ST_PARAM_READABLE);
  props[PROP_SLOW_DOWN_FACTOR] = g_param_spec_double("slow-down-factor",
                                                      "Slow down factor",
                                                      "Factor applied to all animation durations",
                                                      EPSILON, G_MAXDOUBLE, 1.0,
                                                      ST_PARAM_READWRITE);

  g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
on_interface_settings_changed (GSettings   *g_settings,
                               const gchar *key,
                               StSettings  *settings)
{
  if (g_str_equal (key, KEY_ENABLE_ANIMATIONS))
    {
      settings->enable_animations = g_settings_get_boolean (g_settings, key);
      g_object_notify_by_pspec (G_OBJECT (settings), props[PROP_ENABLE_ANIMATIONS]);
    }
  else if (g_str_equal (key, KEY_PRIMARY_PASTE))
    {
      settings->primary_paste = g_settings_get_boolean (g_settings, key);
      g_object_notify_by_pspec (G_OBJECT (settings), props[PROP_PRIMARY_PASTE]);
    }
  else if (g_str_equal (key, KEY_FONT_NAME))
    {
      g_free (settings->font_name);
      settings->font_name = g_settings_get_string (g_settings, key);
      g_object_notify_by_pspec (G_OBJECT (settings), props[PROP_FONT_NAME]);
    }
  else if (g_str_equal (key, KEY_GTK_THEME))
    {
      g_free (settings->gtk_theme);
      settings->gtk_theme = g_settings_get_string (g_settings, key);
      g_object_notify_by_pspec (G_OBJECT (settings), props[PROP_GTK_THEME]);
    }
  else if (g_str_equal (key, KEY_GTK_ICON_THEME))
    {
      g_free (settings->gtk_icon_theme);
      settings->gtk_icon_theme = g_settings_get_string (g_settings, key);
      g_object_notify_by_pspec (G_OBJECT (settings),
                                props[PROP_GTK_ICON_THEME]);
    }
}

static void
on_mouse_settings_changed (GSettings   *g_settings,
                           const gchar *key,
                           StSettings  *settings)
{
  if (g_str_equal (key, KEY_DRAG_THRESHOLD))
    {
      settings->drag_threshold = g_settings_get_int (g_settings, key);
      g_object_notify_by_pspec (G_OBJECT (settings), props[PROP_DRAG_THRESHOLD]);
    }
}

static void
on_a11y_settings_changed (GSettings   *g_settings,
                          const gchar *key,
                          StSettings  *settings)
{
  if (g_str_equal (key, KEY_MAGNIFIER_ACTIVE))
    {
      settings->magnifier_active = g_settings_get_boolean (g_settings, key);
      g_object_notify_by_pspec (G_OBJECT (settings), props[PROP_MAGNIFIER_ACTIVE]);
    }
}

static void
st_settings_init (StSettings *settings)
{
  settings->interface_settings = g_settings_new ("org.gnome.desktop.interface");
  g_signal_connect (settings->interface_settings, "changed",
                    G_CALLBACK (on_interface_settings_changed), settings);

  settings->mouse_settings = g_settings_new ("org.gnome.settings-daemon.peripherals.mouse");
  g_signal_connect (settings->interface_settings, "changed",
                    G_CALLBACK (on_mouse_settings_changed), settings);

  settings->a11y_settings = g_settings_new ("org.gnome.desktop.a11y.applications");
  g_signal_connect (settings->a11y_settings, "changed",
                    G_CALLBACK (on_a11y_settings_changed), settings);

  settings->enable_animations = g_settings_get_boolean (settings->interface_settings,
                                                        KEY_ENABLE_ANIMATIONS);
  settings->primary_paste = g_settings_get_boolean (settings->interface_settings,
                                                    KEY_PRIMARY_PASTE);
  settings->font_name = g_settings_get_string (settings->interface_settings,
                                               KEY_FONT_NAME);
  settings->gtk_theme = g_settings_get_string (settings->interface_settings,
                                               KEY_GTK_THEME);
  settings->gtk_icon_theme = g_settings_get_string (settings->interface_settings,
                                                    KEY_GTK_ICON_THEME);
  settings->drag_threshold = g_settings_get_int (settings->mouse_settings,
                                                 KEY_DRAG_THRESHOLD);
  settings->magnifier_active = g_settings_get_boolean (settings->a11y_settings,
                                                       KEY_MAGNIFIER_ACTIVE);
  settings->slow_down_factor = 1.;
}

/**
 * st_settings_get:
 *
 * Gets the #StSettings
 *
 * Returns: (transfer none): a settings object
 **/
StSettings *
st_settings_get (void)
{
  static StSettings *settings = NULL;

  if (!settings)
    settings = g_object_new (ST_TYPE_SETTINGS, NULL);

  return settings;
}
