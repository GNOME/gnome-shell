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

#define CLUTTER_ENABLE_EXPERIMENTAL_API

#include "clutter-backend-private.h"
#include "clutter-debug.h"
#include "clutter-event-private.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-profile.h"
#include "clutter-stage-manager-private.h"
#include "clutter-stage-private.h"
#include "clutter-stage-window.h"
#include "clutter-version.h"

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#include "deprecated/clutter-backend.h"

#ifdef HAVE_CLUTTER_WAYLAND_COMPOSITOR
#include "wayland/clutter-wayland-compositor.h"
#endif /* HAVE_CLUTTER_WAYLAND_COMPOSITOR */

#include <cogl/cogl.h>

#ifdef CLUTTER_INPUT_X11
#include "x11/clutter-backend-x11.h"
#endif
#ifdef CLUTTER_INPUT_WIN32
#include "win32/clutter-backend-win32.h"
#endif
#ifdef CLUTTER_INPUT_OSX
#include "osx/clutter-backend-osx.h"
#endif
#ifdef CLUTTER_INPUT_GDK
#include "gdk/clutter-backend-gdk.h"
#endif
#ifdef CLUTTER_INPUT_EVDEV
#include "evdev/clutter-device-manager-evdev.h"
#endif
#ifdef CLUTTER_INPUT_TSLIB
/* XXX - should probably warn, here */
#include "tslib/clutter-event-tslib.h"
#endif
#ifdef CLUTTER_WINDOWING_EGL
#include "egl/clutter-backend-eglnative.h"
#endif
#ifdef CLUTTER_INPUT_WAYLAND
#include "wayland/clutter-device-manager-wayland.h"
#endif

#ifdef HAVE_CLUTTER_WAYLAND_COMPOSITOR
#include <cogl/cogl-wayland-server.h>
#include <wayland-server.h>
#include "wayland/clutter-wayland-compositor.h"
#endif

#define DEFAULT_FONT_NAME       "Sans 10"

struct _ClutterBackendPrivate
{
  cairo_font_options_t *font_options;

  gchar *font_name;

  gfloat units_per_em;
  gint32 units_serial;

  GList *event_translators;
};

enum
{
  RESOLUTION_CHANGED,
  FONT_CHANGED,
  SETTINGS_CHANGED,

  LAST_SIGNAL
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ClutterBackend, clutter_backend, G_TYPE_OBJECT)

static guint backend_signals[LAST_SIGNAL] = { 0, };

/* Global for being able to specify a compositor side wayland display
 * pointer before clutter initialization */
#ifdef HAVE_CLUTTER_WAYLAND_COMPOSITOR
static struct wl_display *_wayland_compositor_display;
#endif

static const char *allowed_backend;

static void
clutter_backend_dispose (GObject *gobject)
{
  ClutterBackendPrivate *priv = CLUTTER_BACKEND (gobject)->priv;

  /* clear the events still in the queue of the main context */
  _clutter_clear_events_queue ();

  /* remove all event translators */
  if (priv->event_translators != NULL)
    {
      g_list_free (priv->event_translators);
      priv->event_translators = NULL;
    }

  G_OBJECT_CLASS (clutter_backend_parent_class)->dispose (gobject);
}

