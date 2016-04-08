/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright © 2009, 2010, 2011  Intel Corp.
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
 * Author: Emmanuele Bassi <ebassi@linux.intel.com>
 */

#include "config.h"

#include "clutter-device-manager-core-x11.h"

#include "clutter-backend-x11.h"
#include "clutter-input-device-core-x11.h"
#include "clutter-stage-x11.h"

#include "clutter-backend.h"
#include "clutter-debug.h"
#include "clutter-device-manager-private.h"
#include "clutter-event-private.h"
#include "clutter-event-translator.h"
#include "clutter-stage-private.h"
#include "clutter-private.h"

enum
{
  PROP_0,

  PROP_EVENT_BASE,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST] = { NULL, };

static void clutter_event_translator_iface_init (ClutterEventTranslatorIface *iface);

#define clutter_device_manager_x11_get_type     _clutter_device_manager_x11_get_type

G_DEFINE_TYPE_WITH_CODE (ClutterDeviceManagerX11,
                         clutter_device_manager_x11,
                         CLUTTER_TYPE_DEVICE_MANAGER,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_EVENT_TRANSLATOR,
                                                clutter_event_translator_iface_init));

static inline void
translate_key_event (ClutterBackendX11       *backend_x11,
                     ClutterDeviceManagerX11 *manager_x11,
                     ClutterEvent            *event,
                     XEvent                  *xevent)
{
  ClutterEventX11 *event_x11;
  char buffer[256 + 1];
  int n;

  event->key.type = xevent->xany.type == KeyPress ? CLUTTER_KEY_PRESS
                                                  : CLUTTER_KEY_RELEASE;
  event->key.time = xevent->xkey.time;

  clutter_event_set_device (event, manager_x11->core_keyboard);

  /* KeyEvents have platform specific data associated to them */
  event_x11 = _clutter_event_x11_new ();
  _clutter_event_set_platform_data (event, event_x11);

  event->key.modifier_state = (ClutterModifierType) xevent->xkey.state;
  event->key.hardware_keycode = xevent->xkey.keycode;

  /* keyval is the key ignoring all modifiers ('1' vs. '!') */
  event->key.keyval =
    _clutter_keymap_x11_translate_key_state (backend_x11->keymap,
                                             event->key.hardware_keycode,
                                             &event->key.modifier_state,
                                             NULL);

  event_x11->key_group =
    _clutter_keymap_x11_get_key_group (backend_x11->keymap,
                                       event->key.modifier_state);
  event_x11->key_is_modifier =
    _clutter_keymap_x11_get_is_modifier (backend_x11->keymap,
                                         event->key.hardware_keycode);
  event_x11->num_lock_set =
    _clutter_keymap_x11_get_num_lock_state (backend_x11->keymap);
  event_x11->caps_lock_set =
    _clutter_keymap_x11_get_caps_lock_state (backend_x11->keymap);

  /* unicode_value is the printable representation */
  n = XLookupString (&xevent->xkey, buffer, sizeof (buffer) - 1, NULL, NULL);

  if (n != NoSymbol)
    {
      event->key.unicode_value = g_utf8_get_char_validated (buffer, n);
      if ((event->key.unicode_value != (gunichar) -1) &&
          (event->key.unicode_value != (gunichar) -2))
        goto out;
    }
  else
    event->key.unicode_value = (gunichar)'\0';

out:
  CLUTTER_NOTE (EVENT,
                "%s: win:0x%x, key: %12s (%d)",
                event->any.type == CLUTTER_KEY_PRESS
                  ? "key press  "
                  : "key release",
                (unsigned int) xevent->xkey.window,
                event->key.keyval ? buffer : "(none)",
                event->key.keyval);
  return;
}

