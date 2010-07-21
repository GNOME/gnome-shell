/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By:
 *      Matthew Allum <mallum@openedhand.com>
 *      Emmanuele Bassi <ebassi@linux.intel.com>
 *
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd
 * Copyright (C) 2009, 2010 Intel Corp
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
 */

/**
 * SECTION:clutter-backend
 * @short_description: Backend abstraction
 *
 * Clutter can be compiled against different backends. Each backend
 * has to implement a set of functions, in order to be used by Clutter.
 *
 * #ClutterBackend is the base class abstracting the various implementation;
 * it provides a basic API to query the backend for generic information
 * and settings.
 *
 * #ClutterBackend is available since Clutter 0.4
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-backend.h"
#include "clutter-debug.h"
#include "clutter-fixed.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-profile.h"

#include <cogl/cogl.h>

G_DEFINE_ABSTRACT_TYPE (ClutterBackend, clutter_backend, G_TYPE_OBJECT);

#define DEFAULT_FONT_NAME       "Sans 10"

#define CLUTTER_BACKEND_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_BACKEND, ClutterBackendPrivate))

struct _ClutterBackendPrivate
{
  cairo_font_options_t *font_options;

  gchar *font_name;

  gfloat units_per_em;
  gint32 units_serial;
};

enum
{
  RESOLUTION_CHANGED,
  FONT_CHANGED,
  SETTINGS_CHANGED,

  LAST_SIGNAL
};

static guint backend_signals[LAST_SIGNAL] = { 0, };

static void
clutter_backend_dispose (GObject *gobject)
{
  ClutterMainContext *clutter_context;

  clutter_context = _clutter_context_get_default ();

  if (clutter_context && clutter_context->events_queue)
    {
      g_queue_foreach (clutter_context->events_queue,
                       (GFunc) clutter_event_free,
                       NULL);
      g_queue_free (clutter_context->events_queue);
      clutter_context->events_queue = NULL;
    }

  G_OBJECT_CLASS (clutter_backend_parent_class)->dispose (gobject);
}

static void
clutter_backend_finalize (GObject *gobject)
{
  ClutterBackend *backend = CLUTTER_BACKEND (gobject);

  g_free (backend->priv->font_name);
  clutter_backend_set_font_options (backend, NULL);

  G_OBJECT_CLASS (clutter_backend_parent_class)->finalize (gobject);
}

static gfloat
get_units_per_em (ClutterBackend       *backend,
                  PangoFontDescription *font_desc)
{
  gfloat units_per_em = -1.0;
  gboolean free_font_desc = FALSE;
  gdouble dpi;

  dpi = clutter_backend_get_resolution (backend);

  if (font_desc == NULL)
    {
      const gchar *font_name = clutter_backend_get_font_name (backend);

      if (G_LIKELY (font_name != NULL && *font_name != '\0'))
        {
          font_desc = pango_font_description_from_string (font_name);
          free_font_desc = TRUE;
        }
    }

  if (font_desc != NULL)
    {
      gdouble font_size = 0;
      gint pango_size;
      gboolean is_absolute;

      pango_size = pango_font_description_get_size (font_desc);
      is_absolute = pango_font_description_get_size_is_absolute (font_desc);

      /* "absolute" means "device units" (usually, pixels); otherwise,
       * it means logical units (points)
       */
      if (is_absolute)
        font_size = (gdouble) pango_size / PANGO_SCALE;
      else
        font_size = (gdouble) pango_size / PANGO_SCALE
                  * dpi
                  / 96.0f;

      /* 10 points at 96 DPI is 13.3 pixels */
      units_per_em = (1.2f * font_size)
                   * dpi
                   / 96.0f;
    }
  else
    units_per_em = -1.0f;

  if (free_font_desc)
    pango_font_description_free (font_desc);

  return units_per_em;
}

