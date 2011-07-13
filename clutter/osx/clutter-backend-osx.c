/* Clutter -  An OpenGL based 'interactive canvas' library.
 * OSX backend - initial entry point
 *
 * Copyright (C) 2007-2008  Tommi Komulainen <tommi.komulainen@iki.fi>
 * Copyright (C) 2007  OpenedHand Ltd.
 * Copyright (C) 2011  Crystalnix <vgachkaylo@crystalnix.com>
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
#include "clutter-device-manager-osx.h"
#include "clutter-shader.h"
#include "clutter-stage-osx.h"

#include "clutter-debug.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

#include "cogl/cogl.h"

#import <AppKit/AppKit.h>

G_DEFINE_TYPE (ClutterBackendOSX, clutter_backend_osx, CLUTTER_TYPE_BACKEND)

/*************************************************************************/
static gboolean
clutter_backend_osx_post_parse (ClutterBackend  *backend,
                                GError         **error)
{
  CLUTTER_OSX_POOL_ALLOC();
  /* getting standart dpi for main screen */
  NSDictionary* prop = [[NSScreen mainScreen] deviceDescription];
  NSSize size;
  [[prop valueForKey:@"NSDeviceResolution"] getValue:&size];
  CLUTTER_OSX_POOL_RELEASE();

  /* setting dpi for backend, it needs by font rendering library */
  if (size.height > 0)
    {
      ClutterSettings *settings = clutter_settings_get_default ();
      int font_dpi = size.height * 1024;

      g_object_set (settings, "font-dpi", font_dpi, NULL);

      return TRUE;
    }

  return FALSE;
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

  CLUTTER_OSX_POOL_ALLOC();

  impl = _clutter_stage_osx_new (backend, wrapper);

  CLUTTER_NOTE (BACKEND, "create_stage: wrapper=%p - impl=%p",
                wrapper,
                impl);

  CLUTTER_OSX_POOL_RELEASE();

  return impl;
}

static inline void
clutter_backend_osx_create_device_manager (ClutterBackendOSX *backend_osx)
{
  if (backend_osx->device_manager != NULL)
    return;

  backend_osx->device_manager = g_object_new (CLUTTER_TYPE_DEVICE_MANAGER_OSX,
                                              "backend", CLUTTER_BACKEND(backend_osx),
                                              NULL);
}

static ClutterDeviceManager *
clutter_backend_osx_get_device_manager (ClutterBackend *backend)
{
  ClutterBackendOSX *backend_osx = CLUTTER_BACKEND_OSX (backend);

  clutter_backend_osx_create_device_manager (backend_osx);

  return backend_osx->device_manager;
}

static void
clutter_backend_osx_init_events (ClutterBackend *backend)
{
  ClutterBackendOSX *backend_osx = CLUTTER_BACKEND_OSX (backend);

  if (backend_osx->device_manager != NULL)
    return;

  CLUTTER_NOTE (BACKEND, "init_events");

  clutter_backend_osx_create_device_manager (backend_osx);
  _clutter_events_osx_init ();
}

static gboolean
clutter_backend_osx_create_context (ClutterBackend  *backend,
                                    GError         **error)
{
  ClutterBackendOSX *backend_osx = CLUTTER_BACKEND_OSX (backend);

  CLUTTER_OSX_POOL_ALLOC();

  if (backend_osx->context == nil)
    {
      /* Allocate ourselves a GL context. Since we're supposed to have
       * only one per backend we can just as well create it now.
       */
      NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAStencilSize, 8,
        0
      };

#ifdef MAC_OS_X_VERSION_10_5
      const int sw = 1;
#else
      const long sw = 1;
#endif

      backend_osx->pixel_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];

      backend_osx->context = [[NSOpenGLContext alloc] initWithFormat: backend_osx->pixel_format
                                                        shareContext: nil];

      /* Enable vblank sync - http://developer.apple.com/qa/qa2007/qa1521.html */
      [backend_osx->context setValues:&sw forParameter: NSOpenGLCPSwapInterval];

      CLUTTER_NOTE (BACKEND, "Context was created");
    }

  [backend_osx->context makeCurrentContext];

  CLUTTER_OSX_POOL_RELEASE();

  return TRUE;
}

static void
clutter_backend_osx_ensure_context (ClutterBackend *backend,
                                    ClutterStage   *wrapper)
{
  ClutterBackendOSX *backend_osx = CLUTTER_BACKEND_OSX (backend);

  CLUTTER_OSX_POOL_ALLOC();

  CLUTTER_NOTE (BACKEND, "ensure_context: wrapper=%p", wrapper);

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

/*************************************************************************/

static void
clutter_backend_osx_init (ClutterBackendOSX *backend_osx)
{
  const ProcessSerialNumber psn = { 0, kCurrentProcess };

  backend_osx->context = nil;
  backend_osx->pixel_format = nil;

  /* Bring our app to foreground, background apps don't appear in dock or
   * accept keyboard focus.
   */
  TransformProcessType (&psn, kProcessTransformToForegroundApplication);

  /* Also raise our app to front, otherwise our window will remain under the
   * terminal.
   */
  SetFrontProcess (&psn);

  [NSApplication sharedApplication];
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

  backend_class->post_parse         = clutter_backend_osx_post_parse;
  backend_class->get_features       = clutter_backend_osx_get_features;
  backend_class->create_stage       = clutter_backend_osx_create_stage;
  backend_class->create_context     = clutter_backend_osx_create_context;
  backend_class->ensure_context     = clutter_backend_osx_ensure_context;
  backend_class->init_events        = clutter_backend_osx_init_events;
  backend_class->get_device_manager = clutter_backend_osx_get_device_manager;
}

GType
_clutter_backend_impl_get_type (void)
{
  return clutter_backend_osx_get_type ();
}