static void
clutter_backend_finalize (GObject *gobject)
{
  ClutterBackend *backend = CLUTTER_BACKEND (gobject);

  g_source_destroy (backend->cogl_source);

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
      ClutterSettings *settings;
      gchar *font_name = NULL;

      settings = clutter_settings_get_default ();
      g_object_get (settings, "font-name", &font_name, NULL);

      if (G_LIKELY (font_name != NULL && *font_name != '\0'))
        {
          font_desc = pango_font_description_from_string (font_name);
          free_font_desc = TRUE;

          g_free (font_name);
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
        font_size = dpi * ((gdouble) pango_size / PANGO_SCALE) / 72.0f;

      /* 10 points at 96 DPI is 13.3 pixels */
      units_per_em = (1.2f * font_size) * dpi / 96.0f;
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
  gdouble resolution;
  gint dpi;

  settings = clutter_settings_get_default ();
  g_object_get (settings, "font-dpi", &dpi, NULL);

  if (dpi < 0)
    resolution = 96.0;
  else
    resolution = dpi / 1024.0;

  context = _clutter_context_get_default ();
  if (context->font_map != NULL)
    cogl_pango_font_map_set_resolution (context->font_map, resolution);

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

static gboolean
clutter_backend_real_create_context (ClutterBackend  *backend,
                                     GError         **error)
{
  ClutterBackendClass *klass;
  CoglSwapChain *swap_chain;
  GError *internal_error;

  if (backend->cogl_context != NULL)
    return TRUE;

  klass = CLUTTER_BACKEND_GET_CLASS (backend);

  swap_chain = NULL;
  internal_error = NULL;

  CLUTTER_NOTE (BACKEND, "Creating Cogl renderer");
  if (klass->get_renderer != NULL)
    backend->cogl_renderer = klass->get_renderer (backend, &internal_error);
  else
    backend->cogl_renderer = cogl_renderer_new ();

  if (backend->cogl_renderer == NULL)
    goto error;

#ifdef HAVE_CLUTTER_WAYLAND_COMPOSITOR
  /* If the application is trying to act as a Wayland compositor then
     it needs to have an EGL-based renderer backend */
  if (_wayland_compositor_display)
    cogl_renderer_add_constraint (backend->cogl_renderer,
                                  COGL_RENDERER_CONSTRAINT_USES_EGL);
#endif

  CLUTTER_NOTE (BACKEND, "Connecting the renderer");
  if (!cogl_renderer_connect (backend->cogl_renderer, &internal_error))
    goto error;

  CLUTTER_NOTE (BACKEND, "Creating Cogl swap chain");
  swap_chain = cogl_swap_chain_new ();

  CLUTTER_NOTE (BACKEND, "Creating Cogl display");
  if (klass->get_display != NULL)
    {
      backend->cogl_display = klass->get_display (backend,
                                                  backend->cogl_renderer,
                                                  swap_chain,
                                                  &internal_error);
    }
  else
    {
      CoglOnscreenTemplate *tmpl;
      gboolean res;

      tmpl = cogl_onscreen_template_new (swap_chain);

      /* XXX: I have some doubts that this is a good design.
       *
       * Conceptually should we be able to check an onscreen_template
       * without more details about the CoglDisplay configuration?
       */
      res = cogl_renderer_check_onscreen_template (backend->cogl_renderer,
                                                   tmpl,
                                                   &internal_error);

      if (!res)
        goto error;

      backend->cogl_display = cogl_display_new (backend->cogl_renderer, tmpl);

      /* the display owns the template */
      cogl_object_unref (tmpl);
    }

  if (backend->cogl_display == NULL)
    goto error;

#ifdef HAVE_CLUTTER_WAYLAND_COMPOSITOR
  cogl_wayland_display_set_compositor_display (backend->cogl_display,
                                               _wayland_compositor_display);
#endif

  CLUTTER_NOTE (BACKEND, "Setting up the display");
  if (!cogl_display_setup (backend->cogl_display, &internal_error))
    goto error;

  CLUTTER_NOTE (BACKEND, "Creating the Cogl context");
  backend->cogl_context = cogl_context_new (backend->cogl_display, &internal_error);
  if (backend->cogl_context == NULL)
    goto error;

  backend->cogl_source = cogl_glib_source_new (backend->cogl_context,
                                               G_PRIORITY_DEFAULT);
  g_source_attach (backend->cogl_source, NULL);

  /* the display owns the renderer and the swap chain */
  cogl_object_unref (backend->cogl_renderer);
  cogl_object_unref (swap_chain);

  return TRUE;

error:
  if (backend->cogl_display != NULL)
    {
      cogl_object_unref (backend->cogl_display);
      backend->cogl_display = NULL;
    }

  if (backend->cogl_renderer != NULL)
    {
      cogl_object_unref (backend->cogl_renderer);
      backend->cogl_renderer = NULL;
    }

  if (swap_chain != NULL)
    cogl_object_unref (swap_chain);

  if (internal_error != NULL)
    g_propagate_error (error, internal_error);
  else
    g_set_error_literal (error, CLUTTER_INIT_ERROR,
                         CLUTTER_INIT_ERROR_BACKEND,
                         _("Unable to initialize the Clutter backend"));

  return FALSE;
}

static void
clutter_backend_real_ensure_context (ClutterBackend *backend,
                                     ClutterStage   *stage)
{
  ClutterStageWindow *stage_impl;
  CoglFramebuffer *framebuffer;

  if (stage == NULL)
    return;

  stage_impl = _clutter_stage_get_window (stage);
  if (stage_impl == NULL)
    return;

  framebuffer = _clutter_stage_window_get_active_framebuffer (stage_impl);
  if (framebuffer == NULL)
    return;

  cogl_set_framebuffer (framebuffer);
}

static ClutterFeatureFlags
clutter_backend_real_get_features (ClutterBackend *backend)
{
  ClutterFeatureFlags flags = 0;

  if (cogl_clutter_winsys_has_feature (COGL_WINSYS_FEATURE_MULTIPLE_ONSCREEN))
    {
      CLUTTER_NOTE (BACKEND, "Cogl supports multiple onscreen framebuffers");
      flags |= CLUTTER_FEATURE_STAGE_MULTIPLE;
    }
  else
    {
      CLUTTER_NOTE (BACKEND, "Cogl only supports one onscreen framebuffer");
      flags |= CLUTTER_FEATURE_STAGE_STATIC;
    }

  if (cogl_clutter_winsys_has_feature (COGL_WINSYS_FEATURE_SWAP_THROTTLE))
    {
      CLUTTER_NOTE (BACKEND, "Cogl supports swap buffers throttling");
      flags |= CLUTTER_FEATURE_SYNC_TO_VBLANK;
    }
  else
    CLUTTER_NOTE (BACKEND, "Cogl doesn't support swap buffers throttling");

  if (cogl_clutter_winsys_has_feature (COGL_WINSYS_FEATURE_SWAP_BUFFERS_EVENT))
    {
      CLUTTER_NOTE (BACKEND, "Cogl supports swap buffers complete events");
      flags |= CLUTTER_FEATURE_SWAP_EVENTS;
    }

  return flags;
}

static ClutterStageWindow *
clutter_backend_real_create_stage (ClutterBackend  *backend,
                                   ClutterStage    *wrapper,
                                   GError         **error)
{
  ClutterBackendClass *klass;

  if (!clutter_feature_available (CLUTTER_FEATURE_STAGE_MULTIPLE))
    {
      ClutterStageManager *manager = clutter_stage_manager_get_default ();

      if (clutter_stage_manager_get_default_stage (manager) != NULL)
        {
          g_set_error (error, CLUTTER_INIT_ERROR,
                       CLUTTER_INIT_ERROR_BACKEND,
                       _("The backend of type '%s' does not support "
                         "creating multiple stages"),
                       G_OBJECT_TYPE_NAME (backend));
          return NULL;
        }
    }

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  g_assert (klass->stage_window_type != G_TYPE_INVALID);

  return g_object_new (klass->stage_window_type,
                       "backend", backend,
                       "wrapper", wrapper,
                       NULL);
}

ClutterBackend *
_clutter_create_backend (void)
{
  const char *backend = allowed_backend;
  ClutterBackend *retval = NULL;

  if (backend == NULL)
    {
      const char *backend_env = g_getenv ("CLUTTER_BACKEND");

      if (backend_env != NULL)
	backend = g_intern_string (backend_env);
    }

#ifdef CLUTTER_WINDOWING_OSX
  if (backend == NULL || backend == I_(CLUTTER_WINDOWING_OSX))
    retval = g_object_new (CLUTTER_TYPE_BACKEND_OSX, NULL);
  else
#endif
#ifdef CLUTTER_WINDOWING_WIN32
  if (backend == NULL || backend == I_(CLUTTER_WINDOWING_WIN32))
    retval = g_object_new (CLUTTER_TYPE_BACKEND_WIN32, NULL);
  else
#endif
#ifdef CLUTTER_WINDOWING_X11
  if (backend == NULL || backend == I_(CLUTTER_WINDOWING_X11))
    retval = g_object_new (CLUTTER_TYPE_BACKEND_X11, NULL);
  else
#endif
#ifdef CLUTTER_WINDOWING_WAYLAND
  if (backend == NULL || backend == I_(CLUTTER_WINDOWING_WAYLAND))
    retval = g_object_new (CLUTTER_TYPE_BACKEND_WAYLAND, NULL);
  else
#endif
#ifdef CLUTTER_WINDOWING_EGL
  if (backend == NULL || backend == I_(CLUTTER_WINDOWING_EGL))
    retval = g_object_new (CLUTTER_TYPE_BACKEND_EGL_NATIVE, NULL);
  else
#endif
#ifdef CLUTTER_WINDOWING_GDK
  if (backend == NULL || backend == I_(CLUTTER_WINDOWING_GDK))
    retval = g_object_new (CLUTTER_TYPE_BACKEND_GDK, NULL);
  else
#endif
  if (backend == NULL)
    g_error ("No default Clutter backend found.");
  else
    g_error ("Unsupported Clutter backend: '%s'", backend);

  return retval;
}

static void
clutter_backend_real_init_events (ClutterBackend *backend)
{
  const char *input_backend = NULL;

  input_backend = g_getenv ("CLUTTER_INPUT_BACKEND");
  if (input_backend != NULL)
    input_backend = g_intern_string (input_backend);

#ifdef CLUTTER_INPUT_OSX
  if (clutter_check_windowing_backend (CLUTTER_WINDOWING_OSX) &&
      (input_backend == NULL || input_backend == I_(CLUTTER_INPUT_OSX)))
    {
      _clutter_backend_osx_events_init (backend);
    }
  else
#endif
#ifdef CLUTTER_INPUT_WIN32
  if (clutter_check_windowing_backend (CLUTTER_WINDOWING_WIN32) &&
      (input_backend == NULL || input_backend == I_(CLUTTER_INPUT_WIN32)))
    {
      _clutter_backend_win32_events_init (backend);
    }
  else
#endif
#ifdef CLUTTER_INPUT_X11
  if (clutter_check_windowing_backend (CLUTTER_WINDOWING_X11) &&
      (input_backend == NULL || input_backend == I_(CLUTTER_INPUT_X11)))
    {
      _clutter_backend_x11_events_init (backend);
    }
  else
#endif
#ifdef CLUTTER_INPUT_GDK
  if (clutter_check_windowing_backend (CLUTTER_WINDOWING_GDK) &&
      (input_backend == NULL || input_backend == I_(CLUTTER_INPUT_GDK)))
    {
      _clutter_backend_gdk_events_init (backend);
    }
  else
#endif
#ifdef CLUTTER_INPUT_EVDEV
  /* Evdev can be used regardless of the windowing system */
  if ((input_backend != NULL && strcmp (input_backend, CLUTTER_INPUT_EVDEV) == 0)
#ifdef CLUTTER_WINDOWING_EGL
      /* but we do want to always use it for EGL native */
      || clutter_check_windowing_backend (CLUTTER_WINDOWING_EGL)
#endif
      )
    {
      _clutter_events_evdev_init (backend);
    }
  else
#endif
#ifdef CLUTTER_INPUT_TSLIB
  /* Tslib can be used regardless of the windowing system */
  if (input_backend != NULL &&
      strcmp (input_backend, CLUTTER_INPUT_TSLIB) == 0)
    {
      _clutter_events_tslib_init (backend);
    }
  else
#endif
#ifdef CLUTTER_INPUT_WAYLAND
  if (clutter_check_windowing_backend (CLUTTER_WINDOWING_WAYLAND) &&
      (input_backend == NULL || input_backend == I_(CLUTTER_INPUT_WAYLAND)))
    {
      _clutter_events_wayland_init (backend);
    }
  else
#endif
  if (input_backend != NULL)
    {
      if (input_backend != I_(CLUTTER_INPUT_NULL))
        g_error ("Unrecognized input backend '%s'", input_backend);
    }
  else
    g_error ("Unknown input backend");
}

static ClutterDeviceManager *
clutter_backend_real_get_device_manager (ClutterBackend *backend)
{
  if (G_UNLIKELY (backend->device_manager == NULL))
    {
      g_critical ("No device manager available, expect broken input");
      return NULL;
    }

  return backend->device_manager;
}

static gboolean
clutter_backend_real_translate_event (ClutterBackend *backend,
                                      gpointer        native,
                                      ClutterEvent   *event)
{
  ClutterBackendPrivate *priv = backend->priv;
  GList *l;

  for (l = priv->event_translators;
       l != NULL;
       l = l->next)
    {
      ClutterEventTranslator *translator = l->data;
      ClutterTranslateReturn retval;

      retval = _clutter_event_translator_translate_event (translator,
                                                          native,
                                                          event);

      if (retval == CLUTTER_TRANSLATE_QUEUE)
        return TRUE;

      if (retval == CLUTTER_TRANSLATE_REMOVE)
        return FALSE;
    }

  return FALSE;
}

static void
clutter_backend_class_init (ClutterBackendClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = clutter_backend_dispose;
  gobject_class->finalize = clutter_backend_finalize;

  klass->stage_window_type = G_TYPE_INVALID;

  /**
   * ClutterBackend::resolution-changed:
   * @backend: the #ClutterBackend that emitted the signal
   *
   * The ::resolution-changed signal is emitted each time the font
   * resolutions has been changed through #ClutterSettings.
   *
   * Since: 1.0
   */
  backend_signals[RESOLUTION_CHANGED] =
    g_signal_new (I_("resolution-changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (ClutterBackendClass, resolution_changed),
                  NULL, NULL,
                  _clutter_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * ClutterBackend::font-changed:
   * @backend: the #ClutterBackend that emitted the signal
   *
   * The ::font-changed signal is emitted each time the font options
   * have been changed through #ClutterSettings.
   *
   * Since: 1.0
   */
  backend_signals[FONT_CHANGED] =
    g_signal_new (I_("font-changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (ClutterBackendClass, font_changed),
                  NULL, NULL,
                  _clutter_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * ClutterBackend::settings-changed:
   * @backend: the #ClutterBackend that emitted the signal
   *
   * The ::settings-changed signal is emitted each time the #ClutterSettings
   * properties have been changed.
   *
   * Since: 1.4
   */
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

  klass->init_events = clutter_backend_real_init_events;
  klass->get_device_manager = clutter_backend_real_get_device_manager;
  klass->translate_event = clutter_backend_real_translate_event;
  klass->create_context = clutter_backend_real_create_context;
  klass->ensure_context = clutter_backend_real_ensure_context;
  klass->get_features = clutter_backend_real_get_features;
  klass->create_stage = clutter_backend_real_create_stage;
}

static void
clutter_backend_init (ClutterBackend *self)
{
  self->priv = clutter_backend_get_instance_private (self);
  self->priv->units_per_em = -1.0;
  self->priv->units_serial = 1;
}

void
_clutter_backend_add_options (ClutterBackend *backend,
                              GOptionGroup   *group)
{
  ClutterBackendClass *klass;

  g_assert (CLUTTER_IS_BACKEND (backend));

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (klass->add_options)
    klass->add_options (backend, group);
}

gboolean
_clutter_backend_pre_parse (ClutterBackend  *backend,
                            GError         **error)
{
  ClutterBackendClass *klass;

  g_assert (CLUTTER_IS_BACKEND (backend));

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

  g_assert (CLUTTER_IS_BACKEND (backend));

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
  ClutterStageWindow *stage_window;

  g_assert (CLUTTER_IS_BACKEND (backend));
  g_assert (CLUTTER_IS_STAGE (wrapper));

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (klass->create_stage != NULL)
    stage_window = klass->create_stage (backend, wrapper, error);
  else
    stage_window = NULL;

  if (stage_window == NULL)
    return NULL;

  g_assert (CLUTTER_IS_STAGE_WINDOW (stage_window));

  return stage_window;
}

gboolean
_clutter_backend_create_context (ClutterBackend  *backend,
                                 GError         **error)
{
  ClutterBackendClass *klass;

  klass = CLUTTER_BACKEND_GET_CLASS (backend);

  return klass->create_context (backend, error);
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

  g_assert (CLUTTER_IS_BACKEND (backend));
  g_assert (CLUTTER_IS_STAGE (stage));

  if (current_context_stage != stage || !CLUTTER_ACTOR_IS_REALIZED (stage))
    {
      ClutterStage *new_stage = NULL;

      if (!CLUTTER_ACTOR_IS_REALIZED (stage))
        {
          new_stage = NULL;

          CLUTTER_NOTE (BACKEND,
                        "Stage [%p] is not realized, unsetting the stage",
                        stage);
        }
      else
        {
          new_stage = stage;

          CLUTTER_NOTE (BACKEND,
                        "Setting the new stage [%p]",
                        new_stage);
        }

      /* XXX: Until Cogl becomes fully responsible for backend windows
       * Clutter need to manually keep it informed of the current window size
       *
       * NB: This must be done after we ensure_context above because Cogl
       * always assumes there is a current GL context.
       */
      if (new_stage != NULL)
        {
          float width, height;

          _clutter_backend_ensure_context_internal (backend, new_stage);

          clutter_actor_get_size (CLUTTER_ACTOR (stage), &width, &height);

          cogl_onscreen_clutter_backend_set_size (width, height);

          /* Eventually we will have a separate CoglFramebuffer for
           * each stage and each one will track private projection
           * matrix and viewport state, but until then we need to make
           * sure we update the projection and viewport whenever we
           * switch between stages.
           *
           * This dirty mechanism will ensure they are asserted before
           * the next paint...
           */
          _clutter_stage_dirty_viewport (stage);
          _clutter_stage_dirty_projection (stage);
        }

      /* FIXME: With a NULL stage and thus no active context it may make more
       * sense to clean the context but then re call with the default stage 
       * so at least there is some kind of context in place (as to avoid
       * potential issue of GL calls with no context).
       */
      current_context_stage = new_stage;
    }
  else
    CLUTTER_NOTE (BACKEND, "Stage is the same");
}


ClutterFeatureFlags
_clutter_backend_get_features (ClutterBackend *backend)
{
  ClutterBackendClass *klass;
  GError *error;

  g_assert (CLUTTER_IS_BACKEND (backend));

  klass = CLUTTER_BACKEND_GET_CLASS (backend);

  /* we need to have a context here; so we create the
   * GL context first and the ask for features. if the
   * context already exists this should be a no-op
   */
  error = NULL;
  if (klass->create_context != NULL)
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

  g_assert (CLUTTER_IS_BACKEND (backend));

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  klass->init_events (backend);
}

gfloat
_clutter_backend_get_units_per_em (ClutterBackend       *backend,
                                   PangoFontDescription *font_desc)
{
  ClutterBackendPrivate *priv;

  priv = backend->priv;

  /* recompute for the font description, but do not cache the result */
  if (font_desc != NULL)
    return get_units_per_em (backend, font_desc);

  if (priv->units_per_em < 0)
    priv->units_per_em = get_units_per_em (backend, NULL);

  return priv->units_per_em;
}

void
_clutter_backend_copy_event_data (ClutterBackend     *backend,
                                  const ClutterEvent *src,
                                  ClutterEvent       *dest)
{
  ClutterBackendClass *klass;

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (klass->copy_event_data != NULL)
    klass->copy_event_data (backend, src, dest);
}

void
_clutter_backend_free_event_data (ClutterBackend *backend,
                                  ClutterEvent   *event)
{
  ClutterBackendClass *klass;

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
 * Deprecated: 1.4: Use #ClutterSettings:font-dpi instead
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

  if (resolution < 0)
    return 96.0;

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
const gchar *
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
  return backend->priv->units_serial;
}

gboolean
_clutter_backend_translate_event (ClutterBackend *backend,
                                  gpointer        native,
                                  ClutterEvent   *event)
{
  return CLUTTER_BACKEND_GET_CLASS (backend)->translate_event (backend,
                                                               native,
                                                               event);
}

void
_clutter_backend_add_event_translator (ClutterBackend         *backend,
                                       ClutterEventTranslator *translator)
{
  ClutterBackendPrivate *priv = backend->priv;

  if (g_list_find (priv->event_translators, translator) != NULL)
    return;

  priv->event_translators =
    g_list_prepend (priv->event_translators, translator);
}

void
_clutter_backend_remove_event_translator (ClutterBackend         *backend,
                                          ClutterEventTranslator *translator)
{
  ClutterBackendPrivate *priv = backend->priv;

  if (g_list_find (priv->event_translators, translator) == NULL)
    return;

  priv->event_translators =
    g_list_remove (priv->event_translators, translator);
}

/**
 * clutter_backend_get_cogl_context:
 * @backend: a #ClutterBackend
 *
 * Retrieves the #CoglContext associated with the given clutter
 * @backend. A #CoglContext is required when using some of the
 * experimental 2.0 Cogl API.
 *
 * Since CoglContext is itself experimental API this API should
 * be considered experimental too.
 *
 * This API is not yet supported on OSX because OSX still
 * uses the stub Cogl winsys and the Clutter backend doesn't
 * explicitly create a CoglContext.
 *
 * Return value: (transfer none): The #CoglContext associated with @backend.
 *
 * Since: 1.8
 * Stability: unstable
 */
CoglContext *
clutter_backend_get_cogl_context (ClutterBackend *backend)
{
  return backend->cogl_context;
}

#ifdef HAVE_CLUTTER_WAYLAND_COMPOSITOR
/**
 * clutter_wayland_set_compositor_display:
 * @display: A compositor side struct wl_display pointer
 *
 * This informs Clutter of your compositor side Wayland display
 * object. This must be called before calling clutter_init().
 *
 * Since: 1.8
 * Stability: unstable
 */
void
clutter_wayland_set_compositor_display (void *display)
{
  if (_clutter_context_is_initialized ())
    {
      g_warning ("%s() can only be used before calling clutter_init()",
                 G_STRFUNC);
      return;
    }

  _wayland_compositor_display = display;
}
#endif

/**
 * clutter_set_windowing_backend:
 * @backend_type: the name of a clutter window backend
 *
 * Restricts clutter to only use the specified backend.
 * This must be called before the first API call to clutter, including
 * clutter_get_option_context()
 *
 * Since: 1.16
 */
void
clutter_set_windowing_backend (const char *backend_type)
{
  g_return_if_fail (backend_type != NULL);

  allowed_backend = g_intern_string (backend_type);
}

PangoDirection
_clutter_backend_get_keymap_direction (ClutterBackend *backend)
{
  ClutterBackendClass *klass;

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (klass->get_keymap_direction != NULL)
    return klass->get_keymap_direction (backend);

  return PANGO_DIRECTION_NEUTRAL;
}
