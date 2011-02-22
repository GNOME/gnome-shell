/* Clutter -  An OpenGL based 'interactive canvas' library.
 * OSX backend - integration with NSWindow and NSView
 *
 * Copyright (C) 2007-2008  Tommi Komulainen <tommi.komulainen@iki.fi>
 * Copyright (C) 2007  OpenedHand Ltd.
 * Copyright (C) 2011  Crystalnix  <vgachkaylo@gmail.com>
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
#include "clutter-stage-osx.h"
#include "clutter-backend-osx.h"

#include "clutter-debug.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

#import <AppKit/AppKit.h>

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

#define clutter_stage_osx_get_type      _clutter_stage_osx_get_type

G_DEFINE_TYPE_WITH_CODE (ClutterStageOSX,
                         clutter_stage_osx,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init))

/* FIXME: this should be in clutter-stage.c */
static void
clutter_stage_osx_state_update (ClutterStageOSX   *self,
                                ClutterStageState  unset_flags,
                                ClutterStageState  set_flags);

static ClutterActor *
clutter_stage_osx_get_wrapper (ClutterStageWindow *stage_window);

#define CLUTTER_OSX_FULLSCREEN_WINDOW_LEVEL (NSMainMenuWindowLevel + 1)

/*************************************************************************/
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
      [self setAcceptsMouseMovedEvents:YES];
      [self setContentView: aView];
      [self setTitle:[NSString stringWithUTF8String: aTitle ? aTitle : ""]];
      self->stage_osx = aStage;
    }
  return self;
}

- (BOOL) windowShouldClose: (id) sender
{
  ClutterEvent event;

  CLUTTER_NOTE (BACKEND, "[%p] windowShouldClose", self->stage_osx);

  event.type = CLUTTER_DELETE;
  event.any.stage = CLUTTER_STAGE (self->stage_osx->wrapper);
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
  CLUTTER_NOTE (BACKEND, "[%p] windowDidBecomeKey", self->stage_osx);

  if (self->stage_osx->stage_state & CLUTTER_STAGE_STATE_FULLSCREEN)
    [self setLevel: CLUTTER_OSX_FULLSCREEN_WINDOW_LEVEL];

  clutter_stage_osx_state_update (self->stage_osx, 0, CLUTTER_STAGE_STATE_ACTIVATED);
}

- (void) windowDidResignKey:(NSNotification*)aNotification
{
  CLUTTER_NOTE (BACKEND, "[%p] windowDidResignKey", self->stage_osx);

  if (self->stage_osx->stage_state & CLUTTER_STAGE_STATE_FULLSCREEN)
    {
      [self setLevel: NSNormalWindowLevel];
      [self orderBack: nil];
    }

  clutter_stage_osx_state_update (self->stage_osx, CLUTTER_STAGE_STATE_ACTIVATED, 0);
}

- (NSSize) windowWillResize:(NSWindow *) sender toSize:(NSSize) frameSize
{
  if ( clutter_stage_get_user_resizable (self->stage_osx->wrapper) )
    {
      guint min_width, min_height;
      clutter_stage_get_minimum_size (self->stage_osx->wrapper,
                                      &min_width,
                                      &min_height);
      [self setContentMinSize:NSMakeSize(min_width, min_height)];
      return frameSize;
    }
  else 
    return [self frame].size;
}
@end

/*************************************************************************/
@interface ClutterGLView : NSOpenGLView
{
  ClutterStageOSX *stage_osx;
  NSTrackingRectTag tracking_rect;
}
- (void) drawRect: (NSRect) bounds;
@end

@implementation ClutterGLView
- (id) initWithFrame: (NSRect)aFrame pixelFormat:(NSOpenGLPixelFormat*)aFormat stage:(ClutterStageOSX*)aStage
{
  if ((self = [super initWithFrame:aFrame pixelFormat:aFormat]) != nil)
    {
      self->stage_osx = aStage;
      tracking_rect = [self addTrackingRect:[self bounds] owner:self userData:NULL assumeInside:NO];
    }

  return self;
}

