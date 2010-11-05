/* Clutter.
 * An OpenGL based 'interactive canvas' library.
 * Authored By Matthew Allum  <mallum@openedhand.com>
 * Copyright (C) 2006-2007 OpenedHand
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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>

#include <glib/gi18n-lib.h>

#include <GL/glx.h>
#include <GL/glxext.h>
#include <GL/gl.h>

#include "clutter-backend-glx.h"
#include "clutter-stage-glx.h"
#include "clutter-glx.h"
#include "clutter-profile.h"

#include "clutter-debug.h"
#include "clutter-event-translator.h"
#include "clutter-event.h"
#include "clutter-main.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

#include "cogl/cogl.h"
#include "cogl/cogl-internal.h"

#define clutter_backend_glx_get_type    _clutter_backend_glx_get_type

G_DEFINE_TYPE (ClutterBackendGLX, clutter_backend_glx, CLUTTER_TYPE_BACKEND_X11);

/* singleton object */
static ClutterBackendGLX *backend_singleton = NULL;

static gchar *clutter_vblank = NULL;

G_CONST_RETURN gchar*
_clutter_backend_glx_get_vblank (void)
{
  if (clutter_vblank && strcmp (clutter_vblank, "0") == 0)
    return "none";
  else
    return clutter_vblank;
}

static gboolean
clutter_backend_glx_pre_parse (ClutterBackend  *backend,
                               GError         **error)
{
  ClutterBackendClass *parent_class =
    CLUTTER_BACKEND_CLASS (clutter_backend_glx_parent_class);
  const gchar *env_string;

  env_string = g_getenv ("CLUTTER_VBLANK");
  if (env_string)
    {
      clutter_vblank = g_strdup (env_string);
      env_string = NULL;
    }

  return parent_class->pre_parse (backend, error);
}

static gboolean
clutter_backend_glx_post_parse (ClutterBackend  *backend,
                                GError         **error)
{
  ClutterBackendClass *parent_class =
    CLUTTER_BACKEND_CLASS (clutter_backend_glx_parent_class);

  if (!parent_class->post_parse (backend, error))
    return FALSE;

  return TRUE;
}

static const GOptionEntry entries[] =
{
  { "vblank", 0,
    0,
    G_OPTION_ARG_STRING, &clutter_vblank,
    N_("Set to 'none' or '0' to disable throttling "
       "framerate to vblank"), "OPTION"
  },
  { NULL }
};

static void
clutter_backend_glx_add_options (ClutterBackend *backend,
                                 GOptionGroup   *group)
{
  ClutterBackendClass *parent_class =
    CLUTTER_BACKEND_CLASS (clutter_backend_glx_parent_class);

  g_option_group_add_entries (group, entries);

  parent_class->add_options (backend, group);
}

static void
clutter_backend_glx_finalize (GObject *gobject)
{
  if (backend_singleton)
    backend_singleton = NULL;

  G_OBJECT_CLASS (clutter_backend_glx_parent_class)->finalize (gobject);
}

static void
clutter_backend_glx_dispose (GObject *gobject)
{
  ClutterBackend *backend = CLUTTER_BACKEND (gobject);

  /* Unrealize all shaders, since the GL context is going away */
  /* XXX: Why isn't this done in
   * clutter-backend.c:clutter_backend_dispose ?
   */
  _clutter_shader_release_all ();

  /* We chain up before disposing our CoglContext so that we will
   * destroy all of the stages first. Otherwise the actors may try to
   * make Cogl calls during destruction which would cause a crash */
  G_OBJECT_CLASS (clutter_backend_glx_parent_class)->dispose (gobject);

  cogl_object_unref (backend->cogl_context);
}

static GObject *
clutter_backend_glx_constructor (GType                  gtype,
                                 guint                  n_params,
                                 GObjectConstructParam *params)
{
  GObjectClass *parent_class;
  GObject *retval;

  if (!backend_singleton)
    {
      parent_class = G_OBJECT_CLASS (clutter_backend_glx_parent_class);
      retval = parent_class->constructor (gtype, n_params, params);

      backend_singleton = CLUTTER_BACKEND_GLX (retval);

      return retval;
    }

  g_warning ("Attempting to create a new backend object. This should "
             "never happen, so we return the singleton instance.");

  return g_object_ref (backend_singleton);
}

static ClutterFeatureFlags
clutter_backend_glx_get_features (ClutterBackend *backend)
{
  ClutterBackendGLX *backend_glx = CLUTTER_BACKEND_GLX (backend);
  ClutterBackendClass *parent_class;
  ClutterFeatureFlags flags;

  parent_class = CLUTTER_BACKEND_CLASS (clutter_backend_glx_parent_class);

  flags = parent_class->get_features (backend);

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

  if (cogl_clutter_winsys_has_feature (COGL_WINSYS_FEATURE_SWAP_REGION))
    {
      CLUTTER_NOTE (BACKEND, "Cogl supports swapping buffer regions");
      backend_glx->can_blit_sub_buffer = TRUE;
    }

  CLUTTER_NOTE (BACKEND, "backend features checked");

  return flags;
}

