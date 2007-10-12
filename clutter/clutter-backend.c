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

#include "clutter-fixed.h"
#include "clutter-backend.h"
#include "clutter-private.h"

G_DEFINE_ABSTRACT_TYPE (ClutterBackend, clutter_backend, G_TYPE_OBJECT);

#define CLUTTER_BACKEND_GET_PRIVATE(obj) \
(G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLUTTER_TYPE_BACKEND, ClutterBackendPrivate))

struct _ClutterBackendPrivate
{
  /* settings */
  guint double_click_time;
  guint double_click_distance;

  ClutterFixed resolution;
};

static void
clutter_backend_dispose (GObject *gobject)
{
  ClutterMainContext *clutter_context;

  clutter_context = clutter_context_get_default ();

  if (clutter_context && clutter_context->events_queue)
    {
      g_queue_foreach (clutter_context->events_queue, (GFunc) clutter_event_free, NULL);
      g_queue_free (clutter_context->events_queue);
      clutter_context->events_queue = NULL;
    }

  G_OBJECT_CLASS (clutter_backend_parent_class)->dispose (gobject);
}

static void
clutter_backend_class_init (ClutterBackendClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = clutter_backend_dispose;

  g_type_class_add_private (gobject_class, sizeof (ClutterBackendPrivate));
}

static void
clutter_backend_init (ClutterBackend *backend)
{
  ClutterBackendPrivate *priv;

  priv = backend->priv = CLUTTER_BACKEND_GET_PRIVATE(backend);
  priv->resolution = -1.0;
}

ClutterActor *
_clutter_backend_get_stage (ClutterBackend *backend)
{
  g_return_val_if_fail (CLUTTER_IS_BACKEND (backend), NULL);

  return CLUTTER_BACKEND_GET_CLASS (backend)->get_stage (backend);
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

gboolean
_clutter_backend_init_stage (ClutterBackend  *backend,
                             GError         **error)
{
  ClutterBackendClass *klass;

  g_return_val_if_fail (CLUTTER_IS_BACKEND (backend), FALSE);

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (klass->init_stage)
    return klass->init_stage (backend, error);

  return TRUE;
}

void
_clutter_backend_redraw (ClutterBackend *backend)
{
  ClutterBackendClass *klass;

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (G_LIKELY(klass->redraw))
    klass->redraw (backend);
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

  fixed_dpi = CLUTTER_FLOAT_TO_FIXED (dpi);
  if (priv->resolution != fixed_dpi)
    priv->resolution = fixed_dpi;
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

  return CLUTTER_FIXED_TO_FLOAT (backend->priv->resolution);
}
