/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2014 Canonical Ltd.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 * Authors:
 *   Marco Trevisan <marco.trevisan@canonical.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <mir_toolkit/mir_client_library.h>
#include <string.h>

#include "cogl-winsys-egl-mir-private.h"
#include "cogl-winsys-egl-private.h"
#include "cogl-renderer-private.h"
#include "cogl-onscreen-private.h"
#include "cogl-output-private.h"
#include "cogl-mir-renderer.h"
#include "cogl-error-private.h"

static const CoglWinsysEGLVtable _cogl_winsys_egl_vtable;

static const CoglWinsysVtable *parent_vtable;

typedef struct _CoglRendererMir
{
  MirConnection *mir_connection;
} CoglRendererMir;

typedef struct _CoglDisplayMir
{
  MirSurface *dummy_mir_surface;
} CoglDisplayMir;

typedef struct _CoglOnscreenMir
{
  MirSurface *mir_surface;
  MirSurfaceState last_state;

  CoglBool requested_resize;
  int requested_width;
  int requested_height;
  GMutex mir_event_lock;
} CoglOnscreenMir;


static MirPixelFormat
_mir_connection_get_valid_format (MirConnection *connection)
{
  MirPixelFormat formats[mir_pixel_formats];
  uint32_t valid_formats;
  uint32_t f;

  mir_connection_get_available_surface_formats(connection, formats,
                                               mir_pixel_formats,
                                               &valid_formats);

  for (f = 0; f < valid_formats; f++)
    {
      switch (formats[f])
        {
          case mir_pixel_format_abgr_8888:
          case mir_pixel_format_xbgr_8888:
          case mir_pixel_format_argb_8888:
          case mir_pixel_format_xrgb_8888:
            return formats[f];
          default:
            continue;
        }
  }

  return mir_pixel_format_invalid;
}

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererMir *mir_renderer = egl_renderer->platform;

  if (egl_renderer->edpy)
    eglTerminate (egl_renderer->edpy);

  if (mir_connection_is_valid (mir_renderer->mir_connection))
    {
      if (!mir_connection_is_valid (renderer->foreign_mir_connection))
      {
        mir_connection_set_display_config_change_callback (mir_renderer->mir_connection,
                                                           NULL, NULL);
        mir_connection_release (mir_renderer->mir_connection);
      }
    }

  g_list_free_full (renderer->outputs, (GDestroyNotify)cogl_object_unref);
  renderer->outputs = NULL;

  g_slice_free (CoglRendererMir, egl_renderer->platform);
  g_slice_free (CoglRendererEGL, egl_renderer);
}

static gchar *
_mir_output_get_name (MirDisplayOutput *output)
{
  g_return_val_if_fail (output, NULL);

  switch (output->type)
    {
      case mir_display_output_type_unknown:
        return g_strdup_printf ("None-%u", output->output_id);
      case mir_display_output_type_vga:
        return g_strdup_printf ("VGA-%u", output->output_id);
      case mir_display_output_type_dvii:
      case mir_display_output_type_dvid:
      case mir_display_output_type_dvia:
        return g_strdup_printf ("DVI-%u", output->output_id);
      case mir_display_output_type_composite:
        return g_strdup_printf ("Composite-%u", output->output_id);
      case mir_display_output_type_lvds:
        return g_strdup_printf ("LVDS-%u", output->output_id);
      case mir_display_output_type_component:
        return g_strdup_printf ("CTV-%u", output->output_id);
      case mir_display_output_type_ninepindin:
        return g_strdup_printf ("DIN-%u", output->output_id);
      case mir_display_output_type_displayport:
        return g_strdup_printf ("DP-%u", output->output_id);
      case mir_display_output_type_hdmia:
      case mir_display_output_type_hdmib:
        return g_strdup_printf ("HDMI-%u", output->output_id);
      case mir_display_output_type_svideo:
      case mir_display_output_type_tv:
        return g_strdup_printf ("TV-%u", output->output_id);
      case mir_display_output_type_edp:
        return g_strdup_printf ("eDP-%u", output->output_id);
    }

  return NULL;
}

