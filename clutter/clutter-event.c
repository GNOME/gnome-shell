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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-backend-private.h"
#include "clutter-debug.h"
#include "clutter-event-private.h"
#include "clutter-keysyms.h"
#include "clutter-private.h"

#include <math.h>

/**
 * SECTION:clutter-event
 * @short_description: User and window system events
 *
 * Windowing events handled by Clutter.
 *
 * The events usually come from the windowing backend, but can also
 * be synthesized by Clutter itself or by the application code.
 */

typedef struct _ClutterEventPrivate {
  ClutterEvent base;

  ClutterInputDevice *device;
  ClutterInputDevice *source_device;

  gdouble delta_x;
  gdouble delta_y;

  gpointer platform_data;

  ClutterModifierType button_state;
  ClutterModifierType base_state;
  ClutterModifierType latched_state;
  ClutterModifierType locked_state;

  guint is_pointer_emulated : 1;
} ClutterEventPrivate;

static GHashTable *all_events = NULL;

G_DEFINE_BOXED_TYPE (ClutterEvent, clutter_event,
                     clutter_event_copy,
                     clutter_event_free);

static gboolean
is_event_allocated (const ClutterEvent *event)
{
  if (all_events == NULL)
    return FALSE;

  return g_hash_table_lookup (all_events, event) != NULL;
}

/*
 * _clutter_event_get_platform_data:
 * @event: a #ClutterEvent
 *
 * Retrieves the pointer to platform-specific data inside an event
 *
 * Return value: a pointer to platform-specific data
 *
 * Since: 1.4
 */
gpointer
_clutter_event_get_platform_data (const ClutterEvent *event)
{
  if (!is_event_allocated (event))
    return NULL;

  return ((ClutterEventPrivate *) event)->platform_data;
}

/*< private >
 * _clutter_event_set_platform_data:
 * @event: a #ClutterEvent
 * @data: a pointer to platform-specific data
 *
 * Sets the pointer to platform-specific data inside an event
 *
 * Since: 1.4
 */
void
_clutter_event_set_platform_data (ClutterEvent *event,
                                  gpointer      data)
{
  if (!is_event_allocated (event))
    return;

  ((ClutterEventPrivate *) event)->platform_data = data;
}

void
_clutter_event_set_pointer_emulated (ClutterEvent *event,
                                     gboolean      is_emulated)
{
  if (!is_event_allocated (event))
    return;

  ((ClutterEventPrivate *) event)->is_pointer_emulated = !!is_emulated;
}

/**
 * clutter_event_type:
 * @event: a #ClutterEvent
 *
 * Retrieves the type of the event.
 *
 * Return value: a #ClutterEventType
 */
ClutterEventType
clutter_event_type (const ClutterEvent *event)
{
  g_return_val_if_fail (event != NULL, CLUTTER_NOTHING);

  return event->type;
}

/**
 * clutter_event_get_time:
 * @event: a #ClutterEvent
 *
 * Retrieves the time of the event.
 *
 * Return value: the time of the event, or %CLUTTER_CURRENT_TIME
 *
 * Since: 0.4
 */
guint32
clutter_event_get_time (const ClutterEvent *event)
{
  g_return_val_if_fail (event != NULL, CLUTTER_CURRENT_TIME);

  return event->any.time;
}

/**
 * clutter_event_set_time:
 * @event: a #ClutterEvent
 * @time_: the time of the event
 *
 * Sets the time of the event.
 *
 * Since: 1.8
 */
void
clutter_event_set_time (ClutterEvent *event,
                        guint32       time_)
{
  g_return_if_fail (event != NULL);

  event->any.time = time_;
}

/**
 * clutter_event_get_state:
 * @event: a #ClutterEvent
 *
 * Retrieves the modifier state of the event. In case the window system
 * supports reporting latched and locked modifiers, this function returns
 * the effective state.
 *
 * Return value: the modifier state parameter, or 0
 *
 * Since: 0.4
 */
ClutterModifierType
clutter_event_get_state (const ClutterEvent *event)
{
  g_return_val_if_fail (event != NULL, 0);

  switch (event->type)
    {
    case CLUTTER_KEY_PRESS:
    case CLUTTER_KEY_RELEASE:
      return event->key.modifier_state;

    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
      return event->button.modifier_state;

    case CLUTTER_TOUCH_BEGIN:
    case CLUTTER_TOUCH_UPDATE:
    case CLUTTER_TOUCH_END:
    case CLUTTER_TOUCH_CANCEL:
      return event->touch.modifier_state;

    case CLUTTER_MOTION:
      return event->motion.modifier_state;

    case CLUTTER_SCROLL:
      return event->scroll.modifier_state;

    default:
      break;
    }

  return 0;
}

/**
 * clutter_event_set_state:
 * @event: a #ClutterEvent
 * @state: the modifier state to set
 *
 * Sets the modifier state of the event.
 *
 * Since: 1.8
 */
void
clutter_event_set_state (ClutterEvent        *event,
                         ClutterModifierType  state)
{
  g_return_if_fail (event != NULL);

  switch (event->type)
    {
    case CLUTTER_KEY_PRESS:
    case CLUTTER_KEY_RELEASE:
      event->key.modifier_state = state;
      break;

    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
      event->button.modifier_state = state;
      break;

    case CLUTTER_MOTION:
      event->motion.modifier_state = state;
      break;

    case CLUTTER_TOUCH_BEGIN:
    case CLUTTER_TOUCH_UPDATE:
    case CLUTTER_TOUCH_END:
    case CLUTTER_TOUCH_CANCEL:
      event->touch.modifier_state = state;
      break;

    case CLUTTER_SCROLL:
      event->scroll.modifier_state = state;
      break;

    default:
      break;
    }
}