- (void) drawRect: (NSRect) bounds
{
  _clutter_stage_do_paint (CLUTTER_STAGE (self->stage_osx->wrapper), NULL);
  cogl_flush ();
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

- (void) reshape
{
  stage_osx->requisition_width = [self bounds].size.width;
  stage_osx->requisition_height = [self bounds].size.height;
  clutter_actor_set_size (CLUTTER_ACTOR (self->stage_osx->wrapper),
                          (int)[self bounds].size.width, (int)[self bounds].size.height);

  [self removeTrackingRect:tracking_rect];
  tracking_rect = [self addTrackingRect:[self bounds] owner:self userData:NULL assumeInside:NO];
}

/* Simply forward all events that reach our view to clutter. */

#define EVENT_HANDLER(event) -(void)event:(NSEvent *)theEvent { \
  _clutter_event_osx_put (theEvent, self->stage_osx->wrapper);  \
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
  event.stage = CLUTTER_STAGE (self->wrapper);
  clutter_event_put ((ClutterEvent*)&event);
}

static void
clutter_stage_osx_save_frame (ClutterStageOSX *self)
{
  g_assert (self->window != NULL);

  self->normalFrame = [self->window frame];
  self->haveNormalFrame = TRUE;
}

static void
clutter_stage_osx_set_frame (ClutterStageOSX *self)
{
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
static gboolean
clutter_stage_osx_realize (ClutterStageWindow *stage_window)
{
  ClutterStageOSX *self = CLUTTER_STAGE_OSX (stage_window);
  ClutterBackendOSX *backend_osx;
  gfloat width, height;
  NSRect rect;

  CLUTTER_OSX_POOL_ALLOC();

  CLUTTER_NOTE (BACKEND, "[%p] realize", self);

  backend_osx = CLUTTER_BACKEND_OSX (self->backend);
  /* Call get_size - this will either get the geometry size (which
   * before we create the window is set to 640x480), or if a size
   * is set, it will get that. This lets you set a size on the
   * stage before it's realized.
   */
  clutter_actor_get_size (CLUTTER_ACTOR (self->wrapper), &width, &height);
  self->requisition_width = width; 
  self->requisition_height= height;

  rect = NSMakeRect(0, 0, self->requisition_width, self->requisition_height);

  self->view = [[ClutterGLView alloc]
                initWithFrame: rect
                  pixelFormat: backend_osx->pixel_format
                        stage: self];
  [self->view setOpenGLContext:backend_osx->context];

  self->window = [[ClutterGLWindow alloc]
                  initWithView: self->view
                     UTF8Title: clutter_stage_get_title (CLUTTER_STAGE (self->wrapper))
                         stage: self];
  /* looks better than positioning to 0,0 (bottom right) */
  [self->window center];

  CLUTTER_NOTE (BACKEND, "Stage successfully realized");

  CLUTTER_OSX_POOL_RELEASE();

  return TRUE;
}

static void
clutter_stage_osx_unrealize (ClutterStageWindow *stage_window)
{
  ClutterStageOSX *self = CLUTTER_STAGE_OSX (stage_window);

  CLUTTER_OSX_POOL_ALLOC();

  CLUTTER_NOTE (BACKEND, "[%p] unrealize", self);

  /* ensure we get realize+unrealize properly paired */
  g_return_if_fail (self->view != NULL && self->window != NULL);

  [self->view release];
  [self->window close];

  self->view = NULL;
  self->window = NULL;

  CLUTTER_OSX_POOL_RELEASE();
}

static void
clutter_stage_osx_show (ClutterStageWindow *stage_window,
                        gboolean            do_raise)
{
  ClutterStageOSX *self = CLUTTER_STAGE_OSX (stage_window);
  BOOL isViewHidden;

  CLUTTER_OSX_POOL_ALLOC();

  CLUTTER_NOTE (BACKEND, "[%p] show", self);

  clutter_stage_osx_realize (stage_window);
  clutter_actor_map (CLUTTER_ACTOR (self->wrapper));

  clutter_stage_osx_set_frame (self);

  /* Draw view should be avoided and it is the reason why
   * we should hide OpenGL view while we showing the stage.
   */
  isViewHidden = [self->view isHidden];
  if (isViewHidden == NO)
    [self->view setHidden:YES];

  if (self->acceptFocus)
    [self->window makeKeyAndOrderFront: nil];
  else
    [self->window orderFront: nil];

  [self->view setHidden:isViewHidden];

  /*
   * After hiding we cease to be first responder.
   */
  [self->window makeFirstResponder: self->view];

  CLUTTER_OSX_POOL_RELEASE();
}

static void
clutter_stage_osx_hide (ClutterStageWindow *stage_window)
{
  ClutterStageOSX *self = CLUTTER_STAGE_OSX (stage_window);

  CLUTTER_OSX_POOL_ALLOC();

  CLUTTER_NOTE (BACKEND, "[%p] hide", self);

  [self->window orderOut: nil];

  clutter_actor_unmap (CLUTTER_ACTOR (self->wrapper));

  CLUTTER_OSX_POOL_RELEASE();
}

static void
clutter_stage_osx_get_geometry (ClutterStageWindow *stage_window,
                                ClutterGeometry    *geometry)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterStageOSX *self = CLUTTER_STAGE_OSX (stage_window);

  g_return_if_fail (CLUTTER_IS_BACKEND_OSX (backend));

  geometry->width = self->requisition_width;
  geometry->height = self->requisition_height;
}

