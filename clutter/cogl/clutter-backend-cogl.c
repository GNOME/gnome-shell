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

G_DEFINE_TYPE (ClutterBackendCogl, _clutter_backend_cogl, CLUTTER_TYPE_BACKEND);

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

static void
_clutter_backend_cogl_class_init (ClutterBackendCoglClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructor = clutter_backend_cogl_constructor;
  gobject_class->dispose = clutter_backend_cogl_dispose;
  gobject_class->finalize = clutter_backend_cogl_finalize;
}

static void
_clutter_backend_cogl_init (ClutterBackendCogl *backend_cogl)
{
}