void
_clutter_event_set_state_full (ClutterEvent        *event,
			       ClutterModifierType  button_state,
			       ClutterModifierType  base_state,
			       ClutterModifierType  latched_state,
			       ClutterModifierType  locked_state,
			       ClutterModifierType  effective_state)
{
  ClutterEventPrivate *private = (ClutterEventPrivate*) event;

  private->button_state = button_state;
  private->base_state = base_state;
  private->latched_state = latched_state;
  private->locked_state = locked_state;

  clutter_event_set_state (event, effective_state);
}

/**
 * clutter_event_get_state_full:
 * @event: a #ClutterEvent
 * @button_state: (out) (allow-none): the pressed buttons as a mask
 * @base_state: (out) (allow-none): the regular pressed modifier keys
 * @latched_state: (out) (allow-none): the latched modifier keys (currently released but still valid for one key press/release)
 * @locked_state: (out) (allow-none): the locked modifier keys (valid until the lock key is pressed and released again)
 * @effective_state: (out) (allow-none): the logical OR of all the state bits above
 *
 * Retrieves the decomposition of the keyboard state into button, base,
 * latched, locked and effective. This can be used to transmit to other
 * applications, for example when implementing a wayland compositor.
 *
 * Since: 1.16
 */
void
clutter_event_get_state_full (const ClutterEvent  *event,
			      ClutterModifierType *button_state,
			      ClutterModifierType *base_state,
			      ClutterModifierType *latched_state,
			      ClutterModifierType *locked_state,
			      ClutterModifierType *effective_state)
{
  const ClutterEventPrivate *private = (const ClutterEventPrivate*) event;

  g_return_if_fail (event != NULL);

  if (button_state)
    *button_state = private->button_state;
  if (base_state)
    *base_state = private->base_state;
  if (latched_state)
    *latched_state = private->latched_state;
  if (locked_state)
    *locked_state = private->locked_state;
  if (effective_state)
    *effective_state = clutter_event_get_state (event);
}

/**
 * clutter_event_get_coords:
 * @event: a #ClutterEvent
 * @x: (out): return location for the X coordinate, or %NULL
 * @y: (out): return location for the Y coordinate, or %NULL
 *
 * Retrieves the coordinates of @event and puts them into @x and @y.
 *
 * Since: 0.4
 */
void
clutter_event_get_coords (const ClutterEvent *event,
                          gfloat             *x,
                          gfloat             *y)
{
  ClutterPoint coords;

  g_return_if_fail (event != NULL);

  clutter_event_get_position (event, &coords);

  if (x != NULL)
    *x = coords.x;

  if (y != NULL)
    *y = coords.y;
}

/**
 * clutter_event_get_position:
 * @event: a #ClutterEvent
 * @position: a #ClutterPoint
 *
 * Retrieves the event coordinates as a #ClutterPoint.
 *
 * Since: 1.12
 */
void
clutter_event_get_position (const ClutterEvent *event,
                            ClutterPoint       *position)
{
  g_return_if_fail (event != NULL);
  g_return_if_fail (position != NULL);

  switch (event->type)
    {
    case CLUTTER_NOTHING:
    case CLUTTER_KEY_PRESS:
    case CLUTTER_KEY_RELEASE:
    case CLUTTER_STAGE_STATE:
    case CLUTTER_DESTROY_NOTIFY:
    case CLUTTER_CLIENT_MESSAGE:
    case CLUTTER_DELETE:
    case CLUTTER_EVENT_LAST:
      clutter_point_init (position, 0.f, 0.f);
      break;

    case CLUTTER_ENTER:
    case CLUTTER_LEAVE:
      clutter_point_init (position, event->crossing.x, event->crossing.y);
      break;

    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
      clutter_point_init (position, event->button.x, event->button.y);
      break;

    case CLUTTER_MOTION:
      clutter_point_init (position, event->motion.x, event->motion.y);
      break;

    case CLUTTER_TOUCH_BEGIN:
    case CLUTTER_TOUCH_UPDATE:
    case CLUTTER_TOUCH_END:
    case CLUTTER_TOUCH_CANCEL:
      clutter_point_init (position, event->touch.x, event->touch.y);
      break;

    case CLUTTER_SCROLL:
      clutter_point_init (position, event->scroll.x, event->scroll.y);
      break;
    }

}

/**
 * clutter_event_set_coords:
 * @event: a #ClutterEvent
 * @x: the X coordinate of the event
 * @y: the Y coordinate of the event
 *
 * Sets the coordinates of the @event.
 *
 * Since: 1.8
 */
void
clutter_event_set_coords (ClutterEvent *event,
                          gfloat        x,
                          gfloat        y)
{
  g_return_if_fail (event != NULL);

  switch (event->type)
    {
    case CLUTTER_NOTHING:
    case CLUTTER_KEY_PRESS:
    case CLUTTER_KEY_RELEASE:
    case CLUTTER_STAGE_STATE:
    case CLUTTER_DESTROY_NOTIFY:
    case CLUTTER_CLIENT_MESSAGE:
    case CLUTTER_DELETE:
    case CLUTTER_EVENT_LAST:
      break;

    case CLUTTER_ENTER:
    case CLUTTER_LEAVE:
      event->crossing.x = x;
      event->crossing.y = y;
      break;

    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
      event->button.x = x;
      event->button.y = y;
      break;

    case CLUTTER_MOTION:
      event->motion.x = x;
      event->motion.y = y;
      break;

    case CLUTTER_TOUCH_BEGIN:
    case CLUTTER_TOUCH_UPDATE:
    case CLUTTER_TOUCH_END:
    case CLUTTER_TOUCH_CANCEL:
      event->touch.x = x;
      event->touch.y = y;
      break;

    case CLUTTER_SCROLL:
      event->scroll.x = x;
      event->scroll.y = y;
      break;
    }
}