static void
_mir_update_outputs (CoglRenderer *renderer)
{
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererMir *mir_renderer = egl_renderer->platform;
  MirDisplayConfiguration *dpy_config;
  int i;

  g_list_free_full (renderer->outputs, (GDestroyNotify)cogl_object_unref);
  renderer->outputs = NULL;

  dpy_config = mir_connection_create_display_config (mir_renderer->mir_connection);

  for (i = dpy_config->num_outputs-1; i >= 0 ; i--)
    {
      MirDisplayOutput *o = &dpy_config->outputs[i];
      MirDisplayMode *mode;
      CoglOutput *output;
      gchar *output_name;

      if (!o->used)
        continue;

      output_name = _mir_output_get_name (o);
      mode = &o->modes[o->current_mode];

      output = _cogl_output_new (output_name);
      output->x = o->position_x;
      output->y = o->position_y;
      output->width = mode->horizontal_resolution;
      output->height = mode->vertical_resolution;
      output->mm_width = o->physical_width_mm;
      output->mm_height = o->physical_height_mm;
      output->refresh_rate = mode->refresh_rate;

      /* FIXME, mir does not support this yet */
      output->subpixel_order = COGL_SUBPIXEL_ORDER_UNKNOWN;

      renderer->outputs = g_list_prepend (renderer->outputs, output);

      g_free (output_name);
    }

  mir_display_config_destroy (dpy_config);
}

static void
_mir_display_config_changed_cb(MirConnection* connection, void* data)
{
  CoglRenderer *renderer = data;
  const CoglWinsysVtable *winsys = renderer->winsys_vtable;

  _mir_update_outputs (renderer);

  if (winsys->renderer_outputs_changed != NULL)
    winsys->renderer_outputs_changed (renderer);
}

static CoglBool
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               CoglError **error)
{
  CoglRendererEGL *egl_renderer;
  CoglRendererMir *mir_renderer;
  MirEGLNativeDisplayType mir_native_dpy;

  renderer->winsys = g_slice_new0 (CoglRendererEGL);
  egl_renderer = renderer->winsys;
  mir_renderer = g_slice_new0 (CoglRendererMir);
  egl_renderer->platform = mir_renderer;

  egl_renderer->platform_vtable = &_cogl_winsys_egl_vtable;

  if (mir_connection_is_valid (renderer->foreign_mir_connection))
    {
      mir_renderer->mir_connection = renderer->foreign_mir_connection;
    }
  else
    {
      mir_renderer->mir_connection = mir_connect_sync (NULL, "Cogl Mir Renderer");
      if (!mir_connection_is_valid (mir_renderer->mir_connection))
        {
          _cogl_set_error (error, COGL_WINSYS_ERROR,
                           COGL_WINSYS_ERROR_INIT,
                           "Failed to connect mir display: %s",
                           mir_connection_get_error_message (mir_renderer->mir_connection));
          mir_connection_release (mir_renderer->mir_connection);
          goto error;
        }
    }

  mir_native_dpy =
    mir_connection_get_egl_native_display (mir_renderer->mir_connection);
  egl_renderer->edpy =
    eglGetDisplay (mir_native_dpy);

  if (!_cogl_winsys_egl_renderer_connect_common (renderer, error))
    goto error;

  _mir_update_outputs (renderer);

  if (!mir_connection_is_valid (renderer->foreign_mir_connection))
    {
      /* FIXME: we can't add a config change callback for a foreign connection
       * or we'll block other callbacks on that */
      mir_connection_set_display_config_change_callback (mir_renderer->mir_connection,
                                                         _mir_display_config_changed_cb,
                                                         renderer);
    }

  return TRUE;

error:
  _cogl_winsys_renderer_disconnect (renderer);
  return FALSE;
}

static CoglBool
_cogl_winsys_egl_display_setup (CoglDisplay *display,
                                CoglError **error)
{
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayMir *mir_display;

  mir_display = g_slice_new0 (CoglDisplayMir);
  egl_display->platform = mir_display;

  return TRUE;
}

static void
_cogl_winsys_egl_display_destroy (CoglDisplay *display)
{
  CoglDisplayEGL *egl_display = display->winsys;

  g_slice_free (CoglDisplayMir, egl_display->platform);
}

