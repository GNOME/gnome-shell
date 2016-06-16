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
#include "clutter-build-config.h"
#endif

#define CLUTTER_ENABLE_EXPERIMENTAL_API

#include "clutter-backend-private.h"
#include "clutter-debug.h"
#include "clutter-event-private.h"
#include "clutter-marshal.h"
#include "clutter-mutter.h"
#include "clutter-private.h"
#include "clutter-stage-manager-private.h"
#include "clutter-stage-private.h"
#include "clutter-stage-window.h"
#include "clutter-version.h"
#include "clutter-device-manager-private.h"

#define CLUTTER_DISABLE_DEPRECATION_WARNINGS
#include "deprecated/clutter-backend.h"

#ifdef CLUTTER_HAS_WAYLAND_COMPOSITOR_SUPPORT
#include "wayland/clutter-wayland-compositor.h"
#endif

#include <cogl/cogl.h>

#ifdef CLUTTER_INPUT_X11
#include "x11/clutter-backend-x11.h"
#endif
#ifdef CLUTTER_INPUT_EVDEV
#include "evdev/clutter-device-manager-evdev.h"
#endif
#ifdef CLUTTER_WINDOWING_EGL
#include "egl/clutter-backend-eglnative.h"
#endif

#ifdef CLUTTER_HAS_WAYLAND_COMPOSITOR_SUPPORT
#include <cogl/cogl-wayland-server.h>
#include <wayland-server.h>
#include "wayland/clutter-wayland-compositor.h"
#endif

#define DEFAULT_FONT_NAME       "Sans 10"

enum
{
  RESOLUTION_CHANGED,
  FONT_CHANGED,
  SETTINGS_CHANGED,

  LAST_SIGNAL
};

G_DEFINE_ABSTRACT_TYPE (ClutterBackend, clutter_backend, G_TYPE_OBJECT)

static guint backend_signals[LAST_SIGNAL] = { 0, };

/* Global for being able to specify a compositor side wayland display
 * pointer before clutter initialization */
#ifdef CLUTTER_HAS_WAYLAND_COMPOSITOR_SUPPORT
static struct wl_display *_wayland_compositor_display;
#endif

static void
clutter_backend_dispose (GObject *gobject)
{
  ClutterBackend *backend = CLUTTER_BACKEND (gobject);

  /* clear the events still in the queue of the main context */
  _clutter_clear_events_queue ();

  /* remove all event translators */
  g_clear_pointer (&backend->event_translators, g_list_free);

  g_clear_pointer (&backend->dummy_onscreen, cogl_object_unref);

  G_OBJECT_CLASS (clutter_backend_parent_class)->dispose (gobject);
}

static void
clutter_backend_finalize (GObject *gobject)
{
  ClutterBackend *backend = CLUTTER_BACKEND (gobject);

  g_source_destroy (backend->cogl_source);

  g_free (backend->font_name);
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

  backend->units_per_em = get_units_per_em (backend, NULL);
  backend->units_serial += 1;

  CLUTTER_NOTE (BACKEND, "Units per em: %.2f", backend->units_per_em);
}

static void
clutter_backend_real_font_changed (ClutterBackend *backend)
{
  backend->units_per_em = get_units_per_em (backend, NULL);
  backend->units_serial += 1;

  CLUTTER_NOTE (BACKEND, "Units per em: %.2f", backend->units_per_em);
}

static gboolean
clutter_backend_do_real_create_context (ClutterBackend  *backend,
                                        CoglDriver       driver_id,
                                        GError         **error)
{
  ClutterBackendClass *klass;
  CoglSwapChain *swap_chain;
  GError *internal_error;

  klass = CLUTTER_BACKEND_GET_CLASS (backend);

  swap_chain = NULL;
  internal_error = NULL;

  CLUTTER_NOTE (BACKEND, "Creating Cogl renderer");
  backend->cogl_renderer = klass->get_renderer (backend, &internal_error);

  if (backend->cogl_renderer == NULL)
    goto error;

  CLUTTER_NOTE (BACKEND, "Connecting the renderer");
  cogl_renderer_set_driver (backend->cogl_renderer, driver_id);
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

#ifdef CLUTTER_HAS_WAYLAND_COMPOSITOR_SUPPORT
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

  return FALSE;
}