/**
 * clutter_event_get_source:
 * @event: a #ClutterEvent
 *
 * Retrieves the source #ClutterActor the event originated from, or
 * NULL if the event has no source.
 *
 * Return value: (transfer none): a #ClutterActor
 *
 * Since: 0.6
 */
ClutterActor *
clutter_event_get_source (const ClutterEvent *event)
{
  g_return_val_if_fail (event != NULL, NULL);

  return event->any.source;
}

/**
 * clutter_event_set_source:
 * @event: a #ClutterEvent
 * @actor: (allow-none): a #ClutterActor, or %NULL
 *
 * Sets the source #ClutterActor of @event.
 *
 * Since: 1.8
 */
void
clutter_event_set_source (ClutterEvent *event,
                          ClutterActor *actor)
{
  g_return_if_fail (event != NULL);
  g_return_if_fail (actor == NULL || CLUTTER_IS_ACTOR (actor));

  event->any.source = actor;
}

/**
 * clutter_event_get_stage:
 * @event: a #ClutterEvent
 *
 * Retrieves the source #ClutterStage the event originated for, or
 * %NULL if the event has no stage.
 *
 * Return value: (transfer none): a #ClutterStage
 *
 * Since: 0.8
 */
ClutterStage *
clutter_event_get_stage (const ClutterEvent *event)
{
  g_return_val_if_fail (event != NULL, NULL);

  return event->any.stage;
}

/**
 * clutter_event_set_stage:
 * @event: a #ClutterEvent
 * @stage: (allow-none): a #ClutterStage, or %NULL
 *
 * Sets the source #ClutterStage of the event.
 *
 * Since: 1.8
 */
void
clutter_event_set_stage (ClutterEvent *event,
                         ClutterStage *stage)
{
  g_return_if_fail (event != NULL);
  g_return_if_fail (stage == NULL || CLUTTER_IS_STAGE (stage));

  if (event->any.stage == stage)
    return;

  event->any.stage = stage;
}

/**
 * clutter_event_get_flags:
 * @event: a #ClutterEvent
 *
 * Retrieves the #ClutterEventFlags of @event
 *
 * Return value: the event flags
 *
 * Since: 1.0
 */
ClutterEventFlags
clutter_event_get_flags (const ClutterEvent *event)
{
  g_return_val_if_fail (event != NULL, CLUTTER_EVENT_NONE);

  return event->any.flags;
}

/**
 * clutter_event_set_flags:
 * @event: a #ClutterEvent
 * @flags: a binary OR of #ClutterEventFlags values
 *
 * Sets the #ClutterEventFlags of @event
 *
 * Since: 1.8
 */
void
clutter_event_set_flags (ClutterEvent      *event,
                         ClutterEventFlags  flags)
{
  g_return_if_fail (event != NULL);

  if (event->any.flags == flags)
    return;

  event->any.flags = flags;
  event->any.flags |= CLUTTER_EVENT_FLAG_SYNTHETIC;
}

/**
 * clutter_event_get_related:
 * @event: a #ClutterEvent of type %CLUTTER_ENTER or of
 *   type %CLUTTER_LEAVE
 *
 * Retrieves the related actor of a crossing event.
 *
 * Return value: (transfer none): the related #ClutterActor, or %NULL
 *
 * Since: 1.0
 */
ClutterActor *
clutter_event_get_related (const ClutterEvent *event)
{
  g_return_val_if_fail (event != NULL, NULL);
  g_return_val_if_fail (event->type == CLUTTER_ENTER ||
                        event->type == CLUTTER_LEAVE, NULL);

  return event->crossing.related;
}

/**
 * clutter_event_set_related:
 * @event: a #ClutterEvent of type %CLUTTER_ENTER or %CLUTTER_LEAVE
 * @actor: (allow-none): a #ClutterActor or %NULL
 *
 * Sets the related actor of a crossing event
 *
 * Since: 1.8
 */
void
clutter_event_set_related (ClutterEvent *event,
                           ClutterActor *actor)
{
  g_return_if_fail (event != NULL);
  g_return_if_fail (event->type == CLUTTER_ENTER ||
                    event->type == CLUTTER_LEAVE);
  g_return_if_fail (actor == NULL || CLUTTER_IS_ACTOR (actor));

  if (event->crossing.related == actor)
    return;

  event->crossing.related = actor;
}

/**
 * clutter_event_set_scroll_delta:
 * @event: a #ClutterEvent of type %CLUTTER_SCROLL
 * @dx: delta on the horizontal axis
 * @dy: delta on the vertical axis
 *
 * Sets the precise scrolling information of @event.
 *
 * Since: 1.10
 */
void
clutter_event_set_scroll_delta (ClutterEvent *event,
                                gdouble       dx,
                                gdouble       dy)
{
  g_return_if_fail (event != NULL);
  g_return_if_fail (event->type == CLUTTER_SCROLL);

  if (!is_event_allocated (event))
    return;

  event->scroll.direction = CLUTTER_SCROLL_SMOOTH;

  ((ClutterEventPrivate *) event)->delta_x = dx;
  ((ClutterEventPrivate *) event)->delta_y = dy;
}

/**
 * clutter_event_get_scroll_delta:
 * @event: a #ClutterEvent of type %CLUTTER_SCROLL
 * @dx: (out): return location for the delta on the horizontal axis
 * @dy: (out): return location for the delta on the vertical axis
 *
 * Retrieves the precise scrolling information of @event.
 *
 * The @event has to have a #ClutterScrollEvent.direction value
 * of %CLUTTER_SCROLL_SMOOTH.
 *
 * Since: 1.10
 */