static CoglBool
make_dummy_surface (CoglDisplay *display,
                    CoglError **error)
{
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererMir *mir_renderer = egl_renderer->platform;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayMir *mir_display = egl_display->platform;
  MirEGLNativeWindowType dummy_mir_egl_native_window;
  MirSurfaceParameters surfaceparm;
  const char *error_message;
  const char *error_detail = "";

  surfaceparm.name = "CoglDummySurface";
  surfaceparm.width = 1;
  surfaceparm.height = 1;
  surfaceparm.pixel_format = _mir_connection_get_valid_format (mir_renderer->mir_connection);
  surfaceparm.buffer_usage = mir_buffer_usage_hardware;
  surfaceparm.output_id = mir_display_output_id_invalid;

  mir_display->dummy_mir_surface =
    mir_connection_create_surface_sync (mir_renderer->mir_connection, &surfaceparm);

  if (!mir_surface_is_valid (mir_display->dummy_mir_surface))
    {
      error_message = "Failed to create a dummy mir surface";
      error_detail = mir_surface_get_error_message (mir_display->dummy_mir_surface);
      goto fail;
    }

  dummy_mir_egl_native_window =
    mir_surface_get_egl_native_window (mir_display->dummy_mir_surface);
  if (!dummy_mir_egl_native_window)
    {
      error_message = "Failed to get a dummy mir native egl surface";
      error_detail = mir_surface_get_error_message (mir_display->dummy_mir_surface);
      goto fail;
    }

  egl_display->dummy_surface =
    eglCreateWindowSurface (egl_renderer->edpy,
                            egl_display->egl_config,
                            (EGLNativeWindowType) dummy_mir_egl_native_window,
                            NULL);
  if (egl_display->dummy_surface == EGL_NO_SURFACE)
    {
      error_message = "Unable to create dummy window surface";
      goto fail;
    }

  return TRUE;

 fail:
  _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "%s%s%s", error_message,
                   error_detail != '\0' ? ": " : "",
                   error_detail);

  return FALSE;
}

static CoglBool
_cogl_winsys_egl_context_created (CoglDisplay *display,
                                  CoglError **error)
{
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglDisplayEGL *egl_display = display->winsys;

  if ((egl_renderer->private_features &
       COGL_EGL_WINSYS_FEATURE_SURFACELESS_CONTEXT) == 0 &&
      !make_dummy_surface(display, error))
    return FALSE;

  if (!_cogl_winsys_egl_make_current (display,
                                      egl_display->dummy_surface,
                                      egl_display->dummy_surface,
                                      egl_display->egl_context))
    {
      _cogl_set_error (error,
                       COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_CONTEXT,
                       "%s",
                       "Unable to eglMakeCurrent with dummy surface");
    }

  return TRUE;
}

static void
_cogl_winsys_egl_cleanup_context (CoglDisplay *display)
{
  CoglRenderer *renderer = display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglDisplayEGL *egl_display = display->winsys;
  CoglDisplayMir *mir_display = egl_display->platform;

  if (egl_display->dummy_surface != EGL_NO_SURFACE)
    {
      eglDestroySurface (egl_renderer->edpy, egl_display->dummy_surface);
      egl_display->dummy_surface = EGL_NO_SURFACE;
    }

  if (mir_display->dummy_mir_surface)
    {
      mir_surface_release_sync (mir_display->dummy_mir_surface);
      mir_display->dummy_mir_surface = NULL;
    }
}

static CoglBool
_cogl_winsys_egl_context_init (CoglContext *context,
                               CoglError **error)
{
  COGL_FLAGS_SET (context->features,
                  COGL_FEATURE_ID_ONSCREEN_MULTIPLE, TRUE);
  COGL_FLAGS_SET (context->winsys_features,
                  COGL_WINSYS_FEATURE_MULTIPLE_ONSCREEN,
                  TRUE);

  return TRUE;
}

static void
flush_pending_resize_notifications_cb (void *data,
                                       void *user_data)
{
  CoglFramebuffer *framebuffer = data;

  if (framebuffer->type == COGL_FRAMEBUFFER_TYPE_ONSCREEN)
    {
      CoglOnscreen *onscreen = COGL_ONSCREEN (framebuffer);
      CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
      CoglOnscreenMir *mir_onscreen = egl_onscreen->platform;

      g_mutex_lock (&mir_onscreen->mir_event_lock);

      if (egl_onscreen->pending_resize_notify)
        {
          int w = mir_onscreen->requested_width;
          int h = mir_onscreen->requested_height;

          _cogl_framebuffer_winsys_update_size (framebuffer, w, h);
          _cogl_onscreen_notify_resize (onscreen);

          egl_onscreen->pending_resize_notify = FALSE;
        }

      g_mutex_unlock (&mir_onscreen->mir_event_lock);
    }
}

static void
flush_pending_resize_notifications_idle (void *user_data)
{
  CoglContext *context = user_data;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;

  /* This needs to be disconnected before invoking the callbacks in
   * case the callbacks cause it to be queued again */
  _cogl_closure_disconnect (egl_renderer->resize_notify_idle);
  egl_renderer->resize_notify_idle = NULL;

  g_list_foreach (context->framebuffers,
                  flush_pending_resize_notifications_cb,
                  NULL);
}

