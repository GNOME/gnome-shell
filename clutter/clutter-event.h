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

G_BEGIN_DECLS


enum
{
  /* Map to xlibs masks */
  CLUTTER_BUTTON1_MASK	= (1<<8),
  CLUTTER_BUTTON2_MASK	= (1<<9),
  CLUTTER_BUTTON3_MASK	= (1<<10),
  CLUTTER_BUTTON4_MASK	= (1<<11),
  CLUTTER_BUTTON5_MASK	= (1<<12)
};

typedef enum 
{
  CLUTTER_NOTHING,
  
  CLUTTER_KEY_PRESS,
  CLUTTER_KEY_RELEASE,
  CLUTTER_MOTION,
  CLUTTER_BUTTON_PRESS,
  CLUTTER_2BUTTON_PRESS, 	/* Double click */
  CLUTTER_BUTTON_RELEASE,
  CLUTTER_DELETE_EVENT
} ClutterEventType;

#define CLUTTER_TYPE_EVENT	(clutter_event_get_type ())

typedef union _ClutterEvent ClutterEvent;

typedef struct _ClutterAnyEvent    ClutterAnyEvent;
typedef struct _ClutterKeyEvent    ClutterKeyEvent;
typedef struct _ClutterButtonEvent ClutterButtonEvent;
typedef struct _ClutterMotionEvent ClutterMotionEvent;

typedef struct _ClutterInputDevice ClutterInputDevice;


struct _ClutterAnyEvent
{
  ClutterEventType  type;
};

struct _ClutterKeyEvent
{
  ClutterEventType type;
  guint32          time;
  guint            modifier_state;
  guint            keyval;
  guint16          hardware_keycode;
};

struct _ClutterButtonEvent
{
  ClutterEventType     type;
  guint32              time;
  gint                 x;
  gint                 y;
  guint32              modifier_state;
  guint32              button;
  gdouble             *axes;   /* Future use */
  ClutterInputDevice *device;  /* Future use */
};

struct _ClutterMotionEvent
{
  ClutterEventType     type;
  guint32              time;
  gint                 x;
  gint                 y;
  guint32              modifier_state;
  gdouble             *axes;	/* Future use */
  ClutterInputDevice *device; 	/* Future use */
};

union _ClutterEvent
{
  ClutterEventType   type;
  
  ClutterAnyEvent    any;
  ClutterKeyEvent    key;
  ClutterButtonEvent button;
  ClutterMotionEvent motion;
};

GType clutter_event_get_type (void) G_GNUC_CONST;

ClutterEvent     *clutter_event_new  (ClutterEventType  type);
ClutterEvent     *clutter_event_copy (ClutterEvent     *event);
void              clutter_event_free (ClutterEvent     *event);
ClutterEventType  clutter_event_type (ClutterEvent     *event);

guint32 clutter_key_event_time    (ClutterKeyEvent *keyev);
guint   clutter_key_event_state   (ClutterKeyEvent *keyev);
guint   clutter_key_event_symbol  (ClutterKeyEvent *keyev);
guint16 clutter_key_event_code    (ClutterKeyEvent *keyev);
guint32 clutter_key_event_unicode (ClutterKeyEvent *keyev);

guint32 clutter_button_event_time (ClutterButtonEvent *buttev);
gint    clutter_button_event_x    (ClutterButtonEvent *buttev);
gint    clutter_button_event_y    (ClutterButtonEvent *buttev);

guint32 clutter_keysym_to_unicode (guint keyval);

G_END_DECLS

#endif
