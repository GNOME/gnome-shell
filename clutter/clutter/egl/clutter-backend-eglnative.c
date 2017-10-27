/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010,2011  Intel Corporation.
 *               2011 Giovanni Campagna <scampa.giovanni@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.

 * Authors:
 *  Matthew Allum
 *  Emmanuele Bassi
 *  Robert Bragg
 *  Neil Roberts
 */

#include "clutter-build-config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>

#include "clutter-backend-eglnative.h"

/* This is a Cogl based backend */
#include "cogl/clutter-stage-cogl.h"

#ifdef HAVE_EVDEV
#include "evdev/clutter-device-manager-evdev.h"
#endif

#include "clutter-debug.h"
#include "clutter-private.h"
#include "clutter-main.h"
#include "clutter-stage-private.h"
#include "clutter-settings-private.h"

#ifdef COGL_HAS_EGL_SUPPORT
#include "clutter-egl.h"
#endif

G_DEFINE_TYPE (ClutterBackendEglNative, clutter_backend_egl_native, CLUTTER_TYPE_BACKEND);

static void
clutter_backend_egl_native_dispose (GObject *gobject)
{
  ClutterBackendEglNative *backend_egl_native = CLUTTER_BACKEND_EGL_NATIVE (gobject);

  g_clear_object (&backend_egl_native->xsettings);

  if (backend_egl_native->event_timer != NULL)
    {
      g_timer_destroy (backend_egl_native->event_timer);
      backend_egl_native->event_timer = NULL;
    }

  G_OBJECT_CLASS (clutter_backend_egl_native_parent_class)->dispose (gobject);
}

static void
clutter_backend_egl_native_class_init (ClutterBackendEglNativeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = clutter_backend_egl_native_dispose;
}

typedef struct
{
  cairo_antialias_t cairo_antialias;
  gint clutter_font_antialias;

  cairo_hint_style_t cairo_hint_style;
  const char *clutter_font_hint_style;

  cairo_subpixel_order_t cairo_subpixel_order;
  const char *clutter_font_subpixel_order;
} FontSettings;

static void
get_font_gsettings (GSettings    *xsettings,
                    FontSettings *output)
{
  /* org.gnome.settings-daemon.GsdFontAntialiasingMode */
  static const struct
  {
    cairo_antialias_t cairo_antialias;
    gint clutter_font_antialias;
  }
  antialiasings[] =
  {
    /* none=0      */ {CAIRO_ANTIALIAS_NONE,     0},
    /* grayscale=1 */ {CAIRO_ANTIALIAS_GRAY,     1},
    /* rgba=2      */ {CAIRO_ANTIALIAS_SUBPIXEL, 1},
  };

  /* org.gnome.settings-daemon.GsdFontHinting */
  static const struct
  {
    cairo_hint_style_t cairo_hint_style;
    const char *clutter_font_hint_style;
  }
  hintings[] =
  {
    /* none=0   */ {CAIRO_HINT_STYLE_NONE,   "hintnone"},
    /* slight=1 */ {CAIRO_HINT_STYLE_SLIGHT, "hintslight"},
    /* medium=2 */ {CAIRO_HINT_STYLE_MEDIUM, "hintmedium"},
    /* full=3   */ {CAIRO_HINT_STYLE_FULL,   "hintfull"},
  };

  /* org.gnome.settings-daemon.GsdFontRgbaOrder */
  static const struct
  {
    cairo_subpixel_order_t cairo_subpixel_order;
    const char *clutter_font_subpixel_order;
  }
  rgba_orders[] =
  {
    /* rgba=0 */ {CAIRO_SUBPIXEL_ORDER_RGB,  "rgb"}, /* XXX what is 'rgba'? */
    /* rgb=1  */ {CAIRO_SUBPIXEL_ORDER_RGB,  "rgb"},
    /* bgr=2  */ {CAIRO_SUBPIXEL_ORDER_BGR,  "bgr"},
    /* vrgb=3 */ {CAIRO_SUBPIXEL_ORDER_VRGB, "vrgb"},
    /* vbgr=4 */ {CAIRO_SUBPIXEL_ORDER_VBGR, "vbgr"},
  };
  guint i;

  i = g_settings_get_enum (xsettings, "hinting");
  if (i < G_N_ELEMENTS (hintings))
    {
      output->cairo_hint_style = hintings[i].cairo_hint_style;
      output->clutter_font_hint_style = hintings[i].clutter_font_hint_style;
    }
  else
    {
      output->cairo_hint_style = CAIRO_HINT_STYLE_DEFAULT;
      output->clutter_font_hint_style = NULL;
    }

  i = g_settings_get_enum (xsettings, "antialiasing");
  if (i < G_N_ELEMENTS (antialiasings))
    {
      output->cairo_antialias = antialiasings[i].cairo_antialias;
      output->clutter_font_antialias = antialiasings[i].clutter_font_antialias;
    }
  else
    {
      output->cairo_antialias = CAIRO_ANTIALIAS_DEFAULT;
      output->clutter_font_antialias = -1;
    }

  i = g_settings_get_enum (xsettings, "rgba-order");
  if (i < G_N_ELEMENTS (rgba_orders))
    {
      output->cairo_subpixel_order = rgba_orders[i].cairo_subpixel_order;
      output->clutter_font_subpixel_order = rgba_orders[i].clutter_font_subpixel_order;
    }
  else
    {
      output->cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_DEFAULT;
      output->clutter_font_subpixel_order = NULL;
    }

  if (output->cairo_antialias == CAIRO_ANTIALIAS_GRAY)
    output->clutter_font_subpixel_order = "none";
}