static void _mir_surface_event_cb (MirSurface *surface,
                                   MirEvent const *event,
                                   void *data)
{
  CoglOnscreen *onscreen = data;
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglRenderer *renderer = context->display->renderer;
  CoglMirEvent mir_event = { onscreen, surface, (MirEvent *) event };

  if (event->type == mir_event_type_resize)
    {
      CoglRendererEGL *egl_renderer = renderer->winsys;
      CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
      CoglOnscreenMir *mir_onscreen = egl_onscreen->platform;

      g_mutex_lock (&mir_onscreen->mir_event_lock);

      egl_onscreen->pending_resize_notify = TRUE;
      mir_onscreen->requested_width = event->resize.width;
      mir_onscreen->requested_height = event->resize.height;

      if (!egl_renderer->resize_notify_idle)
        {
          egl_renderer->resize_notify_idle =
            _cogl_poll_renderer_add_idle (renderer,
                                          flush_pending_resize_notifications_idle,
                                          context,
                                          NULL);
        }

      g_mutex_unlock (&mir_onscreen->mir_event_lock);
    }

  _cogl_renderer_handle_native_event (renderer, &mir_event);
}

static CoglBool
_cogl_winsys_egl_onscreen_init (CoglOnscreen *onscreen,
                                EGLConfig egl_config,
                                CoglError **error)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenMir *mir_onscreen;
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererEGL *egl_renderer = renderer->winsys;
  CoglRendererMir *mir_renderer = egl_renderer->platform;
  MirEGLNativeWindowType mir_egl_native_window;
  MirSurfaceParameters surfaceparm;
  MirEventDelegate event_handler;

  mir_onscreen = g_slice_new0 (CoglOnscreenMir);
  egl_onscreen->platform = mir_onscreen;

  if (mir_surface_is_valid (onscreen->foreign_surface))
    {
      mir_onscreen->mir_surface = onscreen->foreign_surface;
    }
  else
    {
      surfaceparm.name = g_get_prgname ();
      surfaceparm.width = cogl_framebuffer_get_width (framebuffer);
      surfaceparm.height = cogl_framebuffer_get_height (framebuffer);
      surfaceparm.pixel_format = _mir_connection_get_valid_format (mir_renderer->mir_connection);
      surfaceparm.buffer_usage = mir_buffer_usage_hardware;
      surfaceparm.output_id = mir_display_output_id_invalid;
      mir_onscreen->mir_surface =
        mir_connection_create_surface_sync (mir_renderer->mir_connection, &surfaceparm);
    }

  if (!mir_surface_is_valid (mir_onscreen->mir_surface))
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                      "Error while creating mir surface for CoglOnscreen: %s",
                      mir_surface_get_error_message (mir_onscreen->mir_surface));
      mir_surface_release_sync (mir_onscreen->mir_surface);
      return FALSE;
    }

  mir_egl_native_window = mir_surface_get_egl_native_window (mir_onscreen->mir_surface);
  if (!mir_egl_native_window)
    {
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                       COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                       "Error while getting mir egl native window "
                       "for CoglOnscreen: %s",
                       mir_surface_get_error_message (mir_onscreen->mir_surface));
      mir_surface_release_sync (mir_onscreen->mir_surface);
      return FALSE;
    }

  egl_onscreen->egl_surface =
    eglCreateWindowSurface (egl_renderer->edpy,
                            egl_config,
                            (EGLNativeWindowType) mir_egl_native_window,
                            NULL);

  mir_onscreen->last_state = mir_surface_get_state (mir_onscreen->mir_surface);

  if (!mir_surface_is_valid (onscreen->foreign_surface))
  {
    event_handler.callback = _mir_surface_event_cb;
    event_handler.context = onscreen;
    mir_surface_set_event_handler (mir_onscreen->mir_surface, &event_handler);
    g_mutex_init (&mir_onscreen->mir_event_lock);
  }

  return TRUE;
}

static void
_cogl_winsys_egl_onscreen_deinit (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenMir *mir_onscreen = egl_onscreen->platform;

  if (mir_onscreen->mir_surface && !onscreen->foreign_surface)
    {
      mir_surface_set_event_handler (mir_onscreen->mir_surface, NULL);
      mir_surface_release_sync (mir_onscreen->mir_surface);
      g_mutex_clear (&mir_onscreen->mir_event_lock);
      mir_onscreen->mir_surface = NULL;
    }

  mir_onscreen->last_state = mir_surface_state_unknown;

  g_slice_free (CoglOnscreenMir, mir_onscreen);
}

