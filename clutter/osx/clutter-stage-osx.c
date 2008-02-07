/* Clutter -  An OpenGL based 'interactive canvas' library.
 * OSX backend - integration with NSWindow and NSView
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
#include "clutter-stage-osx.h"
#include "clutter-backend-osx.h"
#import <AppKit/AppKit.h>

#include <clutter/clutter-debug.h>
#include <clutter/clutter-private.h>

G_DEFINE_TYPE (ClutterStageOSX, clutter_stage_osx, CLUTTER_TYPE_STAGE)

/* FIXME: this should be in clutter-stage.c */
static void
clutter_stage_osx_state_update (ClutterStageOSX   *self,
                                ClutterStageState  unset_flags,
                                ClutterStageState  set_flags);

#define CLUTTER_OSX_FULLSCREEN_WINDOW_LEVEL (NSMainMenuWindowLevel + 1)

/*************************************************************************/
@interface ClutterGLWindow : NSWindow
{
  ClutterStageOSX *stage;
}
@end

@implementation ClutterGLWindow
- (id)initWithView:(NSView *)aView UTF8Title:(const char *)aTitle stage:(ClutterStageOSX *)aStage
{
  if ((self = [super initWithContentRect: [aView frame]
                               styleMask: NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask
                                 backing: NSBackingStoreBuffered
                                   defer: NO]) != nil)
    {
      [self setDelegate: self];
      [self useOptimizedDrawing: YES];
      [self setContentView: aView];
      [self setTitle:[NSString stringWithUTF8String: aTitle]];
      stage = aStage;
    }
  return self;
}

- (BOOL) windowShouldClose: (id) sender
{
  CLUTTER_NOTE (BACKEND, "windowShouldClose");

  ClutterEvent event;
  event.type = CLUTTER_DELETE;
  clutter_event_put (&event);

  return NO;
}

- (NSRect) constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen*)aScreen
{
  /* in fullscreen mode we don't want to be constrained by menubar or dock
   * FIXME: calculate proper constraints depending on fullscreen mode
   */

  return frameRect;
}

- (void) windowDidBecomeKey:(NSNotification*)aNotification
{
  CLUTTER_NOTE (BACKEND, "windowDidBecomeKey");

  if (stage->stage_state & CLUTTER_STAGE_STATE_FULLSCREEN)
    [self setLevel: CLUTTER_OSX_FULLSCREEN_WINDOW_LEVEL];

  clutter_stage_osx_state_update (stage, 0, CLUTTER_STAGE_STATE_ACTIVATED);
}

- (void) windowDidResignKey:(NSNotification*)aNotification
{
  CLUTTER_NOTE (BACKEND, "windowDidResignKey");

  if (stage->stage_state & CLUTTER_STAGE_STATE_FULLSCREEN)
    {
      [self setLevel: NSNormalWindowLevel];
      [self orderBack: nil];
    }

  clutter_stage_osx_state_update (stage, CLUTTER_STAGE_STATE_ACTIVATED, 0);
}
@end

/*************************************************************************/
@interface ClutterGLView : NSOpenGLView
{
  ClutterActor *stage;
}
- (void) drawRect: (NSRect) bounds;
@end

@implementation ClutterGLView
- (id) initWithFrame: (NSRect)aFrame pixelFormat:(NSOpenGLPixelFormat*)aFormat stage:(ClutterActor*)aStage
{
  int sw = 1; 

  if ((self = [super initWithFrame:aFrame pixelFormat:aFormat]) != nil)
    {
      self->stage = aStage;
    }

  /* Enable vblank sync - http://developer.apple.com/qa/qa2007/qa1521.html*/
  [[self openGLContext] setValues:&sw forParameter: NSOpenGLCPSwapInterval];

  return self;
}

- (void) drawRect: (NSRect) bounds
{
  clutter_actor_paint (self->stage);
  [[self openGLContext] flushBuffer];
}

/* In order to receive key events */
- (BOOL) acceptsFirstResponder
{
  return YES;
}

/* We want 0,0 top left */
- (BOOL) isFlipped
{
  return YES;
}

