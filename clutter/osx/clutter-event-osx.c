/* Clutter -  An OpenGL based 'interactive canvas' library.
 * OSX backend - event loops integration
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "config.h"

#include "clutter-osx.h"
#include "clutter-stage-osx.h"

#import <AppKit/AppKit.h>
#include <glib/gmain.h>
#include <clutter/clutter-debug.h>
#include <clutter/clutter-private.h>
#include <clutter/clutter-keysyms.h>

/* Overriding the poll function because the events are not delivered over file
 * descriptors and setting up a GSource would just introduce polling.
 */

static GPollFunc old_poll_func = NULL;

/*************************************************************************/
@interface NSEvent (Clutter)
- (gint)clutterTime;
- (gint)clutterButton;
- (void)clutterX:(gfloat*)ptrX y:(gfloat*)ptrY;
- (gint)clutterModifierState;
- (guint)clutterKeyVal;
@end

@implementation NSEvent (Clutter)
- (gint)clutterTime
{
  return [self timestamp] * 1000;
}

- (gint)clutterButton
{
  switch ([self buttonNumber])
    {
    case 0: return 1;   /* left   */
    case 1: return 3;   /* right  */
    case 2: return 2;   /* middle */
    default: return 1 + [self buttonNumber];
    }
}

- (void)clutterX:(gfloat*)ptrX y:(gfloat*)ptrY
{
  NSView *view = [[self window] contentView];
  NSPoint pt = [view convertPoint:[self locationInWindow] fromView:nil];

  *ptrX = pt.x;
  *ptrY = pt.y;
}

- (gint)clutterModifierState
{
  guint mods = [self modifierFlags];
  gint rv = 0;

  if (mods & NSAlphaShiftKeyMask)
    rv |= CLUTTER_LOCK_MASK;
  if (mods & NSShiftKeyMask)
    rv |= CLUTTER_SHIFT_MASK;
  if (mods & NSControlKeyMask)
    rv |= CLUTTER_CONTROL_MASK;
  if (mods & NSAlternateKeyMask)
    rv |= CLUTTER_MOD1_MASK;
  if (mods & NSCommandKeyMask)
    rv |= CLUTTER_MOD2_MASK;

  return rv;
}

- (guint)clutterKeyVal
{
  /* FIXME: doing this right is a lot of work, see gdkkeys-quartz.c in gtk+
   * For now handle some common/simple keys only. Might not work with other
   * hardware than mine (MacBook Pro, finnish layout). Sorry.
   *
   * charactersIgnoringModifiers ignores most modifiers, not Shift though.
   * So, for all Shift-modified keys we'll end up reporting 'keyval' identical
   * to 'unicode_value'  Instead of <Shift>a or <Shift>3 you'd get <Shift>A
   * and <Shift>#
   */
  unichar c = [[self charactersIgnoringModifiers] characterAtIndex:0];

  /* Latin-1 characters, 1:1 mapping - this ought to be reliable */
  if ((c >= 0x0020 && c <= 0x007e) ||
      (c >= 0x00a0 && c <= 0x00ff))
    return c;

  switch (c)
    {
    /* these should be fairly standard */
    /* (maybe add 0x0008 (Ctrl+H) for backspace too) */
    case 0x000d: return CLUTTER_Return;
    case 0x001b: return CLUTTER_Escape;
    case 0x007f: return CLUTTER_BackSpace;
    /* Defined in NSEvent.h */
    case NSUpArrowFunctionKey:    return CLUTTER_Up;
    case NSDownArrowFunctionKey:  return CLUTTER_Down;
    case NSLeftArrowFunctionKey:  return CLUTTER_Left;
    case NSRightArrowFunctionKey: return CLUTTER_Right;
    case NSF1FunctionKey:         return CLUTTER_F1;
    case NSF2FunctionKey:         return CLUTTER_F2;
    case NSF3FunctionKey:         return CLUTTER_F3;
    case NSF4FunctionKey:         return CLUTTER_F4;
    case NSF5FunctionKey:         return CLUTTER_F5;
    case NSF6FunctionKey:         return CLUTTER_F6;
    case NSF7FunctionKey:         return CLUTTER_F7;
    case NSF8FunctionKey:         return CLUTTER_F8;
    case NSF9FunctionKey:         return CLUTTER_F9;
    case NSF10FunctionKey:        return CLUTTER_F10;
    case NSF11FunctionKey:        return CLUTTER_F11;
    case NSF12FunctionKey:        return CLUTTER_F12;
    case NSInsertFunctionKey:     return CLUTTER_Insert;
    case NSDeleteFunctionKey:     return CLUTTER_Delete;
    case NSHomeFunctionKey:       return CLUTTER_Home;
    case NSEndFunctionKey:        return CLUTTER_End;
    case NSPageUpFunctionKey:     return CLUTTER_Page_Up;
    case NSPageDownFunctionKey:   return CLUTTER_Page_Down;
    }

  CLUTTER_NOTE (BACKEND, "unhandled unicode key 0x%x (%d)", c, c);

  /* hardware dependent, worksforme(tm) Redundant due to above, left around as
   * example.
   */
  switch ([self keyCode])
    {
    case 115: return CLUTTER_Home;
    case 116: return CLUTTER_Page_Up;
    case 117: return CLUTTER_Delete;
    case 119: return CLUTTER_End;
    case 121: return CLUTTER_Page_Down;
    case 123: return CLUTTER_Left;
    case 124: return CLUTTER_Right;
    case 125: return CLUTTER_Down;
    case 126: return CLUTTER_Up;
    }

  return 0;
}
@end

