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

#include <stdlib.h>
#include <string.h>

#include "cogl-util.h"
#include "cogl-private.h"
#include "cogl-object.h"
#include "cogl-context-private.h"
#include "cogl-util-gl-private.h"

#include "cogl-renderer.h"
#include "cogl-renderer-private.h"
#include "cogl-display-private.h"
#include "cogl-winsys-private.h"
#include "cogl-winsys-stub-private.h"
#include "cogl-config-private.h"
#include "cogl-error-private.h"

#ifdef COGL_HAS_EGL_PLATFORM_XLIB_SUPPORT
#include "cogl-winsys-egl-x11-private.h"
#endif
#ifdef COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
#include "cogl-winsys-egl-wayland-private.h"
#endif
#ifdef COGL_HAS_EGL_PLATFORM_KMS_SUPPORT
#include "cogl-winsys-egl-kms-private.h"
#endif
#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
#include "cogl-winsys-egl-gdl-private.h"
#endif
#ifdef COGL_HAS_EGL_PLATFORM_ANDROID_SUPPORT
#include "cogl-winsys-egl-android-private.h"
#endif
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT
#include "cogl-winsys-egl-null-private.h"
#endif
#ifdef COGL_HAS_GLX_SUPPORT
#include "cogl-winsys-glx-private.h"
#endif
#ifdef COGL_HAS_WGL_SUPPORT
#include "cogl-winsys-wgl-private.h"
#endif
#ifdef COGL_HAS_SDL_SUPPORT
#include "cogl-winsys-sdl-private.h"
#endif

#ifdef COGL_HAS_XLIB_SUPPORT
#include "cogl-xlib-renderer.h"
#endif

typedef const CoglWinsysVtable *(*CoglWinsysVtableGetter) (void);

#ifdef HAVE_COGL_GL
extern const CoglTextureDriver _cogl_texture_driver_gl;
extern const CoglDriverVtable _cogl_driver_gl;
#endif
#if defined (HAVE_COGL_GLES) || defined (HAVE_COGL_GLES2)
extern const CoglTextureDriver _cogl_texture_driver_gles;
extern const CoglDriverVtable _cogl_driver_gles;
#endif

extern const CoglDriverVtable _cogl_driver_nop;

typedef struct _CoglDriverDescription
{
  CoglDriver id;
  const char *name;
  CoglRendererConstraint constraints;
  CoglPrivateFeatureFlags private_feature_flags;
  const CoglDriverVtable *vtable;
  const CoglTextureDriver *texture_driver;
  const char *libgl_name;
} CoglDriverDescription;

_COGL_STATIC_ASSERT(sizeof (CoglPrivateFeatureFlags) ==
                    sizeof (unsigned long),
                    "Private feature flags don't fit in long");

static CoglDriverDescription _cogl_drivers[] =
{
#ifdef HAVE_COGL_GL
  {
    COGL_DRIVER_GL,
    "gl",
    0,
    COGL_PRIVATE_FEATURE_ANY_GL |
      COGL_PRIVATE_FEATURE_GL_FIXED |
      COGL_PRIVATE_FEATURE_GL_PROGRAMMABLE,
    &_cogl_driver_gl,
    &_cogl_texture_driver_gl,
    COGL_GL_LIBNAME,
  },
  {
    COGL_DRIVER_GL3,
    "gl3",
    0,
    COGL_PRIVATE_FEATURE_ANY_GL |
      COGL_PRIVATE_FEATURE_GL_PROGRAMMABLE,
    &_cogl_driver_gl,
    &_cogl_texture_driver_gl,
    COGL_GL_LIBNAME,
  },
#endif
#ifdef HAVE_COGL_GLES2
  {
    COGL_DRIVER_GLES2,
    "gles2",
    COGL_RENDERER_CONSTRAINT_SUPPORTS_COGL_GLES2,
    COGL_PRIVATE_FEATURE_ANY_GL |
      COGL_PRIVATE_FEATURE_GL_EMBEDDED |
      COGL_PRIVATE_FEATURE_GL_PROGRAMMABLE,
    &_cogl_driver_gles,
    &_cogl_texture_driver_gles,
    COGL_GLES2_LIBNAME,
  },
#endif
#ifdef HAVE_COGL_GLES
  {
    COGL_DRIVER_GLES1,
    "gles1",
    0,
    COGL_PRIVATE_FEATURE_ANY_GL |
      COGL_PRIVATE_FEATURE_GL_EMBEDDED |
      COGL_PRIVATE_FEATURE_GL_FIXED,
    &_cogl_driver_gles,
    &_cogl_texture_driver_gles,
    COGL_GLES1_LIBNAME,
  },
#endif
#ifdef USING_EMSCRIPTEN
  {
    COGL_DRIVER_WEBGL,
    "webgl",
    0,
    COGL_PRIVATE_FEATURE_ANY_GL |
      COGL_PRIVATE_FEATURE_GL_EMBEDDED |
      COGL_PRIVATE_FEATURE_GL_PROGRAMMABLE |
      COGL_PRIVATE_FEATURE_GL_WEB,
    &_cogl_driver_gles,
    &_cogl_texture_driver_gles,
    NULL,
  },
#endif
  {
    COGL_DRIVER_NOP,
    "nop",
    0, /* constraints satisfied */
    0, /* flags */
    &_cogl_driver_nop,
    NULL, /* texture driver */
    NULL /* libgl_name */
  }
};