static void
_cogl_winsys_onscreen_set_visibility (CoglOnscreen *onscreen,
                                      CoglBool visibility)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenMir *mir_onscreen = egl_onscreen->platform;
  MirSurfaceState new_state, current_state;

  current_state = mir_surface_get_state (mir_onscreen->mir_surface);

  if ((visibility && current_state != mir_surface_state_minimized) ||
      (!visibility && current_state == mir_surface_state_minimized))
    {
      return;
    }

  if (!visibility)
    {
      new_state = mir_surface_state_minimized;
      mir_onscreen->last_state = current_state;
    }
  else
    {
      new_state = mir_onscreen->last_state;
    }

  mir_surface_set_state (mir_onscreen->mir_surface, new_state);
}

static void
mir_surface_recreate (CoglOnscreen *onscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenMir *mir_onscreen = egl_onscreen->platform;
  const CoglWinsysVtable *winsys = _cogl_framebuffer_get_winsys (framebuffer);
  MirSurfaceState last_state, current_state;
  int w, h;

  g_mutex_lock (&mir_onscreen->mir_event_lock);
  mir_onscreen->requested_resize = FALSE;
  w = mir_onscreen->requested_width;
  h = mir_onscreen->requested_height;
  last_state = mir_onscreen->last_state;
  current_state = mir_surface_get_state (mir_onscreen->mir_surface);
  g_mutex_unlock (&mir_onscreen->mir_event_lock);

  winsys->onscreen_deinit (onscreen);

  _cogl_framebuffer_winsys_update_size (framebuffer, w, h);

  winsys->onscreen_init (onscreen, NULL);
  winsys->onscreen_bind (onscreen);

  egl_onscreen = onscreen->winsys;
  mir_onscreen = egl_onscreen->platform;

  g_mutex_lock (&mir_onscreen->mir_event_lock);
  mir_onscreen->last_state = last_state;
  mir_surface_set_state (mir_onscreen->mir_surface, current_state);
  _cogl_onscreen_queue_full_dirty (onscreen);
  _cogl_onscreen_notify_resize (onscreen);
  g_mutex_unlock (&mir_onscreen->mir_event_lock);
}

static void
_cogl_winsys_onscreen_swap_buffers_with_damage (CoglOnscreen *onscreen,
                                                const int *rectangles,
                                                int n_rectangles)
{
  CoglOnscreenEGL *egl_onscreen = onscreen->winsys;
  CoglOnscreenMir *mir_onscreen = egl_onscreen->platform;

  if (mir_onscreen->requested_resize)
    mir_surface_recreate (onscreen);

  parent_vtable->onscreen_swap_buffers_with_damage (onscreen,
                                                    rectangles,
                                                    n_rectangles);
}

CoglBool
cogl_mir_renderer_set_foreign_connection (CoglRenderer *renderer,
                                          MirConnection *connection)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_renderer (renderer), FALSE);
  _COGL_RETURN_VAL_IF_FAIL (mir_connection_is_valid (connection), FALSE);

  /* NB: Renderers are considered immutable once connected */
  _COGL_RETURN_VAL_IF_FAIL (!renderer->connected, FALSE);

  renderer->foreign_mir_connection = connection;
  return TRUE;
}

MirConnection *
cogl_mir_renderer_get_connection (CoglRenderer *renderer)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_renderer (renderer), NULL);

  if (mir_connection_is_valid (renderer->foreign_mir_connection))
    return renderer->foreign_mir_connection;

  if (renderer->connected)
    {
      CoglRendererEGL *egl_renderer = renderer->winsys;
      CoglRendererMir *mir_renderer = egl_renderer->platform;

      if (mir_connection_is_valid (mir_renderer->mir_connection))
        return mir_renderer->mir_connection;
    }

  return NULL;
}

void
cogl_mir_renderer_add_event_listener (CoglRenderer *renderer,
                                      CoglMirEventCallback func,
                                      void *data)
{
  _COGL_RETURN_IF_FAIL (cogl_is_renderer (renderer));

  _cogl_renderer_add_native_filter (renderer,
                                    (CoglNativeFilterFunc)func, data);
}