void
clutter_event_get_scroll_delta (const ClutterEvent *event,
                                gdouble            *dx,
                                gdouble            *dy)
{
  gdouble delta_x, delta_y;

  g_return_if_fail (event != NULL);
  g_return_if_fail (event->type == CLUTTER_SCROLL);
  g_return_if_fail (event->scroll.direction == CLUTTER_SCROLL_SMOOTH);

  delta_x = delta_y = 0;

  if (is_event_allocated (event))
    {
      delta_x = ((ClutterEventPrivate *) event)->delta_x;
      delta_y = ((ClutterEventPrivate *) event)->delta_y;
    }

  if (dx != NULL)
    *dx = delta_x;

  if (dy != NULL)
    *dy = delta_y;
}

/**
 * clutter_event_get_scroll_direction:
 * @event: a #ClutterEvent of type %CLUTTER_SCROLL
 *
 * Retrieves the direction of the scrolling of @event
 *
 * Return value: the scrolling direction
 *
 * Since: 1.0
 */
ClutterScrollDirection
clutter_event_get_scroll_direction (const ClutterEvent *event)
{
  g_return_val_if_fail (event != NULL, CLUTTER_SCROLL_UP);
  g_return_val_if_fail (event->type == CLUTTER_SCROLL, CLUTTER_SCROLL_UP);

  return event->scroll.direction;
}

/**
 * clutter_event_set_scroll_direction:
 * @event: a #ClutterEvent
 * @direction: the scrolling direction
 *
 * Sets the direction of the scrolling of @event
 *
 * Since: 1.8
 */
void
clutter_event_set_scroll_direction (ClutterEvent           *event,
                                    ClutterScrollDirection  direction)
{
  g_return_if_fail (event != NULL);
  g_return_if_fail (event->type == CLUTTER_SCROLL);

  event->scroll.direction = direction;
}

/**
 * clutter_event_get_button:
 * @event: a #ClutterEvent of type %CLUTTER_BUTTON_PRESS or
 *   of type %CLUTTER_BUTTON_RELEASE
 *
 * Retrieves the button number of @event
 *
 * Return value: the button number
 *
 * Since: 1.0
 */
guint32
clutter_event_get_button (const ClutterEvent *event)
{
  g_return_val_if_fail (event != NULL, 0);
  g_return_val_if_fail (event->type == CLUTTER_BUTTON_PRESS ||
                        event->type == CLUTTER_BUTTON_RELEASE, 0);

  return event->button.button;
}

/**
 * clutter_event_set_button:
 * @event: a #ClutterEvent or type %CLUTTER_BUTTON_PRESS or
 *   of type %CLUTTER_BUTTON_RELEASE
 * @button: the button number
 *
 * Sets the button number of @event
 *
 * Since: 1.8
 */
void
clutter_event_set_button (ClutterEvent *event,
                          guint32       button)
{
  g_return_if_fail (event != NULL);
  g_return_if_fail (event->type == CLUTTER_BUTTON_PRESS ||
                    event->type == CLUTTER_BUTTON_RELEASE);

  event->button.button = button;
}

/**
 * clutter_event_get_click_count:
 * @event: a #ClutterEvent of type %CLUTTER_BUTTON_PRESS or
 *   of type %CLUTTER_BUTTON_RELEASE
 *
 * Retrieves the number of clicks of @event
 *
 * Return value: the click count
 *
 * Since: 1.0
 */
guint32
clutter_event_get_click_count (const ClutterEvent *event)
{
  g_return_val_if_fail (event != NULL, 0);
  g_return_val_if_fail (event->type == CLUTTER_BUTTON_PRESS ||
                        event->type == CLUTTER_BUTTON_RELEASE, 0);

  return event->button.click_count;
}

/* keys */

/**
 * clutter_event_get_key_symbol:
 * @event: a #ClutterEvent of type %CLUTTER_KEY_PRESS or
 *   of type %CLUTTER_KEY_RELEASE
 *
 * Retrieves the key symbol of @event
 *
 * Return value: the key symbol representing the key
 *
 * Since: 1.0
 */
guint
clutter_event_get_key_symbol (const ClutterEvent *event)
{
  g_return_val_if_fail (event != NULL, 0);
  g_return_val_if_fail (event->type == CLUTTER_KEY_PRESS ||
                        event->type == CLUTTER_KEY_RELEASE, 0);

  return event->key.keyval;
}

/**
 * clutter_event_set_key_symbol:
 * @event: a #ClutterEvent of type %CLUTTER_KEY_PRESS
 *   or %CLUTTER_KEY_RELEASE
 * @key_sym: the key symbol representing the key
 *
 * Sets the key symbol of @event.
 *
 * Since: 1.8
 */
void
clutter_event_set_key_symbol (ClutterEvent *event,
                              guint         key_sym)
{
  g_return_if_fail (event != NULL);
  g_return_if_fail (event->type == CLUTTER_KEY_PRESS ||
                    event->type == CLUTTER_KEY_RELEASE);

  event->key.keyval = key_sym;
}

/**
 * clutter_event_get_key_code:
 * @event: a #ClutterEvent of type %CLUTTER_KEY_PRESS or
 *    of type %CLUTTER_KEY_RELEASE
 *
 * Retrieves the keycode of the key that caused @event
 *
 * Return value: The keycode representing the key
 *
 * Since: 1.0
 */
guint16
clutter_event_get_key_code (const ClutterEvent *event)
{
  g_return_val_if_fail (event != NULL, 0);
  g_return_val_if_fail (event->type == CLUTTER_KEY_PRESS ||
                        event->type == CLUTTER_KEY_RELEASE, 0);

  return event->key.hardware_keycode;
}

