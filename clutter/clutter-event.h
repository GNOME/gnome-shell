/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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

#ifndef _HAVE_CLUTTER_EVENT_H
#define _HAVE_CLUTTER_EVENT_H

#include <glib-object.h>
#include <clutter/clutter-types.h>

#define CLUTTER_TYPE_EVENT	(clutter_event_get_type ())
#define CLUTTER_PRIORITY_EVENTS (G_PRIORITY_DEFAULT)
#define CLUTTER_CURRENT_TIME    0L

G_BEGIN_DECLS

typedef enum {
  CLUTTER_SHIFT_MASK    = 1 << 0,
  CLUTTER_LOCK_MASK     = 1 << 1,
  CLUTTER_CONTROL_MASK  = 1 << 2,
  CLUTTER_MOD1_MASK     = 1 << 3,
  CLUTTER_MOD2_MASK     = 1 << 4,
  CLUTTER_MOD3_MASK     = 1 << 5,
  CLUTTER_MOD4_MASK     = 1 << 6,
  CLUTTER_MOD5_MASK     = 1 << 7,
  CLUTTER_BUTTON1_MASK  = 1 << 8,
  CLUTTER_BUTTON2_MASK  = 1 << 9,
  CLUTTER_BUTTON3_MASK  = 1 << 10,
  CLUTTER_BUTTON4_MASK  = 1 << 11,
  CLUTTER_BUTTON5_MASK  = 1 << 12
} ClutterModifierType;

typedef enum 
{
  CLUTTER_NOTHING = 0,
  
  CLUTTER_KEY_PRESS,
  CLUTTER_KEY_RELEASE,
  CLUTTER_MOTION,
  CLUTTER_BUTTON_PRESS,
  CLUTTER_2BUTTON_PRESS, 	/* Double click */
  CLUTTER_3BUTTON_PRESS,        /* Triple click */
  CLUTTER_BUTTON_RELEASE,
  CLUTTER_SCROLL,
  CLUTTER_STAGE_STATE,
  CLUTTER_DESTROY_NOTIFY,
  CLUTTER_CLIENT_MESSAGE,
  CLUTTER_DELETE
} ClutterEventType;

typedef enum
{
  CLUTTER_SCROLL_UP,
  CLUTTER_SCROLL_DOWN,
  CLUTTER_SCROLL_LEFT,
  CLUTTER_SCROLL_RIGHT
} ClutterScrollDirection;

typedef enum
{
  CLUTTER_STAGE_STATE_FULLSCREEN       = (1<<1),
  CLUTTER_STAGE_STATE_OFFSCREEN        = (1<<2),
  CLUTTER_STAGE_STATE_POINTER_ENTER    = (1<<3),
  CLUTTER_STAGE_STATE_POINTER_LEAVE    = (1<<4),
  CLUTTER_STAGE_STATE_ACTIVATED        = (1<<5),
} ClutterStageState;

typedef union _ClutterEvent ClutterEvent;

typedef struct _ClutterAnyEvent         ClutterAnyEvent;
typedef struct _ClutterButtonEvent      ClutterButtonEvent;
typedef struct _ClutterKeyEvent         ClutterKeyEvent;
typedef struct _ClutterMotionEvent      ClutterMotionEvent;
typedef struct _ClutterScrollEvent      ClutterScrollEvent;
typedef struct _ClutterStageStateEvent  ClutterStageStateEvent;

typedef struct _ClutterInputDevice      ClutterInputDevice;

struct _ClutterAnyEvent
{
  ClutterEventType  type;
};

struct _ClutterKeyEvent
{
  ClutterEventType type;
  guint32 time;
  ClutterModifierType modifier_state;
  guint keyval;
  guint16 hardware_keycode;
  ClutterActor *source;
};

struct _ClutterButtonEvent
{
  ClutterEventType type;
  guint32 time;
  gint x;
  gint y;
  ClutterModifierType modifier_state;
  guint32 button;
  gdouble *axes; /* Future use */
  ClutterInputDevice *device; /* Future use */
  ClutterActor *source;
};

struct _ClutterMotionEvent
{
  ClutterEventType type;
  guint32 time;
  gint x;
  gint y;
  ClutterModifierType modifier_state;
  gdouble *axes; /* Future use */
  ClutterInputDevice *device; /* Future use */
  ClutterActor *source;
};

struct _ClutterScrollEvent
{
  ClutterEventType type;
  guint32 time;
  gint x;
  gint y;
  ClutterScrollDirection direction;
  ClutterModifierType modifier_state;
  gdouble *axes; /* future use */
  ClutterInputDevice *device; /* future use */
  ClutterActor *source;
};

struct _ClutterStageStateEvent
{
  ClutterEventType type;
  ClutterStageState changed_mask;
  ClutterStageState new_state;
};

union _ClutterEvent
{
  ClutterEventType type;
  
  ClutterAnyEvent any;
  ClutterButtonEvent button;
  ClutterKeyEvent key;
  ClutterMotionEvent motion;
  ClutterScrollEvent scroll;
  ClutterStageStateEvent stage_state;
};

GType clutter_event_get_type (void) G_GNUC_CONST;

gboolean            clutter_events_pending   (void);
ClutterEvent *      clutter_event_get        (void);
ClutterEvent *      clutter_event_peek       (void);
void                clutter_event_put        (ClutterEvent     *event);
ClutterEvent *      clutter_event_new        (ClutterEventType  type);
ClutterEvent *      clutter_event_copy       (ClutterEvent     *event);
void                clutter_event_free       (ClutterEvent     *event);
ClutterEventType    clutter_event_type       (ClutterEvent     *event);
guint32             clutter_event_get_time   (ClutterEvent     *event);
ClutterModifierType clutter_event_get_state  (ClutterEvent     *event);
void                clutter_event_get_coords (ClutterEvent     *event,
                                              gint             *x,
                                              gint             *y);

guint   clutter_key_event_symbol  (ClutterKeyEvent *keyev);
guint16 clutter_key_event_code    (ClutterKeyEvent *keyev);
guint32 clutter_key_event_unicode (ClutterKeyEvent *keyev);

guint32 clutter_button_event_button (ClutterButtonEvent *buttev);

guint32 clutter_keysym_to_unicode (guint keyval);

ClutterActor* clutter_event_get_source (ClutterEvent *event);

G_END_DECLS

#endif
