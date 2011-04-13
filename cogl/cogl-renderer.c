/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl.h"
#include "cogl-internal.h"
#include "cogl-object.h"

#include "cogl-renderer.h"
#include "cogl-renderer-private.h"
#include "cogl-display-private.h"
#include "cogl-winsys-private.h"

static void _cogl_renderer_free (CoglRenderer *renderer);

COGL_OBJECT_DEFINE (Renderer, renderer);

typedef struct _CoglNativeFilterClosure
{
  CoglNativeFilterFunc func;
  void *data;
} CoglNativeFilterClosure;

GQuark
cogl_renderer_error_quark (void)
{
  return g_quark_from_static_string ("cogl-renderer-error-quark");
}

static void
native_filter_closure_free (CoglNativeFilterClosure *closure)
{
  g_slice_free (CoglNativeFilterClosure, closure);
}

static void
_cogl_renderer_free (CoglRenderer *renderer)
{
#ifdef COGL_HAS_FULL_WINSYS
  _cogl_winsys_renderer_disconnect (renderer);
#endif

  g_slist_foreach (renderer->event_filters,
                   (GFunc) native_filter_closure_free,
                   NULL);
  g_slist_free (renderer->event_filters);

  g_free (renderer);
}

CoglRenderer *
cogl_renderer_new (void)
{
  CoglRenderer *renderer = g_new0 (CoglRenderer, 1);

  renderer->connected = FALSE;
  renderer->event_filters = NULL;

  return _cogl_renderer_object_new (renderer);
}

#if COGL_HAS_XLIB_SUPPORT
void
cogl_renderer_xlib_set_foreign_display (CoglRenderer *renderer,
                                        Display *xdisplay)
{
  g_return_if_fail (cogl_is_renderer (renderer));

  /* NB: Renderers are considered immutable once connected */
  g_return_if_fail (!renderer->connected);

  renderer->foreign_xdpy = xdisplay;
}

Display *
cogl_renderer_xlib_get_foreign_display (CoglRenderer *renderer)
{
  g_return_val_if_fail (cogl_is_renderer (renderer), NULL);

  return renderer->foreign_xdpy;
}
#endif /* COGL_HAS_XLIB_SUPPORT */

gboolean
cogl_renderer_check_onscreen_template (CoglRenderer *renderer,
                                       CoglOnscreenTemplate *onscreen_template,
                                       GError **error)
{
#ifdef COGL_HAS_FULL_WINSYS
  CoglDisplay *display;

  if (!_cogl_winsys_renderer_connect (renderer, error))
    return FALSE;

  display = cogl_display_new (renderer, onscreen_template);
  if (!cogl_display_setup (display, error))
    {
      cogl_object_unref (display);
      return FALSE;
    }

  cogl_object_unref (display);
#endif
  return TRUE;
}

/* Final connection API */

gboolean
cogl_renderer_connect (CoglRenderer *renderer, GError **error)
{
  if (renderer->connected)
    return TRUE;

#ifdef COGL_HAS_FULL_WINSYS
  if (!_cogl_winsys_renderer_connect (renderer, error))
    return FALSE;
#endif

  renderer->connected = TRUE;
  return TRUE;
}

CoglFilterReturn
cogl_renderer_handle_native_event (CoglRenderer *renderer,
                                   void *event)
{
  GSList *l, *next;

  /* Pass the event on to all of the registered filters in turn */
  for (l = renderer->event_filters; l; l = next)
    {
      CoglNativeFilterClosure *closure = l->data;

      /* The next pointer is taken now so that we can handle the
         closure being removed during emission */
      next = l->next;

      if (closure->func (event, closure->data) == COGL_FILTER_REMOVE)
        return COGL_FILTER_REMOVE;
    }

  /* If the backend for the renderer also wants to see the events, it
     should just register its own filter */

  return COGL_FILTER_CONTINUE;
}

void
cogl_renderer_add_native_filter (CoglRenderer *renderer,
                                 CoglNativeFilterFunc func,
                                 void *data)
{
  CoglNativeFilterClosure *closure;

  closure = g_slice_new (CoglNativeFilterClosure);
  closure->func = func;
  closure->data = data;

  renderer->event_filters = g_slist_prepend (renderer->event_filters, closure);
}

void
cogl_renderer_remove_native_filter (CoglRenderer *renderer,
                                    CoglNativeFilterFunc func,
                                    void *data)
{
  GSList *l, *prev = NULL;

  for (l = renderer->event_filters; l; prev = l, l = l->next)
    {
      CoglNativeFilterClosure *closure = l->data;

      if (closure->func == func && closure->data == data)
        {
          native_filter_closure_free (closure);
          if (prev)
            prev->next = g_slist_delete_link (prev->next, l);
          else
            renderer->event_filters =
              g_slist_delete_link (renderer->event_filters, l);
          break;
        }
    }
}