- (void) setFrameSize: (NSSize) aSize
{
  CLUTTER_NOTE (BACKEND, "setFrameSize: %dx%d",
                (int)aSize.width, (int)aSize.height);

  [super setFrameSize: aSize];

  clutter_actor_set_size (self->stage, (int)aSize.width, (int)aSize.height);

  CLUTTER_SET_PRIVATE_FLAGS(self->stage, CLUTTER_ACTOR_SYNC_MATRICES);
}

/* Simply forward all events that reach our view to clutter. */

#define EVENT_HANDLER(event) -(void)event:(NSEvent *)theEvent { \
  _clutter_event_osx_put (theEvent);                            \
}
EVENT_HANDLER(mouseDown)
EVENT_HANDLER(mouseDragged)
EVENT_HANDLER(mouseUp)
EVENT_HANDLER(mouseMoved)
EVENT_HANDLER(mouseEntered)
EVENT_HANDLER(mouseExited)
EVENT_HANDLER(rightMouseDown)
EVENT_HANDLER(rightMouseDragged)
EVENT_HANDLER(rightMouseUp)
EVENT_HANDLER(otherMouseDown)
EVENT_HANDLER(otherMouseDragged)
EVENT_HANDLER(otherMouseUp)
EVENT_HANDLER(scrollWheel)
EVENT_HANDLER(keyDown)
EVENT_HANDLER(keyUp)
EVENT_HANDLER(flagsChanged)
EVENT_HANDLER(helpRequested)
EVENT_HANDLER(tabletPoint)
EVENT_HANDLER(tabletProximity)

#undef EVENT_HANDLER
@end

/*************************************************************************/
static void
clutter_stage_osx_state_update (ClutterStageOSX   *self,
                                ClutterStageState  unset_flags,
                                ClutterStageState  set_flags)
{
  ClutterStageStateEvent event;

  event.new_state = self->stage_state;
  event.new_state |= set_flags;
  event.new_state &= ~unset_flags;

  if (event.new_state == self->stage_state)
    return;

  event.changed_mask = event.new_state ^ self->stage_state;

  self->stage_state = event.new_state;

  event.type = CLUTTER_STAGE_STATE;
  clutter_event_put ((ClutterEvent*)&event);
}

static void
clutter_stage_osx_save_frame (ClutterStageOSX *self)
{
  if (CLUTTER_ACTOR_IS_REALIZED (CLUTTER_ACTOR (self)))
    {
      g_assert (self->window != NULL);

      self->normalFrame = [self->window frame];
      self->haveNormalFrame = TRUE;
    }
}

static void
clutter_stage_osx_set_frame (ClutterStageOSX *self)
{
  g_assert (CLUTTER_ACTOR_IS_REALIZED (CLUTTER_ACTOR (self)));
  g_assert (self->window != NULL);

  if (self->stage_state & CLUTTER_STAGE_STATE_FULLSCREEN)
    {
      /* Raise above the menubar (and dock) covering the whole screen.
       *
       * NOTE: This effectively breaks Option-Tabbing as our window covers
       * all other applications completely. However we deal with the situation
       * by lowering the window to the bottom of the normal level stack on
       * windowDidResignKey notification.
       */
      [self->window setLevel: CLUTTER_OSX_FULLSCREEN_WINDOW_LEVEL];

      [self->window setFrame: [self->window frameRectForContentRect: [[self->window screen] frame]] display: NO];
    }
  else
    {
      [self->window setLevel: NSNormalWindowLevel];

      if (self->haveNormalFrame)
        [self->window setFrame: self->normalFrame display: NO];
      else
        /* looks better than positioning to 0,0 (bottom right) */
        [self->window center];
    }
}