static ClutterTranslateReturn
clutter_device_manager_x11_translate_event (ClutterEventTranslator *translator,
                                            gpointer                native,
                                            ClutterEvent           *event)
{
  ClutterDeviceManagerX11 *manager_x11;
  ClutterBackendX11 *backend_x11;
  ClutterStageX11 *stage_x11;
  ClutterTranslateReturn res;
  ClutterStage *stage;
  XEvent *xevent;
  int window_scale;

  manager_x11 = CLUTTER_DEVICE_MANAGER_X11 (translator);
  backend_x11 = CLUTTER_BACKEND_X11 (clutter_get_default_backend ());

  xevent = native;

  stage = clutter_x11_get_stage_from_window (xevent->xany.window);
  if (stage == NULL)
    return CLUTTER_TRANSLATE_CONTINUE;

  if (CLUTTER_ACTOR_IN_DESTRUCTION (stage))
    return CLUTTER_TRANSLATE_CONTINUE;

  stage_x11 = CLUTTER_STAGE_X11 (_clutter_stage_get_window (stage));

  window_scale = stage_x11->scale_factor;

  event->any.stage = stage;

  res = CLUTTER_TRANSLATE_CONTINUE;

  switch (xevent->type)
    {
    case KeyPress:
      translate_key_event (backend_x11, manager_x11, event, xevent);
      _clutter_stage_x11_set_user_time (stage_x11, xevent->xkey.time);
      res = CLUTTER_TRANSLATE_QUEUE;
      break;

    case KeyRelease:
      /* old-style X11 terminals require that even modern X11 send
       * KeyPress/KeyRelease pairs when auto-repeating. for this
       * reason modern(-ish) API like XKB has a way to detect
       * auto-repeat and do a single KeyRelease at the end of a
       * KeyPress sequence.
       *
       * this check emulates XKB's detectable auto-repeat; we peek
       * the next event and check if it's a KeyPress for the same key
       * and timestamp - and then ignore it if it matches the
       * KeyRelease
       *
       * if we have XKB, and autorepeat is enabled, then this becomes
       * a no-op
       */
      if (!backend_x11->have_xkb_autorepeat && XPending (xevent->xkey.display))
        {
          XEvent next_event;

          XPeekEvent (xevent->xkey.display, &next_event);

          if (next_event.type == KeyPress &&
              next_event.xkey.keycode == xevent->xkey.keycode &&
              next_event.xkey.time == xevent->xkey.time)
            {
              res = CLUTTER_TRANSLATE_REMOVE;
              break;
            }
        }

      translate_key_event (backend_x11, manager_x11, event, xevent);
      res = CLUTTER_TRANSLATE_QUEUE;
      break;

    case ButtonPress:
      CLUTTER_NOTE (EVENT,
                    "button press: win: 0x%x, coords: %d, %d, button: %d",
                    (unsigned int) stage_x11->xwin,
                    xevent->xbutton.x,
                    xevent->xbutton.y,
                    xevent->xbutton.button);

      switch (xevent->xbutton.button)
        {
        case 4: /* up */
        case 5: /* down */
        case 6: /* left */
        case 7: /* right */
          event->scroll.type = CLUTTER_SCROLL;

          if (xevent->xbutton.button == 4)
            event->scroll.direction = CLUTTER_SCROLL_UP;
          else if (xevent->xbutton.button == 5)
            event->scroll.direction = CLUTTER_SCROLL_DOWN;
          else if (xevent->xbutton.button == 6)
            event->scroll.direction = CLUTTER_SCROLL_LEFT;
          else
            event->scroll.direction = CLUTTER_SCROLL_RIGHT;

          event->scroll.time = xevent->xbutton.time;
          event->scroll.x = xevent->xbutton.x / window_scale;
          event->scroll.y = xevent->xbutton.y / window_scale;
          event->scroll.modifier_state = xevent->xbutton.state;
          event->scroll.axes = NULL;
          break;

        default:
          event->button.type = event->type = CLUTTER_BUTTON_PRESS;
          event->button.time = xevent->xbutton.time;
          event->button.x = xevent->xbutton.x / window_scale;
          event->button.y = xevent->xbutton.y / window_scale;
          event->button.modifier_state = xevent->xbutton.state;
          event->button.button = xevent->xbutton.button;
          event->button.axes = NULL;
          break;
        }

      clutter_event_set_device (event, manager_x11->core_pointer);

      _clutter_stage_x11_set_user_time (stage_x11, xevent->xbutton.time);
      res = CLUTTER_TRANSLATE_QUEUE;
      break;

    case ButtonRelease:
      CLUTTER_NOTE (EVENT,
                    "button press: win: 0x%x, coords: %d, %d, button: %d",
                    (unsigned int) stage_x11->xwin,
                    xevent->xbutton.x,
                    xevent->xbutton.y,
                    xevent->xbutton.button);

      /* scroll events don't have a corresponding release */
      if (xevent->xbutton.button == 4 ||
          xevent->xbutton.button == 5 ||
          xevent->xbutton.button == 6 ||
          xevent->xbutton.button == 7)
        {
          res = CLUTTER_TRANSLATE_REMOVE;
          break;
        }

      event->button.type = event->type = CLUTTER_BUTTON_RELEASE;
      event->button.time = xevent->xbutton.time;
      event->button.x = xevent->xbutton.x / window_scale;
      event->button.y = xevent->xbutton.y / window_scale;
      event->button.modifier_state = xevent->xbutton.state;
      event->button.button = xevent->xbutton.button;
      event->button.axes = NULL;
      clutter_event_set_device (event, manager_x11->core_pointer);
      res = CLUTTER_TRANSLATE_QUEUE;
      break;

    case MotionNotify:
      CLUTTER_NOTE (EVENT,
                    "motion: win: 0x%x, coords: %d, %d",
                    (unsigned int) stage_x11->xwin,
                    xevent->xmotion.x,
                    xevent->xmotion.y);

      event->motion.type = event->type = CLUTTER_MOTION;
      event->motion.time = xevent->xmotion.time;
      event->motion.x = xevent->xmotion.x / window_scale;
      event->motion.y = xevent->xmotion.y / window_scale;
      event->motion.modifier_state = xevent->xmotion.state;
      event->motion.axes = NULL;
      clutter_event_set_device (event, manager_x11->core_pointer);
      res = CLUTTER_TRANSLATE_QUEUE;
      break;

    case EnterNotify:
      CLUTTER_NOTE (EVENT, "Entering the stage (time:%u)",
                    (unsigned int) xevent->xcrossing.time);

      event->crossing.type = CLUTTER_ENTER;
      event->crossing.time = xevent->xcrossing.time;
      event->crossing.x = xevent->xcrossing.x / window_scale;
      event->crossing.y = xevent->xcrossing.y / window_scale;
      event->crossing.source = CLUTTER_ACTOR (stage);
      event->crossing.related = NULL;
      clutter_event_set_device (event, manager_x11->core_pointer);

      _clutter_input_device_set_stage (manager_x11->core_pointer, stage);

      res = CLUTTER_TRANSLATE_QUEUE;
      break;

    case LeaveNotify:
      if (manager_x11->core_pointer->stage == NULL)
        {
          CLUTTER_NOTE (EVENT, "Discarding LeaveNotify for "
                               "ButtonRelease event off-stage");
          res = CLUTTER_TRANSLATE_REMOVE;
          break;
        }

      /* we know that we are leaving the stage here */
      CLUTTER_NOTE (EVENT, "Leaving the stage (time:%u)",
                    (unsigned int) xevent->xcrossing.time);

      event->crossing.type = CLUTTER_LEAVE;
      event->crossing.time = xevent->xcrossing.time;
      event->crossing.x = xevent->xcrossing.x / window_scale;
      event->crossing.y = xevent->xcrossing.y / window_scale;
      event->crossing.source = CLUTTER_ACTOR (stage);
      event->crossing.related = NULL;
      clutter_event_set_device (event, manager_x11->core_pointer);

      _clutter_input_device_set_stage (manager_x11->core_pointer, NULL);

      res = CLUTTER_TRANSLATE_QUEUE;
      break;

    default:
      break;
    }

  return res;
}