static void
init_font_options (ClutterBackendEglNative *backend_egl_native)
{
  GSettings *xsettings = backend_egl_native->xsettings;
  cairo_font_options_t *options = cairo_font_options_create ();
  FontSettings fs;

  get_font_gsettings (xsettings, &fs);

  cairo_font_options_set_hint_style (options, fs.cairo_hint_style);
  cairo_font_options_set_antialias (options, fs.cairo_antialias);
  cairo_font_options_set_subpixel_order (options, fs.cairo_subpixel_order);

  clutter_backend_set_font_options (CLUTTER_BACKEND (backend_egl_native),
                                    options);

  cairo_font_options_destroy (options);
}

static gboolean
on_xsettings_change_event (GSettings *xsettings,
                           gpointer   keys,
                           gint       n_keys,
                           gpointer   user_data)
{
  /*
   * A simpler alternative to this function that does not update the screen
   * immediately (like macOS :P):
   *
   *   init_font_options (CLUTTER_BACKEND_EGL_NATIVE (user_data));
   *
   * which has the added benefit of eliminating the need for all the
   * FontSettings.clutter_ fields. However the below approach is better for
   * testing settings and more consistent with the existing x11 backend...
   */
  ClutterSettings *csettings = clutter_settings_get_default ();
  FontSettings fs;
  gint hinting;

  get_font_gsettings (xsettings, &fs);
  hinting = fs.cairo_hint_style == CAIRO_HINT_STYLE_NONE ? 0 : 1;
  g_object_set (csettings,
                "font-hinting",        hinting,
                "font-hint-style",     fs.clutter_font_hint_style,
                "font-antialias",      fs.clutter_font_antialias,
                "font-subpixel-order", fs.clutter_font_subpixel_order,
                NULL);

  return FALSE;
}

static void
clutter_backend_egl_native_init (ClutterBackendEglNative *backend_egl_native)
{
  static const gchar xsettings_path[] = "org.gnome.settings-daemon.plugins.xsettings";
  GSettingsSchemaSource *source = g_settings_schema_source_get_default ();
  GSettingsSchema *schema = g_settings_schema_source_lookup (source,
                                                             xsettings_path,
                                                             FALSE);

  if (!schema)
    {
      g_warning ("Failed to find schema: %s", xsettings_path);
    }
  else
    {
      backend_egl_native->xsettings = g_settings_new_full (schema, NULL, NULL);
      if (backend_egl_native->xsettings)
        {
          init_font_options (backend_egl_native);
          g_signal_connect (backend_egl_native->xsettings, "change-event",
                            G_CALLBACK (on_xsettings_change_event),
                            backend_egl_native);
        }
    }

  backend_egl_native->event_timer = g_timer_new ();
}