/**
 * clutter_event_set_key_code:
 * @event: a #ClutterEvent of type %CLUTTER_KEY_PRESS
 *   or %CLUTTER_KEY_RELEASE
 * @key_code: the keycode representing the key
 *
 * Sets the keycode of the @event.
 *
 * Since: 1.8
 */
void
clutter_event_set_key_code (ClutterEvent *event,
                            guint16       key_code)
{
  g_return_if_fail (event != NULL);
  g_return_if_fail (event->type == CLUTTER_KEY_PRESS ||
                    event->type == CLUTTER_KEY_RELEASE);

  event->key.hardware_keycode = key_code;
}

/**
 * clutter_event_get_key_unicode:
 * @event: a #ClutterEvent of type %CLUTTER_KEY_PRESS
 *   or %CLUTTER_KEY_RELEASE
 *
 * Retrieves the unicode value for the key that caused @keyev.
 *
 * Return value: The unicode value representing the key
 */
gunichar
clutter_event_get_key_unicode (const ClutterEvent *event)
{
  g_return_val_if_fail (event != NULL, 0);
  g_return_val_if_fail (event->type == CLUTTER_KEY_PRESS ||
                        event->type == CLUTTER_KEY_RELEASE, 0);

  if (event->key.unicode_value)
    return event->key.unicode_value;
  else
    return clutter_keysym_to_unicode (event->key.keyval);
}

/**
 * clutter_event_set_key_unicode:
 * @event: a #ClutterEvent of type %CLUTTER_KEY_PRESS
 *   or %CLUTTER_KEY_RELEASE
 * @key_unicode: the Unicode value representing the key
 *
 * Sets the Unicode value of @event.
 *
 * Since: 1.8
 */
void
clutter_event_set_key_unicode (ClutterEvent *event,
                               gunichar      key_unicode)
{
  g_return_if_fail (event != NULL);
  g_return_if_fail (event->type == CLUTTER_KEY_PRESS ||
                    event->type == CLUTTER_KEY_RELEASE);

  event->key.unicode_value = key_unicode;
}

/**
 * clutter_event_get_event_sequence:
 * @event: a #ClutterEvent of type %CLUTTER_TOUCH_BEGIN,
 *   %CLUTTER_TOUCH_UPDATE, %CLUTTER_TOUCH_END, or
 *   %CLUTTER_TOUCH_CANCEL
 *
 * Retrieves the #ClutterEventSequence of @event.
 *
 * Return value: (transfer none): the event sequence, or %NULL
 *
 * Since: 1.10
 */
ClutterEventSequence *
clutter_event_get_event_sequence (const ClutterEvent *event)
{
  g_return_val_if_fail (event != NULL, NULL);

  if (event->type == CLUTTER_TOUCH_BEGIN ||
      event->type == CLUTTER_TOUCH_UPDATE ||
      event->type == CLUTTER_TOUCH_END ||
      event->type == CLUTTER_TOUCH_CANCEL)
    return event->touch.sequence;

  return NULL;
}

/**
 * clutter_event_get_device_id:
 * @event: a clutter event 
 *
 * Retrieves the events device id if set.
 *
 * Return value: A unique identifier for the device or -1 if the event has
 *   no specific device set.
 */
gint
clutter_event_get_device_id (const ClutterEvent *event)
{
  ClutterInputDevice *device = NULL;

  g_return_val_if_fail (event != NULL, CLUTTER_POINTER_DEVICE);

  device = clutter_event_get_device (event);
  if (device != NULL)
    return clutter_input_device_get_device_id (device);

  return -1;
}

/**
 * clutter_event_get_device_type:
 * @event: a #ClutterEvent
 *
 * Retrieves the type of the device for @event
 *
 * Return value: the #ClutterInputDeviceType for the device, if
 *   any is set
 *
 * Since: 1.0
 */
ClutterInputDeviceType
clutter_event_get_device_type (const ClutterEvent *event)
{
  ClutterInputDevice *device = NULL;

  g_return_val_if_fail (event != NULL, CLUTTER_POINTER_DEVICE);

  device = clutter_event_get_device (event);
  if (device != NULL)
    return clutter_input_device_get_device_type (device);

  return CLUTTER_POINTER_DEVICE;
}

/**
 * clutter_event_set_device:
 * @event: a #ClutterEvent
 * @device: (allow-none): a #ClutterInputDevice, or %NULL
 *
 * Sets the device for @event.
 *
 * Since: 1.6
 */
void
clutter_event_set_device (ClutterEvent       *event,
                          ClutterInputDevice *device)
{
  g_return_if_fail (event != NULL);
  g_return_if_fail (device == NULL || CLUTTER_IS_INPUT_DEVICE (device));

  if (is_event_allocated (event))
    {
      ClutterEventPrivate *real_event = (ClutterEventPrivate *) event;

      real_event->device = device;
    }

  switch (event->type)
    {
    case CLUTTER_NOTHING:
    case CLUTTER_STAGE_STATE:
    case CLUTTER_DESTROY_NOTIFY:
    case CLUTTER_CLIENT_MESSAGE:
    case CLUTTER_DELETE:
    case CLUTTER_EVENT_LAST:
      break;

    case CLUTTER_ENTER:
    case CLUTTER_LEAVE:
      event->crossing.device = device;
      break;

    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
      event->button.device = device;
      break;

    case CLUTTER_MOTION:
      event->motion.device = device;
      break;

    case CLUTTER_SCROLL:
      event->scroll.device = device;
      break;

    case CLUTTER_TOUCH_BEGIN:
    case CLUTTER_TOUCH_UPDATE:
    case CLUTTER_TOUCH_END:
    case CLUTTER_TOUCH_CANCEL:
      event->touch.device = device;
      break;

    case CLUTTER_KEY_PRESS:
    case CLUTTER_KEY_RELEASE:
      event->key.device = device;
      break;
    }
}

