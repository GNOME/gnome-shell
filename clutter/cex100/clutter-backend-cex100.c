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

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>

#include "clutter-backend-eglnative.h"

/* This is a Cogl based backend */
#include "cogl/clutter-stage-cogl.h"

#ifdef HAVE_EVDEV
#include "clutter-device-manager-evdev.h"
#endif

#include "clutter-debug.h"
#include "clutter-private.h"
#include "clutter-main.h"
#include "clutter-stage-private.h"

#ifdef COGL_HAS_EGL_SUPPORT
#include "clutter-egl.h"
#endif

#include "clutter-cex100.h"

static gdl_plane_id_t gdl_plane = GDL_PLANE_ID_UPP_C;
static guint gdl_n_buffers = CLUTTER_CEX100_TRIPLE_BUFFERING;

#define clutter_backend_cex100_get_type     _clutter_backend_cex100_get_type

G_DEFINE_TYPE (ClutterBackendCex100, clutter_backend_cex100, CLUTTER_TYPE_BACKEND);

static void
clutter_backend_cex100_dispose (GObject *gobject)
{
  ClutterBackendCex100 *backend_cex100 = CLUTTER_BACKEND_CEX100 (gobject);

  if (backend_cex100->event_timer != NULL)
    {
      g_timer_destroy (backend_cex100->event_timer);
      backend_cex100->event_timer = NULL;
    }

  G_OBJECT_CLASS (clutter_backend_cex100_parent_class)->dispose (gobject);
}

static CoglDisplay *
clutter_backend_cex100_get_display (ClutterBackend  *backend,
                                    CoglRenderer    *renderer,
                                    CoglSwapChain   *swap_chain,
                                    GError         **error)
{
  CoglOnscreenTemplate *onscreen_template = NULL;
  CoglDisplay *display;

  swap_chain = cogl_swap_chain_new ();

#if defined(COGL_HAS_GDL_SUPPORT)
  cogl_swap_chain_set_length (swap_chain, gdl_n_buffers);
#endif

  onscreen_template = cogl_onscreen_template_new (swap_chain);

  /* XXX: I have some doubts that this is a good design.
   * Conceptually should we be able to check an onscreen_template
   * without more details about the CoglDisplay configuration?
   */
  if (!cogl_renderer_check_onscreen_template (renderer,
                                              onscreen_template,
                                              error))
    goto error;

  display = cogl_display_new (renderer, onscreen_template);

#if defined(COGL_HAS_GDL_SUPPORT)
  cogl_gdl_display_set_plane (cogl_display, gdl_plane);
#endif

  return display;
}

static void
clutter_backend_cex100_class_init (ClutterBackendCex100Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->dispose = clutter_backend_cex100_dispose;

  backend_class->stage_window_type = CLUTTER_TYPE_STAGE_COGL;

  backend_class->get_display = clutter_backend_cex100_get_display;
}

static void
clutter_backend_cex100_init (ClutterBackendCex100 *backend_cex100)
{
  backend_cex100->event_timer = g_timer_new ();
}

/**
 * clutter_cex100_set_plane:
 * @plane: a GDL plane
 *
 * Intel CE3100 and CE4100 have several planes (frame buffers) and a
 * hardware blender to blend the planes togeteher and produce the final
 * image.
 *
 * clutter_cex100_set_plane() let's you configure the GDL plane where
 * the stage will be drawn. By default Clutter will pick UPP_C
 * (GDL_PLANE_ID_UPP_C).
 *
 * This function has to be called before clutter_init().
 *
 * Since: 1.6
 */
void
clutter_cex100_set_plane (gdl_plane_id_t plane)
{
  g_return_if_fail (plane >= GDL_PLANE_ID_UPP_A && plane <= GDL_PLANE_ID_UPP_E);

  gdl_plane = plane;
}

/**
 * clutter_cex100_set_buffering_mode:
 * @mode: a #ClutterCex100BufferingMode
 *
 * Configure the buffering mode of the underlying GDL plane. The GDL
 * surface used by Clutter to draw can be backed up by either one or two
 * back buffers thus being double or triple buffered, respectively.
 *
 * Clutter defaults to %CLUTTER_CEX100_TRIPLE_BUFFERING.
 *
 * This function has to be called before clutter_init().
 *
 * Since: 1.6
 */
void
clutter_cex100_set_buffering_mode (ClutterCex100BufferingMode mode)
{
  g_return_if_fail (mode == CLUTTER_CEX100_DOUBLE_BUFFERING ||
                    mode == CLUTTER_CEX100_TRIPLE_BUFFERING);

  gdl_n_buffers = mode;
}

/**
 * clutter_cex100_get_egl_display:
 *
 * Retrieves the EGL display used by Clutter, if it supports the
 * EGL windowing system and if it is running using an EGL backend.
 *
 * Return value: the EGL display used by Clutter, or 0
 *
 * Since: 1.10
 */
EGLDisplay
clutter_cex100_get_egl_display (void)
{
  ClutterBackend *backend;

  if (!_clutter_context_is_initialized ())
    {
      g_critical ("The Clutter backend has not been initialized yet");
      return 0;
    }

  backend = clutter_get_default_backend ();

  if (!CLUTTER_IS_BACKEND_CEX100 (backend))
    {
      g_critical ("The Clutter backend is not a CEX100 backend");
      return 0;
    }

#if COGL_HAS_EGL_SUPPORT
  return cogl_egl_context_get_egl_display (backend->cogl_context);
#else
  return 0;
#endif
}
