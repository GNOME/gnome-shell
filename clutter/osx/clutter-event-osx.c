/* Clutter -  An OpenGL based 'interactive canvas' library.
 * OSX backend - event loops integration
 *
 * Copyright (C) 2007-2008  Tommi Komulainen <tommi.komulainen@iki.fi>
 * Copyright (C) 2007  OpenedHand Ltd.
 * Copyright (C) 2011  Crystalnix  <vgachkaylo@crystalnix.com>
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

#include "clutter-device-manager-osx.h"
#include "clutter-stage-osx.h"

#import <AppKit/AppKit.h>

#include <glib.h>

#include "clutter-debug.h"
#include "clutter-device-manager.h"
#include "clutter-event-private.h"
#include "clutter-keysyms.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

#define WHEEL_DELTA 1

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
  guint type = [self type];
  gint rv = 0;

  /* add key masks */
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

  /* add button mask */
  if ((type == NSLeftMouseDragged) ||
      (type == NSRightMouseDragged) ||
      (type == NSOtherMouseDragged))
    rv |= CLUTTER_BUTTON1_MASK << [self buttonNumber];

  return rv;
}

- (guint)clutterKeyVal
{
  unichar c;

  /* FIXME: doing this right is a lot of work, see gdkkeys-quartz.c in gtk+
   * For now handle some common/simple keys only. Might not work with other
   * hardware than mine (MacBook Pro, finnish layout). Sorry.
   *
   * charactersIgnoringModifiers ignores most modifiers, not Shift though.
   * So, for all Shift-modified keys we'll end up reporting 'keyval' identical
   * to 'unicode_value'  Instead of <Shift>a or <Shift>3 you'd get <Shift>A
   * and <Shift>#
   */

  if ([self type] == NSFlagsChanged)
    {
      switch ([self keyCode])
        {
        case 54: // Right Command
          return CLUTTER_KEY_Meta_R;
        case 55: // Left Command
          return CLUTTER_KEY_Meta_L;
        case 57: // Capslock
          return CLUTTER_KEY_Caps_Lock;
        case 56: // Left Shift
          return CLUTTER_KEY_Shift_L;
        case 60: // Right Shift
          return CLUTTER_KEY_Shift_R;
        case 58: // Left Alt
          return CLUTTER_KEY_Alt_L;
        case 61: // Right Alt
          return CLUTTER_KEY_Alt_R;
        case 59: // Left Ctrl
          return CLUTTER_KEY_Control_L;
        case 62: // Right Ctrl
          return CLUTTER_KEY_Control_R;
        case 63: // Function
          return CLUTTER_KEY_function;
        default: // No such key??!??
          CLUTTER_NOTE (EVENT, "Got NSFlagsChanged event with keyCode not a known modifier key: %d",
                              [self keyCode]);
          return CLUTTER_KEY_VoidSymbol;
        }
    }

  c = [[self charactersIgnoringModifiers] characterAtIndex:0];

  /* Latin-1 characters, 1:1 mapping - this ought to be reliable */
  if ((c >= 0x0020 && c <= 0x007e) ||
      (c >= 0x00a0 && c <= 0x00ff))
    return c;

  switch (c)
    {
    /* these should be fairly standard */
    /* (maybe add 0x0008 (Ctrl+H) for backspace too) */
    case 0x000d:
      return CLUTTER_KEY_Return;
    case 0x001b:
      return CLUTTER_KEY_Escape;
    case 0x007f:
      return CLUTTER_KEY_BackSpace;
    /* Defined in NSEvent.h */
    case NSUpArrowFunctionKey:
      return CLUTTER_KEY_Up;
    case NSDownArrowFunctionKey:
      return CLUTTER_KEY_Down;
    case NSLeftArrowFunctionKey:
      return CLUTTER_KEY_Left;
    case NSRightArrowFunctionKey:
      return CLUTTER_KEY_Right;
    case NSF1FunctionKey:
      return CLUTTER_KEY_F1;
    case NSF2FunctionKey:
      return CLUTTER_KEY_F2;
    case NSF3FunctionKey:
      return CLUTTER_KEY_F3;
    case NSF4FunctionKey:
      return CLUTTER_KEY_F4;
    case NSF5FunctionKey:
      return CLUTTER_KEY_F5;
    case NSF6FunctionKey:
      return CLUTTER_KEY_F6;
    case NSF7FunctionKey:
      return CLUTTER_KEY_F7;
    case NSF8FunctionKey:
      return CLUTTER_KEY_F8;
    case NSF9FunctionKey:
      return CLUTTER_KEY_F9;
    case NSF10FunctionKey:
      return CLUTTER_KEY_F10;
    case NSF11FunctionKey:
      return CLUTTER_KEY_F11;
    case NSF12FunctionKey:
      return CLUTTER_KEY_F12;
    case NSInsertFunctionKey:
      return CLUTTER_KEY_Insert;
    case NSDeleteFunctionKey:
      return CLUTTER_KEY_Delete;
    case NSHomeFunctionKey:
      return CLUTTER_KEY_Home;
    case NSEndFunctionKey:
      return CLUTTER_KEY_End;
    case NSPageUpFunctionKey:
      return CLUTTER_KEY_Page_Up;
    case NSPageDownFunctionKey:
      return CLUTTER_KEY_Page_Down;
    }

  CLUTTER_NOTE (BACKEND, "unhandled unicode key 0x%x (%d)", c, c);

  /* hardware dependent, worksforme(tm) Redundant due to above, left around as
   * example.
   */
  switch ([self keyCode])
    {
    case 115:
      return CLUTTER_KEY_Home;
    case 116:
      return CLUTTER_KEY_Page_Up;
    case 117:
      return CLUTTER_KEY_Delete;
    case 119:
      return CLUTTER_KEY_End;
    case 121:
      return CLUTTER_KEY_Page_Down;
    case 123:
      return CLUTTER_KEY_Left;
    case 124:
      return CLUTTER_KEY_Right;
    case 125:
      return CLUTTER_KEY_Down;
    case 126:
      return CLUTTER_KEY_Up;
    }

  return 0;
}
@end