/**
 * clutter_event_get_device:
 * @event: a #ClutterEvent
 *
 * Retrieves the #ClutterInputDevice for the event.
 * If you want the physical device the event originated from, use
 * clutter_event_get_source_device().
 *
 * The #ClutterInputDevice structure is completely opaque and should
 * be cast to the platform-specific implementation.
 *
 * Return value: (transfer none): the #ClutterInputDevice or %NULL. The
 *   returned device is owned by the #ClutterEvent and it should not
 *   be unreferenced
 *
 * Since: 1.0
 */
ClutterInputDevice *
clutter_event_get_device (const ClutterEvent *event)
{
  ClutterInputDevice *device = NULL;

  g_return_val_if_fail (event != NULL, NULL);

  if (is_event_allocated (event))
    {
      ClutterEventPrivate *real_event = (ClutterEventPrivate *) event;

      if (real_event->device != NULL)
        return real_event->device;
    }

  switch (event->type)
    {
    case CLUTTER_NOTHING:
    case CLUTTER_STAGE_STATE:
    case CLUTTER_DESTROY_NOTIFY:
    case CLUTTER_CLIENT_MESSAGE:
    case CLUTTER_DELETE:
    case CLUTTER_EVENT_LAST:
      break;

    case CLUTTER_ENTER:
    case CLUTTER_LEAVE:
      device = event->crossing.device;
      break;

    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
      device = event->button.device;
      break;

    case CLUTTER_MOTION:
      device = event->motion.device;
      break;

    case CLUTTER_SCROLL:
      device = event->scroll.device;
      break;

    case CLUTTER_TOUCH_BEGIN:
    case CLUTTER_TOUCH_UPDATE:
    case CLUTTER_TOUCH_END:
    case CLUTTER_TOUCH_CANCEL:
      device = event->touch.device;
      break;

    case CLUTTER_KEY_PRESS:
    case CLUTTER_KEY_RELEASE:
      device = event->key.device;
      break;
    }

  return device;
}

/**
 * clutter_event_new:
 * @type: The type of event.
 *
 * Creates a new #ClutterEvent of the specified type.
 *
 * Return value: (transfer full): A newly allocated #ClutterEvent.
 */
ClutterEvent *
clutter_event_new (ClutterEventType type)
{
  ClutterEvent *new_event;
  ClutterEventPrivate *priv;

  priv = g_slice_new0 (ClutterEventPrivate);

  new_event = (ClutterEvent *) priv;
  new_event->type = new_event->any.type = type;

  if (G_UNLIKELY (all_events == NULL))
    all_events = g_hash_table_new (NULL, NULL);

  g_hash_table_replace (all_events, priv, GUINT_TO_POINTER (1));

  return new_event;
}

/**
 * clutter_event_copy:
 * @event: A #ClutterEvent.
 *
 * Copies @event.
 *
 * Return value: (transfer full): A newly allocated #ClutterEvent
 */
ClutterEvent *
clutter_event_copy (const ClutterEvent *event)
{
  ClutterEvent *new_event;
  ClutterEventPrivate *new_real_event;
  ClutterInputDevice *device;
  gint n_axes = 0;

  g_return_val_if_fail (event != NULL, NULL);

  new_event = clutter_event_new (CLUTTER_NOTHING);
  new_real_event = (ClutterEventPrivate *) new_event;

  *new_event = *event;

  if (is_event_allocated (event))
    {
      ClutterEventPrivate *real_event = (ClutterEventPrivate *) event;

      new_real_event->device = real_event->device;
      new_real_event->source_device = real_event->source_device;
      new_real_event->delta_x = real_event->delta_x;
      new_real_event->delta_y = real_event->delta_y;
      new_real_event->is_pointer_emulated = real_event->is_pointer_emulated;
      new_real_event->base_state = real_event->base_state;
      new_real_event->button_state = real_event->button_state;
      new_real_event->latched_state = real_event->latched_state;
      new_real_event->locked_state = real_event->locked_state;
    }

  device = clutter_event_get_device (event);
  if (device != NULL)
    n_axes = clutter_input_device_get_n_axes (device);

  switch (event->type)
    {
    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
      if (event->button.axes != NULL)
        new_event->button.axes = g_memdup (event->button.axes,
                                           sizeof (gdouble) * n_axes);
      break;

    case CLUTTER_SCROLL:
      if (event->scroll.axes != NULL)
        new_event->scroll.axes = g_memdup (event->scroll.axes,
                                           sizeof (gdouble) * n_axes);
      break;

    case CLUTTER_MOTION:
      if (event->motion.axes != NULL)
        new_event->motion.axes = g_memdup (event->motion.axes,
                                           sizeof (gdouble) * n_axes);
      break;

    case CLUTTER_TOUCH_BEGIN:
    case CLUTTER_TOUCH_UPDATE:
    case CLUTTER_TOUCH_END:
    case CLUTTER_TOUCH_CANCEL:
      if (event->touch.axes != NULL)
        new_event->touch.axes = g_memdup (event->touch.axes,
                                          sizeof (gdouble) * n_axes);
      break;

    default:
      break;
    }

  if (is_event_allocated (event))
    _clutter_backend_copy_event_data (clutter_get_default_backend (),
                                      event,
                                      new_event);

  return new_event;
}

/**
 * clutter_event_free:
 * @event: A #ClutterEvent.
 *
 * Frees all resources used by @event.
 */
