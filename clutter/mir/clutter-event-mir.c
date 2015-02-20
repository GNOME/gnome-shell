/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2014 Canonical Ltd.
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
 * Authors:
 *  Marco Trevisan <marco.trevisan@canonical.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-mir.h"
#include "clutter-private.h"
#include "clutter-event-private.h"
#include "clutter-stage-mir.h"
#include "clutter-stage-private.h"
#include "clutter-backend-mir-priv.h"
#include "clutter-device-manager-private.h"
#include "evdev/clutter-xkb-utils.h"

#include "clutter-event-mir.h"

#define NANO_TO_MILLI(x) ((x) / 1000000)

/* Using the clutter threads lock would cause a dead-lock when resizing */
static GMutex mir_event_lock;

static gboolean
clutter_event_source_mir_check (GSource *source)
{
  gboolean retval;

  g_mutex_lock (&mir_event_lock);

  retval = clutter_events_pending ();

  g_mutex_unlock (&mir_event_lock);

  return retval;
}

static gboolean
clutter_event_source_mir_prepare (GSource *source, gint *timeout)
{
  *timeout = -1;
  return clutter_event_source_mir_check (source);
}

static gboolean
clutter_event_source_mir_dispatch (GSource *source,
                                   GSourceFunc callback,
                                   gpointer data)
{
  ClutterEvent *event;

  g_mutex_lock (&mir_event_lock);
  _clutter_threads_acquire_lock ();

  event = clutter_event_get ();

  if (event)
    {
      /* forward the event into clutter for emission etc. */
      _clutter_stage_queue_event (event->any.stage, event, FALSE);
    }

  _clutter_threads_release_lock ();
  g_mutex_unlock (&mir_event_lock);

  return TRUE;
}

static void
clutter_event_source_mir_finalize (GSource *source)
{
  g_mutex_clear (&mir_event_lock);
}

static GSourceFuncs clutter_event_source_mir_funcs = {
    clutter_event_source_mir_prepare,
    clutter_event_source_mir_check,
    clutter_event_source_mir_dispatch,
    clutter_event_source_mir_finalize
};

GSource *
_clutter_event_source_mir_new (void)
{
  GSource *source;

  source = g_source_new (&clutter_event_source_mir_funcs, sizeof (GSource));

  g_mutex_init (&mir_event_lock);
  g_source_set_priority (source, CLUTTER_PRIORITY_EVENTS);
  g_source_attach (source, NULL);

  return source;
}


static ClutterModifierType
translate_mir_modifier (unsigned int key_modifiers, MirMotionButton button_state)
{
  ClutterModifierType clutter_modifiers = 0;

  if (key_modifiers == mir_key_modifier_none && button_state == 0)
    return clutter_modifiers;

  if (key_modifiers & mir_key_modifier_alt)
    clutter_modifiers |= CLUTTER_MOD1_MASK;

  if (key_modifiers & mir_key_modifier_shift)
    clutter_modifiers |= CLUTTER_SHIFT_MASK;

  if (key_modifiers & mir_key_modifier_ctrl)
    clutter_modifiers |= CLUTTER_CONTROL_MASK;

  if (key_modifiers & mir_key_modifier_meta)
    clutter_modifiers |= CLUTTER_META_MASK;

  if (key_modifiers & mir_key_modifier_caps_lock)
    clutter_modifiers |= CLUTTER_LOCK_MASK;

  if (button_state & mir_motion_button_primary)
    clutter_modifiers |= CLUTTER_BUTTON1_MASK;

  if (button_state & mir_motion_button_secondary)
    clutter_modifiers |= CLUTTER_BUTTON3_MASK;

  if (button_state & mir_motion_button_tertiary)
    clutter_modifiers |= CLUTTER_BUTTON2_MASK;

  return clutter_modifiers;
}

static gunichar
get_unicode_value (int32_t key_code)
{
  gunichar unicode = '\0';
  char text[8];
  int size;

  size = xkb_keysym_to_utf8 (key_code, text, sizeof (text));

  if (size > 0)
    {
     unicode = g_utf8_get_char_validated (text, size);

      if (unicode == -1 || unicode == -2)
        unicode = '\0';
    }

  return unicode;
}