static CoglWinsysVtableGetter _cogl_winsys_vtable_getters[] =
{
#ifdef COGL_HAS_GLX_SUPPORT
  _cogl_winsys_glx_get_vtable,
#endif
#ifdef COGL_HAS_EGL_PLATFORM_XLIB_SUPPORT
  _cogl_winsys_egl_xlib_get_vtable,
#endif
#ifdef COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
  _cogl_winsys_egl_wayland_get_vtable,
#endif
#ifdef COGL_HAS_EGL_PLATFORM_KMS_SUPPORT
  _cogl_winsys_egl_kms_get_vtable,
#endif
#ifdef COGL_HAS_EGL_PLATFORM_GDL_SUPPORT
  _cogl_winsys_egl_gdl_get_vtable,
#endif
#ifdef COGL_HAS_EGL_PLATFORM_ANDROID_SUPPORT
  _cogl_winsys_egl_android_get_vtable,
#endif
#ifdef COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT
  _cogl_winsys_egl_null_get_vtable,
#endif
#ifdef COGL_HAS_WGL_SUPPORT
  _cogl_winsys_wgl_get_vtable,
#endif
#ifdef COGL_HAS_SDL_SUPPORT
  _cogl_winsys_sdl_get_vtable,
#endif
  _cogl_winsys_stub_get_vtable,
};

static void _cogl_renderer_free (CoglRenderer *renderer);

COGL_OBJECT_DEFINE (Renderer, renderer);

typedef struct _CoglNativeFilterClosure
{
  CoglNativeFilterFunc func;
  void *data;
} CoglNativeFilterClosure;

uint32_t
cogl_renderer_error_quark (void)
{
  return g_quark_from_static_string ("cogl-renderer-error-quark");
}

static const CoglWinsysVtable *
_cogl_renderer_get_winsys (CoglRenderer *renderer)
{
  return renderer->winsys_vtable;
}

static void
native_filter_closure_free (CoglNativeFilterClosure *closure)
{
  g_slice_free (CoglNativeFilterClosure, closure);
}

static void
_cogl_renderer_free (CoglRenderer *renderer)
{
  const CoglWinsysVtable *winsys = _cogl_renderer_get_winsys (renderer);

  _cogl_closure_list_disconnect_all (&renderer->idle_closures);

  if (winsys)
    winsys->renderer_disconnect (renderer);

#ifndef HAVE_DIRECTLY_LINKED_GL_LIBRARY
  if (renderer->libgl_module)
    g_module_close (renderer->libgl_module);
#endif

  g_slist_foreach (renderer->event_filters,
                   (GFunc) native_filter_closure_free,
                   NULL);
  g_slist_free (renderer->event_filters);

  g_array_free (renderer->poll_fds, TRUE);

  g_free (renderer);
}

CoglRenderer *
cogl_renderer_new (void)
{
  CoglRenderer *renderer = g_new0 (CoglRenderer, 1);

  _cogl_init ();

  renderer->connected = FALSE;
  renderer->event_filters = NULL;

  renderer->poll_fds = g_array_new (FALSE, TRUE, sizeof (CoglPollFD));

  _cogl_list_init (&renderer->idle_closures);

#ifdef COGL_HAS_XLIB_SUPPORT
  renderer->xlib_enable_event_retrieval = TRUE;
#endif

#ifdef COGL_HAS_WIN32_SUPPORT
  renderer->win32_enable_event_retrieval = TRUE;
#endif

#ifdef COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT
  renderer->wayland_enable_event_dispatch = TRUE;
#endif

  return _cogl_renderer_object_new (renderer);
}