static void
clutter_event_translator_iface_init (ClutterEventTranslatorIface *iface)
{
  iface->translate_event = clutter_device_manager_x11_translate_event;
}

static void
clutter_device_manager_x11_constructed (GObject *gobject)
{
  ClutterDeviceManagerX11 *manager_x11;
  ClutterBackendX11 *backend_x11;

  manager_x11 = CLUTTER_DEVICE_MANAGER_X11 (gobject);

  g_object_get (gobject, "backend", &backend_x11, NULL);
  g_assert (backend_x11 != NULL);

  manager_x11->core_pointer =
    g_object_new (CLUTTER_TYPE_INPUT_DEVICE_X11,
                  "name", "Core Pointer",
                  "has-cursor", TRUE,
                  "device-type", CLUTTER_POINTER_DEVICE,
                  "device-manager", manager_x11,
                  "device-mode", CLUTTER_INPUT_MODE_MASTER,
                  "backend", backend_x11,
                  "enabled", TRUE,
                  NULL);
  CLUTTER_NOTE (BACKEND, "Added core pointer device");

  manager_x11->core_keyboard =
    g_object_new (CLUTTER_TYPE_INPUT_DEVICE_X11,
                  "name", "Core Keyboard",
                  "has-cursor", FALSE,
                  "device-type", CLUTTER_KEYBOARD_DEVICE,
                  "device-manager", manager_x11,
                  "device-mode", CLUTTER_INPUT_MODE_MASTER,
                  "backend", backend_x11,
                  "enabled", TRUE,
                  NULL);
  CLUTTER_NOTE (BACKEND, "Added core keyboard device");

  /* associate core devices */
  _clutter_input_device_set_associated_device (manager_x11->core_pointer,
                                               manager_x11->core_keyboard);
  _clutter_input_device_set_associated_device (manager_x11->core_keyboard,
                                               manager_x11->core_pointer);

  if (G_OBJECT_CLASS (clutter_device_manager_x11_parent_class)->constructed)
    G_OBJECT_CLASS (clutter_device_manager_x11_parent_class)->constructed (gobject);
}