/*************************************************************************/
static gboolean
clutter_event_osx_translate (NSEvent *nsevent, ClutterEvent *event)
{
  event->any.time = [nsevent clutterTime];

  switch ([nsevent type])
    {
    case NSLeftMouseDown:
    case NSRightMouseDown:
    case NSOtherMouseDown:
      event->type = CLUTTER_BUTTON_PRESS;
      /* fall through */
    case NSLeftMouseUp:
    case NSRightMouseUp:
    case NSOtherMouseUp:
      if (event->type != CLUTTER_BUTTON_PRESS)
        event->type = CLUTTER_BUTTON_RELEASE;

      event->button.button = [nsevent clutterButton];
      event->button.click_count = [nsevent clickCount];
      event->motion.modifier_state = [nsevent clutterModifierState];
      [nsevent clutterX:&(event->button.x) y:&(event->button.y)];

      CLUTTER_NOTE (EVENT, "button %d %s at %d,%d clicks=%d",
                    [nsevent buttonNumber],
                    event->type == CLUTTER_BUTTON_PRESS ? "press" : "release",
                    event->button.x, event->button.y,
                    event->button.click_count);
      return TRUE;

    case NSMouseMoved:
    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSOtherMouseDragged:
      event->type = CLUTTER_MOTION;

      [nsevent clutterX:&(event->motion.x) y:&(event->motion.y)];
      event->motion.modifier_state = [nsevent clutterModifierState];

      CLUTTER_NOTE (EVENT, "motion %d at %d,%d",
                    [nsevent buttonNumber],
                    event->button.x, event->button.y);
      return TRUE;

    case NSKeyDown:
      event->type = CLUTTER_KEY_PRESS;
      /* fall through */
    case NSKeyUp:
      if (event->type != CLUTTER_KEY_PRESS)
        event->type = CLUTTER_KEY_RELEASE;

      event->key.hardware_keycode = [nsevent keyCode];
      event->key.modifier_state = [nsevent clutterModifierState];
      event->key.keyval = [nsevent clutterKeyVal];
      event->key.unicode_value = [[nsevent characters] characterAtIndex:0];

      CLUTTER_NOTE (EVENT, "key %d (%s) (%s) %s, keyval %d",
                    [nsevent keyCode],
                    [[nsevent characters] UTF8String],
                    [[nsevent charactersIgnoringModifiers] UTF8String],
                    event->type == CLUTTER_KEY_PRESS ? "press" : "release",
                    event->key.keyval);
      return TRUE;

    default:
      CLUTTER_NOTE (EVENT, "unhandled event %d", [nsevent type]);
      break;
    }

  return FALSE;
}

void
_clutter_event_osx_put (NSEvent *nsevent, ClutterStage *wrapper)
{
  ClutterEvent event = { 0, };

  event.any.stage = wrapper;

  if (clutter_event_osx_translate (nsevent, &event))
    {
      g_assert (event.type != CLUTTER_NOTHING);
      clutter_event_put (&event);
    }
}

