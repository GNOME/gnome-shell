/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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

G_DEFINE_ABSTRACT_TYPE (ClutterBackend, clutter_backend, G_TYPE_OBJECT);

#define DEFAULT_FONT_NAME       "Sans 10"

#define CLUTTER_BACKEND_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_BACKEND, ClutterBackendPrivate))

struct _ClutterBackendPrivate
{
  /* settings */
  guint double_click_time;
  guint double_click_distance;

  ClutterFixed resolution;

  cairo_font_options_t *font_options;

  gchar *font_name;
};

enum
{
  RESOLUTION_CHANGED,
  FONT_CHANGED,

  LAST_SIGNAL
};

static guint backend_signals[LAST_SIGNAL] = { 0, };

static void
clutter_backend_dispose (GObject *gobject)
{
  ClutterBackendPrivate *priv = CLUTTER_BACKEND (gobject)->priv;
  ClutterMainContext *clutter_context;

  clutter_context = clutter_context_get_default ();

  if (clutter_context && clutter_context->events_queue)
    {
      g_queue_foreach (clutter_context->events_queue, (GFunc) clutter_event_free, NULL);
      g_queue_free (clutter_context->events_queue);
      clutter_context->events_queue = NULL;
    }

  g_free (priv->font_name);

  clutter_backend_set_font_options (CLUTTER_BACKEND (gobject), NULL);

  G_OBJECT_CLASS (clutter_backend_parent_class)->dispose (gobject);
}