#ifdef COGL_HAS_XLIB_SUPPORT
void
cogl_xlib_renderer_set_foreign_display (CoglRenderer *renderer,
                                        Display *xdisplay)
{
  _COGL_RETURN_IF_FAIL (cogl_is_renderer (renderer));

  /* NB: Renderers are considered immutable once connected */
  _COGL_RETURN_IF_FAIL (!renderer->connected);

  renderer->foreign_xdpy = xdisplay;

  /* If the application is using a foreign display then we can assume
     it will also do its own event retrieval */
  cogl_xlib_renderer_set_event_retrieval_enabled (renderer, FALSE);
}

Display *
cogl_xlib_renderer_get_foreign_display (CoglRenderer *renderer)
{
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_renderer (renderer), NULL);

  return renderer->foreign_xdpy;
}

void
cogl_xlib_renderer_set_event_retrieval_enabled (CoglRenderer *renderer,
                                                CoglBool enable)
{
  _COGL_RETURN_IF_FAIL (cogl_is_renderer (renderer));
  /* NB: Renderers are considered immutable once connected */
  _COGL_RETURN_IF_FAIL (!renderer->connected);

  renderer->xlib_enable_event_retrieval = enable;
}
#endif /* COGL_HAS_XLIB_SUPPORT */

CoglBool
cogl_renderer_check_onscreen_template (CoglRenderer *renderer,
                                       CoglOnscreenTemplate *onscreen_template,
                                       CoglError **error)
{
  CoglDisplay *display;

  if (!cogl_renderer_connect (renderer, error))
    return FALSE;

  display = cogl_display_new (renderer, onscreen_template);
  if (!cogl_display_setup (display, error))
    {
      cogl_object_unref (display);
      return FALSE;
    }

  cogl_object_unref (display);

  return TRUE;
}

typedef CoglBool (*DriverCallback) (CoglDriverDescription *description,
                                    void *user_data);

static void
foreach_driver_description (CoglDriver driver_override,
                            DriverCallback callback,
                            void *user_data)
{
#ifdef COGL_DEFAULT_DRIVER
  const CoglDriverDescription *default_driver = NULL;
#endif
  int i;

  if (driver_override != COGL_DRIVER_ANY)
    {
      for (i = 0; i < G_N_ELEMENTS (_cogl_drivers); i++)
        {
          if (_cogl_drivers[i].id == driver_override)
            {
              callback (&_cogl_drivers[i], user_data);
              return;
            }
        }

      g_warn_if_reached ();
      return;
    }

#ifdef COGL_DEFAULT_DRIVER
  for (i = 0; i < G_N_ELEMENTS (_cogl_drivers); i++)
    {
      const CoglDriverDescription *desc = &_cogl_drivers[i];
      if (g_ascii_strcasecmp (desc->name, COGL_DEFAULT_DRIVER) == 0)
        {
          default_driver = desc;
          break;
        }
    }

  if (default_driver)
    {
      if (!callback (default_driver, user_data))
        return;
    }
#endif

  for (i = 0; i < G_N_ELEMENTS (_cogl_drivers); i++)
    {
#ifdef COGL_DEFAULT_DRIVER
      if (&_cogl_drivers[i] == default_driver)
        continue;
#endif

      if (!callback (&_cogl_drivers[i], user_data))
        return;
    }
}

static CoglDriver
driver_name_to_id (const char *name)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (_cogl_drivers); i++)
    {
      if (g_ascii_strcasecmp (_cogl_drivers[i].name, name) == 0)
        return _cogl_drivers[i].id;
    }

  return COGL_DRIVER_ANY;
}

static const char *
driver_id_to_name (CoglDriver id)
{
  switch (id)
    {
      case COGL_DRIVER_GL:
        return "gl";
      case COGL_DRIVER_GL3:
        return "gl3";
      case COGL_DRIVER_GLES1:
        return "gles1";
      case COGL_DRIVER_GLES2:
        return "gles2";
      case COGL_DRIVER_WEBGL:
        return "webgl";
      case COGL_DRIVER_NOP:
        return "nop";
      case COGL_DRIVER_ANY:
        g_warn_if_reached ();
        return "any";
    }

  g_warn_if_reached ();
  return "unknown";
}

typedef struct _SatisfyConstraintsState
{
  GList *constraints;
  const CoglDriverDescription *driver_description;
} SatisfyConstraintsState;

