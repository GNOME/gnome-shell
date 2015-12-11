/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2014 Canonical Ltd.
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
 *
 * Authors:
 *  Marco Trevisan <marco.trevisan@canonical.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-debug.h"
#include "clutter-main.h"
#include "clutter-stage-private.h"

#include "mir/clutter-backend-mir-priv.h"
#include "mir/clutter-backend-mir.h"
#include "mir/clutter-device-manager-mir.h"
#include "mir/clutter-event-mir.h"
#include "mir/clutter-stage-mir.h"
#include "mir/clutter-mir.h"

G_DEFINE_TYPE (ClutterBackendMir, clutter_backend_mir, CLUTTER_TYPE_BACKEND);

static MirConnection *_foreign_connection = NULL;
static gboolean _no_event_dispatch = FALSE;

static gboolean
clutter_backend_mir_post_parse (ClutterBackend  *backend,
                                GError         **error)
{
  ClutterBackendMir *backend_mir = CLUTTER_BACKEND_MIR (backend);

  backend_mir->mir_connection = _foreign_connection;
  if (backend_mir->mir_connection == NULL)
    backend_mir->mir_connection = mir_connect_sync (NULL, "Clutter");

  if (!mir_connection_is_valid (backend_mir->mir_connection))
    {
      g_set_error (error, CLUTTER_INIT_ERROR,
                   CLUTTER_INIT_ERROR_BACKEND,
                   "Failed to open Mir display socket %s",
                   mir_connection_get_error_message (backend_mir->mir_connection));
      mir_connection_release (backend_mir->mir_connection);
      return FALSE;
    }

  g_object_set (clutter_settings_get_default (), "font-dpi", 96 * 1024, NULL);

  return TRUE;
}

static CoglRenderer *
clutter_backend_mir_get_renderer (ClutterBackend  *backend,
                                  GError         **error)
{
  ClutterBackendMir *backend_mir = CLUTTER_BACKEND_MIR (backend);
  CoglRenderer *renderer;

  CLUTTER_NOTE (BACKEND, "Creating a new Mir renderer");

  renderer = cogl_renderer_new ();

  cogl_renderer_set_winsys_id (renderer, COGL_WINSYS_ID_EGL_MIR);
  cogl_mir_renderer_set_foreign_connection (renderer,
                                            backend_mir->mir_connection);

  return renderer;
}

static CoglDisplay *
clutter_backend_mir_get_display (ClutterBackend  *backend,
                                 CoglRenderer    *renderer,
                                 CoglSwapChain   *swap_chain,
                                 GError         **error)
{
  CoglOnscreenTemplate *onscreen_template = NULL;
  CoglDisplay *display;

  onscreen_template = cogl_onscreen_template_new (swap_chain);

  if (!cogl_renderer_check_onscreen_template (renderer,
                                              onscreen_template,
                                              error))
    goto error;

  display = cogl_display_new (renderer, onscreen_template);

  return display;

error:
  if (onscreen_template)
    cogl_object_unref (onscreen_template);

  return NULL;
}

static void
on_mir_event_cb (CoglMirEvent *mir_event,
                 void *data)
{
  ClutterBackend *backend = data;
  _clutter_mir_handle_event (backend, mir_event->surface, mir_event->event);
}

void
_clutter_events_mir_init (ClutterBackend *backend)
{
  ClutterBackendMir *backend_mir = CLUTTER_BACKEND_MIR (backend);
  CoglRenderer *cogl_renderer = backend->cogl_renderer;

  backend->device_manager = _clutter_device_manager_mir_new (backend);

  if (_no_event_dispatch)
    return;

  cogl_mir_renderer_add_event_listener (cogl_renderer, on_mir_event_cb, backend);
  backend_mir->mir_source = _clutter_event_source_mir_new ();
}


static void
clutter_backend_mir_init (ClutterBackendMir *backend_mir)
{
}

static void
clutter_backend_mir_dispose (GObject *gobject)
{
  ClutterBackend *backend = CLUTTER_BACKEND (gobject);
  ClutterBackendMir *backend_mir = CLUTTER_BACKEND_MIR (backend);
  CoglRenderer *cogl_renderer = backend->cogl_renderer;

  g_clear_object (&backend->device_manager);
  g_clear_pointer (&backend_mir->mir_source, g_source_unref);
  cogl_mir_renderer_remove_event_listener (cogl_renderer, on_mir_event_cb,
                                           backend);

  G_OBJECT_CLASS (clutter_backend_mir_parent_class)->dispose (gobject);
}

static void
clutter_backend_mir_class_init (ClutterBackendMirClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->dispose = clutter_backend_mir_dispose;

  backend_class->stage_window_type = CLUTTER_TYPE_STAGE_MIR;

  backend_class->post_parse = clutter_backend_mir_post_parse;
  backend_class->get_renderer = clutter_backend_mir_get_renderer;
  backend_class->get_display = clutter_backend_mir_get_display;
}

ClutterBackend *
clutter_backend_mir_new (void)
{
  return g_object_new (CLUTTER_TYPE_BACKEND_MIR, NULL);
}

/**
 * clutter_mir_set_connection:
 * @connection: pointer to a mir connection
 *
 * Sets the display connection Clutter should use; must be called
 * before clutter_init(), clutter_init_with_args() or other functions
 * pertaining Clutter's initialization process.
 *
 * If you are parsing the command line arguments by retrieving Clutter's
 * #GOptionGroup with clutter_get_option_group() and calling
 * g_option_context_parse() yourself, you should also call
 * clutter_mir_set_connection() before g_option_context_parse().
 *
 * Since: 1.22
 */
void
clutter_mir_set_connection (MirConnection *connection)
{
  g_return_if_fail (mir_connection_is_valid (connection));

  if (_clutter_context_is_initialized ())
    {
      g_warning ("%s() can only be used before calling clutter_init()",
                 G_STRFUNC);
      return;
    }

  _foreign_connection = connection;
}

/**
 * clutter_mir_disable_event_retrieval:
 *
 * Disables the dispatch of the events in the main loop.
 *
 * This is useful for integrating Clutter with another library that will do the
 * event dispatch;
 *
 * This function can only be called before calling clutter_init().
 *
 * This function should not be normally used by applications.
 *
 * Since: 1.22
 */
void
clutter_mir_disable_event_retrieval (void)
{
  if (_clutter_context_is_initialized ())
    {
      g_warning ("%s() can only be used before calling clutter_init()",
                 G_STRFUNC);
      return;
    }

  _no_event_dispatch = TRUE;
}