void
_clutter_mir_handle_event (ClutterBackend *backend,
                           MirSurface *surface,
                           MirEvent *mir_event)
{
  ClutterStageManager *stage_manager;
  ClutterInputDevice *device = NULL;
  ClutterStage *stage = NULL;
  ClutterEvent *event = NULL;
  ClutterModifierType modifiers;
  MirMotionButton button_state;
  MirMotionPointer *pointer;
  const GSList *l;

  stage_manager = clutter_stage_manager_get_default ();

  for (l = clutter_stage_manager_peek_stages (stage_manager); l; l = l->next)
    {
      ClutterStage* tmp_stage = l->data;

      if (CLUTTER_IS_STAGE (tmp_stage) &&
          clutter_mir_stage_get_mir_surface (tmp_stage) == surface)
        {
          stage = tmp_stage;
          break;
        }
    }

  if (!stage)
    return;

  g_mutex_lock (&mir_event_lock);

  button_state = CLUTTER_STAGE_MIR (stage)->button_state;

  switch (mir_event->type)
    {
      case mir_event_type_key:
        if (mir_event->key.action == mir_key_action_multiple)
          break;

        device = clutter_device_manager_get_core_device (backend->device_manager,
                                                         CLUTTER_KEYBOARD_DEVICE);

        event = clutter_event_new (mir_event->key.action == mir_key_action_down ?
                                   CLUTTER_KEY_PRESS : CLUTTER_KEY_RELEASE);

        modifiers = translate_mir_modifier (mir_event->key.modifiers, button_state);
        event->key.time = NANO_TO_MILLI (mir_event->key.event_time);
        event->key.modifier_state = modifiers;
        event->key.keyval = mir_event->key.key_code;
        event->key.hardware_keycode = mir_event->key.scan_code + 8;
        event->key.unicode_value = get_unicode_value (mir_event->key.key_code);
        break;

      case mir_event_type_motion:
        pointer = mir_event->motion.pointer_coordinates;
        device = clutter_device_manager_get_core_device (backend->device_manager,
                                                         CLUTTER_POINTER_DEVICE);

        /* We need to send an ENTER event again if the stage is not focused anymore */
        if (mir_event->motion.action != mir_motion_action_hover_enter &&
            mir_event->motion.action != mir_motion_action_hover_exit &&
            !_clutter_input_device_get_stage (device))
          {
            ClutterEvent *new_event = clutter_event_new (CLUTTER_ENTER);
            modifiers = translate_mir_modifier (mir_event->motion.modifiers,
                                                button_state);

            clutter_event_set_time (new_event, NANO_TO_MILLI (mir_event->motion.event_time));
            clutter_event_set_state (new_event, modifiers);
            clutter_event_set_coords (new_event, pointer->x, pointer->y);

            _clutter_input_device_set_stage (device, stage);

            clutter_event_set_stage (new_event, stage);
            clutter_event_set_device (new_event, device);
            clutter_event_set_source_device (new_event, device);

            _clutter_event_push (new_event, FALSE);
          }

        switch (mir_event->motion.action)
          {
            case mir_motion_action_down:
            case mir_motion_action_pointer_down:
            case mir_motion_action_up:
            case mir_motion_action_pointer_up:
              event = clutter_event_new ((mir_event->motion.action ==
                                            mir_motion_action_down ||
                                          mir_event->motion.action ==
                                            mir_motion_action_pointer_down) ?
                                          CLUTTER_BUTTON_PRESS :
                                          CLUTTER_BUTTON_RELEASE);

              event->button.button = 1;
              event->button.click_count = 1;

              button_state ^= mir_event->motion.button_state;

              if (button_state == 0 || (button_state & mir_motion_button_primary))
                event->button.button = 1;
              else if (button_state & mir_motion_button_secondary)
                event->button.button = 3;
              else if (button_state & mir_motion_button_tertiary)
                event->button.button = 2;
              else if (button_state & mir_motion_button_back)
                event->button.button = 8;
              else if (button_state & mir_motion_button_forward)
                event->button.button = 9;

              button_state = mir_event->motion.button_state;
              CLUTTER_STAGE_MIR (stage)->button_state = button_state;

              break;
            case mir_motion_action_scroll:
              event = clutter_event_new (CLUTTER_SCROLL);
              if (ABS (pointer->hscroll) == 1 && pointer->vscroll == 0)
                {
                  clutter_event_set_scroll_direction (event, pointer->hscroll < 0 ?
                                                             CLUTTER_SCROLL_LEFT :
                                                             CLUTTER_SCROLL_RIGHT);
                }
              else if (ABS (pointer->vscroll) == 1 && pointer->hscroll == 0)
                {
                  clutter_event_set_scroll_direction (event, pointer->vscroll < 0 ?
                                                             CLUTTER_SCROLL_DOWN :
                                                             CLUTTER_SCROLL_UP);
                }
              else
                {
                  clutter_event_set_scroll_delta (event, -pointer->hscroll, -pointer->vscroll);
                }
              break;

            case mir_motion_action_move:
            case mir_motion_action_hover_move:
              event = clutter_event_new (CLUTTER_MOTION);
              break;

            case mir_motion_action_hover_enter:
              event = clutter_event_new (CLUTTER_ENTER);
              _clutter_input_device_set_stage (device, stage);
              break;

            case mir_motion_action_hover_exit:
              event = clutter_event_new (CLUTTER_LEAVE);
              _clutter_input_device_set_stage (device, NULL);
              break;
          }

        if (event)
          {
            modifiers = translate_mir_modifier (mir_event->motion.modifiers,
                                                button_state);

            clutter_event_set_time (event, NANO_TO_MILLI (mir_event->motion.event_time));
            clutter_event_set_state (event, modifiers);
            clutter_event_set_coords (event, pointer->x, pointer->y);
          }

        break;

      case mir_event_type_surface:
        switch (mir_event->surface.attrib)
          {
            case mir_surface_attrib_state:
              if (mir_event->surface.value == mir_surface_state_fullscreen)
                {
                  _clutter_stage_update_state (stage,
                                               0,
                                               CLUTTER_STAGE_STATE_FULLSCREEN);
                }
              else
                {
                  _clutter_stage_update_state (stage,
                                               CLUTTER_STAGE_STATE_FULLSCREEN,
                                               0);
                }
              break;

            case mir_surface_attrib_focus:
              if (mir_event->surface.value == mir_surface_focused)
                {
                  _clutter_stage_update_state (stage,
                                               0,
                                               CLUTTER_STAGE_STATE_ACTIVATED);
                }
              else /* if (mir_event->surface.value == mir_surface_unfocused) */
                {
                  _clutter_stage_update_state (stage,
                                               CLUTTER_STAGE_STATE_ACTIVATED,
                                               0);
                }
              break;

            default:
              break;
          }
        break;

      case mir_event_type_close_surface:
          event = clutter_event_new (CLUTTER_DESTROY_NOTIFY);
        break;

      default:
        break;
    }

  if (event)
    {
      clutter_event_set_stage (event, stage);
      clutter_event_set_device (event, device);
      clutter_event_set_source_device (event, device);

      _clutter_event_push (event, FALSE);
    }

    g_mutex_unlock (&mir_event_lock);

    if (event)
      g_main_context_wakeup (NULL);
}