/*************************************************************************/
static void
clutter_stage_osx_realize (ClutterActor *actor)
{
  ClutterStageOSX *self = CLUTTER_STAGE_OSX (actor);
  ClutterBackendOSX *backend_osx;
  gboolean is_offscreen;

  CLUTTER_NOTE (BACKEND, "realize");

  CLUTTER_OSX_POOL_ALLOC();

  g_object_get (actor, "offscreen", &is_offscreen, NULL);

  if (is_offcreen)
    {
      g_warning("OSX Backend does not yet support offscreen rendering\n");
      CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
      return;
    }

  if (CLUTTER_ACTOR_CLASS (clutter_stage_osx_parent_class)->realize)
    CLUTTER_ACTOR_CLASS (clutter_stage_osx_parent_class)->realize (actor);

  backend_osx = CLUTTER_BACKEND_OSX (self->backend);

  NSRect rect = NSMakeRect(0, 0, self->requisition_width, self->requisition_height);

  self->view = [[ClutterGLView alloc]
                initWithFrame: rect
                  pixelFormat: backend_osx->pixel_format
                        stage: actor];

  self->window = [[ClutterGLWindow alloc]
                  initWithView: self->view
                     UTF8Title: clutter_stage_get_title (CLUTTER_STAGE (self))
                         stage: self];

  /* looks better than positioning to 0,0 (bottom right) */
  [self->window center];

  /* To not miss all textures created with the context created in the backend
   * make sure we share the context. (By default NSOpenGLView creates its own
   * context.)
   */
  NSOpenGLContext *context = backend_osx->context;

  [self->view setOpenGLContext: context];
  [context setView: self->view];


  CLUTTER_OSX_POOL_RELEASE();

  CLUTTER_SET_PRIVATE_FLAGS(self, CLUTTER_ACTOR_SYNC_MATRICES);
}

static void
clutter_stage_osx_unrealize (ClutterActor *actor)
{
  ClutterStageOSX *self = CLUTTER_STAGE_OSX (actor);

  CLUTTER_NOTE (BACKEND, "unrealize");

  CLUTTER_OSX_POOL_ALLOC();

  [self->view release];
  [self->window close];

  self->view = NULL;
  self->window = NULL;

  CLUTTER_OSX_POOL_RELEASE();

  if (CLUTTER_ACTOR_CLASS (clutter_stage_osx_parent_class)->unrealize)
    CLUTTER_ACTOR_CLASS (clutter_stage_osx_parent_class)->unrealize (actor);
}

static void
clutter_stage_osx_show (ClutterActor *actor)
{
  ClutterStageOSX *self = CLUTTER_STAGE_OSX (actor);

  CLUTTER_NOTE (BACKEND, "show");

  CLUTTER_ACTOR_SET_FLAGS (actor, CLUTTER_ACTOR_MAPPED);

  if (CLUTTER_ACTOR_CLASS (clutter_stage_osx_parent_class)->show)
    CLUTTER_ACTOR_CLASS (clutter_stage_osx_parent_class)->show (actor);

  CLUTTER_OSX_POOL_ALLOC();

  clutter_stage_osx_set_frame (self);

  [self->window makeKeyAndOrderFront: nil];

  CLUTTER_OSX_POOL_RELEASE();
}

static void
clutter_stage_osx_hide (ClutterActor *actor)
{
  ClutterStageOSX *self = CLUTTER_STAGE_OSX (actor);

  CLUTTER_NOTE (BACKEND, "hide");

  CLUTTER_OSX_POOL_ALLOC();

  [self->window orderOut: nil];

  CLUTTER_OSX_POOL_RELEASE();

  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_MAPPED);

  if (CLUTTER_ACTOR_CLASS (clutter_stage_osx_parent_class)->hide)
    CLUTTER_ACTOR_CLASS (clutter_stage_osx_parent_class)->hide (actor);
}

static void
clutter_stage_osx_query_coords (ClutterActor    *actor,
                                ClutterActorBox *box)
{
  ClutterStageOSX *self = CLUTTER_STAGE_OSX (actor);

  CLUTTER_OSX_POOL_ALLOC();

  box->x1 = 0;
  box->y1 = 0;
  box->x2 = box->x1 + CLUTTER_UNITS_FROM_FLOAT (self->requisition_width);
  box->y2 = box->y1 + CLUTTER_UNITS_FROM_FLOAT (self->requisition_height);

  CLUTTER_OSX_POOL_RELEASE();
}