static const struct {
  const char *driver_name;
  const char *driver_desc;
  CoglDriver driver_id;
} all_known_drivers[] = {
  { "gl3", "OpenGL 3.2 core profile", COGL_DRIVER_GL3 },
  { "gl", "OpenGL legacy profile", COGL_DRIVER_GL },
  { "gles2", "OpenGL ES 2.0", COGL_DRIVER_GLES2 },
  { "any", "Default Cogl driver", COGL_DRIVER_ANY },
};

static const char *allowed_drivers;

static gboolean
clutter_backend_real_create_context (ClutterBackend  *backend,
                                     GError         **error)
{
  GError *internal_error = NULL;
  const char *drivers_list;
  char **known_drivers;
  gboolean allow_any;
  int i;

  if (backend->cogl_context != NULL)
    return TRUE;

  if (allowed_drivers == NULL)
    allowed_drivers = CLUTTER_DRIVERS;

  allow_any = strstr (allowed_drivers, "*") != NULL;

  drivers_list = g_getenv ("CLUTTER_DRIVER");
  if (drivers_list == NULL)
    drivers_list = allowed_drivers;

  known_drivers = g_strsplit (drivers_list, ",", 0);

  for (i = 0; backend->cogl_context == NULL && known_drivers[i] != NULL; i++)
    {
      const char *driver_name = known_drivers[i];
      gboolean is_any = g_str_equal (driver_name, "*");
      int j;

      for (j = 0; j < G_N_ELEMENTS (all_known_drivers); j++)
        {
          if (!allow_any && !is_any && !strstr (driver_name, all_known_drivers[j].driver_name))
            continue;

          if ((allow_any && is_any) ||
              (is_any && strstr (allowed_drivers, all_known_drivers[j].driver_name)) ||
              g_str_equal (all_known_drivers[j].driver_name, driver_name))
            {
              CLUTTER_NOTE (BACKEND, "Checking for the %s driver", all_known_drivers[j].driver_desc);

              if (clutter_backend_do_real_create_context (backend, all_known_drivers[j].driver_id, &internal_error))
                break;

              if (internal_error)
                {
                  CLUTTER_NOTE (BACKEND, "Unable to use the %s driver: %s",
                                all_known_drivers[j].driver_desc,
                                internal_error->message);
                  g_clear_error (&internal_error);
                }
            }
        }
    }

  g_strfreev (known_drivers);

  if (backend->cogl_context == NULL)
    {
      if (internal_error != NULL)
        g_propagate_error (error, internal_error);
      else
        g_set_error_literal (error, CLUTTER_INIT_ERROR,
                             CLUTTER_INIT_ERROR_BACKEND,
                            _("Unable to initialize the Clutter backend: no available drivers found."));

      return FALSE;
    }

  backend->cogl_source = cogl_glib_source_new (backend->cogl_context, G_PRIORITY_DEFAULT);
  g_source_attach (backend->cogl_source, NULL);

  return TRUE;
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

static const char *allowed_backends;

static ClutterBackend * (* custom_backend_func) (void);

static const struct {
  const char *name;
  ClutterBackend * (* create_backend) (void);
} available_backends[] = {
#ifdef CLUTTER_WINDOWING_X11
  { CLUTTER_WINDOWING_X11, clutter_backend_x11_new },
#endif
#ifdef CLUTTER_WINDOWING_EGL
  { CLUTTER_WINDOWING_EGL, clutter_backend_egl_native_new },
#endif
  { NULL, NULL },
};

void
clutter_set_custom_backend_func (ClutterBackend *(* func) (void))
{
  custom_backend_func = func;
}

ClutterBackend *
_clutter_create_backend (void)
{
  const char *backends_list;
  ClutterBackend *retval;
  gboolean allow_any;
  char **backends;
  int i;

  if (custom_backend_func)
    {
      retval = custom_backend_func ();

      if (!retval)
        g_error ("Failed to create custom backend.");

      return retval;
    }

  if (allowed_backends == NULL)
    allowed_backends = "*";

  allow_any = strstr (allowed_backends, "*") != NULL;

  backends_list = g_getenv ("CLUTTER_BACKEND");
  if (backends_list == NULL)
    backends_list = allowed_backends;

  backends = g_strsplit (backends_list, ",", 0);

  retval = NULL;

  for (i = 0; retval == NULL && backends[i] != NULL; i++)
    {
      const char *backend = backends[i];
      gboolean is_any = g_str_equal (backend, "*");
      int j;

      for (j = 0; available_backends[j].name != NULL; j++)
        {
          if ((is_any && allow_any) ||
              (is_any && strstr (allowed_backends, available_backends[j].name)) ||
              g_str_equal (backend, available_backends[j].name))
            {
              retval = available_backends[j].create_backend ();
              if (retval != NULL)
                break;
            }
        }
    }

  g_strfreev (backends);

  if (retval == NULL)
    g_error ("No default Clutter backend found.");

  return retval;
}

static void
clutter_backend_real_init_events (ClutterBackend *backend)
{
  const char *input_backend = NULL;

  input_backend = g_getenv ("CLUTTER_INPUT_BACKEND");
  if (input_backend != NULL)
    input_backend = g_intern_string (input_backend);

#ifdef CLUTTER_INPUT_X11
  if (clutter_check_windowing_backend (CLUTTER_WINDOWING_X11) &&
      (input_backend == NULL || input_backend == I_(CLUTTER_INPUT_X11)))
    {
      _clutter_backend_x11_events_init (backend);
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
  GList *l;

  for (l = backend->event_translators;
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
  klass->get_features = clutter_backend_real_get_features;
}

static void
clutter_backend_init (ClutterBackend *self)
{
  self->units_per_em = -1.0;
  self->units_serial = 1;

  self->dummy_onscreen = COGL_INVALID_HANDLE;
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
  /* recompute for the font description, but do not cache the result */
  if (font_desc != NULL)
    return get_units_per_em (backend, font_desc);

  if (backend->units_per_em < 0)
    backend->units_per_em = get_units_per_em (backend, NULL);

  return backend->units_per_em;
}

void
_clutter_backend_copy_event_data (ClutterBackend     *backend,
                                  const ClutterEvent *src,
                                  ClutterEvent       *dest)
{
  ClutterEventExtenderInterface *iface;
  ClutterBackendClass *klass;

  klass = CLUTTER_BACKEND_GET_CLASS (backend);
  if (CLUTTER_IS_EVENT_EXTENDER (backend->device_manager))
    {
      iface = CLUTTER_EVENT_EXTENDER_GET_IFACE (backend->device_manager);
      iface->copy_event_data (CLUTTER_EVENT_EXTENDER (backend->device_manager),
                              src, dest);
    }
  else if (klass->copy_event_data != NULL)
    klass->copy_event_data (backend, src, dest);
}

void
_clutter_backend_free_event_data (ClutterBackend *backend,
                                  ClutterEvent   *event)
{
  ClutterEventExtenderInterface *iface;
  ClutterBackendClass *klass;

  klass = CLUTTER_BACKEND_GET_CLASS (backend);

  if (CLUTTER_IS_EVENT_EXTENDER (backend->device_manager))
    {
      iface = CLUTTER_EVENT_EXTENDER_GET_IFACE (backend->device_manager);
      iface->free_event_data (CLUTTER_EVENT_EXTENDER (backend->device_manager),
                              event);
    }
  else if (klass->free_event_data != NULL)
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
  g_return_if_fail (CLUTTER_IS_BACKEND (backend));

  if (backend->font_options != options)
    {
      if (backend->font_options)
        cairo_font_options_destroy (backend->font_options);

      if (options)
        backend->font_options = cairo_font_options_copy (options);
      else
        backend->font_options = NULL;

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
  g_return_val_if_fail (CLUTTER_IS_BACKEND (backend), NULL);

  if (G_LIKELY (backend->font_options))
    return backend->font_options;

  backend->font_options = cairo_font_options_create ();

  cairo_font_options_set_hint_style (backend->font_options, CAIRO_HINT_STYLE_NONE);
  cairo_font_options_set_subpixel_order (backend->font_options, CAIRO_SUBPIXEL_ORDER_DEFAULT);
  cairo_font_options_set_antialias (backend->font_options, CAIRO_ANTIALIAS_DEFAULT);

  g_signal_emit (backend, backend_signals[FONT_CHANGED], 0);

  return backend->font_options;
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
  ClutterSettings *settings;

  g_return_val_if_fail (CLUTTER_IS_BACKEND (backend), NULL);

  settings = clutter_settings_get_default ();

  /* XXX yuck. but we return a const pointer, so we need to
   * store it in the backend
   */
  g_free (backend->font_name);
  g_object_get (settings, "font-name", &backend->font_name, NULL);

  return backend->font_name;
}

gint32
_clutter_backend_get_units_serial (ClutterBackend *backend)
{
  return backend->units_serial;
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
  if (g_list_find (backend->event_translators, translator) != NULL)
    return;

  backend->event_translators =
    g_list_prepend (backend->event_translators, translator);
}

void
_clutter_backend_remove_event_translator (ClutterBackend         *backend,
                                          ClutterEventTranslator *translator)
{
  if (g_list_find (backend->event_translators, translator) == NULL)
    return;

  backend->event_translators =
    g_list_remove (backend->event_translators, translator);
}

/**
 * clutter_backend_get_cogl_context: (skip)
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

#ifdef CLUTTER_HAS_WAYLAND_COMPOSITOR_SUPPORT
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
 * @backend_type: a comma separated list of windowing backends
 *
 * Restricts Clutter to only use the specified backend or list of backends.
 *
 * You can use one of the `CLUTTER_WINDOWING_*` symbols, e.g.
 *
 * |[<!-- language="C" -->
 *   clutter_set_windowing_backend (CLUTTER_WINDOWING_X11);
 * ]|
 *
 * Will force Clutter to use the X11 windowing and input backend, and terminate
 * if the X11 backend could not be initialized successfully.
 *
 * Since Clutter 1.26, you can also use a comma-separated list of windowing
 * system backends to provide a fallback in case backends are not available or
 * enabled, e.g.:
 *
 * |[<!-- language="C" -->
 *   clutter_set_windowing_backend ("gdk,wayland,x11");
 * ]|
 *
 * Will make Clutter test for the GDK, Wayland, and X11 backends in that order.
 *
 * You can use the `*` special value to ask Clutter to use the internally
 * defined list of backends. For instance:
 *
 * |[<!-- language="C" -->
 *   clutter_set_windowing_backend ("x11,wayland,*");
 * ]|
 *
 * Will make Clutter test the X11 and Wayland backends, and then fall back
 * to the internal list of available backends.
 *
 * This function must be called before the first API call to Clutter, including
 * clutter_get_option_context()
 *
 * Since: 1.16
 */
void
clutter_set_windowing_backend (const char *backend_type)
{
  g_return_if_fail (backend_type != NULL);

  allowed_backends = g_strdup (backend_type);
}

void
clutter_try_set_windowing_backend (const char *backend_type)
{
  if (allowed_backends == NULL)
    clutter_set_windowing_backend (backend_type);
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

void
_clutter_backend_reset_cogl_framebuffer (ClutterBackend *backend)
{
  if (backend->dummy_onscreen == COGL_INVALID_HANDLE)
    {
      CoglError *internal_error = NULL;

      backend->dummy_onscreen = cogl_onscreen_new (backend->cogl_context, 1, 1);

      if (!cogl_framebuffer_allocate (COGL_FRAMEBUFFER (backend->dummy_onscreen),
                                      &internal_error))
        {
          g_critical ("Unable to create dummy onscreen: %s", internal_error->message);
          cogl_error_free (internal_error);
          return;
        }
    }

  cogl_set_framebuffer (COGL_FRAMEBUFFER (backend->dummy_onscreen));
}

void
clutter_set_allowed_drivers (const char *drivers)
{
  if (_clutter_context_is_initialized ())
    {
      g_warning ("Clutter has already been initialized.\n");
      return;
    }

  allowed_drivers = g_strdup (drivers);
}