void
clutter_event_free (ClutterEvent *event)
{
  if (G_LIKELY (event != NULL))
    {
      _clutter_backend_free_event_data (clutter_get_default_backend (), event);

      switch (event->type)
        {
        case CLUTTER_BUTTON_PRESS:
        case CLUTTER_BUTTON_RELEASE:
          g_free (event->button.axes);
          break;

        case CLUTTER_MOTION:
          g_free (event->motion.axes);
          break;

        case CLUTTER_SCROLL:
          g_free (event->scroll.axes);
          break;

        case CLUTTER_TOUCH_BEGIN:
        case CLUTTER_TOUCH_UPDATE:
        case CLUTTER_TOUCH_END:
        case CLUTTER_TOUCH_CANCEL:
          g_free (event->touch.axes);
          break;

        default:
          break;
        }

      g_hash_table_remove (all_events, event);
      g_slice_free (ClutterEventPrivate, (ClutterEventPrivate *) event);
    }
}

/**
 * clutter_event_get:
 *
 * Pops an event off the event queue. Applications should not need to call 
 * this.
 *
 * Return value: A #ClutterEvent or NULL if queue empty
 *
 * Since: 0.4
 */
ClutterEvent *
clutter_event_get (void)
{
  ClutterMainContext *context = _clutter_context_get_default ();

  if (context->events_queue == NULL)
    return NULL;

  if (g_queue_is_empty (context->events_queue))
    return NULL;

  return g_queue_pop_tail (context->events_queue);
}

/**
 * clutter_event_peek:
 * 
 * Returns a pointer to the first event from the event queue but 
 * does not remove it. 
 *
 * Return value: (transfer none): A #ClutterEvent or NULL if queue empty.
 *
 * Since: 0.4
 */
ClutterEvent *
clutter_event_peek (void)
{
  ClutterMainContext *context = _clutter_context_get_default ();

  g_return_val_if_fail (context != NULL, NULL);
  
  if (context->events_queue == NULL)
    return NULL;

  if (g_queue_is_empty (context->events_queue))
    return NULL;

  return g_queue_peek_tail (context->events_queue);
}

void
_clutter_event_push (const ClutterEvent *event,
                     gboolean            do_copy)
{
  ClutterMainContext *context = _clutter_context_get_default ();
  ClutterInputDevice *device;

  g_assert (context != NULL);

  if (context->events_queue == NULL)
    context->events_queue = g_queue_new ();

  /* disabled devices don't propagate events */
  device = clutter_event_get_device (event);
  if (device != NULL)
    {
      if (!clutter_input_device_get_enabled (device))
        return;
    }

  if (do_copy)
    {
      ClutterEvent *copy;

      copy = clutter_event_copy (event);
      event = copy;
    }

  g_queue_push_head (context->events_queue, (gpointer) event);
}

/**
 * clutter_event_put:
 * @event: a #ClutterEvent
 *
 * Puts a copy of the event on the back of the event queue. The event will
 * have the %CLUTTER_EVENT_FLAG_SYNTHETIC flag set. If the source is set
 * event signals will be emitted for this source and capture/bubbling for
 * its ancestors. If the source is not set it will be generated by picking
 * or use the actor that currently has keyboard focus
 *
 * Since: 0.6
 */
void
clutter_event_put (const ClutterEvent *event)
{
  _clutter_event_push (event, TRUE);
}

/**
 * clutter_events_pending:
 *
 * Checks if events are pending in the event queue.
 *
 * Return value: TRUE if there are pending events, FALSE otherwise.
 *
 * Since: 0.4
 */
gboolean
clutter_events_pending (void)
{
  ClutterMainContext *context = _clutter_context_get_default ();

  g_return_val_if_fail (context != NULL, FALSE);

  if (context->events_queue == NULL)
    return FALSE;

  return g_queue_is_empty (context->events_queue) == FALSE;
}

/**
 * clutter_get_current_event_time:
 *
 * Retrieves the timestamp of the last event, if there is an
 * event or if the event has a timestamp.
 *
 * Return value: the event timestamp, or %CLUTTER_CURRENT_TIME
 *
 * Since: 1.0
 */
guint32
clutter_get_current_event_time (void)
{
  const ClutterEvent* event;

  event = clutter_get_current_event ();

  if (event != NULL)
    return clutter_event_get_time (event);

  return CLUTTER_CURRENT_TIME;
}

/**
 * clutter_get_current_event:
 *
 * If an event is currently being processed, return that event.
 * This function is intended to be used to access event state
 * that might not be exposed by higher-level widgets.  For
 * example, to get the key modifier state from a Button 'clicked'
 * event.
 *
 * Return value: (transfer none): The current ClutterEvent, or %NULL if none
 *
 * Since: 1.2
 */
const ClutterEvent *
clutter_get_current_event (void)
{
  ClutterMainContext *context = _clutter_context_get_default ();

  g_return_val_if_fail (context != NULL, NULL);

  return context->current_event != NULL ? context->current_event->data : NULL;
}

/**
 * clutter_event_get_source_device:
 * @event: a #ClutterEvent
 *
 * Retrieves the hardware device that originated the event.
 *
 * If you need the virtual device, use clutter_event_get_device().
 *
 * If no hardware device originated this event, this function will
 * return the same device as clutter_event_get_device().
 *
 * Return value: (transfer none): a pointer to a #ClutterInputDevice
 *   or %NULL
 *
 * Since: 1.6
 */
ClutterInputDevice *
clutter_event_get_source_device (const ClutterEvent *event)
{
  ClutterEventPrivate *real_event;

  if (!is_event_allocated (event))
    return NULL;

  real_event = (ClutterEventPrivate *) event;

  if (real_event->source_device != NULL)
    return real_event->source_device;

  return clutter_event_get_device (event);
}