static void
clutter_stage_osx_resize (ClutterStageWindow *stage_window,
                          gint                width,
                          gint                height)
{
  ClutterStageOSX *self = CLUTTER_STAGE_OSX (stage_window);
  ClutterActor *actor = clutter_stage_osx_get_wrapper (stage_window);
  guint min_width, min_height;
  NSSize size;

  CLUTTER_OSX_POOL_ALLOC ();

  clutter_stage_get_minimum_size (CLUTTER_STAGE (actor),
                                  &min_width,
                                  &min_height);
  [self->window setContentMinSize:NSMakeSize(min_width, min_height)];
  
  width = width < min_width ? min_width : width;
  height = height < min_height ? min_height : height;

  self->requisition_width = width;
  self->requisition_height = height;

  size = NSMakeSize (self->requisition_width, self->requisition_height);
  [self->window setContentSize: size];

  CLUTTER_OSX_POOL_RELEASE ();
}

/*************************************************************************/
static ClutterActor *
clutter_stage_osx_get_wrapper (ClutterStageWindow *stage_window)
{
  ClutterStageOSX *self = CLUTTER_STAGE_OSX (stage_window);

  return CLUTTER_ACTOR (self->wrapper);
}

static void
clutter_stage_osx_set_title (ClutterStageWindow *stage_window,
                             const char         *title)
{
  ClutterStageOSX *self = CLUTTER_STAGE_OSX (stage_window);

  CLUTTER_OSX_POOL_ALLOC();

  CLUTTER_NOTE (BACKEND, "[%p] set_title: %s", self, title);

  [self->window setTitle:[NSString stringWithUTF8String: title ? title : ""]];

  CLUTTER_OSX_POOL_RELEASE();
}