/*************************************************************************/

static void
take_and_queue_event (ClutterEvent *event)
{
  _clutter_event_push (event, FALSE);
}

static void
process_scroll_event (ClutterEvent *event,
                      gboolean isVertical)
{
  ClutterStageWindow *impl;
  ClutterStageOSX *stage_osx;
  gfloat *scroll_pos;

  impl = _clutter_stage_get_window (event->any.stage);
  stage_osx = CLUTTER_STAGE_OSX (impl);
  
  scroll_pos = isVertical
             ? &(stage_osx->scroll_pos_y)
             : &(stage_osx->scroll_pos_x);
 
  while (abs (*scroll_pos) >= WHEEL_DELTA) 
    {
      ClutterEvent *event_gen = clutter_event_new (CLUTTER_SCROLL);

      event_gen->scroll.time = event->any.time;
      event_gen->scroll.modifier_state = event->scroll.modifier_state;
      event_gen->any.stage = event->any.stage;

      event_gen->scroll.x = event->scroll.x;
      event_gen->scroll.y = event->scroll.y;

      if (*scroll_pos > 0)
        {
          event_gen->scroll.direction = isVertical ? CLUTTER_SCROLL_UP : CLUTTER_SCROLL_RIGHT;
          *scroll_pos -= WHEEL_DELTA;
        }
      else
        {
          event_gen->scroll.direction = isVertical ? CLUTTER_SCROLL_DOWN : CLUTTER_SCROLL_LEFT;
          *scroll_pos += WHEEL_DELTA;
        }

      clutter_event_set_device (event_gen, clutter_event_get_device (event));

      take_and_queue_event (event_gen);
      
      CLUTTER_NOTE (EVENT, "scroll %s at %f,%f",
                    (event_gen->scroll.direction == CLUTTER_SCROLL_UP) ? "UP" :
                    ( 
                    (event_gen->scroll.direction == CLUTTER_SCROLL_DOWN) ? "DOWN" :
                    (
                    (event_gen->scroll.direction == CLUTTER_SCROLL_RIGHT) ? "RIGHT" : "LEFT")),
                    event->scroll.x, event->scroll.y);
    } 
}

