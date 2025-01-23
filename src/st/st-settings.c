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

#include "config.h"

#include <math.h>
#include <gio/gio.h>

#include "st-private.h"
#include "st-settings.h"
#include "st-enum-types.h"

#define KEY_ENABLE_ANIMATIONS     "enable-animations"
#define KEY_PRIMARY_PASTE         "gtk-enable-primary-paste"
#define KEY_DRAG_THRESHOLD        "drag-threshold"
#define KEY_FONT_NAME             "font-name"
#define KEY_COLOR_SCHEME          "color-scheme"
#define KEY_ACCENT_COLOR          "accent-color"
#define KEY_HIGH_CONTRAST         "high-contrast"
#define KEY_GTK_ICON_THEME        "icon-theme"
#define KEY_MAGNIFIER_ACTIVE      "screen-magnifier-enabled"
#define KEY_DISABLE_SHOW_PASSWORD "disable-show-password"

enum {
  PROP_0,
  PROP_ENABLE_ANIMATIONS,
  PROP_PRIMARY_PASTE,
  PROP_DRAG_THRESHOLD,
  PROP_FONT_NAME,
  PROP_COLOR_SCHEME,
  PROP_ACCENT_COLOR,
  PROP_HIGH_CONTRAST,
  PROP_GTK_ICON_THEME,
  PROP_MAGNIFIER_ACTIVE,
  PROP_SLOW_DOWN_FACTOR,
  PROP_DISABLE_SHOW_PASSWORD,
  N_PROPS
};

GParamSpec *props[N_PROPS] = { 0 };

struct _StSettings
{
  GObject parent_object;
  GSettings *interface_settings;
  GSettings *mouse_settings;
  GSettings *a11y_applications_settings;
  GSettings *a11y_interface_settings;
  GSettings *lockdown_settings;

  gchar *font_name;
  gboolean high_contrast;
  gchar *gtk_icon_theme;
  int inhibit_animations_count;
  gboolean enable_animations;
  gboolean primary_paste;
  gboolean magnifier_active;
  gboolean disable_show_password;
  gint drag_threshold;
  double slow_down_factor;
  StSystemColorScheme color_scheme;
  StSystemAccentColor accent_color;
};

G_DEFINE_TYPE (StSettings, st_settings, G_TYPE_OBJECT)

#define EPSILON (1e-10)

static gboolean
get_enable_animations (StSettings *settings)
{
  if (settings->inhibit_animations_count > 0)
    return FALSE;
  else
    return settings->enable_animations;
}

gboolean
st_settings_get_enable_animations (StSettings *settings)
{
  g_return_val_if_fail (ST_IS_SETTINGS (settings), FALSE);

  return get_enable_animations (settings);
}

gboolean
st_settings_get_primary_paste (StSettings *settings)
{
  g_return_val_if_fail (ST_IS_SETTINGS (settings), FALSE);

  return settings->primary_paste;
}

int
st_settings_get_drag_threshold (StSettings *settings)
{
  g_return_val_if_fail (ST_IS_SETTINGS (settings), -1);

  return settings->drag_threshold;
}

const char *
st_settings_get_font_name (StSettings *settings)
{
  g_return_val_if_fail (ST_IS_SETTINGS (settings), NULL);

  return settings->font_name;
}

const char *
st_settings_get_gtk_icon_theme (StSettings *settings)
{
  g_return_val_if_fail (ST_IS_SETTINGS (settings), NULL);

  return settings->gtk_icon_theme;
}

StSystemColorScheme
st_settings_get_color_scheme (StSettings *settings)
{
  g_return_val_if_fail (ST_IS_SETTINGS (settings), ST_SYSTEM_COLOR_SCHEME_DEFAULT);

  return settings->color_scheme;
}

StSystemAccentColor
st_settings_get_accent_color (StSettings *settings)
{
  g_return_val_if_fail (ST_IS_SETTINGS (settings), ST_SYSTEM_ACCENT_COLOR_BLUE);

  return settings->accent_color;
}

gboolean
st_settings_get_high_contrast (StSettings *settings)
{
  g_return_val_if_fail (ST_IS_SETTINGS (settings), FALSE);

  return settings->high_contrast;
}

gboolean
st_settings_get_magnifier_active (StSettings *settings)
{
  g_return_val_if_fail (ST_IS_SETTINGS (settings), FALSE);

  return settings->magnifier_active;
}

gboolean
st_settings_get_disable_show_password (StSettings *settings)
{
  g_return_val_if_fail (ST_IS_SETTINGS (settings), FALSE);

  return settings->disable_show_password;
}

double
st_settings_get_slow_down_factor (StSettings *settings)
{
  g_return_val_if_fail (ST_SETTINGS (settings), -1);

  return settings->slow_down_factor;
}