static CoglBool
satisfy_constraints (CoglDriverDescription *description,
                     void *user_data)
{
  SatisfyConstraintsState *state = user_data;
  GList *l;

  for (l = state->constraints; l; l = l->next)
    {
      CoglRendererConstraint constraint = GPOINTER_TO_UINT (l->data);

      /* If the driver doesn't satisfy any constraint then continue
       * to the next driver description */
      if (!(constraint & description->constraints))
        return TRUE;
    }

  state->driver_description = description;

  return FALSE;
}

static CoglBool
_cogl_renderer_choose_driver (CoglRenderer *renderer,
                              CoglError **error)
{
  const char *driver_name = g_getenv ("COGL_DRIVER");
  CoglDriver driver_override = COGL_DRIVER_ANY;
  const char *invalid_override = NULL;
  const char *libgl_name;
  SatisfyConstraintsState state;
  const CoglDriverDescription *desc;

  if (!driver_name)
    driver_name = _cogl_config_driver;

  if (driver_name)
    {
      driver_override = driver_name_to_id (driver_name);
      if (driver_override == COGL_DRIVER_ANY)
        invalid_override = driver_name;
    }
  else
    driver_override = renderer->driver_override;

  if (driver_override != COGL_DRIVER_ANY)
    {
      CoglBool found = FALSE;
      int i;

      for (i = 0; i < G_N_ELEMENTS (_cogl_drivers); i++)
        {
          if (_cogl_drivers[i].id == driver_override)
            {
              found = TRUE;
              break;
            }
        }
      if (!found)
        invalid_override = driver_id_to_name (driver_override);
    }

  if (invalid_override)
    {
      _cogl_set_error (error,
                       COGL_RENDERER_ERROR,
                       COGL_RENDERER_ERROR_BAD_CONSTRAINT,
                       "Driver \"%s\" is not available",
                       invalid_override);
      return FALSE;
    }

  state.driver_description = NULL;
  state.constraints = renderer->constraints;

  foreach_driver_description (driver_override,
                              satisfy_constraints,
                              &state);

  if (!state.driver_description)
    {
      _cogl_set_error (error,
                       COGL_RENDERER_ERROR,
                       COGL_RENDERER_ERROR_BAD_CONSTRAINT,
                       "No suitable driver found");
      return FALSE;
    }

  desc = state.driver_description;
  renderer->driver = desc->id;
  renderer->driver_vtable = desc->vtable;
  renderer->texture_driver = desc->texture_driver;
  renderer->private_feature_flags = desc->private_feature_flags;
  libgl_name = desc->libgl_name;

#ifndef HAVE_DIRECTLY_LINKED_GL_LIBRARY

  if (renderer->private_feature_flags & COGL_PRIVATE_FEATURE_ANY_GL)
    {
      renderer->libgl_module = g_module_open (libgl_name,
                                              G_MODULE_BIND_LAZY);

      if (renderer->libgl_module == NULL)
        {
          _cogl_set_error (error, COGL_DRIVER_ERROR,
                       COGL_DRIVER_ERROR_FAILED_TO_LOAD_LIBRARY,
                       "Failed to dynamically open the GL library \"%s\"",
                       libgl_name);
          return FALSE;
        }
    }

#endif /* HAVE_DIRECTLY_LINKED_GL_LIBRARY */

  return TRUE;
}

/* Final connection API */