static void
clutter_stage_osx_set_fullscreen (ClutterStageWindow *stage_window,
                                  gboolean            fullscreen)
{
  ClutterStageOSX *self = CLUTTER_STAGE_OSX (stage_window);

  CLUTTER_OSX_POOL_ALLOC();

  CLUTTER_NOTE (BACKEND, "[%p] set_fullscreen: %u", self, fullscreen);

  /* Make sure to update the state before clutter_stage_osx_set_frame.
   *
   * Toggling fullscreen isn't atomic, there's two "events" involved:
   *  - stage state change (via state_update)
   *  - stage size change (via set_frame -> setFrameSize / set_size)
   *
   * We do state change first. Not sure there's any difference.
   */
  if (fullscreen)
    {
      clutter_stage_osx_state_update (self, 0, CLUTTER_STAGE_STATE_FULLSCREEN);
      clutter_stage_osx_save_frame (self);
    }
  else
    clutter_stage_osx_state_update (self, CLUTTER_STAGE_STATE_FULLSCREEN, 0);

  clutter_stage_osx_set_frame (self);

  CLUTTER_OSX_POOL_RELEASE();
}
static void
clutter_stage_osx_set_cursor_visible (ClutterStageWindow *stage_window,
                                      gboolean            cursor_visible)
{
  CLUTTER_OSX_POOL_ALLOC();

  if (cursor_visible)
    [NSCursor unhide];
  else
    [NSCursor hide];

  CLUTTER_OSX_POOL_RELEASE();
}

static void
clutter_stage_osx_set_user_resizable (ClutterStageWindow *stage_window,
                                      gboolean            is_resizable)
{
  ClutterStageOSX *self = CLUTTER_STAGE_OSX (stage_window);

  CLUTTER_OSX_POOL_ALLOC();

  [self->window setShowsResizeIndicator:is_resizable];

  CLUTTER_OSX_POOL_RELEASE();
}

static void
clutter_stage_osx_set_accept_focus (ClutterStageWindow *stage_window,
                                    gboolean            accept_focus)
{
  ClutterStageOSX *self = CLUTTER_STAGE_OSX (stage_window);

  CLUTTER_OSX_POOL_ALLOC();

  self->acceptFocus = !!accept_focus;

  CLUTTER_OSX_POOL_RELEASE();
}

static void
clutter_stage_osx_redraw (ClutterStageWindow *stage_window)
{
  ClutterStageOSX *stage_osx = CLUTTER_STAGE_OSX (stage_window);

  CLUTTER_OSX_POOL_ALLOC();

  if (stage_osx->view != NULL)
    [stage_osx->view setNeedsDisplay: YES];

  CLUTTER_OSX_POOL_RELEASE();
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  iface->get_wrapper    = clutter_stage_osx_get_wrapper;
  iface->set_title      = clutter_stage_osx_set_title;
  iface->set_fullscreen = clutter_stage_osx_set_fullscreen;
  iface->show           = clutter_stage_osx_show;
  iface->hide           = clutter_stage_osx_hide;
  iface->realize        = clutter_stage_osx_realize;
  iface->unrealize      = clutter_stage_osx_unrealize;
  iface->get_geometry   = clutter_stage_osx_get_geometry;
  iface->resize         = clutter_stage_osx_resize;
  iface->set_cursor_visible = clutter_stage_osx_set_cursor_visible;
  iface->set_user_resizable = clutter_stage_osx_set_user_resizable;
  iface->set_accept_focus   = clutter_stage_osx_set_accept_focus;
  iface->redraw             = clutter_stage_osx_redraw;
}

/*************************************************************************/
ClutterStageWindow *
_clutter_stage_osx_new (ClutterBackend *backend,
                        ClutterStage   *wrapper)
{
  ClutterStageOSX *self;

  self = g_object_new (CLUTTER_TYPE_STAGE_OSX, NULL);
  self->backend = backend;
  self->wrapper = wrapper;

  return CLUTTER_STAGE_WINDOW(self);
}

/*************************************************************************/
static void
clutter_stage_osx_init (ClutterStageOSX *self)
{
  self->requisition_width  = 640;
  self->requisition_height = 480;
  self->acceptFocus = TRUE;
}

static void
clutter_stage_osx_finalize (GObject *gobject)
{
  G_OBJECT_CLASS (clutter_stage_osx_parent_class)->finalize (gobject);
}

static void
clutter_stage_osx_dispose (GObject *gobject)
{
  G_OBJECT_CLASS (clutter_stage_osx_parent_class)->dispose (gobject);
}

static void
clutter_stage_osx_class_init (ClutterStageOSXClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = clutter_stage_osx_finalize;
  gobject_class->dispose = clutter_stage_osx_dispose;
}