ClutterBackend *
clutter_backend_egl_native_new (void)
{
  return g_object_new (CLUTTER_TYPE_BACKEND_EGL_NATIVE, NULL);
}

/**
 * clutter_eglx_display:
 *
 * Retrieves the EGL display used by Clutter.
 *
 * Return value: the EGL display, or 0
 *
 * Since: 0.6
 *
 * Deprecated: 1.6: Use clutter_egl_get_egl_display() instead.
 */
EGLDisplay
clutter_eglx_display (void)
{
  return clutter_egl_get_egl_display ();
}

/**
 * clutter_egl_display:
 *
 * Retrieves the EGL display used by Clutter.
 *
 * Return value: the EGL display used by Clutter, or 0
 *
 * Since: 0.6
 *
 * Deprecated: 1.6: Use clutter_egl_get_egl_display() instead.
 */
EGLDisplay
clutter_egl_display (void)
{
  return clutter_egl_get_egl_display ();
}

/**
 * clutter_egl_get_egl_display:
 *
 * Retrieves the EGL display used by Clutter, if it supports the
 * EGL windowing system and if it is running using an EGL backend.
 *
 * Return value: the EGL display used by Clutter, or 0
 *
 * Since: 1.6
 */
EGLDisplay
clutter_egl_get_egl_display (void)
{
  ClutterBackend *backend;

  if (!_clutter_context_is_initialized ())
    {
      g_critical ("The Clutter backend has not been initialized yet");
      return 0;
    }

  backend = clutter_get_default_backend ();

  if (!CLUTTER_IS_BACKEND_EGL_NATIVE (backend))
    {
      g_critical ("The Clutter backend is not an EGL backend");
      return 0;
    }

#if COGL_HAS_EGL_SUPPORT
  return cogl_egl_context_get_egl_display (backend->cogl_context);
#else
  return 0;
#endif
}

/**
 * clutter_egl_freeze_master_clock:
 *
 * Freezing the master clock makes Clutter stop processing events,
 * redrawing, and advancing timelines. This is necessary when implementing
 * a display server, to ensure that Clutter doesn't keep trying to page
 * flip when DRM master has been dropped, e.g. when VT switched away.
 *
 * The master clock starts out running, so if you are VT switched away on
 * startup, you need to call this immediately.
 *
 * If you're also using the evdev backend, make sure to also use
 * clutter_evdev_release_devices() to make sure that Clutter doesn't also
 * access revoked evdev devices when VT switched away.
 *
 * To unthaw a frozen master clock, use clutter_egl_thaw_master_clock().
 *
 * Since: 1.20
 */
void
clutter_egl_freeze_master_clock (void)
{
  ClutterMasterClock *master_clock;

  g_return_if_fail (CLUTTER_IS_BACKEND_EGL_NATIVE (clutter_get_default_backend ()));

  master_clock = _clutter_master_clock_get_default ();
  _clutter_master_clock_set_paused (master_clock, TRUE);
}

/**
 * clutter_egl_thaw_master_clock:
 *
 * Thaws a master clock that has previously been frozen with
 * clutter_egl_freeze_master_clock(), and start pumping the master clock
 * again at the next iteration. Note that if you're switching back to your
 * own VT, you should probably also queue a stage redraw with
 * clutter_stage_ensure_redraw().
 *
 * Since: 1.20
 */
void
clutter_egl_thaw_master_clock (void)
{
  ClutterMasterClock *master_clock;

  g_return_if_fail (CLUTTER_IS_BACKEND_EGL_NATIVE (clutter_get_default_backend ()));

  master_clock = _clutter_master_clock_get_default ();
  _clutter_master_clock_set_paused (master_clock, FALSE);

  _clutter_master_clock_start_running (master_clock);
}