void
st_settings_set_slow_down_factor (StSettings *settings,
                                  double      factor)
{
  g_return_if_fail (ST_IS_SETTINGS (settings));

  if (fabs (settings->slow_down_factor - factor) < EPSILON)
    return;

  settings->slow_down_factor = factor;
  g_object_notify_by_pspec (G_OBJECT (settings), props[PROP_SLOW_DOWN_FACTOR]);
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
  g_object_unref (settings->a11y_applications_settings);
  g_object_unref (settings->a11y_interface_settings);
  g_object_unref (settings->lockdown_settings);
  g_free (settings->font_name);
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
    case PROP_HIGH_CONTRAST:
      g_value_set_boolean (value, settings->high_contrast);
      break;
    case PROP_GTK_ICON_THEME:
      g_value_set_string (value, settings->gtk_icon_theme);
      break;
    case PROP_COLOR_SCHEME:
      g_value_set_enum (value, settings->color_scheme);
      break;
    case PROP_ACCENT_COLOR:
      g_value_set_enum (value, settings->accent_color);
      break;
    case PROP_MAGNIFIER_ACTIVE:
      g_value_set_boolean (value, settings->magnifier_active);
      break;
    case PROP_SLOW_DOWN_FACTOR:
      g_value_set_double (value, settings->slow_down_factor);
      break;
    case PROP_DISABLE_SHOW_PASSWORD:
      g_value_set_boolean (value, settings->disable_show_password);
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

  /**
   * StSettings:enable-animations:
   *
   * Whether animations are enabled.
   */
  props[PROP_ENABLE_ANIMATIONS] = g_param_spec_boolean ("enable-animations", NULL, NULL,
                                                        TRUE,
                                                        ST_PARAM_READABLE);

  /**
   * StSettings:primary-paste:
   *
   * Whether pasting from the `PRIMARY` selection is supported (eg. middle-click
   * paste).
   */
  props[PROP_PRIMARY_PASTE] = g_param_spec_boolean ("primary-paste", NULL, NULL,
                                                    TRUE,
                                                    ST_PARAM_READABLE);

  /**
   * StSettings:drag-threshold:
   *
   * The threshold before a drag operation begins.
   */
  props[PROP_DRAG_THRESHOLD] = g_param_spec_int ("drag-threshold", NULL, NULL,
                                                 0, G_MAXINT, 8,
                                                 ST_PARAM_READABLE);

  /**
   * StSettings:font-name:
   *
   * The current font name.
   */
  props[PROP_FONT_NAME] = g_param_spec_string ("font-name", NULL, NULL,
                                               "",
                                               ST_PARAM_READABLE);

  /**
   * StSettings:high-contrast:
   *
   * Whether the accessibility high contrast mode is enabled.
   */
  props[PROP_HIGH_CONTRAST] = g_param_spec_boolean ("high-contrast", NULL, NULL,
                                                    FALSE,
                                                    ST_PARAM_READABLE);

  /**
   * StSettings:gtk-icon-theme:
   *
   * The current GTK icon theme
   */
  props[PROP_GTK_ICON_THEME] = g_param_spec_string ("gtk-icon-theme", NULL, NULL,
                                                    "",
                                                    ST_PARAM_READABLE);

  /**
   * StSettings:color-scheme:
   *
   * The preferred color-scheme
   */
  props[PROP_COLOR_SCHEME] = g_param_spec_enum ("color-scheme", NULL, NULL,
                                                ST_TYPE_SYSTEM_COLOR_SCHEME,
                                                ST_SYSTEM_COLOR_SCHEME_DEFAULT,
                                                ST_PARAM_READABLE);

  /**
   * StSettings:accent-color:
   *
   * The current accent color.
   */
  props[PROP_ACCENT_COLOR] = g_param_spec_enum ("accent-color", NULL, NULL,
                                                ST_TYPE_SYSTEM_ACCENT_COLOR,
                                                ST_SYSTEM_ACCENT_COLOR_BLUE,
                                                ST_PARAM_READABLE);

  /**
   * StSettings:magnifier-active:
   *
   * Whether the accessibility magnifier is active.
   */
  props[PROP_MAGNIFIER_ACTIVE] = g_param_spec_boolean("magnifier-active", NULL, NULL,
                                                      FALSE,
                                                      ST_PARAM_READABLE);

  /**
   * StSettings:slow-down-factor:
   *
   * The slow-down factor applied to all animation durations.
   */
  props[PROP_SLOW_DOWN_FACTOR] = g_param_spec_double("slow-down-factor", NULL, NULL,
                                                      EPSILON, G_MAXDOUBLE, 1.0,
                                                      ST_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * StSettings:disable-show-password:
   *
   *  Whether password showing can be locked down
   */
  props[PROP_DISABLE_SHOW_PASSWORD] = g_param_spec_boolean("disable-show-password", NULL, NULL,
                                                           FALSE,
                                                           ST_PARAM_READABLE);

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
  else if (g_str_equal (key, KEY_GTK_ICON_THEME))
    {
      g_free (settings->gtk_icon_theme);
      settings->gtk_icon_theme = g_settings_get_string (g_settings, key);
      g_object_notify_by_pspec (G_OBJECT (settings),
                                props[PROP_GTK_ICON_THEME]);
    }
  else if (g_str_equal (key, KEY_COLOR_SCHEME))
    {
      settings->color_scheme = g_settings_get_enum (g_settings, key);
      g_object_notify_by_pspec (G_OBJECT (settings),
                                props[PROP_COLOR_SCHEME]);
    }
  else if (g_str_equal (key, KEY_ACCENT_COLOR))
    {
      settings->accent_color = g_settings_get_enum (g_settings, key);
      g_object_notify_by_pspec (G_OBJECT (settings), props[PROP_ACCENT_COLOR]);
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
on_a11y_applications_settings_changed (GSettings   *g_settings,
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
on_a11y_interface_settings_changed (GSettings   *g_settings,
                                    const gchar *key,
                                    StSettings  *settings)
{
  if (g_str_equal (key, KEY_HIGH_CONTRAST))
    {
      settings->high_contrast = g_settings_get_boolean (g_settings, key);
      g_object_notify_by_pspec (G_OBJECT (settings), props[PROP_HIGH_CONTRAST]);
    }
}

static void
on_lockdown_settings_changed (GSettings   *g_settings,
                              const gchar *key,
                              StSettings  *settings)
{
  if (g_str_equal (key, KEY_DISABLE_SHOW_PASSWORD))
    {
      settings->disable_show_password = g_settings_get_boolean (g_settings, key);
      g_object_notify_by_pspec (G_OBJECT (settings), props[PROP_DISABLE_SHOW_PASSWORD]);
    }
}

static void
st_settings_init (StSettings *settings)
{
  settings->interface_settings = g_settings_new ("org.gnome.desktop.interface");
  g_signal_connect (settings->interface_settings, "changed",
                    G_CALLBACK (on_interface_settings_changed), settings);

  settings->mouse_settings = g_settings_new ("org.gnome.desktop.peripherals.mouse");
  g_signal_connect (settings->mouse_settings, "changed",
                    G_CALLBACK (on_mouse_settings_changed), settings);

  settings->a11y_applications_settings = g_settings_new ("org.gnome.desktop.a11y.applications");
  g_signal_connect (settings->a11y_applications_settings, "changed",
                    G_CALLBACK (on_a11y_applications_settings_changed), settings);

  settings->a11y_interface_settings = g_settings_new ("org.gnome.desktop.a11y.interface");
  g_signal_connect (settings->a11y_interface_settings, "changed",
                    G_CALLBACK (on_a11y_interface_settings_changed), settings);

  settings->lockdown_settings = g_settings_new ("org.gnome.desktop.lockdown");
  g_signal_connect (settings->lockdown_settings, "changed",
                    G_CALLBACK (on_lockdown_settings_changed), settings);

  settings->enable_animations = g_settings_get_boolean (settings->interface_settings,
                                                        KEY_ENABLE_ANIMATIONS);
  settings->primary_paste = g_settings_get_boolean (settings->interface_settings,
                                                    KEY_PRIMARY_PASTE);
  settings->font_name = g_settings_get_string (settings->interface_settings,
                                               KEY_FONT_NAME);
  settings->gtk_icon_theme = g_settings_get_string (settings->interface_settings,
                                                    KEY_GTK_ICON_THEME);
  settings->color_scheme = g_settings_get_enum (settings->interface_settings,
                                                KEY_COLOR_SCHEME);
  settings->accent_color = g_settings_get_enum (settings->interface_settings,
                                                KEY_ACCENT_COLOR);
  settings->drag_threshold = g_settings_get_int (settings->mouse_settings,
                                                 KEY_DRAG_THRESHOLD);
  settings->magnifier_active = g_settings_get_boolean (settings->a11y_applications_settings,
                                                       KEY_MAGNIFIER_ACTIVE);
  settings->high_contrast = g_settings_get_boolean (settings->a11y_interface_settings,
                                                    KEY_HIGH_CONTRAST);
  settings->slow_down_factor = 1.;
  settings->disable_show_password = g_settings_get_boolean (settings->lockdown_settings, KEY_DISABLE_SHOW_PASSWORD);
}

/**
 * st_settings_get:
 *
 * Gets the global #StSettings object.
 *
 * Returns: (transfer none): the global #StSettings object
 **/
StSettings *
st_settings_get (void)
{
  static StSettings *settings = NULL;

  if (!settings)
    settings = g_object_new (ST_TYPE_SETTINGS, NULL);

  return settings;
}