static void
clutter_stage_osx_request_coords (ClutterActor    *actor,
                                  ClutterActorBox *box)
{
  ClutterStageOSX *self = CLUTTER_STAGE_OSX (actor);

  CLUTTER_NOTE (BACKEND, "request_coords: %d,%d %dx%d",
                CLUTTER_UNITS_TO_INT (box->x1),
                CLUTTER_UNITS_TO_INT (box->y1),
                CLUTTER_UNITS_TO_INT (box->x2 - box->x1),
                CLUTTER_UNITS_TO_INT (box->y2 - box->y1));

  self->requisition_width  = CLUTTER_UNITS_TO_INT (box->x2 - box->x1);
  self->requisition_height = CLUTTER_UNITS_TO_INT (box->y2 - box->y1);

  if (CLUTTER_ACTOR_IS_REALIZED (actor))
    {
      CLUTTER_OSX_POOL_ALLOC();

      NSSize size = NSMakeSize(self->requisition_width,
                               self->requisition_height);
      [self->window setContentSize: size];

      CLUTTER_OSX_POOL_RELEASE();
    }
}

/*************************************************************************/
static void
clutter_stage_osx_set_title (ClutterStage *stage,
                             const char   *title)
{
  ClutterStageOSX *self = CLUTTER_STAGE_OSX (stage);

  CLUTTER_NOTE (BACKEND, "set_title: %s", title);

  CLUTTER_OSX_POOL_ALLOC();

  if (CLUTTER_ACTOR_IS_REALIZED (CLUTTER_ACTOR (stage)))
    [self->window setTitle:[NSString stringWithUTF8String:title]];

  CLUTTER_OSX_POOL_RELEASE();
}

static void
clutter_stage_osx_set_fullscreen (ClutterStage *stage,
                                  gboolean      fullscreen)
{
  ClutterStageOSX *self = CLUTTER_STAGE_OSX (stage);

  CLUTTER_NOTE (BACKEND, "set_fullscreen: %u", fullscreen);

  CLUTTER_OSX_POOL_ALLOC();

  /* Make sure to update the state before clutter_stage_osx_set_frame.
   *
   * Toggling fullscreen isn't atomic, there's two "events" involved:
   *  - stage state change (via state_update)
   *  - stage size change (via set_frame -> setFrameSize / set_size)
   *
   * We do state change first. Not sure there's any difference.
   */
  if (fullscreen)
    clutter_stage_osx_state_update (self, 0, CLUTTER_STAGE_STATE_FULLSCREEN);
  else
    clutter_stage_osx_state_update (self, CLUTTER_STAGE_STATE_FULLSCREEN, 0);

  if (CLUTTER_ACTOR_IS_REALIZED (CLUTTER_ACTOR (self)))
    {
      if (fullscreen)
        clutter_stage_osx_save_frame (self);

      clutter_stage_osx_set_frame (self);
    }
  else if (fullscreen)
    {
      /* FIXME: if you go fullscreen before realize we throw away the normal
       * stage size and can't return. Need to maintain them separately.
       */
      NSSize size = [[NSScreen mainScreen] frame].size;

      clutter_actor_set_size (CLUTTER_ACTOR (self),
                              (int)size.width, (int)size.height);
    }

  CLUTTER_OSX_POOL_RELEASE();
}

/*************************************************************************/
ClutterActor *
clutter_stage_osx_new (ClutterBackend *backend)
{
  ClutterStageOSX *self;

  self = g_object_new (CLUTTER_TYPE_STAGE_OSX, NULL);
  self->backend = backend;

  return CLUTTER_ACTOR(self);
}

/*************************************************************************/
static void
clutter_stage_osx_init (ClutterStageOSX *self)
{
  self->requisition_width  = 640;
  self->requisition_height = 480;

  CLUTTER_SET_PRIVATE_FLAGS(self, CLUTTER_ACTOR_SYNC_MATRICES);
}

static void
clutter_stage_osx_class_init (ClutterStageOSXClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  ClutterStageClass *stage_class = CLUTTER_STAGE_CLASS (klass);

  actor_class->realize   = clutter_stage_osx_realize;
  actor_class->unrealize = clutter_stage_osx_unrealize;
  actor_class->show      = clutter_stage_osx_show;
  actor_class->hide      = clutter_stage_osx_hide;

  actor_class->query_coords   = clutter_stage_osx_query_coords;
  actor_class->request_coords = clutter_stage_osx_request_coords;

  stage_class->set_title = clutter_stage_osx_set_title;
  stage_class->set_fullscreen = clutter_stage_osx_set_fullscreen;
}