typedef struct {
  CFSocketRef        sock;
  CFRunLoopSourceRef source;

  gushort            revents;
} SocketInfo;

static void
socket_activity_cb (CFSocketRef           sock,
                    CFSocketCallBackType  cbtype,
                    CFDataRef             address,
                    const void           *data,
                    void                 *info)
{
  SocketInfo *si = info;

  if (cbtype & kCFSocketReadCallBack)
    si->revents |= G_IO_IN;
  if (cbtype & kCFSocketWriteCallBack)
    si->revents |= G_IO_OUT;
}

static gint
clutter_event_osx_poll_func (GPollFD *ufds, guint nfds, gint timeout)
{
  NSDate     *until_date;
  NSEvent    *nsevent;
  SocketInfo *sockets = NULL;
  gint        n_active = 0;

  CLUTTER_OSX_POOL_ALLOC();

  if (timeout == -1)
    until_date = [NSDate distantFuture];
  else if (timeout == 0)
    until_date = [NSDate distantPast];
  else
    until_date = [NSDate dateWithTimeIntervalSinceNow:timeout/1000.0];

  /* File descriptors appear to be similar enough to sockets so that they can
   * be used in CFRunLoopSource.
   *
   * We could also launch a thread to call old_poll_func and signal the main
   * thread. No idea which way is better.
   */
  if (nfds > 0)
    {
      CFRunLoopRef run_loop;

      run_loop = [[NSRunLoop currentRunLoop] getCFRunLoop];
      sockets = g_new (SocketInfo, nfds);

      int i;
      for (i = 0; i < nfds; i++)
        {
          SocketInfo *si = &sockets[i];
          CFSocketCallBackType cbtype;

          cbtype = 0;
          if (ufds[i].events & G_IO_IN)
            cbtype |= kCFSocketReadCallBack;
          if (ufds[i].events & G_IO_OUT)
            cbtype |= kCFSocketWriteCallBack;
          /* FIXME: how to handle G_IO_HUP and G_IO_ERR? */

          const CFSocketContext ctxt = {
            0, si, NULL, NULL, NULL
          };
          si->sock = CFSocketCreateWithNative (NULL, ufds[i].fd, cbtype, socket_activity_cb, &ctxt);
          si->source = CFSocketCreateRunLoopSource (NULL, si->sock, 0);
          si->revents = 0;

          CFRunLoopAddSource (run_loop, si->source, kCFRunLoopCommonModes);
        }
    }

  nsevent = [NSApp nextEventMatchingMask: NSAnyEventMask
                               untilDate: until_date
                                  inMode: NSDefaultRunLoopMode
                                 dequeue: YES];

  /* Push the events to NSApplication which will do some magic(?) and forward
   * interesting events to our view. While we could do event translation here
   * we'd also need to filter out clicks on titlebar, and perhaps do special
   * handling for the first click (couldn't figure it out - always ended up
   * missing a screen refresh) and maybe other things.
   */
  [NSApp sendEvent:nsevent];

  if (nfds > 0)
    {
      int i;
      for (i = 0; i < nfds; i++)
        {
          SocketInfo *si = &sockets[i];

          if ((ufds[i].revents = si->revents) != 0)
            n_active++;

          /* Invalidating the source also removes it from run loop and
           * guarantees the callback is never called again.
           * CFRunLoopRemoveSource removes the source from the loop, but might
           * still call the callback which would be badly timed.
           */
          CFRunLoopSourceInvalidate (si->source);
          CFRelease (si->source);
          CFRelease (si->sock);
        }

      g_free (sockets);
    }

  /* FIXME this could result in infinite loop */
  ClutterEvent *event = clutter_event_get ();
  while (event)
    {
      clutter_do_event (event);
      clutter_event_free (event);
      event = clutter_event_get ();
    }

  CLUTTER_OSX_POOL_RELEASE();

  return n_active;
}

void
_clutter_events_osx_init (void)
{
  g_assert (old_poll_func == NULL);

  old_poll_func = g_main_context_get_poll_func (NULL);
  g_main_context_set_poll_func (NULL, clutter_event_osx_poll_func);
}

void
_clutter_events_osx_uninit (void)
{
  if (old_poll_func)
    {
      g_main_context_set_poll_func (NULL, old_poll_func);
      old_poll_func = NULL;
    }
}