static void
clutter_device_manager_x11_add_device (ClutterDeviceManager *manager,
                                       ClutterInputDevice   *device)
{
  ClutterDeviceManagerX11 *manager_x11 = CLUTTER_DEVICE_MANAGER_X11 (manager);

  manager_x11->devices = g_slist_prepend (manager_x11->devices, device);
  g_hash_table_replace (manager_x11->devices_by_id,
                        GINT_TO_POINTER (device->id),
                        device);

  /* blow the cache */
  g_slist_free (manager_x11->all_devices);
  manager_x11->all_devices = NULL;
}

static void
clutter_device_manager_x11_remove_device (ClutterDeviceManager *manager,
                                          ClutterInputDevice   *device)
{
  ClutterDeviceManagerX11 *manager_x11 = CLUTTER_DEVICE_MANAGER_X11 (manager);

  g_hash_table_remove (manager_x11->devices_by_id,
                       GINT_TO_POINTER (device->id));
  manager_x11->devices = g_slist_remove (manager_x11->devices, device);

  /* blow the cache */
  g_slist_free (manager_x11->all_devices);
  manager_x11->all_devices = NULL;
}

static const GSList *
clutter_device_manager_x11_get_devices (ClutterDeviceManager *manager)
{
  ClutterDeviceManagerX11 *manager_x11 = CLUTTER_DEVICE_MANAGER_X11 (manager);

  /* cache the devices list so that we can keep the core pointer
   * and keyboard outside of the ManagerX11:devices list
   */
  if (manager_x11->all_devices == NULL)
    {
      GSList *all_devices = manager_x11->devices;

      all_devices = g_slist_prepend (all_devices, manager_x11->core_keyboard);
      all_devices = g_slist_prepend (all_devices, manager_x11->core_pointer);

      manager_x11->all_devices = all_devices;
    }
    
  return CLUTTER_DEVICE_MANAGER_X11 (manager)->all_devices;
}

static ClutterInputDevice *
clutter_device_manager_x11_get_core_device (ClutterDeviceManager   *manager,
                                            ClutterInputDeviceType  type)
{
  ClutterDeviceManagerX11 *manager_x11;

  manager_x11 = CLUTTER_DEVICE_MANAGER_X11 (manager);

  switch (type)
    {
    case CLUTTER_POINTER_DEVICE:
      return manager_x11->core_pointer;

    case CLUTTER_KEYBOARD_DEVICE:
      return manager_x11->core_keyboard;

    default:
      return NULL;
    }

  return NULL;
}

static ClutterInputDevice *
clutter_device_manager_x11_get_device (ClutterDeviceManager *manager,
                                       gint                  id)
{
  ClutterDeviceManagerX11 *manager_x11 = CLUTTER_DEVICE_MANAGER_X11 (manager);

  return g_hash_table_lookup (manager_x11->devices_by_id,
                              GINT_TO_POINTER (id));
}

static void
clutter_device_manager_x11_set_property (GObject      *gobject,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ClutterDeviceManagerX11 *manager_x11 = CLUTTER_DEVICE_MANAGER_X11 (gobject);

  switch (prop_id)
    {
    case PROP_EVENT_BASE:
      manager_x11->xi_event_base = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_device_manager_x11_class_init (ClutterDeviceManagerX11Class *klass)
{
  ClutterDeviceManagerClass *manager_class;
  GObjectClass *gobject_class;

  obj_props[PROP_EVENT_BASE] =
    g_param_spec_int ("event-base",
                      "Event Base",
                      "The first XI event",
                      -1, G_MAXINT,
                      -1,
                      CLUTTER_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->constructed = clutter_device_manager_x11_constructed;
  gobject_class->set_property = clutter_device_manager_x11_set_property;

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
  
  manager_class = CLUTTER_DEVICE_MANAGER_CLASS (klass);
  manager_class->add_device = clutter_device_manager_x11_add_device;
  manager_class->remove_device = clutter_device_manager_x11_remove_device;
  manager_class->get_devices = clutter_device_manager_x11_get_devices;
  manager_class->get_core_device = clutter_device_manager_x11_get_core_device;
  manager_class->get_device = clutter_device_manager_x11_get_device;
}

static void
clutter_device_manager_x11_init (ClutterDeviceManagerX11 *self)
{
  self->devices_by_id = g_hash_table_new (NULL, NULL);
}
