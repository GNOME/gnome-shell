/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010,2011  Intel Corporation.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>

#include "clutter-backend-cogl.h"
#include "clutter-stage-cogl.h"

#include "clutter-debug.h"
#include "clutter-private.h"
#include "clutter-main.h"
#include "clutter-stage-private.h"

static ClutterBackendCogl *backend_singleton = NULL;

static gchar *clutter_vblank = NULL;

G_DEFINE_TYPE (ClutterBackendCogl, _clutter_backend_cogl, CLUTTER_TYPE_BACKEND);

const gchar*
_clutter_backend_cogl_get_vblank (void)
{
  if (clutter_vblank && strcmp (clutter_vblank, "0") == 0)
    return "none";
  else
    return clutter_vblank;
}

static gboolean
clutter_backend_cogl_pre_parse (ClutterBackend  *backend,
                                GError         **error)
{
  const gchar *env_string;

  env_string = g_getenv ("CLUTTER_VBLANK");
  if (env_string)
    {
      clutter_vblank = g_strdup (env_string);
      env_string = NULL;
    }

  return TRUE;
}

static gboolean
clutter_backend_cogl_post_parse (ClutterBackend  *backend,
                                 GError         **error)
{
  return TRUE;
}

static void
clutter_backend_cogl_finalize (GObject *gobject)
{
  if (backend_singleton)
    backend_singleton = NULL;

  G_OBJECT_CLASS (_clutter_backend_cogl_parent_class)->finalize (gobject);
}

static void
clutter_backend_cogl_dispose (GObject *gobject)
{
  ClutterBackend *backend = CLUTTER_BACKEND (gobject);

  if (backend->cogl_context)
    {
      cogl_object_unref (backend->cogl_context);
      backend->cogl_context = NULL;
    }

  G_OBJECT_CLASS (_clutter_backend_cogl_parent_class)->dispose (gobject);
}

static GObject *
clutter_backend_cogl_constructor (GType                  gtype,
                                  guint                  n_params,
                                  GObjectConstructParam *params)
{
  GObjectClass *parent_class;
  GObject *retval;

  if (backend_singleton == NULL)
    {
      parent_class = G_OBJECT_CLASS (_clutter_backend_cogl_parent_class);
      retval = parent_class->constructor (gtype, n_params, params);

      backend_singleton = CLUTTER_BACKEND_COGL (retval);

      return retval;
    }

  g_warning ("Attempting to create a new backend object. This should "
             "never happen, so we return the singleton instance.");

  return g_object_ref (backend_singleton);
}

static ClutterFeatureFlags
clutter_backend_cogl_get_features (ClutterBackend *backend)
{
  ClutterBackendCogl *backend_cogl = CLUTTER_BACKEND_COGL (backend);
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

  if (cogl_clutter_winsys_has_feature (COGL_WINSYS_FEATURE_SWAP_REGION))
    {
      CLUTTER_NOTE (BACKEND, "Cogl supports swapping buffer regions");
      backend_cogl->can_blit_sub_buffer = TRUE;
    }

  return flags;
}

static void
clutter_backend_cogl_ensure_context (ClutterBackend *backend,
                                     ClutterStage   *stage)
{
  ClutterStageCogl *stage_cogl;

  /* ignore ensuring the context on an empty stage */
  if (stage == NULL)
    return;

  stage_cogl =
    CLUTTER_STAGE_COGL (_clutter_stage_get_window (stage));

  cogl_set_framebuffer (COGL_FRAMEBUFFER (stage_cogl->onscreen));
}

static void
_clutter_backend_cogl_class_init (ClutterBackendCoglClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  gobject_class->constructor = clutter_backend_cogl_constructor;
  gobject_class->dispose = clutter_backend_cogl_dispose;
  gobject_class->finalize = clutter_backend_cogl_finalize;

  backend_class->pre_parse = clutter_backend_cogl_pre_parse;
  backend_class->post_parse = clutter_backend_cogl_post_parse;
  backend_class->get_features = clutter_backend_cogl_get_features;
  backend_class->ensure_context = clutter_backend_cogl_ensure_context;
}

static void
_clutter_backend_cogl_init (ClutterBackendCogl *backend_cogl)
{
}