static void
clutter_backend_real_resolution_changed (ClutterBackend *backend)
{
  ClutterBackendPrivate *priv = backend->priv;
  ClutterMainContext *context;
  ClutterSettings *settings;
  gint dpi;

  context = _clutter_context_get_default ();

  settings = clutter_settings_get_default ();
  g_object_get (settings, "font-dpi", &dpi, NULL);

  if (context->font_map != NULL)
    cogl_pango_font_map_set_resolution (context->font_map, dpi / 1024.0);

  priv->units_per_em = get_units_per_em (backend, NULL);
  priv->units_serial += 1;

  CLUTTER_NOTE (BACKEND, "Units per em: %.2f", priv->units_per_em);
}

static void
clutter_backend_real_font_changed (ClutterBackend *backend)
{
  ClutterBackendPrivate *priv = backend->priv;

  priv->units_per_em = get_units_per_em (backend, NULL);
  priv->units_serial += 1;

  CLUTTER_NOTE (BACKEND, "Units per em: %.2f", priv->units_per_em);
}

static void
clutter_backend_class_init (ClutterBackendClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = clutter_backend_dispose;
  gobject_class->finalize = clutter_backend_finalize;

  g_type_class_add_private (gobject_class, sizeof (ClutterBackendPrivate));

  backend_signals[RESOLUTION_CHANGED] =
    g_signal_new (I_("resolution-changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (ClutterBackendClass, resolution_changed),
                  NULL, NULL,
                  _clutter_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  backend_signals[FONT_CHANGED] =
    g_signal_new (I_("font-changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (ClutterBackendClass, font_changed),
                  NULL, NULL,
                  _clutter_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  backend_signals[SETTINGS_CHANGED] =
    g_signal_new (I_("settings-changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (ClutterBackendClass, settings_changed),
                  NULL, NULL,
                  _clutter_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  klass->resolution_changed = clutter_backend_real_resolution_changed;
  klass->font_changed = clutter_backend_real_font_changed;
}

static void
clutter_backend_init (ClutterBackend *backend)
{
  ClutterBackendPrivate *priv;

  priv = backend->priv = CLUTTER_BACKEND_GET_PRIVATE (backend);

  priv->units_per_em = -1.0;
  priv->units_serial = 1;
}

void
_clutter_backend_add_options (ClutterBackend *backend,
                              GOptionGroup   *group)
{
  ClutterBackendClass *klass;

  g_return_if_fail (CLUTTER_IS_BACKEND (backend));

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (klass->add_options)
    klass->add_options (backend, group);
}

gboolean
_clutter_backend_pre_parse (ClutterBackend  *backend,
                            GError         **error)
{
  ClutterBackendClass *klass;

  g_return_val_if_fail (CLUTTER_IS_BACKEND (backend), FALSE);

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (klass->pre_parse)
    return klass->pre_parse (backend, error);

  return TRUE;
}

gboolean
_clutter_backend_post_parse (ClutterBackend  *backend,
                             GError         **error)
{
  ClutterBackendClass *klass;

  g_return_val_if_fail (CLUTTER_IS_BACKEND (backend), FALSE);

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (klass->post_parse)
    return klass->post_parse (backend, error);

  return TRUE;
}

ClutterStageWindow *
_clutter_backend_create_stage (ClutterBackend  *backend,
                               ClutterStage    *wrapper,
                               GError         **error)
{
  ClutterBackendClass *klass;
  ClutterStageManager *stage_manager;
  ClutterStageWindow *stage_window;

  g_return_val_if_fail (CLUTTER_IS_BACKEND (backend), NULL);
  g_return_val_if_fail (CLUTTER_IS_STAGE (wrapper), NULL);

  stage_manager = clutter_stage_manager_get_default ();

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (klass->create_stage != NULL)
    stage_window = klass->create_stage (backend, wrapper, error);
  else
    stage_window = NULL;

  if (stage_window == NULL)
    return NULL;

  g_assert (CLUTTER_IS_STAGE_WINDOW (stage_window));
  _clutter_stage_set_window (wrapper, stage_window);
  _clutter_stage_manager_add_stage (stage_manager, wrapper);

  return stage_window;
}

void
_clutter_backend_redraw (ClutterBackend *backend,
                         ClutterStage   *stage)
{
  ClutterBackendClass *klass;
  CLUTTER_STATIC_COUNTER (redraw_counter,
                          "_clutter_backend_redraw counter",
                          "Increments for each _clutter_backend_redraw call",
                          0 /* no application private data */);
  CLUTTER_STATIC_TIMER (redraw_timer,
                        "Master Clock", /* parent */
                        "Redrawing",
                        "The time spent redrawing everything",
                        0 /* no application private data */);

  CLUTTER_COUNTER_INC (_clutter_uprof_context, redraw_counter);
  CLUTTER_TIMER_START (_clutter_uprof_context, redraw_timer);

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (G_LIKELY (klass->redraw))
    klass->redraw (backend, stage);

  CLUTTER_TIMER_STOP (_clutter_uprof_context, redraw_timer);
}

gboolean
_clutter_backend_create_context (ClutterBackend  *backend,
                                 GError         **error)
{
  ClutterBackendClass *klass;

  g_return_val_if_fail (CLUTTER_IS_BACKEND (backend), FALSE);

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (klass->create_context)
    return klass->create_context (backend, error);

  return TRUE;
}

void
_clutter_backend_ensure_context_internal (ClutterBackend  *backend,
                                          ClutterStage    *stage)
{
  ClutterBackendClass *klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (G_LIKELY (klass->ensure_context))
    klass->ensure_context (backend, stage);
}

void
_clutter_backend_ensure_context (ClutterBackend *backend,
                                 ClutterStage   *stage)
{
  static ClutterStage *current_context_stage = NULL;

  g_return_if_fail (CLUTTER_IS_BACKEND (backend));
  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  if (current_context_stage != stage || !CLUTTER_ACTOR_IS_REALIZED (stage))
    {
      ClutterStage *new_stage = NULL;

      if (!CLUTTER_ACTOR_IS_REALIZED (stage))
        {
          new_stage = NULL;

          CLUTTER_NOTE (MULTISTAGE,
                        "Stage [%p] is not realized, unsetting the stage",
                        stage);
        }
      else
        {
          new_stage = stage;

          CLUTTER_NOTE (MULTISTAGE,
                        "Setting the new stage [%p]",
                        new_stage);
        }

      _clutter_backend_ensure_context_internal (backend, new_stage);

      /* XXX: Until Cogl becomes fully responsible for backend windows
       * Clutter need to manually keep it informed of the current window size
       *
       * NB: This must be done after we ensure_context above because Cogl
       * always assumes there is a current GL context.
       */
      if (new_stage)
        {
          float width, height;

          clutter_actor_get_size (CLUTTER_ACTOR (stage), &width, &height);

          _cogl_onscreen_clutter_backend_set_size (width, height);
        }

      /* FIXME: With a NULL stage and thus no active context it may make more
       * sense to clean the context but then re call with the default stage 
       * so at least there is some kind of context in place (as to avoid
       * potential issue of GL calls with no context)
       */
      current_context_stage = new_stage;

      /* if the new stage has a different size than the previous one
       * we need to update the viewport; we do it by simply setting the
       * SYNC_MATRICES flag and letting the next redraw cycle take care
       * of calling glViewport()
       */
      if (current_context_stage)
        {
          CLUTTER_SET_PRIVATE_FLAGS (current_context_stage,
                                     CLUTTER_SYNC_MATRICES);
        }
    }
  else
    CLUTTER_NOTE (MULTISTAGE, "Stage is the same");
}


ClutterFeatureFlags
_clutter_backend_get_features (ClutterBackend *backend)
{
  ClutterBackendClass *klass;
  GError *error;

  g_return_val_if_fail (CLUTTER_IS_BACKEND (backend), 0);

  klass = CLUTTER_BACKEND_GET_CLASS (backend);

  /* we need to have a context here; so we create the
   * GL context first and the ask for features. if the
   * context already exists this should be a no-op
   */
  error = NULL;
  if (klass->create_context)
    {
      gboolean res;

      res = klass->create_context (backend, &error);
      if (!res)
        {
          if (error)
            {
              g_critical ("Unable to create a context: %s", error->message);
              g_error_free (error);
            }
          else
            g_critical ("Unable to create a context: unknown error");

          return 0;
        }
    }

  if (klass->get_features)
    return klass->get_features (backend);
  
  return 0;
}

void
_clutter_backend_init_events (ClutterBackend *backend)
{
  ClutterBackendClass *klass;
  ClutterMainContext  *clutter_context;

  clutter_context = _clutter_context_get_default ();

  g_return_if_fail (CLUTTER_IS_BACKEND (backend));
  g_return_if_fail (clutter_context != NULL);

  clutter_context->events_queue = g_queue_new ();

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (klass->init_events)
    klass->init_events (backend);
}

gfloat
_clutter_backend_get_units_per_em (ClutterBackend       *backend,
                                   PangoFontDescription *font_desc)
{
  ClutterBackendPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_BACKEND (backend), 0);

  priv = backend->priv;

  /* recompute for the font description, but do not cache the result */
  if (font_desc != NULL)
    return get_units_per_em (backend, font_desc);

  if (priv->units_per_em < 0)
    priv->units_per_em = get_units_per_em (backend, NULL);

  return priv->units_per_em;
}

void
_clutter_backend_copy_event_data (ClutterBackend *backend,
                                  ClutterEvent   *src,
                                  ClutterEvent   *dest)
{
  ClutterBackendClass *klass;

  g_return_if_fail (CLUTTER_IS_BACKEND (backend));
  g_return_if_fail (src != NULL);
  g_return_if_fail (dest != NULL);

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (klass->copy_event_data != NULL)
    klass->copy_event_data (backend, src, dest);
}

void
_clutter_backend_free_event_data (ClutterBackend *backend,
                                  ClutterEvent   *event)
{
  ClutterBackendClass *klass;

  g_return_if_fail (CLUTTER_IS_BACKEND (backend));
  g_return_if_fail (event != NULL);

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (klass->free_event_data != NULL)
    klass->free_event_data (backend, event);
}

/**
 * clutter_get_default_backend:
 *
 * Retrieves the default #ClutterBackend used by Clutter. The
 * #ClutterBackend holds backend-specific configuration options.
 *
 * Return value: (transfer none): the default backend. You should
 *   not ref or unref the returned object. Applications should rarely
 *   need to use this.
 *
 * Since: 0.4
 */
ClutterBackend *
clutter_get_default_backend (void)
{
  ClutterMainContext *clutter_context;

  clutter_context = _clutter_context_get_default ();

  return clutter_context->backend;
}

/* FIXME: below should probably be moved into clutter_main */

/**
 * clutter_backend_set_double_click_time:
 * @backend: a #ClutterBackend
 * @msec: milliseconds between two button press events
 *
 * Sets the maximum time between two button press events, used to
 * verify whether it's a double click event or not.
 *
 * Since: 0.4
 *
 * Deprecated: 1.4: Use #ClutterSettings:double-click-time instead
 */
void
clutter_backend_set_double_click_time (ClutterBackend *backend,
                                       guint           msec)
{
  ClutterSettings *settings = clutter_settings_get_default ();

  g_object_set (settings, "double-click-time", msec, NULL);
}

/**
 * clutter_backend_get_double_click_time:
 * @backend: a #ClutterBackend
 *
 * Gets the maximum time between two button press events, as set
 * by clutter_backend_set_double_click_time().
 *
 * Return value: a time in milliseconds
 *
 * Since: 0.4
 *
 * Deprecated: 1.4: Use #ClutterSettings:double-click-time instead
 */
guint
clutter_backend_get_double_click_time (ClutterBackend *backend)
{
  ClutterSettings *settings = clutter_settings_get_default ();
  gint retval;

  g_object_get (settings, "double-click-time", &retval, NULL);

  return retval;
}

/**
 * clutter_backend_set_double_click_distance:
 * @backend: a #ClutterBackend
 * @distance: a distance, in pixels
 *
 * Sets the maximum distance used to verify a double click event.
 *
 * Since: 0.4
 *
 * Deprecated: 1.4: Use #ClutterSettings:double-click-distance instead
 */
void
clutter_backend_set_double_click_distance (ClutterBackend *backend,
                                           guint           distance)
{
  ClutterSettings *settings = clutter_settings_get_default ();

  g_object_set (settings, "double-click-distance", distance, NULL);
}

/**
 * clutter_backend_get_double_click_distance:
 * @backend: a #ClutterBackend
 *
 * Retrieves the distance used to verify a double click event
 *
 * Return value: a distance, in pixels.
 *
 * Since: 0.4
 *
 * Deprecated: 1.4: Use #ClutterSettings:double-click-distance instead
 */
guint
clutter_backend_get_double_click_distance (ClutterBackend *backend)
{
  ClutterSettings *settings = clutter_settings_get_default ();
  gint retval;

  g_object_get (settings, "double-click-distance", &retval, NULL);

  return retval;
}

/**
 * clutter_backend_set_resolution:
 * @backend: a #ClutterBackend
 * @dpi: the resolution in "dots per inch" (Physical inches aren't
 *   actually involved; the terminology is conventional).
 *
 * Sets the resolution for font handling on the screen. This is a
 * scale factor between points specified in a #PangoFontDescription
 * and cairo units. The default value is 96, meaning that a 10 point
 * font will be 13 units high. (10 * 96. / 72. = 13.3).
 *
 * Applications should never need to call this function.
 *
 * Since: 0.4
 *
 * Deprecated: Use #ClutterSettings:font-dpi instead
 */
void
clutter_backend_set_resolution (ClutterBackend *backend,
                                gdouble         dpi)
{
  ClutterSettings *settings;
  gint resolution;

  g_return_if_fail (CLUTTER_IS_BACKEND (backend));

  if (dpi < 0)
    resolution = -1;
  else
    resolution = dpi * 1024;

  settings = clutter_settings_get_default ();
  g_object_set (settings, "font-dpi", resolution, NULL);
}

/**
 * clutter_backend_get_resolution:
 * @backend: a #ClutterBackend
 *
 * Gets the resolution for font handling on the screen.
 *
 * The resolution is a scale factor between points specified in a
 * #PangoFontDescription and cairo units. The default value is 96.0,
 * meaning that a 10 point font will be 13 units
 * high (10 * 96. / 72. = 13.3).
 *
 * Clutter will set the resolution using the current backend when
 * initializing; the resolution is also stored in the
 * #ClutterSettings:font-dpi property.
 *
 * Return value: the current resolution, or -1 if no resolution
 *   has been set.
 *
 * Since: 0.4
 */
gdouble
clutter_backend_get_resolution (ClutterBackend *backend)
{
  ClutterSettings *settings;
  gint resolution;

  g_return_val_if_fail (CLUTTER_IS_BACKEND (backend), -1.0);

  settings = clutter_settings_get_default ();
  g_object_get (settings, "font-dpi", &resolution, NULL);

  return resolution / 1024.0;
}

/**
 * clutter_backend_set_font_options:
 * @backend: a #ClutterBackend
 * @options: Cairo font options for the backend, or %NULL
 *
 * Sets the new font options for @backend. The #ClutterBackend will
 * copy the #cairo_font_options_t.
 *
 * If @options is %NULL, the first following call to
 * clutter_backend_get_font_options() will return the default font
 * options for @backend.
 *
 * This function is intended for actors creating a Pango layout
 * using the PangoCairo API.
 *
 * Since: 0.8
 */
void
clutter_backend_set_font_options (ClutterBackend             *backend,
                                  const cairo_font_options_t *options)
{
  ClutterBackendPrivate *priv;

  g_return_if_fail (CLUTTER_IS_BACKEND (backend));

  priv = backend->priv;

  if (priv->font_options != options)
    {
      if (priv->font_options)
        cairo_font_options_destroy (priv->font_options);

      if (options)
        priv->font_options = cairo_font_options_copy (options);
      else
        priv->font_options = NULL;

      g_signal_emit (backend, backend_signals[FONT_CHANGED], 0);
    }
}

/**
 * clutter_backend_get_font_options:
 * @backend: a #ClutterBackend
 *
 * Retrieves the font options for @backend.
 *
 * Return value: (transfer none): the font options of the #ClutterBackend.
 *   The returned #cairo_font_options_t is owned by the backend and should
 *   not be modified or freed
 *
 * Since: 0.8
 */
const cairo_font_options_t *
clutter_backend_get_font_options (ClutterBackend *backend)
{
  ClutterBackendPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_BACKEND (backend), NULL);

  priv = backend->priv;

  if (G_LIKELY (priv->font_options))
    return priv->font_options;

  priv->font_options = cairo_font_options_create ();

  cairo_font_options_set_hint_style (priv->font_options,
                                     CAIRO_HINT_STYLE_NONE);
  cairo_font_options_set_subpixel_order (priv->font_options,
                                         CAIRO_SUBPIXEL_ORDER_DEFAULT);
  cairo_font_options_set_antialias (priv->font_options,
                                    CAIRO_ANTIALIAS_DEFAULT);

  g_signal_emit (backend, backend_signals[FONT_CHANGED], 0);

  return priv->font_options;
}

/**
 * clutter_backend_set_font_name:
 * @backend: a #ClutterBackend
 * @font_name: the name of the font
 *
 * Sets the default font to be used by Clutter. The @font_name string
 * must either be %NULL, which means that the font name from the
 * default #ClutterBackend will be used; or be something that can
 * be parsed by the pango_font_description_from_string() function.
 *
 * Since: 1.0
 *
 * Deprecated: 1.4: Use #ClutterSettings:font-name instead
 */
void
clutter_backend_set_font_name (ClutterBackend *backend,
                               const gchar    *font_name)
{
  ClutterSettings *settings = clutter_settings_get_default ();

  g_object_set (settings, "font-name", font_name, NULL);
}

/**
 * clutter_backend_get_font_name:
 * @backend: a #ClutterBackend
 *
 * Retrieves the default font name as set by
 * clutter_backend_set_font_name().
 *
 * Return value: the font name for the backend. The returned string is
 *   owned by the #ClutterBackend and should never be modified or freed
 *
 * Since: 1.0
 *
 * Deprecated: 1.4: Use #ClutterSettings:font-name instead
 */
G_CONST_RETURN gchar *
clutter_backend_get_font_name (ClutterBackend *backend)
{
  ClutterBackendPrivate *priv;
  ClutterSettings *settings;

  g_return_val_if_fail (CLUTTER_IS_BACKEND (backend), NULL);

  priv = backend->priv;

  settings = clutter_settings_get_default ();

  /* XXX yuck. but we return a const pointer, so we need to
   * store it in the backend
   */
  g_free (priv->font_name);
  g_object_get (settings, "font-name", &priv->font_name, NULL);

  return priv->font_name;
}

gint32
_clutter_backend_get_units_serial (ClutterBackend *backend)
{
  g_return_val_if_fail (CLUTTER_IS_BACKEND (backend), 0);

  return backend->priv->units_serial;
}