static void
clutter_backend_class_init (ClutterBackendClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = clutter_backend_dispose;

  g_type_class_add_private (gobject_class, sizeof (ClutterBackendPrivate));

  backend_signals[RESOLUTION_CHANGED] =
    g_signal_new (I_("resolution-changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterBackendClass, resolution_changed),
                  NULL, NULL,
                  clutter_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  backend_signals[FONT_CHANGED] =
    g_signal_new (I_("font-changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ClutterBackendClass, font_changed),
                  NULL, NULL,
                  clutter_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static void
clutter_backend_init (ClutterBackend *backend)
{
  ClutterBackendPrivate *priv;

  priv = backend->priv = CLUTTER_BACKEND_GET_PRIVATE(backend);
  priv->resolution = -1.0;
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

ClutterActor *
_clutter_backend_create_stage (ClutterBackend  *backend,
                               ClutterStage    *wrapper,
                               GError         **error)
{
  ClutterMainContext  *context;
  ClutterBackendClass *klass;
  ClutterActor        *stage = NULL;

  g_return_val_if_fail (CLUTTER_IS_BACKEND (backend), FALSE);
  g_return_val_if_fail (CLUTTER_IS_STAGE (wrapper), FALSE);

  context = clutter_context_get_default ();

  if (!context->stage_manager)
    context->stage_manager = clutter_stage_manager_get_default ();

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (klass->create_stage)
    stage = klass->create_stage (backend, wrapper, error);

  if (!stage)
    return NULL;

  g_assert (CLUTTER_IS_STAGE_WINDOW (stage));
  _clutter_stage_set_window (wrapper, CLUTTER_STAGE_WINDOW (stage));
  _clutter_stage_manager_add_stage (context->stage_manager, wrapper);

  return stage;
}

void
_clutter_backend_redraw (ClutterBackend *backend,
                         ClutterStage   *stage)
{
  ClutterBackendClass *klass;

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (G_LIKELY (klass->redraw))
    klass->redraw (backend, stage);
}

void
_clutter_backend_ensure_context (ClutterBackend *backend,
                                 ClutterStage   *stage)
{
  ClutterBackendClass *klass;
  static ClutterStage *current_context_stage = NULL;

  g_return_if_fail (CLUTTER_IS_BACKEND (backend));
  g_return_if_fail (CLUTTER_IS_STAGE (stage));

  if (current_context_stage != stage || !CLUTTER_ACTOR_IS_REALIZED (stage))
    {
      if (!CLUTTER_ACTOR_IS_REALIZED (stage))
        {
          CLUTTER_NOTE (MULTISTAGE, "Stage is not realized, unsetting");
          stage = NULL;
        }
      else
        CLUTTER_NOTE (MULTISTAGE, "Setting the new stage [%p]", stage);
      
      klass = CLUTTER_BACKEND_GET_CLASS (backend);
      if (G_LIKELY (klass->ensure_context))
        klass->ensure_context (backend, stage);
      
      /* FIXME: With a NULL stage and thus no active context it may make more
       * sense to clean the context but then re call with the default stage 
       * so at least there is some kind of context in place (as to avoid
       * potential issue of GL calls with no context)
       */
      current_context_stage = stage;
      
      if (stage)
        CLUTTER_SET_PRIVATE_FLAGS (stage, CLUTTER_ACTOR_SYNC_MATRICES);
    }
  else
    CLUTTER_NOTE (MULTISTAGE, "Stage is the same");
}


ClutterFeatureFlags
_clutter_backend_get_features (ClutterBackend *backend)
{
  ClutterBackendClass *klass;

  g_return_val_if_fail (CLUTTER_IS_BACKEND (backend), 0);

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (klass->get_features)
    return klass->get_features (backend);
  
  return 0;
}

void
_clutter_backend_init_events (ClutterBackend *backend)
{
  ClutterBackendClass *klass;
  ClutterMainContext  *clutter_context;

  clutter_context = clutter_context_get_default ();

  g_return_if_fail (CLUTTER_IS_BACKEND (backend));
  g_return_if_fail (clutter_context != NULL);

  clutter_context->events_queue = g_queue_new ();

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (klass->init_events)
    klass->init_events (backend);
}


/**
 * clutter_get_default_backend:
 *
 * FIXME
 *
 * Return value: the default backend. You should not ref or
 * unref the returned object. Applications should not rarely need
 * to use this.
 *
 * Since: 0.4
 */
ClutterBackend *
clutter_get_default_backend (void)
{
  ClutterMainContext *clutter_context;

  clutter_context = clutter_context_get_default ();

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
 */
void
clutter_backend_set_double_click_time (ClutterBackend *backend,
                                       guint           msec)
{
  g_return_if_fail (CLUTTER_IS_BACKEND (backend));

  backend->priv->double_click_time = msec;
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
 */
guint
clutter_backend_get_double_click_time (ClutterBackend *backend)
{
  g_return_val_if_fail (CLUTTER_IS_BACKEND (backend), 0);

  return backend->priv->double_click_time;
}

/**
 * clutter_backend_set_double_click_distance:
 * @backend: a #ClutterBackend
 * @distance: a distance, in pixels
 *
 * Sets the maximum distance used to verify a double click event.
 *
 * Since: 0.4
 */
void
clutter_backend_set_double_click_distance (ClutterBackend *backend,
                                           guint           distance)
{
  g_return_if_fail (CLUTTER_IS_BACKEND (backend));
  
  backend->priv->double_click_distance = distance;
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
 */
guint
clutter_backend_get_double_click_distance (ClutterBackend *backend)
{
  g_return_val_if_fail (CLUTTER_IS_BACKEND (backend), 0);

  return backend->priv->double_click_distance;
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
 */
void
clutter_backend_set_resolution (ClutterBackend *backend,
                                gdouble         dpi)
{
  ClutterFixed fixed_dpi;
  ClutterBackendPrivate *priv;

  g_return_if_fail (CLUTTER_IS_BACKEND (backend));

  if (dpi < 0)
    dpi = -1.0;

  priv = backend->priv;

  fixed_dpi = COGL_FIXED_FROM_FLOAT (dpi);
  if (priv->resolution != fixed_dpi)
    priv->resolution = fixed_dpi;

  if (CLUTTER_CONTEXT ()->font_map)
    cogl_pango_font_map_set_resolution (CLUTTER_CONTEXT ()->font_map, dpi);

  g_signal_emit (backend, backend_signals[RESOLUTION_CHANGED], 0);
}

/**
 * clutter_backend_get_resolution:
 * @backend: a #ClutterBackend
 *
 * Gets the resolution for font handling on the screen; see
 * clutter_backend_set_resolution() for full details.
 * 
 * Return value: the current resolution, or -1 if no resolution
 *   has been set.
 *
 * Since: 0.4
 */
gdouble
clutter_backend_get_resolution (ClutterBackend *backend)
{
  g_return_val_if_fail (CLUTTER_IS_BACKEND (backend), -1.0);

  return COGL_FIXED_TO_FLOAT (backend->priv->resolution);
}

/**
 * clutter_backend_set_font_options:
 * @backend: a #ClutterBackend
 * @options: Cairo font options for the backend, or %NULL
 *
 * Sets the new font options for @backend. If @options is %NULL,
 * the first following call to clutter_backend_get_font_options()
 * will return the default font options for @backend.
 *
 * This function is intended for actors creating a Pango layout
 * using the PangoCairo API.
 *
 * Since: 0.8
 */
void
clutter_backend_set_font_options (ClutterBackend       *backend,
                                  cairo_font_options_t *options)
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
 * Return value: the font options of the #ClutterBackend
 *
 * Since: 0.8
 */
cairo_font_options_t *
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
 */
void
clutter_backend_set_font_name (ClutterBackend *backend,
                               const gchar    *font_name)
{
  ClutterBackendPrivate *priv;

  g_return_if_fail (CLUTTER_IS_BACKEND (backend));

  priv = backend->priv;

  g_free (priv->font_name);

  if (font_name == NULL || *font_name == '\0')
    priv->font_name = g_strdup (DEFAULT_FONT_NAME);
  else
    priv->font_name = g_strdup (font_name);

  g_signal_emit (backend, backend_signals[FONT_CHANGED], 0);
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
 */
G_CONST_RETURN gchar *
clutter_backend_get_font_name (ClutterBackend *backend)
{
  ClutterBackendPrivate *priv;

  g_return_val_if_fail (CLUTTER_IS_BACKEND (backend), NULL);

  priv = backend->priv;

  if (G_LIKELY (priv->font_name))
    return priv->font_name;

  priv->font_name = g_strdup (DEFAULT_FONT_NAME);

  return priv->font_name;
}