CoglBool
cogl_renderer_connect (CoglRenderer *renderer, CoglError **error)
{
  int i;
  GString *error_message;
  CoglBool constraints_failed = FALSE;

  if (renderer->connected)
    return TRUE;

  /* The driver needs to be chosen before connecting the renderer
     because eglInitialize requires the library containing the GL API
     to be loaded before its called */
  if (!_cogl_renderer_choose_driver (renderer, error))
    return FALSE;

  error_message = g_string_new ("");
  for (i = 0; i < G_N_ELEMENTS (_cogl_winsys_vtable_getters); i++)
    {
      const CoglWinsysVtable *winsys = _cogl_winsys_vtable_getters[i]();
      CoglError *tmp_error = NULL;
      GList *l;
      CoglBool skip_due_to_constraints = FALSE;

      if (renderer->winsys_id_override != COGL_WINSYS_ID_ANY)
        {
          if (renderer->winsys_id_override != winsys->id)
            continue;
        }
      else
        {
          char *user_choice = getenv ("COGL_RENDERER");
          if (!user_choice)
            user_choice = _cogl_config_renderer;
          if (user_choice &&
              g_ascii_strcasecmp (winsys->name, user_choice) != 0)
            continue;
        }

      for (l = renderer->constraints; l; l = l->next)
        {
          CoglRendererConstraint constraint = GPOINTER_TO_UINT (l->data);
          if (!(winsys->constraints & constraint))
            {
              skip_due_to_constraints = TRUE;
              break;
            }
        }
      if (skip_due_to_constraints)
        {
          constraints_failed |= TRUE;
          continue;
        }

      /* At least temporarily we will associate this winsys with
       * the renderer in-case ->renderer_connect calls API that
       * wants to query the current winsys... */
      renderer->winsys_vtable = winsys;

      if (!winsys->renderer_connect (renderer, &tmp_error))
        {
          g_string_append_c (error_message, '\n');
          g_string_append (error_message, tmp_error->message);
          cogl_error_free (tmp_error);
        }
      else
        {
          renderer->connected = TRUE;
          g_string_free (error_message, TRUE);
          return TRUE;
        }
    }

  if (!renderer->connected)
    {
      if (constraints_failed)
        {
          _cogl_set_error (error, COGL_RENDERER_ERROR,
                       COGL_RENDERER_ERROR_BAD_CONSTRAINT,
                       "Failed to connected to any renderer due to constraints");
          return FALSE;
        }

      renderer->winsys_vtable = NULL;
      _cogl_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "Failed to connected to any renderer: %s",
                   error_message->str);
      g_string_free (error_message, TRUE);
      return FALSE;
    }

  return TRUE;
}

CoglFilterReturn
_cogl_renderer_handle_native_event (CoglRenderer *renderer,
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
_cogl_renderer_add_native_filter (CoglRenderer *renderer,
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
_cogl_renderer_remove_native_filter (CoglRenderer *renderer,
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

void
cogl_renderer_set_winsys_id (CoglRenderer *renderer,
                             CoglWinsysID winsys_id)
{
  _COGL_RETURN_IF_FAIL (!renderer->connected);

  renderer->winsys_id_override = winsys_id;
}

CoglWinsysID
cogl_renderer_get_winsys_id (CoglRenderer *renderer)
{
  _COGL_RETURN_VAL_IF_FAIL (renderer->connected, 0);

  return renderer->winsys_vtable->id;
}

void *
_cogl_renderer_get_proc_address (CoglRenderer *renderer,
                                 const char *name,
                                 CoglBool in_core)
{
  const CoglWinsysVtable *winsys = _cogl_renderer_get_winsys (renderer);

  return winsys->renderer_get_proc_address (renderer, name, in_core);
}

int
cogl_renderer_get_n_fragment_texture_units (CoglRenderer *renderer)
{
  int n = 0;

  _COGL_GET_CONTEXT (ctx, 0);

#if defined (HAVE_COGL_GL) || defined (HAVE_COGL_GLES2)
  if (cogl_has_feature (ctx, COGL_FEATURE_ID_GLSL) ||
      cogl_has_feature (ctx, COGL_FEATURE_ID_ARBFP))
    GE (ctx, glGetIntegerv (GL_MAX_TEXTURE_IMAGE_UNITS, &n));
#endif

  return n;
}

void
cogl_renderer_add_constraint (CoglRenderer *renderer,
                              CoglRendererConstraint constraint)
{
  g_return_if_fail (!renderer->connected);
  renderer->constraints = g_list_prepend (renderer->constraints,
                                          GUINT_TO_POINTER (constraint));
}

void
cogl_renderer_remove_constraint (CoglRenderer *renderer,
                                 CoglRendererConstraint constraint)
{
  g_return_if_fail (!renderer->connected);
  renderer->constraints = g_list_remove (renderer->constraints,
                                         GUINT_TO_POINTER (constraint));
}

void
cogl_renderer_set_driver (CoglRenderer *renderer,
                          CoglDriver driver)
{
  _COGL_RETURN_IF_FAIL (!renderer->connected);
  renderer->driver_override = driver;
}

CoglDriver
cogl_renderer_get_driver (CoglRenderer *renderer)
{
  _COGL_RETURN_VAL_IF_FAIL (renderer->connected, 0);

  return renderer->driver;
}

void
cogl_renderer_foreach_output (CoglRenderer *renderer,
                              CoglOutputCallback callback,
                              void *user_data)
{
  GList *l;

  _COGL_RETURN_IF_FAIL (renderer->connected);
  _COGL_RETURN_IF_FAIL (callback != NULL);

  for (l = renderer->outputs; l; l = l->next)
    callback (l->data, user_data);
}