static XVisualInfo *
clutter_backend_glx_get_visual_info (ClutterBackendX11 *backend_x11)
{
  return cogl_clutter_winsys_xlib_get_visual_info ();
}

static gboolean
clutter_backend_glx_create_context (ClutterBackend  *backend,
                                    GError         **error)
{
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);
  CoglSwapChain *swap_chain = NULL;
  CoglOnscreenTemplate *onscreen_template = NULL;

  if (backend->cogl_context)
    return TRUE;

  backend->cogl_renderer = cogl_renderer_new ();
  cogl_renderer_xlib_set_foreign_display (backend->cogl_renderer,
                                          backend_x11->xdpy);
  if (!cogl_renderer_connect (backend->cogl_renderer, error))
    goto error;

  swap_chain = cogl_swap_chain_new ();
  cogl_swap_chain_set_has_alpha (swap_chain,
                                 clutter_x11_get_use_argb_visual ());

  onscreen_template = cogl_onscreen_template_new (swap_chain);
  cogl_object_unref (swap_chain);

  if (!cogl_renderer_check_onscreen_template (backend->cogl_renderer,
                                              onscreen_template,
                                              error))
    goto error;

  backend->cogl_display = cogl_display_new (backend->cogl_renderer,
                                            onscreen_template);
  cogl_object_unref (backend->cogl_renderer);
  cogl_object_unref (onscreen_template);

  if (!cogl_display_setup (backend->cogl_display, error))
    goto error;

  backend->cogl_context = cogl_context_new (backend->cogl_display, error);
  if (!backend->cogl_context)
    goto error;

  /* XXX: eventually this should go away but a lot of Cogl code still
   * depends on a global default context. */
  cogl_set_default_context (backend->cogl_context);

  return TRUE;

error:
  if (backend->cogl_display)
    {
      cogl_object_unref (backend->cogl_display);
      backend->cogl_display = NULL;
    }

  if (onscreen_template)
    cogl_object_unref (onscreen_template);
  if (swap_chain)
    cogl_object_unref (swap_chain);

  if (backend->cogl_renderer)
    {
      cogl_object_unref (backend->cogl_renderer);
      backend->cogl_renderer = NULL;
    }
  return FALSE;
}

static ClutterStageWindow *
clutter_backend_glx_create_stage (ClutterBackend  *backend,
                                  ClutterStage    *wrapper,
                                  GError         **error)
{
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);
  ClutterEventTranslator *translator;
  ClutterStageWindow *stage_window;
  ClutterStageX11 *stage_x11;

  stage_window = g_object_new (CLUTTER_TYPE_STAGE_GLX, NULL);

  /* copy backend data into the stage */
  stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  stage_x11->wrapper = wrapper;
  stage_x11->backend = backend_x11;

  translator = CLUTTER_EVENT_TRANSLATOR (stage_x11);
  _clutter_backend_add_event_translator (backend, translator);

  CLUTTER_NOTE (BACKEND,
                "GLX stage created[%p] (dpy:%p, screen:%d, root:%u, wrap:%p)",
                stage_window,
                backend_x11->xdpy,
                backend_x11->xscreen_num,
                (unsigned int) backend_x11->xwin_root,
                wrapper);

  return stage_window;
}

static void
clutter_backend_glx_ensure_context (ClutterBackend *backend,
                                    ClutterStage   *stage)
{
  ClutterStageGLX *stage_glx =
    CLUTTER_STAGE_GLX (_clutter_stage_get_window (stage));

  cogl_set_framebuffer (COGL_FRAMEBUFFER (stage_glx->onscreen));
}

static void
clutter_backend_glx_class_init (ClutterBackendGLXClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);
  ClutterBackendX11Class *backendx11_class = CLUTTER_BACKEND_X11_CLASS (klass);

  gobject_class->constructor = clutter_backend_glx_constructor;
  gobject_class->dispose     = clutter_backend_glx_dispose;
  gobject_class->finalize    = clutter_backend_glx_finalize;

  backend_class->pre_parse      = clutter_backend_glx_pre_parse;
  backend_class->post_parse     = clutter_backend_glx_post_parse;
  backend_class->create_stage   = clutter_backend_glx_create_stage;
  backend_class->add_options    = clutter_backend_glx_add_options;
  backend_class->get_features   = clutter_backend_glx_get_features;
  backend_class->create_context = clutter_backend_glx_create_context;
  backend_class->ensure_context = clutter_backend_glx_ensure_context;

  backendx11_class->get_visual_info = clutter_backend_glx_get_visual_info;
}

static void
clutter_backend_glx_init (ClutterBackendGLX *backend_glx)
{
}

/* every backend must implement this function */
GType
_clutter_backend_impl_get_type (void)
{
  return _clutter_backend_glx_get_type ();
}
