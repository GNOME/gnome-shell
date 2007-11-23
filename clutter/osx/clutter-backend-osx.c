/* Clutter -  An OpenGL based 'interactive canvas' library.
 * OSX backend - initial entry point
 *
 * Copyright (C) 2007  Tommi Komulainen <tommi.komulainen@iki.fi>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "config.h"

#include "clutter-osx.h"
#include "clutter-backend-osx.h"
#include "clutter-stage-osx.h"
#include "../clutter-private.h"

#include <clutter/clutter-debug.h>
#import <AppKit/AppKit.h>

G_DEFINE_TYPE (ClutterBackendOSX, clutter_backend_osx, CLUTTER_TYPE_BACKEND)

/*************************************************************************/
static gboolean
clutter_backend_osx_post_parse (ClutterBackend  *backend,
                                GError         **error)
{
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

  /* Initialize(?) OpenGL -- without this glGetString crashes
   *
   * Program received signal EXC_BAD_ACCESS, Could not access memory.
   * Reason: KERN_PROTECTION_FAILURE at address: 0x00000ac0
   * 0x92b22b2f in glGetString ()
   */
  [NSOpenGLView defaultPixelFormat];

  CLUTTER_OSX_POOL_RELEASE();

  return TRUE;
}

static ClutterFeatureFlags
clutter_backend_osx_get_features (ClutterBackend *backend)
{
  return CLUTTER_FEATURE_STAGE_USER_RESIZE;
}

static gboolean
clutter_backend_osx_init_stage (ClutterBackend  *backend,
                                GError         **error)
{
  ClutterBackendOSX *self = CLUTTER_BACKEND_OSX (backend);
  ClutterActor *stage;

  CLUTTER_NOTE (BACKEND, "init_stage");

  CLUTTER_OSX_POOL_ALLOC();

  g_assert (self->stage == NULL);

  /* Allocate ourselves a GL context. We need one this early for clutter to
   * manage textures.
   */
  NSOpenGLPixelFormatAttribute attrs[] = {
    NSOpenGLPFADoubleBuffer,
    0
  };
  self->pixel_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
  self->context = [[NSOpenGLContext alloc]
                   initWithFormat: self->pixel_format
                     shareContext: nil];
  [self->context makeCurrentContext];

  stage = clutter_stage_osx_new (backend);
  self->stage = g_object_ref_sink (stage);

  CLUTTER_OSX_POOL_RELEASE();

  return TRUE;
}

static void
clutter_backend_osx_init_events (ClutterBackend *backend)
{
  CLUTTER_NOTE (BACKEND, "init_events");

  _clutter_events_osx_init ();
}

static ClutterActor *
clutter_backend_osx_get_stage (ClutterBackend *backend)
{
  ClutterBackendOSX *self = CLUTTER_BACKEND_OSX (backend);

  return self->stage;
}

static void
clutter_backend_osx_redraw (ClutterBackend *backend)
{
  ClutterBackendOSX *self = CLUTTER_BACKEND_OSX (backend);
  ClutterStageOSX *stage_osx;

  stage_osx = CLUTTER_STAGE_OSX (self->stage);
  [stage_osx->view setNeedsDisplay: YES];
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

  if (self->stage)
    {
      CLUTTER_UNSET_PRIVATE_FLAGS (self->stage, CLUTTER_ACTOR_IS_TOPLEVEL);
      clutter_actor_destroy (self->stage);
      self->stage = NULL;
    }

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

  backend_class->post_parse   = clutter_backend_osx_post_parse;
  backend_class->get_features = clutter_backend_osx_get_features;
  backend_class->init_stage  = clutter_backend_osx_init_stage;
  backend_class->init_events = clutter_backend_osx_init_events;
  backend_class->get_stage   = clutter_backend_osx_get_stage;
  backend_class->redraw      = clutter_backend_osx_redraw;
}

GType
_clutter_backend_impl_get_type (void)
{
  return clutter_backend_osx_get_type ();
}