void
cogl_mir_renderer_remove_event_listener (CoglRenderer *renderer,
                                         CoglMirEventCallback func,
                                         void *data)
{
  _COGL_RETURN_IF_FAIL (cogl_is_renderer (renderer));

  _cogl_renderer_remove_native_filter (renderer,
                                       (CoglNativeFilterFunc)func, data);
}

CoglBool
cogl_mir_onscreen_set_foreign_surface (CoglOnscreen *onscreen,
                                       MirSurface *surface)
{
  CoglFramebuffer *fb;
  MirSurfaceParameters parameters;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_onscreen (surface), FALSE);
  _COGL_RETURN_VAL_IF_FAIL (mir_surface_is_valid (surface), FALSE);

  fb = COGL_FRAMEBUFFER (onscreen);
  _COGL_RETURN_VAL_IF_FAIL (!fb->allocated, FALSE);

  mir_surface_get_parameters (surface, &parameters);
  _COGL_RETURN_VAL_IF_FAIL (parameters.buffer_usage == mir_buffer_usage_hardware, FALSE);

  onscreen->foreign_surface = surface;
  return TRUE;
}

MirSurface *
cogl_mir_onscreen_get_surface (CoglOnscreen *onscreen)
{
  CoglOnscreenEGL *egl_onscreen;
  CoglOnscreenMir *mir_onscreen;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_onscreen (onscreen), NULL);

  if (!COGL_FRAMEBUFFER (onscreen)->allocated)
    return NULL;

  egl_onscreen = onscreen->winsys;
  mir_onscreen = egl_onscreen->platform;

  if (mir_surface_is_valid (mir_onscreen->mir_surface))
    return mir_onscreen->mir_surface;

  return NULL;
}

void
cogl_mir_onscreen_resize (CoglOnscreen *onscreen,
                          int           width,
                          int           height)
{
  CoglFramebuffer *framebuffer;
  CoglOnscreenEGL *egl_onscreen;
  CoglOnscreenMir *mir_onscreen;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_onscreen (onscreen), NULL);

  framebuffer = COGL_FRAMEBUFFER (onscreen);

  if (cogl_framebuffer_get_width (framebuffer) == width &&
      cogl_framebuffer_get_height (framebuffer) == height)
    {
      return;
    }

  if (!framebuffer->allocated)
    {
      _cogl_framebuffer_winsys_update_size (framebuffer, width, height);
      _cogl_onscreen_notify_resize (onscreen);
    }
  else if (!onscreen->foreign_surface)
    {
      egl_onscreen = onscreen->winsys;
      mir_onscreen = egl_onscreen->platform;

      g_mutex_lock (&mir_onscreen->mir_event_lock);
      mir_onscreen->requested_resize = TRUE;
      mir_onscreen->requested_width = width;
      mir_onscreen->requested_height = height;
      g_mutex_unlock (&mir_onscreen->mir_event_lock);

      if (!framebuffer->mid_scene)
        mir_surface_recreate (onscreen);
    }
}

static const CoglWinsysEGLVtable
_cogl_winsys_egl_vtable =
  {
    .display_setup = _cogl_winsys_egl_display_setup,
    .display_destroy = _cogl_winsys_egl_display_destroy,
    .context_created = _cogl_winsys_egl_context_created,
    .cleanup_context = _cogl_winsys_egl_cleanup_context,
    .context_init = _cogl_winsys_egl_context_init,
    .onscreen_init = _cogl_winsys_egl_onscreen_init,
    .onscreen_deinit = _cogl_winsys_egl_onscreen_deinit
  };

const CoglWinsysVtable *
_cogl_winsys_egl_mir_get_vtable (void)
{
  static CoglBool vtable_inited = FALSE;
  static CoglWinsysVtable vtable;

  if (!vtable_inited)
    {
      /* The EGL_MIR winsys is a subclass of the EGL winsys so we
         start by copying its vtable */

      parent_vtable = _cogl_winsys_egl_get_vtable ();
      vtable = *parent_vtable;

      vtable.id = COGL_WINSYS_ID_EGL_MIR;
      vtable.name = "EGL_MIR";

      vtable.renderer_connect = _cogl_winsys_renderer_connect;
      vtable.renderer_disconnect = _cogl_winsys_renderer_disconnect;

      vtable.onscreen_set_visibility =
        _cogl_winsys_onscreen_set_visibility;
      vtable.onscreen_swap_buffers_with_damage =
        _cogl_winsys_onscreen_swap_buffers_with_damage;

      vtable_inited = TRUE;
    }

  return &vtable;
}
