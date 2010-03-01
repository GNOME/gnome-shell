/* Clutter -  An OpenGL based 'interactive canvas' library.
 * OSX backend - initial entry point
 *
 * Copyright (C) 2007-2008  Tommi Komulainen <tommi.komulainen@iki.fi>
 * Copyright (C) 2007  OpenedHand Ltd.
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
#include "config.h"

#include "clutter-osx.h"
#include "clutter-backend-osx.h"
#include "clutter-stage-osx.h"
#include "../clutter-private.h"
#include "cogl/cogl.h"

#include <clutter/clutter-debug.h>
#import <AppKit/AppKit.h>

G_DEFINE_TYPE (ClutterBackendOSX, clutter_backend_osx, CLUTTER_TYPE_BACKEND)

/*************************************************************************/
static gboolean
clutter_backend_osx_post_parse (ClutterBackend  *backend,
                                GError         **error)
{
  ClutterBackendOSX *self = CLUTTER_BACKEND_OSX (backend);

  CLUTTER_NOTE (BACKEND, "post_parse");

  CLUTTER_OSX_POOL_ALLOC();

  /* Bring our app to foreground, background apps don't appear in dock or
   * accept keyboard focus.
   */
  const ProcessSerialNumber psn = { 0, kCurrentProcess };
  TransformProcessType (&psn, kProcessTransformToForegroundApplication);

  /* Also raise our app to front, otherwise our window will remain under the
   * terminal.
   */
  SetFrontProcess (&psn);

  [NSApplication sharedApplication];

  /* Allocate ourselves a GL context. Since we're supposed to have only one per
   * backend we can just as well create it now.
   */
  NSOpenGLPixelFormatAttribute attrs[] = {
    NSOpenGLPFADoubleBuffer,
    0
  };
  self->pixel_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
  self->context = [[NSOpenGLContext alloc]
                   initWithFormat: self->pixel_format
                     shareContext: nil];

  /* Enable vblank sync - http://developer.apple.com/qa/qa2007/qa1521.html */
  #ifdef MAC_OS_X_VERSION_10_5
	const int sw = 1;
  #else
    const long sw = 1;
  #endif
  [self->context setValues:&sw forParameter: NSOpenGLCPSwapInterval];

  /* FIXME: move the debugging bits to cogl */
  [self->context makeCurrentContext];
  CLUTTER_NOTE(BACKEND, "GL information\n"
               "    GL_VENDOR: %s\n"
               "  GL_RENDERER: %s\n"
               "   GL_VERSION: %s\n"
               "GL_EXTENSIONS: %s\n",
               glGetString (GL_VENDOR),
               glGetString (GL_RENDERER),
               glGetString (GL_VERSION),
               glGetString (GL_EXTENSIONS));
  [NSOpenGLContext clearCurrentContext];

  CLUTTER_OSX_POOL_RELEASE();

  return TRUE;
}

static ClutterFeatureFlags
clutter_backend_osx_get_features (ClutterBackend *backend)
{
  return CLUTTER_FEATURE_STAGE_MULTIPLE|CLUTTER_FEATURE_STAGE_USER_RESIZE;
}

static ClutterStageWindow*
clutter_backend_osx_create_stage (ClutterBackend  *backend,
                                  ClutterStage    *wrapper,
                                  GError         **error)
{
  ClutterStageWindow *impl;

  CLUTTER_NOTE (BACKEND, "create_stage: wrapper=%p", wrapper);

  CLUTTER_OSX_POOL_ALLOC();

  impl = clutter_stage_osx_new (backend, wrapper);

  CLUTTER_NOTE (BACKEND, "create_stage: impl=%p", impl);

  CLUTTER_OSX_POOL_RELEASE();

  return impl;
}

static void
clutter_backend_osx_init_events (ClutterBackend *backend)
{
  CLUTTER_NOTE (BACKEND, "init_events");

  _clutter_events_osx_init ();
}

static void
clutter_backend_osx_ensure_context (ClutterBackend *backend,
                                    ClutterStage   *wrapper)
{
  ClutterBackendOSX *backend_osx = CLUTTER_BACKEND_OSX (backend);

  CLUTTER_NOTE (BACKEND, "ensure_context: wrapper=%p", wrapper);

  CLUTTER_OSX_POOL_ALLOC();

  if (wrapper)
    {
      ClutterStageWindow *impl = _clutter_stage_get_window (wrapper);
      ClutterStageOSX *stage_osx;

      g_assert (CLUTTER_IS_STAGE_OSX (impl));
      stage_osx = CLUTTER_STAGE_OSX (impl);

      [backend_osx->context setView:stage_osx->view];
      [backend_osx->context makeCurrentContext];
    }
  else
    {
      [backend_osx->context clearDrawable];
      [NSOpenGLContext clearCurrentContext];
    }

  CLUTTER_OSX_POOL_RELEASE();
}

static void
clutter_backend_osx_redraw (ClutterBackend *backend, ClutterStage *wrapper)
{
  ClutterStageWindow *impl = _clutter_stage_get_window (wrapper);
  ClutterStageOSX *stage_osx = CLUTTER_STAGE_OSX (impl);

  CLUTTER_OSX_POOL_ALLOC();

  [stage_osx->view setNeedsDisplay: YES];

  CLUTTER_OSX_POOL_RELEASE();
}

/*************************************************************************/

static void
clutter_backend_osx_init (ClutterBackendOSX *self)
{
}

static void
clutter_backend_osx_dispose (GObject *object)
{
  ClutterBackendOSX *self = CLUTTER_BACKEND_OSX (object);

  _clutter_shader_release_all ();

  [self->context release];
  self->context = NULL;

  [self->pixel_format release];
  self->pixel_format = NULL;

  G_OBJECT_CLASS (clutter_backend_osx_parent_class)->dispose (object);
}

static void
clutter_backend_osx_class_init (ClutterBackendOSXClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterBackendClass *backend_class = CLUTTER_BACKEND_CLASS (klass);

  object_class->dispose = clutter_backend_osx_dispose;

  backend_class->post_parse       = clutter_backend_osx_post_parse;
  backend_class->get_features     = clutter_backend_osx_get_features;
  backend_class->create_stage     = clutter_backend_osx_create_stage;
  backend_class->ensure_context   = clutter_backend_osx_ensure_context;
  backend_class->init_events      = clutter_backend_osx_init_events;
  backend_class->redraw           = clutter_backend_osx_redraw;
}

GType
_clutter_backend_impl_get_type (void)
{
  return clutter_backend_osx_get_type ();
}
