#include <clutter/clutter.h>
#include "clutter-backend-fruity.h"
#include "clutter-stage-fruity.h"
#include "../clutter-main.h"
#include "../clutter-private.h"
#import <UIKit/UIKit.h>

#import  <Foundation/Foundation.h>
#import  <CoreFoundation/CoreFoundation.h>
#include <GraphicsServices/GraphicsServices.h>
#include <OpenGLES/gl.h>
#include <glib.h>

#import  <UIKit/UIView.h>
#import  <UIKit/UITextView.h>
#import  <UIKit/UIHardware.h>
#import  <UIKit/UINavigationBar.h>
#import  <UIKit/UIView-Geometry.h>
#include "clutter-fruity.h"

static EGLDisplay mEGLDisplay;
static EGLContext mEGLContext;
static EGLSurface mEGLSurface;
static CoreSurfaceBufferRef mScreenSurface;
static gboolean alive = TRUE;

@interface StageView : UIView
{
}

@end

static CoreSurfaceBufferRef CreateSurface(int w, int h)
{
  int pitch = w * 2, allocSize = 2 * w * h;
  char *pixelFormat = "565L";
  CFMutableDictionaryRef dict;

  dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFDictionarySetValue(dict, kCoreSurfaceBufferGlobal,        kCFBooleanTrue);
  CFDictionarySetValue(dict, kCoreSurfaceBufferMemoryRegion,  CFSTR("PurpleGFXMem"));
  CFDictionarySetValue(dict, kCoreSurfaceBufferPitch,         CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pitch));
  CFDictionarySetValue(dict, kCoreSurfaceBufferWidth,         CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &w));
  CFDictionarySetValue(dict, kCoreSurfaceBufferHeight,        CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &h));
  CFDictionarySetValue(dict, kCoreSurfaceBufferPixelFormat,   CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, pixelFormat));
  CFDictionarySetValue(dict, kCoreSurfaceBufferAllocSize,     CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &allocSize));
  return CoreSurfaceBufferCreate(dict);
}


@implementation StageView

- (void) mouseDown:(GSEvent*)event
{
    CGPoint location= GSEventGetLocationInWindow(event);
    ClutterBackendEGL *backend_fruity = CLUTTER_BACKEND_EGL (clutter_get_default_backend());
    ClutterStage *stage = CLUTTER_STAGE_EGL(backend_fruity->stage)->wrapper;
    ClutterEvent *cev;
    float x = location.x;
    float y = location.y;

    cev = clutter_event_new (CLUTTER_BUTTON_PRESS);
    cev->button.x = x;
    cev->button.y = y;
    cev->button.button = 1;
    cev->button.time = clutter_get_timestamp () / 1000;
    cev->any.stage = stage;

    clutter_do_event (cev);
    clutter_event_free (cev);
}

- (void) mouseUp:(GSEvent*)event
{
    ClutterEvent *cev;
    ClutterBackendEGL *backend_fruity = CLUTTER_BACKEND_EGL (clutter_get_default_backend());
    ClutterStage *stage = CLUTTER_STAGE_EGL(backend_fruity->stage)->wrapper;
    CGPoint location= GSEventGetLocationInWindow(event);
    float x = location.x;
    float y = location.y;

    cev = clutter_event_new (CLUTTER_BUTTON_RELEASE);
    cev->button.x = x;
    cev->button.y = y;
    cev->button.button = 1;
    cev->button.time = clutter_get_timestamp () / 1000;
    cev->any.stage = stage;
    clutter_do_event (cev);
    clutter_event_free (cev);
}

- (void) mouseDragged:(GSEvent*)event
{
    ClutterEvent *cev;
    ClutterBackendEGL *backend_fruity = CLUTTER_BACKEND_EGL (clutter_get_default_backend());
    ClutterStage *stage = CLUTTER_STAGE_EGL(backend_fruity->stage)->wrapper;
    CGPoint location= GSEventGetLocationInWindow(event);
    float x = location.x;
    float y = location.y;

    cev = clutter_event_new (CLUTTER_MOTION);
    cev->motion.x = x;
    cev->motion.y = y;
    cev->motion.time = clutter_get_timestamp () / 1000;
    cev->any.stage = stage;
    clutter_do_event (cev);
    clutter_event_free (cev);
}

@end


@interface ClutterUIKit : UIApplication
{
  StageView *stage_view;
}
@end

@implementation ClutterUIKit

- (void) applicationDidFinishLaunching: (id) unused
{
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    [UIHardware _setStatusBarHeight:0.0f];
    [self setStatusBarMode:2 orientation:0 duration:0.0f fenceID:0];

    CGRect screenRect = [UIHardware fullScreenApplicationContentRect];
    UIWindow* window = [[UIWindow alloc] initWithContentRect: screenRect];

    [window orderFront: self];
    [window makeKey: self];
    [window _setHidden: NO];

    [NSTimer 
        scheduledTimerWithTimeInterval:0.0025
        target: self 
        selector: @selector(update) 
        userInfo: nil
        repeats: YES
    ];

    StageView *stageView = [StageView alloc];
    [stageView initWithFrame: screenRect];
    [window setContentView: stageView];

    stage_view = stageView;

    [pool release];
}

- (void)applicationWillTerminate
{
  /* FIXME: here we should do things to shut down the uikit application */
  [stage_view release];
  ClutterBackendEGL *backend_fruity = CLUTTER_BACKEND_EGL (clutter_get_default_backend());
  ClutterStageEGL   *stage_fruity;
  stage_fruity = CLUTTER_STAGE_EGL(backend_fruity->stage);

  alive = FALSE;
  clutter_actor_unrealize (CLUTTER_ACTOR (stage_fruity));
  clutter_main_quit ();
}

- (void)applicationWillSuspend
{
  ClutterBackendEGL *backend_fruity = CLUTTER_BACKEND_EGL (clutter_get_default_backend());
  ClutterStageEGL   *stage_fruity;
  stage_fruity = CLUTTER_STAGE_EGL(backend_fruity->stage);
  alive = FALSE;
}

- (void)applicationDidResumeFromUnderLock
{
  alive = TRUE;
  [stage_view setNeedsDisplay];
}

- (void) update
{
   if (alive && g_main_context_pending (NULL))
      g_main_context_iteration (NULL, FALSE);
}

@end

void clutter_fruity_main (void)
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  UIApplicationMain(0, NULL, [ClutterUIKit class]);
  [pool release];
}