static gboolean
clutter_event_osx_translate (NSEvent *nsevent,
                             ClutterEvent *event)
{
  ClutterDeviceManagerOSX *manager_osx;
  ClutterDeviceManager *manager;
  ClutterStageOSX *stage_osx;
  ClutterStageWindow *impl;
  ClutterStage *stage;

  manager = clutter_device_manager_get_default ();
  if (manager == NULL)
    return FALSE;

  stage = event->any.stage;
  impl = _clutter_stage_get_window (event->any.stage);
  stage_osx = CLUTTER_STAGE_OSX (impl);
  manager_osx = CLUTTER_DEVICE_MANAGER_OSX (manager);

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
      clutter_event_set_device (event, manager_osx->core_pointer);

      CLUTTER_NOTE (EVENT, "button %d %s at %f,%f clicks=%d",
                    (int)[nsevent buttonNumber],
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
      clutter_event_set_device (event, manager_osx->core_pointer);

      CLUTTER_NOTE (EVENT, "motion %d at %f,%f",
                    (int)[nsevent buttonNumber],
                    event->button.x, event->button.y);
      return TRUE;

    case NSMouseEntered:
      event->type = CLUTTER_ENTER;

      [nsevent clutterX:&(event->crossing.x) y:&(event->crossing.y)];
      event->crossing.related = NULL;
      event->crossing.source = CLUTTER_ACTOR (stage);
      clutter_event_set_device (event, manager_osx->core_pointer);

      _clutter_input_device_set_stage (manager_osx->core_pointer, stage);

      CLUTTER_NOTE (EVENT, "enter at %f,%f",
                    event->crossing.x, event->crossing.y);
      return TRUE;

    case NSMouseExited:
      event->type = CLUTTER_LEAVE;

      [nsevent clutterX:&(event->crossing.x) y:&(event->crossing.y)];
      event->crossing.related = NULL;
      event->crossing.source = CLUTTER_ACTOR (stage);
      clutter_event_set_device (event, manager_osx->core_pointer);

      _clutter_input_device_set_stage (manager_osx->core_pointer, NULL);

      CLUTTER_NOTE (EVENT, "exit at %f,%f",
                    event->crossing.x, event->crossing.y);
      return TRUE;

    case NSScrollWheel:
      stage_osx->scroll_pos_x += [nsevent deltaX];
      stage_osx->scroll_pos_y += [nsevent deltaY];
      
      [nsevent clutterX:&(event->scroll.x) y:&(event->scroll.y)];
      event->scroll.modifier_state = [nsevent clutterModifierState];
      clutter_event_set_device (event, manager_osx->core_pointer);
      
      process_scroll_event (event, TRUE);
      process_scroll_event (event, FALSE);
      break;
      
    case NSFlagsChanged:
      // FIXME: This logic fails if the user presses both Shift keys at once, for example:
      // we treat releasing one of them as keyDown.
      switch ([nsevent keyCode])
        {
        case 54: // Right Command
        case 55: // Left Command
          if ([nsevent modifierFlags] & NSCommandKeyMask)
            event->type = CLUTTER_KEY_PRESS;
          break;

        case 57: // Capslock
          if ([nsevent modifierFlags] & NSAlphaShiftKeyMask)
            event->type = CLUTTER_KEY_PRESS;
          break;

        case 56: // Left Shift
        case 60: // Right Shift
          if ([nsevent modifierFlags] & NSShiftKeyMask)
            event->type = CLUTTER_KEY_PRESS;
          break;

        case 58: // Left Alt
        case 61: // Right Alt
          if ([nsevent modifierFlags] & NSAlternateKeyMask)
            event->type = CLUTTER_KEY_PRESS;
          break;

        case 59: // Left Ctrl
        case 62: // Right Ctrl
          if ([nsevent modifierFlags] & NSControlKeyMask)
            event->type = CLUTTER_KEY_PRESS;
          break;

        case 63: // Function
          if ([nsevent modifierFlags] & NSFunctionKeyMask)
            event->type = CLUTTER_KEY_PRESS;
          break;
        }
      /* fall through */
    case NSKeyDown:
      if ([nsevent type] == NSKeyDown)
        event->type = CLUTTER_KEY_PRESS;
      /* fall through */
    case NSKeyUp:
      if (event->type != CLUTTER_KEY_PRESS)
        event->type = CLUTTER_KEY_RELEASE;

      event->key.hardware_keycode = [nsevent keyCode];
      event->key.modifier_state = [nsevent clutterModifierState];
      event->key.keyval = [nsevent clutterKeyVal];
      event->key.unicode_value = ([nsevent type] == NSFlagsChanged)
                                    ? (gunichar)'\0'
                                    : [[nsevent characters] characterAtIndex:0];
      clutter_event_set_device (event, manager_osx->core_keyboard);

      CLUTTER_NOTE (EVENT, "key %d (%s) (%s) %s, keyval %d",
                    [nsevent keyCode],
                    ([nsevent type] == NSFlagsChanged) ? "NULL" : [[nsevent characters] UTF8String],
                    ([nsevent type] == NSFlagsChanged) ? "NULL" : [[nsevent charactersIgnoringModifiers] UTF8String],
                    (event->type == CLUTTER_KEY_PRESS) ? "press" : "release",
                    event->key.keyval);
      return TRUE;

    default:
      CLUTTER_NOTE (EVENT, "unhandled event %d", (int)[nsevent type]);
      break;
    }

  return FALSE;
}

void
_clutter_event_osx_put (NSEvent      *nsevent,
                        ClutterStage *wrapper)
{
  ClutterEvent *event = clutter_event_new (CLUTTER_NOTHING);

  /* common fields */
  event->any.stage = wrapper;
  event->any.time = [nsevent clutterTime];

  if (clutter_event_osx_translate (nsevent, event))
    {
      g_assert (event->type != CLUTTER_NOTHING);

      _clutter_event_push (event, FALSE);
    }
  else
    clutter_event_free (event);
}