/**
 * clutter_event_set_source_device:
 * @event: a #ClutterEvent
 * @device: (allow-none): a #ClutterInputDevice
 *
 * Sets the source #ClutterInputDevice for @event.
 *
 * The #ClutterEvent must have been created using clutter_event_new().
 *
 * Since: 1.8
 */
void
clutter_event_set_source_device (ClutterEvent       *event,
                                 ClutterInputDevice *device)
{
  ClutterEventPrivate *real_event;

  g_return_if_fail (event != NULL);
  g_return_if_fail (device == NULL || CLUTTER_IS_INPUT_DEVICE (device));

  if (!is_event_allocated (event))
    return;

  real_event = (ClutterEventPrivate *) event;
  real_event->source_device = device;
}

/**
 * clutter_event_get_axes:
 * @event: a #ClutterEvent
 * @n_axes: (out): return location for the number of axes returned
 *
 * Retrieves the array of axes values attached to the event.
 *
 * Return value: (transfer none): an array of axis values
 *
 * Since: 1.6
 */
gdouble *
clutter_event_get_axes (const ClutterEvent *event,
                        guint              *n_axes)
{
  gdouble *retval = NULL;
  guint len = 0;

  switch (event->type)
    {
    case CLUTTER_NOTHING:
    case CLUTTER_STAGE_STATE:
    case CLUTTER_DESTROY_NOTIFY:
    case CLUTTER_CLIENT_MESSAGE:
    case CLUTTER_DELETE:
    case CLUTTER_ENTER:
    case CLUTTER_LEAVE:
    case CLUTTER_KEY_PRESS:
    case CLUTTER_KEY_RELEASE:
    case CLUTTER_EVENT_LAST:
      break;

    case CLUTTER_SCROLL:
      retval = event->scroll.axes;
      break;

    case CLUTTER_BUTTON_PRESS:
    case CLUTTER_BUTTON_RELEASE:
      retval = event->button.axes;
      break;

    case CLUTTER_TOUCH_BEGIN:
    case CLUTTER_TOUCH_UPDATE:
    case CLUTTER_TOUCH_END:
    case CLUTTER_TOUCH_CANCEL:
      retval = event->touch.axes;
      break;

    case CLUTTER_MOTION:
      retval = event->motion.axes;
      break;
    }

  if (retval != NULL)
    {
      ClutterInputDevice *device;

      device = clutter_event_get_device (event);
      if (device != NULL)
        len = clutter_input_device_get_n_axes (device);
      else
        retval = NULL;
    }

  if (n_axes)
    *n_axes = len;

  return retval;
}

/**
 * clutter_event_get_distance:
 * @source: a #ClutterEvent
 * @target: a #ClutterEvent
 *
 * Retrieves the distance between two events, a @source and a @target.
 *
 * Return value: the distance between two #ClutterEvent
 *
 * Since: 1.12
 */
float
clutter_event_get_distance (const ClutterEvent *source,
                            const ClutterEvent *target)
{
  ClutterPoint p0, p1;

  clutter_event_get_position (source, &p0);
  clutter_event_get_position (source, &p1);

  return clutter_point_distance (&p0, &p1, NULL, NULL);
}

/**
 * clutter_event_get_angle:
 * @source: a #ClutterEvent
 * @target: a #ClutterEvent
 *
 * Retrieves the angle relative from @source to @target.
 *
 * The direction of the angle is from the position X axis towards
 * the positive Y axis.
 *
 * Return value: the angle between two #ClutterEvent
 *
 * Since: 1.12
 */
double
clutter_event_get_angle (const ClutterEvent *source,
                         const ClutterEvent *target)
{
  ClutterPoint p0, p1;
  float x_distance, y_distance;
  double angle;

  clutter_event_get_position (source, &p0);
  clutter_event_get_position (target, &p1);

  if (clutter_point_equals (&p0, &p1))
    return 0;

  clutter_point_distance (&p0, &p1, &x_distance, &y_distance);

  angle = atan2 (x_distance, y_distance);

  /* invert the angle, and shift it by 90 degrees */
  angle = (2.0 * G_PI) - angle;
  angle += G_PI / 2.0;

  /* keep the angle within the [ 0, 360 ] interval */
  angle = fmod (angle, 2.0 * G_PI);

  return angle;
}

/**
 * clutter_event_has_shift_modifier:
 * @event: a #ClutterEvent
 *
 * Checks whether @event has the Shift modifier mask set.
 *
 * Return value: %TRUE if the event has the Shift modifier mask set
 *
 * Since: 1.12
 */
gboolean
clutter_event_has_shift_modifier (const ClutterEvent *event)
{
  return (clutter_event_get_state (event) & CLUTTER_SHIFT_MASK) != FALSE;
}

/**
 * clutter_event_has_control_modifier:
 * @event: a #ClutterEvent
 *
 * Checks whether @event has the Control modifier mask set.
 *
 * Return value: %TRUE if the event has the Control modifier mask set
 *
 * Since: 1.12
 */
gboolean
clutter_event_has_control_modifier (const ClutterEvent *event)
{
  return (clutter_event_get_state (event) & CLUTTER_CONTROL_MASK) != FALSE;
}

/**
 * clutter_event_is_pointer_emulated:
 * @event: a #ClutterEvent
 *
 * Checks whether a pointer @event has been generated by the windowing
 * system. The returned value can be used to distinguish between events
 * synthesized by the windowing system itself (as opposed by Clutter).
 *
 * Return value: %TRUE if the event is pointer emulated
 *
 * Since: 1.12
 */
gboolean
clutter_event_is_pointer_emulated (const ClutterEvent *event)
{
  g_return_val_if_fail (event != NULL, FALSE);

  if (!is_event_allocated (event))
    return FALSE;

  return ((ClutterEventPrivate *) event)->is_pointer_emulated;
}
