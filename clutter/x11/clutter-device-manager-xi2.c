/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright © 2011  Intel Corp.
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

#include <stdint.h>

#include "clutter-device-manager-xi2.h"

#include "clutter-backend-x11.h"
#include "clutter-input-device-xi2.h"
#include "clutter-stage-x11.h"

#include "clutter-backend.h"
#include "clutter-debug.h"
#include "clutter-device-manager-private.h"
#include "clutter-event-private.h"
#include "clutter-event-translator.h"
#include "clutter-stage-private.h"
#include "clutter-private.h"

#include <X11/extensions/XInput2.h>

enum
{
  PROP_0,

  PROP_OPCODE,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST] = { NULL, };

static const char *clutter_input_axis_atom_names[] = {
  "Abs X",              /* CLUTTER_INPUT_AXIS_X */
  "Abs Y",              /* CLUTTER_INPUT_AXIS_Y */
  "Abs Pressure",       /* CLUTTER_INPUT_AXIS_PRESSURE */
  "Abs Tilt X",         /* CLUTTER_INPUT_AXIS_XTILT */
  "Abs Tilt Y",         /* CLUTTER_INPUT_AXIS_YTILT */
  "Abs Wheel",          /* CLUTTER_INPUT_AXIS_WHEEL */
  "Abs Distance",       /* CLUTTER_INPUT_AXIS_DISTANCE */
};

#define N_AXIS_ATOMS    G_N_ELEMENTS (clutter_input_axis_atom_names)

static Atom clutter_input_axis_atoms[N_AXIS_ATOMS] = { 0, };

static void clutter_event_translator_iface_init (ClutterEventTranslatorIface *iface);

#define clutter_device_manager_xi2_get_type     _clutter_device_manager_xi2_get_type

G_DEFINE_TYPE_WITH_CODE (ClutterDeviceManagerXI2,
                         clutter_device_manager_xi2,
                         CLUTTER_TYPE_DEVICE_MANAGER,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_EVENT_TRANSLATOR,
                                                clutter_event_translator_iface_init));

static void
translate_valuator_class (Display             *xdisplay,
                          ClutterInputDevice  *device,
                          XIValuatorClassInfo *class)
{
  static gboolean atoms_initialized = FALSE;
  ClutterInputAxis i, axis = CLUTTER_INPUT_AXIS_IGNORE;

  if (G_UNLIKELY (!atoms_initialized))
    {
      XInternAtoms (xdisplay,
                    (char **) clutter_input_axis_atom_names, N_AXIS_ATOMS,
                    False,
                    clutter_input_axis_atoms);

      atoms_initialized = TRUE;
    }

  for (i = 0;
       i < N_AXIS_ATOMS;
       i += 1)
    {
      if (clutter_input_axis_atoms[i] == class->label)
        {
          axis = i + 1;
          break;
        }
    }

  _clutter_input_device_add_axis (device, axis,
                                  class->min,
                                  class->max,
                                  class->resolution);

  CLUTTER_NOTE (BACKEND,
                "Added axis '%s' (min:%.2f, max:%.2fd, res:%d) of device %d",
                clutter_input_axis_atom_names[axis],
                class->min,
                class->max,
                class->resolution,
                device->id);
}

static void
translate_device_classes (Display             *xdisplay,
                          ClutterInputDevice  *device,
                          XIAnyClassInfo     **classes,
                          guint                n_classes)
{
  gint i;

  for (i = 0; i < n_classes; i++)
    {
      XIAnyClassInfo *class_info = classes[i];

      switch (class_info->type)
        {
        case XIKeyClass:
          {
            XIKeyClassInfo *key_info = (XIKeyClassInfo *) class_info;
            gint j;

            _clutter_input_device_set_n_keys (device,
                                              key_info->num_keycodes);

            for (j = 0; j < key_info->num_keycodes; j++)
              {
                clutter_input_device_set_key (device, j,
                                              key_info->keycodes[i],
                                              0);
              }
          }
          break;

        case XIValuatorClass:
          translate_valuator_class (xdisplay, device,
                                    (XIValuatorClassInfo *) class_info);
          break;

#ifdef HAVE_XINPUT_2_2
        case XIScrollClass:
          {
            XIScrollClassInfo *scroll_info = (XIScrollClassInfo *) class_info;
            ClutterScrollDirection direction;

            if (scroll_info->scroll_type == XIScrollTypeVertical)
              direction = CLUTTER_SCROLL_DOWN;
            else
              direction = CLUTTER_SCROLL_RIGHT;

            CLUTTER_NOTE (BACKEND, "Scroll valuator %d: %s, increment: %f",
                          scroll_info->number,
                          scroll_info->scroll_type == XIScrollTypeVertical
                            ? "vertical"
                            : "horizontal",
                          scroll_info->increment);

            _clutter_input_device_add_scroll_info (device,
                                                   scroll_info->number,
                                                   direction,
                                                   scroll_info->increment);
          }
          break;
#endif /* HAVE_XINPUT_2_2 */

        default:
          break;
        }
    }
}

static gboolean
is_touch_device (XIAnyClassInfo         **classes,
                 guint                    n_classes,
                 ClutterInputDeviceType  *device_type,
                 guint                   *n_touch_points)
{
#ifdef HAVE_XINPUT_2_2
  guint i;

  for (i = 0; i < n_classes; i++)
    {
      XITouchClassInfo *class = (XITouchClassInfo *) classes[i];

      if (class->type != XITouchClass)
        continue;

      if (class->num_touches > 0)
        {
          if (class->mode == XIDirectTouch)
            *device_type = CLUTTER_TOUCHSCREEN_DEVICE;
          else if (class->mode == XIDependentTouch)
            *device_type = CLUTTER_TOUCHPAD_DEVICE;
          else
            continue;

          *n_touch_points = class->num_touches;

          return TRUE;
        }
    }
#endif

  return FALSE;
}

static ClutterInputDevice *
create_device (ClutterDeviceManagerXI2 *manager_xi2,
               ClutterBackendX11       *backend_x11,
               XIDeviceInfo            *info)
{
  ClutterInputDeviceType source, touch_source;
  ClutterInputDevice *retval;
  ClutterInputMode mode;
  gboolean is_enabled;
  guint num_touches = 0;

  if (info->use == XIMasterKeyboard || info->use == XISlaveKeyboard)
    source = CLUTTER_KEYBOARD_DEVICE;
  else if (info->use == XISlavePointer &&
           is_touch_device (info->classes, info->num_classes,
                            &touch_source,
                            &num_touches))
    {
      source = touch_source;
    }
  else
    {
      gchar *name;

      name = g_ascii_strdown (info->name, -1);

      if (strstr (name, "eraser") != NULL)
        source = CLUTTER_ERASER_DEVICE;
      else if (strstr (name, "cursor") != NULL)
        source = CLUTTER_CURSOR_DEVICE;
      else if (strstr (name, "wacom") != NULL || strstr (name, "pen") != NULL)
        source = CLUTTER_PEN_DEVICE;
      else
        source = CLUTTER_POINTER_DEVICE;

      g_free (name);
    }

  switch (info->use)
    {
    case XIMasterKeyboard:
    case XIMasterPointer:
      mode = CLUTTER_INPUT_MODE_MASTER;
      is_enabled = TRUE;
      break;

    case XISlaveKeyboard:
    case XISlavePointer:
      mode = CLUTTER_INPUT_MODE_SLAVE;
      is_enabled = FALSE;
      break;

    case XIFloatingSlave:
    default:
      mode = CLUTTER_INPUT_MODE_FLOATING;
      is_enabled = FALSE;
      break;
    }

  retval = g_object_new (CLUTTER_TYPE_INPUT_DEVICE_XI2,
                         "name", info->name,
                         "id", info->deviceid,
                         "has-cursor", (info->use == XIMasterPointer),
                         "device-manager", manager_xi2,
                         "device-type", source,
                         "device-mode", mode,
                         "backend", backend_x11,
                         "enabled", is_enabled,
                         NULL);

  translate_device_classes (backend_x11->xdpy, retval,
                            info->classes,
                            info->num_classes);

  CLUTTER_NOTE (BACKEND, "Created device '%s' (id: %d, has-cursor: %s)",
                info->name,
                info->deviceid,
                info->use == XIMasterPointer ? "yes" : "no");

  return retval;
}

static ClutterInputDevice *
add_device (ClutterDeviceManagerXI2 *manager_xi2,
            ClutterBackendX11       *backend_x11,
            XIDeviceInfo            *info,
            gboolean                 in_construction)
{
  ClutterInputDevice *device;

  device = create_device (manager_xi2, backend_x11, info);

  /* we don't go through the DeviceManager::add_device() vfunc because
   * that emits the signal, and we only do it conditionally
   */
  g_hash_table_replace (manager_xi2->devices_by_id,
                        GINT_TO_POINTER (info->deviceid),
                        g_object_ref (device));

  if (info->use == XIMasterPointer ||
      info->use == XIMasterKeyboard)
    {
      manager_xi2->master_devices =
        g_list_prepend (manager_xi2->master_devices, device);
    }
  else if (info->use == XISlavePointer ||
           info->use == XISlaveKeyboard ||
           info->use == XIFloatingSlave)
    {
      manager_xi2->slave_devices =
        g_list_prepend (manager_xi2->slave_devices, device);
    }
  else
    g_warning ("Unhandled device: %s",
               clutter_input_device_get_device_name (device));

  /* relationships between devices and signal emissions are not
   * necessary while we're constructing the device manager instance
   */
  if (!in_construction)
    {
      if (info->use == XISlavePointer || info->use == XISlaveKeyboard)
        {
          ClutterInputDevice *master;

          master = g_hash_table_lookup (manager_xi2->devices_by_id,
                                        GINT_TO_POINTER (info->attachment));
          _clutter_input_device_set_associated_device (device, master);
          _clutter_input_device_add_slave (master, device);
        }

      /* blow the cache */
      g_slist_free (manager_xi2->all_devices);
      manager_xi2->all_devices = NULL;

      g_signal_emit_by_name (manager_xi2, "device-added", device);
    }

  return device;
}

static void
remove_device (ClutterDeviceManagerXI2 *manager_xi2,
               gint                     device_id)
{
  ClutterInputDevice *device;

  device = g_hash_table_lookup (manager_xi2->devices_by_id,
                                GINT_TO_POINTER (device_id));

  if (device != NULL)
    {
      manager_xi2->master_devices =
        g_list_remove (manager_xi2->master_devices, device);
      manager_xi2->slave_devices =
        g_list_remove (manager_xi2->slave_devices, device);

      /* blow the cache */
      g_slist_free (manager_xi2->all_devices);
      manager_xi2->all_devices = NULL;

      g_signal_emit_by_name (manager_xi2, "device-removed", device);

      g_object_run_dispose (G_OBJECT (device));

      g_hash_table_remove (manager_xi2->devices_by_id,
                           GINT_TO_POINTER (device_id));
    }
}

static void
translate_hierarchy_event (ClutterBackendX11       *backend_x11,
                           ClutterDeviceManagerXI2 *manager_xi2,
                           XIHierarchyEvent        *ev)
{
  int i;

  for (i = 0; i < ev->num_info; i++)
    {
      if (ev->info[i].flags & XIDeviceEnabled)
        {
          XIDeviceInfo *info;
          int n_devices;

          CLUTTER_NOTE (EVENT, "Hierarchy event: device enabled");

          clutter_x11_trap_x_errors ();
          info = XIQueryDevice (backend_x11->xdpy,
                                ev->info[i].deviceid,
                                &n_devices);
          clutter_x11_untrap_x_errors ();
          if (info != NULL)
            {
              add_device (manager_xi2, backend_x11, &info[0], FALSE);
              XIFreeDeviceInfo (info);
            }
        }
      else if (ev->info[i].flags & XIDeviceDisabled)
        {
          CLUTTER_NOTE (EVENT, "Hierarchy event: device disabled");

          remove_device (manager_xi2, ev->info[i].deviceid);
        }
      else if ((ev->info[i].flags & XISlaveAttached) ||
               (ev->info[i].flags & XISlaveDetached))
        {
          ClutterInputDevice *master, *slave;
          XIDeviceInfo *info;
          int n_devices;
          gboolean send_changed = FALSE;

          CLUTTER_NOTE (EVENT, "Hierarchy event: slave %s",
                        (ev->info[i].flags & XISlaveAttached)
                          ? "attached"
                          : "detached");

          slave = g_hash_table_lookup (manager_xi2->devices_by_id,
                                       GINT_TO_POINTER (ev->info[i].deviceid));
          master = clutter_input_device_get_associated_device (slave);

          /* detach the slave in both cases */
          if (master != NULL)
            {
              _clutter_input_device_remove_slave (master, slave);
              _clutter_input_device_set_associated_device (slave, NULL);

              send_changed = TRUE;
            }

          /* and attach the slave to the new master if needed */
          if (ev->info[i].flags & XISlaveAttached)
            {
              clutter_x11_trap_x_errors ();
              info = XIQueryDevice (backend_x11->xdpy,
                                    ev->info[i].deviceid,
                                    &n_devices);
              clutter_x11_untrap_x_errors ();
              if (info != NULL)
                {
                  master = g_hash_table_lookup (manager_xi2->devices_by_id,
                                                GINT_TO_POINTER (info->attachment));
                  if (master != NULL)
                    {
                      _clutter_input_device_set_associated_device (slave, master);
                      _clutter_input_device_add_slave (master, slave);

                      send_changed = TRUE;
                    }
                  XIFreeDeviceInfo (info);
                }
            }

          if (send_changed)
            {
              ClutterStage *stage = _clutter_input_device_get_stage (master);
              if (stage != NULL)
                _clutter_stage_x11_events_device_changed (CLUTTER_STAGE_X11 (_clutter_stage_get_window (stage)), 
                                                          master,
                                                          CLUTTER_DEVICE_MANAGER (manager_xi2));
            }
        }
    }
}

static void
clutter_device_manager_xi2_select_events (ClutterDeviceManager *manager,
                                          Window                xwindow,
                                          XIEventMask          *event_mask)
{
  Display *xdisplay;

  xdisplay = clutter_x11_get_default_display ();

  XISelectEvents (xdisplay, xwindow, event_mask, 1);
}

static ClutterStage *
get_event_stage (ClutterEventTranslator *translator,
                 XIEvent                *xi_event)
{
  Window xwindow = None;

  switch (xi_event->evtype)
    {
    case XI_KeyPress:
    case XI_KeyRelease:
    case XI_ButtonPress:
    case XI_ButtonRelease:
    case XI_Motion:
#ifdef HAVE_XINPUT_2_2
    case XI_TouchBegin:
    case XI_TouchUpdate:
    case XI_TouchEnd:
#endif /* HAVE_XINPUT_2_2 */
      {
        XIDeviceEvent *xev = (XIDeviceEvent *) xi_event;

        xwindow = xev->event;
      }
      break;

    case XI_Enter:
    case XI_Leave:
    case XI_FocusIn:
    case XI_FocusOut:
      {
        XIEnterEvent *xev = (XIEnterEvent *) xi_event;

        xwindow = xev->event;
      }
      break;

    default:
      break;
    }

  if (xwindow == None)
    return NULL;

  return clutter_x11_get_stage_from_window (xwindow);
}

/*
 * print_key_sym: Translate a symbol to its printable form if any
 * @symbol: the symbol to translate
 * @buffer: the buffer where to put the translated string
 * @len: size of the buffer
 *
 * Translates @symbol into a printable representation in @buffer, if possible.
 *
 * Return value: The number of bytes of the translated string, 0 if the
 *               symbol can't be printed
 *
 * Note: The code is derived from libX11's src/KeyBind.c
 *       Copyright 1985, 1987, 1998  The Open Group
 *
 * Note: This code works for Latin-1 symbols. clutter_keysym_to_unicode()
 *       does the work for the other keysyms.
 */
static int
print_keysym (uint32_t symbol,
              char    *buffer,
              int      len)
{
  unsigned long high_bytes;
  unsigned char c;

  high_bytes = symbol >> 8;
  if (!(len &&
        ((high_bytes == 0) ||
         ((high_bytes == 0xFF) &&
          (((symbol >= CLUTTER_KEY_BackSpace) &&
            (symbol <= CLUTTER_KEY_Clear)) ||
           (symbol == CLUTTER_KEY_Return) ||
           (symbol == CLUTTER_KEY_Escape) ||
           (symbol == CLUTTER_KEY_KP_Space) ||
           (symbol == CLUTTER_KEY_KP_Tab) ||
           (symbol == CLUTTER_KEY_KP_Enter) ||
           ((symbol >= CLUTTER_KEY_KP_Multiply) &&
            (symbol <= CLUTTER_KEY_KP_9)) ||
           (symbol == CLUTTER_KEY_KP_Equal) ||
           (symbol == CLUTTER_KEY_Delete))))))
    return 0;

  /* if X keysym, convert to ascii by grabbing low 7 bits */
  if (symbol == CLUTTER_KEY_KP_Space)
    c = CLUTTER_KEY_space & 0x7F; /* patch encoding botch */
  else if (high_bytes == 0xFF)
    c = symbol & 0x7F;
  else
    c = symbol & 0xFF;

  buffer[0] = c;
  return 1;
}

static gdouble *
translate_axes (ClutterInputDevice *device,
                gdouble             x,
                gdouble             y,
                ClutterStageX11    *stage_x11,
                XIValuatorState    *valuators)
{
  guint n_axes = clutter_input_device_get_n_axes (device);
  guint i;
  gdouble *retval;
  double *values;

  retval = g_new0 (gdouble, n_axes);
  values = valuators->values;

  for (i = 0; i < valuators->mask_len * 8; i++)
    {
      ClutterInputAxis axis;
      gdouble val;

      if (!XIMaskIsSet (valuators->mask, i))
        continue;

      axis = clutter_input_device_get_axis (device, i);
      val = *values++;

      switch (axis)
        {
        case CLUTTER_INPUT_AXIS_X:
          retval[i] = x;
          break;

        case CLUTTER_INPUT_AXIS_Y:
          retval[i] = y;
          break;

        default:
          _clutter_input_device_translate_axis (device, i, val, &retval[i]);
          break;
        }
    }

  return retval;
}

static gdouble
scroll_valuators_changed (ClutterInputDevice *device,
                          XIValuatorState    *valuators,
                          gdouble            *dx_p,
                          gdouble            *dy_p)
{
  gboolean retval = FALSE;
  guint n_axes, n_val, i;
  double *values;

  n_axes = clutter_input_device_get_n_axes (device);
  values = valuators->values;

  *dx_p = *dy_p = 0.0;

  n_val = 0;

  for (i = 0; i < MIN (valuators->mask_len * 8, n_axes); i++)
    {
      ClutterScrollDirection direction;
      gdouble delta;

      if (!XIMaskIsSet (valuators->mask, i))
        continue;

      if (_clutter_input_device_get_scroll_delta (device, i,
                                                  values[n_val],
                                                  &direction,
                                                  &delta))
        {
          retval = TRUE;

          if (direction == CLUTTER_SCROLL_UP ||
              direction == CLUTTER_SCROLL_DOWN)
            *dy_p = delta;
          else
            *dx_p = delta;
        }

      n_val += 1;
    }

  return retval;
}

static void
clutter_device_manager_xi2_select_stage_events (ClutterDeviceManager *manager,
                                                ClutterStage         *stage)
{
  ClutterBackendX11 *backend_x11;
  ClutterStageX11 *stage_x11;
  XIEventMask xi_event_mask;
  unsigned char *mask;
  int len;

  backend_x11 = CLUTTER_BACKEND_X11 (clutter_get_default_backend ());
  stage_x11 = CLUTTER_STAGE_X11 (_clutter_stage_get_window (stage));

  len = XIMaskLen (XI_LASTEVENT);
  mask = g_new0 (unsigned char, len);

  XISetMask (mask, XI_Motion);
  XISetMask (mask, XI_ButtonPress);
  XISetMask (mask, XI_ButtonRelease);
  XISetMask (mask, XI_KeyPress);
  XISetMask (mask, XI_KeyRelease);
  XISetMask (mask, XI_Enter);
  XISetMask (mask, XI_Leave);

#ifdef HAVE_XINPUT_2_2
  /* enable touch event support if we're running on XInput 2.2 */
  if (backend_x11->xi_minor >= 2)
    {
      XISetMask (mask, XI_TouchBegin);
      XISetMask (mask, XI_TouchUpdate);
      XISetMask (mask, XI_TouchEnd);
    }
#endif /* HAVE_XINPUT_2_2 */

  xi_event_mask.deviceid = XIAllMasterDevices;
  xi_event_mask.mask = mask;
  xi_event_mask.mask_len = len;

  XISelectEvents (backend_x11->xdpy, stage_x11->xwin, &xi_event_mask, 1);

  g_free (mask);
}

static ClutterTranslateReturn
clutter_device_manager_xi2_translate_event (ClutterEventTranslator *translator,
                                            gpointer                native,
                                            ClutterEvent           *event)
{
  ClutterDeviceManagerXI2 *manager_xi2 = CLUTTER_DEVICE_MANAGER_XI2 (translator);
  ClutterTranslateReturn retval = CLUTTER_TRANSLATE_CONTINUE;
  ClutterBackendX11 *backend_x11;
  ClutterStageX11 *stage_x11 = NULL;
  ClutterStage *stage = NULL;
  ClutterInputDevice *device, *source_device;
  XGenericEventCookie *cookie;
  XIEvent *xi_event;
  XEvent *xevent;

  backend_x11 = CLUTTER_BACKEND_X11 (clutter_get_default_backend ());

  xevent = native;

  cookie = &xevent->xcookie;

  if (cookie->type != GenericEvent ||
      cookie->extension != manager_xi2->opcode)
    return CLUTTER_TRANSLATE_CONTINUE;

  xi_event = (XIEvent *) cookie->data;

  if (!xi_event)
    return CLUTTER_TRANSLATE_REMOVE;

  if (!(xi_event->evtype == XI_HierarchyChanged ||
        xi_event->evtype == XI_DeviceChanged))
    {
      stage = get_event_stage (translator, xi_event);
      if (stage == NULL || CLUTTER_ACTOR_IN_DESTRUCTION (stage))
        return CLUTTER_TRANSLATE_CONTINUE;
      else
        stage_x11 = CLUTTER_STAGE_X11 (_clutter_stage_get_window (stage));
    }

  event->any.stage = stage;

  switch (xi_event->evtype)
    {
    case XI_HierarchyChanged:
      {
        XIHierarchyEvent *xev = (XIHierarchyEvent *) xi_event;

        translate_hierarchy_event (backend_x11, manager_xi2, xev);
      }
      retval = CLUTTER_TRANSLATE_REMOVE;
      break;

    case XI_DeviceChanged:
      {
        XIDeviceChangedEvent *xev = (XIDeviceChangedEvent *) xi_event;

        device = g_hash_table_lookup (manager_xi2->devices_by_id,
                                      GINT_TO_POINTER (xev->deviceid));
        source_device = g_hash_table_lookup (manager_xi2->devices_by_id,
                                             GINT_TO_POINTER (xev->sourceid));
        if (device)
          {
            _clutter_input_device_reset_axes (device);
            translate_device_classes (backend_x11->xdpy,
                                      device,
                                      xev->classes,
                                      xev->num_classes);
          }

        if (source_device)
          _clutter_input_device_reset_scroll_info (source_device);
      }
      retval = CLUTTER_TRANSLATE_REMOVE;
      break;

    case XI_KeyPress:
    case XI_KeyRelease:
      {
        XIDeviceEvent *xev = (XIDeviceEvent *) xi_event;
        ClutterEventX11 *event_x11;
        char buffer[7] = { 0, };
        gunichar n;

        event->key.type = event->type = (xev->evtype == XI_KeyPress)
                                      ? CLUTTER_KEY_PRESS
                                      : CLUTTER_KEY_RELEASE;

        event->key.time = xev->time;
        event->key.stage = stage;
	_clutter_input_device_xi2_translate_state (event, &xev->mods, &xev->buttons, &xev->group);
        event->key.hardware_keycode = xev->detail;

          /* keyval is the key ignoring all modifiers ('1' vs. '!') */
        event->key.keyval =
          _clutter_keymap_x11_translate_key_state (backend_x11->keymap,
                                                   event->key.hardware_keycode,
                                                   &event->key.modifier_state,
                                                   NULL);

        /* KeyEvents have platform specific data associated to them */
        event_x11 = _clutter_event_x11_new ();
        _clutter_event_set_platform_data (event, event_x11);

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

        source_device = g_hash_table_lookup (manager_xi2->devices_by_id,
                                             GINT_TO_POINTER (xev->sourceid));
        clutter_event_set_source_device (event, source_device);

        device = g_hash_table_lookup (manager_xi2->devices_by_id,
                                      GINT_TO_POINTER (xev->deviceid));
        clutter_event_set_device (event, device);

        /* XXX keep this in sync with the evdev device manager */
        n = print_keysym (event->key.keyval, buffer, sizeof (buffer));
        if (n == 0)
          {
            /* not printable */
            event->key.unicode_value = (gunichar) '\0';
          }
        else
          {
            event->key.unicode_value = g_utf8_get_char_validated (buffer, n);
            if (event->key.unicode_value == -1 ||
                event->key.unicode_value == -2)
              event->key.unicode_value = (gunichar) '\0';
          }

        CLUTTER_NOTE (EVENT,
                      "%s: win:0x%x device:%d source:%d, key: %12s (%d)",
                      event->any.type == CLUTTER_KEY_PRESS
                        ? "key press  "
                        : "key release",
                      (unsigned int) stage_x11->xwin,
                      xev->deviceid,
                      xev->sourceid,
                      event->key.keyval ? buffer : "(none)",
                      event->key.keyval);

        if (xi_event->evtype == XI_KeyPress)
          _clutter_stage_x11_set_user_time (stage_x11, event->key.time);

        retval = CLUTTER_TRANSLATE_QUEUE;
      }
      break;

    case XI_ButtonPress:
    case XI_ButtonRelease:
      {
        XIDeviceEvent *xev = (XIDeviceEvent *) xi_event;

        source_device = g_hash_table_lookup (manager_xi2->devices_by_id,
                                             GINT_TO_POINTER (xev->sourceid));
        device = g_hash_table_lookup (manager_xi2->devices_by_id,
                                      GINT_TO_POINTER (xev->deviceid));

        /* Set the stage for core events coming out of nowhere (see bug #684509) */
        if (clutter_input_device_get_device_mode (device) == CLUTTER_INPUT_MODE_MASTER &&
            clutter_input_device_get_pointer_stage (device) == NULL &&
            stage != NULL)
          _clutter_input_device_set_stage (device, stage);

        switch (xev->detail)
          {
          case 4:
          case 5:
          case 6:
          case 7:
            /* we only generate Scroll events on ButtonPress */
            if (xi_event->evtype == XI_ButtonRelease)
              return CLUTTER_TRANSLATE_REMOVE;

            event->scroll.type = event->type = CLUTTER_SCROLL;

            if (xev->detail == 4)
              event->scroll.direction = CLUTTER_SCROLL_UP;
            else if (xev->detail == 5)
              event->scroll.direction = CLUTTER_SCROLL_DOWN;
            else if (xev->detail == 6)
              event->scroll.direction = CLUTTER_SCROLL_LEFT;
            else
              event->scroll.direction = CLUTTER_SCROLL_RIGHT;

            event->scroll.stage = stage;

            event->scroll.time = xev->time;
            event->scroll.x = xev->event_x;
            event->scroll.y = xev->event_y;
	    _clutter_input_device_xi2_translate_state (event,
						       &xev->mods,
						       &xev->buttons,
						       &xev->group);

            clutter_event_set_source_device (event, source_device);
            clutter_event_set_device (event, device);

            event->scroll.axes = translate_axes (event->scroll.device,
                                                 event->scroll.x,
                                                 event->scroll.y,
                                                 stage_x11,
                                                 &xev->valuators);

            CLUTTER_NOTE (EVENT,
                          "scroll: win:0x%x, device:%d '%s', time:%d "
                          "(direction:%s, "
                          "x:%.2f, y:%.2f, "
                          "emulated:%s)",
                          (unsigned int) stage_x11->xwin,
                          device->id,
                          device->device_name,
                          event->any.time,
                          event->scroll.direction == CLUTTER_SCROLL_UP ? "up" :
                          event->scroll.direction == CLUTTER_SCROLL_DOWN ? "down" :
                          event->scroll.direction == CLUTTER_SCROLL_LEFT ? "left" :
                          event->scroll.direction == CLUTTER_SCROLL_RIGHT ? "right" :
                          "invalid",
                          event->scroll.x,
                          event->scroll.y,
#ifdef HAVE_XINPUT_2_2
                          (xev->flags & XIPointerEmulated) ? "yes" : "no"
#else
                          "no"
#endif
                          );
            break;

          default:
            event->button.type = event->type =
              (xi_event->evtype == XI_ButtonPress) ? CLUTTER_BUTTON_PRESS
                                                   : CLUTTER_BUTTON_RELEASE;

            event->button.stage = stage;

            event->button.time = xev->time;
            event->button.x = xev->event_x;
            event->button.y = xev->event_y;
            event->button.button = xev->detail;
	    _clutter_input_device_xi2_translate_state (event,
						       &xev->mods,
						       &xev->buttons,
						       &xev->group);

            clutter_event_set_source_device (event, source_device);
            clutter_event_set_device (event, device);

            event->button.axes = translate_axes (event->button.device,
                                                 event->button.x,
                                                 event->button.y,
                                                 stage_x11,
                                                 &xev->valuators);

            CLUTTER_NOTE (EVENT,
                          "%s: win:0x%x, device:%d '%s', time:%d "
                          "(button:%d, "
                          "x:%.2f, y:%.2f, "
                          "axes:%s, "
                          "emulated:%s)",
                          event->any.type == CLUTTER_BUTTON_PRESS
                            ? "button press  "
                            : "button release",
                          (unsigned int) stage_x11->xwin,
                          device->id,
                          device->device_name,
                          event->any.time,
                          event->button.button,
                          event->button.x,
                          event->button.y,
                          event->button.axes != NULL ? "yes" : "no",
#ifdef HAVE_XINPUT_2_2
                          (xev->flags & XIPointerEmulated) ? "yes" : "no"
#else
                          "no"
#endif
                          );
            break;
          }

        if (source_device != NULL && device->stage != NULL)
          _clutter_input_device_set_stage (source_device, device->stage);

#ifdef HAVE_XINPUT_2_2
        if (xev->flags & XIPointerEmulated)
          _clutter_event_set_pointer_emulated (event, TRUE);
#endif /* HAVE_XINPUT_2_2 */

        if (xi_event->evtype == XI_ButtonPress)
          _clutter_stage_x11_set_user_time (stage_x11, event->button.time);

        retval = CLUTTER_TRANSLATE_QUEUE;
      }
      break;

    case XI_Motion:
      {
        XIDeviceEvent *xev = (XIDeviceEvent *) xi_event;
        gdouble delta_x, delta_y;

        source_device = g_hash_table_lookup (manager_xi2->devices_by_id,
                                             GINT_TO_POINTER (xev->sourceid));
        device = g_hash_table_lookup (manager_xi2->devices_by_id,
                                      GINT_TO_POINTER (xev->deviceid));

        /* Set the stage for core events coming out of nowhere (see bug #684509) */
        if (clutter_input_device_get_device_mode (device) == CLUTTER_INPUT_MODE_MASTER &&
            clutter_input_device_get_pointer_stage (device) == NULL &&
            stage != NULL)
          _clutter_input_device_set_stage (device, stage);

        if (scroll_valuators_changed (source_device,
                                      &xev->valuators,
                                      &delta_x, &delta_y))
          {
            event->scroll.type = event->type = CLUTTER_SCROLL;
            event->scroll.direction = CLUTTER_SCROLL_SMOOTH;

            event->scroll.stage = stage;
            event->scroll.time = xev->time;
            event->scroll.x = xev->event_x;
            event->scroll.y = xev->event_y;
	    _clutter_input_device_xi2_translate_state (event,
						       &xev->mods,
						       &xev->buttons,
						       &xev->group);

            clutter_event_set_scroll_delta (event, delta_x, delta_y);
            clutter_event_set_source_device (event, source_device);
            clutter_event_set_device (event, device);

            CLUTTER_NOTE (EVENT,
                          "smooth scroll: win:0x%x device:%d '%s' (x:%.2f, y:%.2f, delta:%f, %f)",
                          (unsigned int) stage_x11->xwin,
                          event->scroll.device->id,
                          event->scroll.device->device_name,
                          event->scroll.x,
                          event->scroll.y,
                          delta_x, delta_y);

            retval = CLUTTER_TRANSLATE_QUEUE;
            break;
          }

        event->motion.type = event->type = CLUTTER_MOTION;

        event->motion.stage = stage;

        event->motion.time = xev->time;
        event->motion.x = xev->event_x;
        event->motion.y = xev->event_y;
	_clutter_input_device_xi2_translate_state (event,
						   &xev->mods,
						   &xev->buttons,
						   &xev->group);

        clutter_event_set_source_device (event, source_device);
        clutter_event_set_device (event, device);

        event->motion.axes = translate_axes (event->motion.device,
                                             event->motion.x,
                                             event->motion.y,
                                             stage_x11,
                                             &xev->valuators);

        if (source_device != NULL && device->stage != NULL)
          _clutter_input_device_set_stage (source_device, device->stage);

#ifdef HAVE_XINPUT_2_2
        if (xev->flags & XIPointerEmulated)
          _clutter_event_set_pointer_emulated (event, TRUE);
#endif /* HAVE_XINPUT_2_2 */

        CLUTTER_NOTE (EVENT, "motion: win:0x%x device:%d '%s' (x:%.2f, y:%.2f, axes:%s)",
                      (unsigned int) stage_x11->xwin,
                      event->motion.device->id,
                      event->motion.device->device_name,
                      event->motion.x,
                      event->motion.y,
                      event->motion.axes != NULL ? "yes" : "no");

        retval = CLUTTER_TRANSLATE_QUEUE;
      }
      break;

#ifdef HAVE_XINPUT_2_2
    case XI_TouchBegin:
    case XI_TouchEnd:
      {
        XIDeviceEvent *xev = (XIDeviceEvent *) xi_event;

        source_device = g_hash_table_lookup (manager_xi2->devices_by_id,
                                             GINT_TO_POINTER (xev->sourceid));

        if (xi_event->evtype == XI_TouchBegin)
          event->touch.type = event->type = CLUTTER_TOUCH_BEGIN;
        else
          event->touch.type = event->type = CLUTTER_TOUCH_END;

        event->touch.stage = stage;
        event->touch.time = xev->time;
        event->touch.x = xev->event_x;
        event->touch.y = xev->event_y;
	_clutter_input_device_xi2_translate_state (event,
						   &xev->mods,
						   &xev->buttons,
						   &xev->group);

        clutter_event_set_source_device (event, source_device);

        device = g_hash_table_lookup (manager_xi2->devices_by_id,
                                      GINT_TO_POINTER (xev->deviceid));
        clutter_event_set_device (event, device);

        event->touch.axes = translate_axes (event->touch.device,
                                            event->motion.x,
                                            event->motion.y,
                                            stage_x11,
                                            &xev->valuators);

        if (xi_event->evtype == XI_TouchBegin)
          {
            event->touch.modifier_state |= CLUTTER_BUTTON1_MASK;

            _clutter_stage_x11_set_user_time (stage_x11, event->touch.time);
          }

        event->touch.sequence = GUINT_TO_POINTER (xev->detail);

        if (xev->flags & XITouchEmulatingPointer)
          _clutter_event_set_pointer_emulated (event, TRUE);

        CLUTTER_NOTE (EVENT, "touch %s: win:0x%x device:%d '%s' (seq:%d, x:%.2f, y:%.2f, axes:%s)",
                      event->type == CLUTTER_TOUCH_BEGIN ? "begin" : "end",
                      (unsigned int) stage_x11->xwin,
                      event->touch.device->id,
                      event->touch.device->device_name,
                      GPOINTER_TO_UINT (event->touch.sequence),
                      event->touch.x,
                      event->touch.y,
                      event->touch.axes != NULL ? "yes" : "no");

        retval = CLUTTER_TRANSLATE_QUEUE;
      }
      break;

    case XI_TouchUpdate:
      {
        XIDeviceEvent *xev = (XIDeviceEvent *) xi_event;

        source_device = g_hash_table_lookup (manager_xi2->devices_by_id,
                                             GINT_TO_POINTER (xev->sourceid));

        event->touch.type = event->type = CLUTTER_TOUCH_UPDATE;
        event->touch.stage = stage;
        event->touch.time = xev->time;
        event->touch.sequence = GUINT_TO_POINTER (xev->detail);
        event->touch.x = xev->event_x;
        event->touch.y = xev->event_y;

        clutter_event_set_source_device (event, source_device);

        device = g_hash_table_lookup (manager_xi2->devices_by_id,
                                      GINT_TO_POINTER (xev->deviceid));
        clutter_event_set_device (event, device);

        event->touch.axes = translate_axes (event->touch.device,
                                            event->motion.x,
                                            event->motion.y,
                                            stage_x11,
                                            &xev->valuators);

	_clutter_input_device_xi2_translate_state (event,
						   &xev->mods,
						   &xev->buttons,
						   &xev->group);
        event->touch.modifier_state |= CLUTTER_BUTTON1_MASK;

        if (xev->flags & XITouchEmulatingPointer)
          _clutter_event_set_pointer_emulated (event, TRUE);

        CLUTTER_NOTE (EVENT, "touch update: win:0x%x device:%d '%s' (seq:%d, x:%.2f, y:%.2f, axes:%s)",
                      (unsigned int) stage_x11->xwin,
                      event->touch.device->id,
                      event->touch.device->device_name,
                      GPOINTER_TO_UINT (event->touch.sequence),
                      event->touch.x,
                      event->touch.y,
                      event->touch.axes != NULL ? "yes" : "no");

        retval = CLUTTER_TRANSLATE_QUEUE;
      }
      break;
#endif /* HAVE_XINPUT_2_2 */

    case XI_Enter:
    case XI_Leave:
      {
        XIEnterEvent *xev = (XIEnterEvent *) xi_event;

        device = g_hash_table_lookup (manager_xi2->devices_by_id,
                                      GINT_TO_POINTER (xev->deviceid));

        source_device = g_hash_table_lookup (manager_xi2->devices_by_id,
                                             GINT_TO_POINTER (xev->sourceid));

        if (xi_event->evtype == XI_Enter)
          {
            event->crossing.type = event->type = CLUTTER_ENTER;

            event->crossing.stage = stage;
            event->crossing.source = CLUTTER_ACTOR (stage);
            event->crossing.related = NULL;

            event->crossing.time = xev->time;
            event->crossing.x = xev->event_x;
            event->crossing.y = xev->event_y;

            _clutter_input_device_set_stage (device, stage);
          }
        else
          {
            if (device->stage == NULL)
              {
                CLUTTER_NOTE (EVENT,
                              "Discarding Leave for ButtonRelease "
                              "event off-stage");

                retval = CLUTTER_TRANSLATE_REMOVE;
                break;
              }

            event->crossing.type = event->type = CLUTTER_LEAVE;

            event->crossing.stage = stage;
            event->crossing.source = CLUTTER_ACTOR (stage);
            event->crossing.related = NULL;

            event->crossing.time = xev->time;
            event->crossing.x = xev->event_x;
            event->crossing.y = xev->event_y;

            _clutter_input_device_set_stage (device, NULL);
          }

        _clutter_input_device_reset_scroll_info (source_device);

        clutter_event_set_device (event, device);
        clutter_event_set_source_device (event, source_device);

        retval = CLUTTER_TRANSLATE_QUEUE;
      }
      break;

    case XI_FocusIn:
    case XI_FocusOut:
      retval = CLUTTER_TRANSLATE_CONTINUE;
      break;
    }

  return retval;
}

static void
clutter_event_translator_iface_init (ClutterEventTranslatorIface *iface)
{
  iface->translate_event = clutter_device_manager_xi2_translate_event;
}

static void
clutter_device_manager_xi2_add_device (ClutterDeviceManager *manager,
                                       ClutterInputDevice   *device)
{
  /* XXX implement */
}

static void
clutter_device_manager_xi2_remove_device (ClutterDeviceManager *manager,
                                          ClutterInputDevice   *device)
{
  /* XXX implement */
}

static const GSList *
clutter_device_manager_xi2_get_devices (ClutterDeviceManager *manager)
{
  ClutterDeviceManagerXI2 *manager_xi2 = CLUTTER_DEVICE_MANAGER_XI2 (manager);
  GSList *all_devices = NULL;
  GList *l;

  if (manager_xi2->all_devices != NULL)
    return manager_xi2->all_devices;

  for (l = manager_xi2->master_devices; l != NULL; l = l->next)
    all_devices = g_slist_prepend (all_devices, l->data);

  for (l = manager_xi2->slave_devices; l != NULL; l = l->next)
    all_devices = g_slist_prepend (all_devices, l->data);

  manager_xi2->all_devices = g_slist_reverse (all_devices);

  return manager_xi2->all_devices;
}

static ClutterInputDevice *
clutter_device_manager_xi2_get_device (ClutterDeviceManager *manager,
                                       gint                  id)
{
  ClutterDeviceManagerXI2 *manager_xi2 = CLUTTER_DEVICE_MANAGER_XI2 (manager);

  return g_hash_table_lookup (manager_xi2->devices_by_id,
                              GINT_TO_POINTER (id));
}

static ClutterInputDevice *
clutter_device_manager_xi2_get_core_device (ClutterDeviceManager   *manager,
                                            ClutterInputDeviceType  device_type)
{
  ClutterDeviceManagerXI2 *manager_xi2 = CLUTTER_DEVICE_MANAGER_XI2 (manager);
  ClutterBackendX11 *backend_x11;
  ClutterInputDevice *device;
  int device_id;

  backend_x11 =
    CLUTTER_BACKEND_X11 (_clutter_device_manager_get_backend (manager));

  XIGetClientPointer (backend_x11->xdpy, None, &device_id);

  device = g_hash_table_lookup (manager_xi2->devices_by_id,
                                GINT_TO_POINTER (device_id));

  switch (device_type)
    {
    case CLUTTER_POINTER_DEVICE:
      return device;

    case CLUTTER_KEYBOARD_DEVICE:
      return clutter_input_device_get_associated_device (device);

    default:
      break;
    }

  return NULL;
}

static void
relate_masters (gpointer key,
                gpointer value,
                gpointer data)
{
  ClutterDeviceManagerXI2 *manager_xi2 = data;
  ClutterInputDevice *device, *relative;

  device = g_hash_table_lookup (manager_xi2->devices_by_id, key);
  relative = g_hash_table_lookup (manager_xi2->devices_by_id, value);

  _clutter_input_device_set_associated_device (device, relative);
  _clutter_input_device_set_associated_device (relative, device);
}

static void
relate_slaves (gpointer key,
               gpointer value,
               gpointer data)
{
  ClutterDeviceManagerXI2 *manager_xi2 = data;
  ClutterInputDevice *master, *slave;

  slave = g_hash_table_lookup (manager_xi2->devices_by_id, key);
  master = g_hash_table_lookup (manager_xi2->devices_by_id, value);

  _clutter_input_device_set_associated_device (slave, master);
  _clutter_input_device_add_slave (master, slave);
}

static void
clutter_device_manager_xi2_constructed (GObject *gobject)
{
  ClutterDeviceManagerXI2 *manager_xi2 = CLUTTER_DEVICE_MANAGER_XI2 (gobject);
  ClutterDeviceManager *manager = CLUTTER_DEVICE_MANAGER (gobject);
  ClutterBackendX11 *backend_x11;
  GHashTable *masters, *slaves;
  XIDeviceInfo *info;
  XIEventMask event_mask;
  unsigned char mask[2] = { 0, };
  int n_devices, i;

  backend_x11 =
    CLUTTER_BACKEND_X11 (_clutter_device_manager_get_backend (manager));

  masters = g_hash_table_new (NULL, NULL);
  slaves = g_hash_table_new (NULL, NULL);

  info = XIQueryDevice (backend_x11->xdpy, XIAllDevices, &n_devices);

  for (i = 0; i < n_devices; i++)
    {
      XIDeviceInfo *xi_device = &info[i];

      add_device (manager_xi2, backend_x11, xi_device, TRUE);

      if (xi_device->use == XIMasterPointer ||
          xi_device->use == XIMasterKeyboard)
        {
          g_hash_table_insert (masters,
                               GINT_TO_POINTER (xi_device->deviceid),
                               GINT_TO_POINTER (xi_device->attachment));
        }
      else if (xi_device->use == XISlavePointer ||
               xi_device->use == XISlaveKeyboard)
        {
          g_hash_table_insert (slaves,
                               GINT_TO_POINTER (xi_device->deviceid),
                               GINT_TO_POINTER (xi_device->attachment));
        }
    }

  XIFreeDeviceInfo (info);

  g_hash_table_foreach (masters, relate_masters, manager_xi2);
  g_hash_table_destroy (masters);

  g_hash_table_foreach (slaves, relate_slaves, manager_xi2);
  g_hash_table_destroy (slaves);

  XISetMask (mask, XI_HierarchyChanged);
  XISetMask (mask, XI_DeviceChanged);

  event_mask.deviceid = XIAllDevices;
  event_mask.mask_len = sizeof (mask);
  event_mask.mask = mask;

  clutter_device_manager_xi2_select_events (manager,
                                            clutter_x11_get_root_window (),
                                            &event_mask);

  if (G_OBJECT_CLASS (clutter_device_manager_xi2_parent_class)->constructed)
    G_OBJECT_CLASS (clutter_device_manager_xi2_parent_class)->constructed (gobject);
}

static void
clutter_device_manager_xi2_set_property (GObject      *gobject,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ClutterDeviceManagerXI2 *manager_xi2 = CLUTTER_DEVICE_MANAGER_XI2 (gobject);

  switch (prop_id)
    {
    case PROP_OPCODE:
      manager_xi2->opcode = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_device_manager_xi2_class_init (ClutterDeviceManagerXI2Class *klass)
{
  ClutterDeviceManagerClass *manager_class;
  GObjectClass *gobject_class;

  obj_props[PROP_OPCODE] =
    g_param_spec_int ("opcode",
                      "Opcode",
                      "The XI2 opcode",
                      -1, G_MAXINT,
                      -1,
                      CLUTTER_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->constructed = clutter_device_manager_xi2_constructed;
  gobject_class->set_property = clutter_device_manager_xi2_set_property;

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
  
  manager_class = CLUTTER_DEVICE_MANAGER_CLASS (klass);
  manager_class->add_device = clutter_device_manager_xi2_add_device;
  manager_class->remove_device = clutter_device_manager_xi2_remove_device;
  manager_class->get_devices = clutter_device_manager_xi2_get_devices;
  manager_class->get_core_device = clutter_device_manager_xi2_get_core_device;
  manager_class->get_device = clutter_device_manager_xi2_get_device;
  manager_class->select_stage_events = clutter_device_manager_xi2_select_stage_events;
}

static void
clutter_device_manager_xi2_init (ClutterDeviceManagerXI2 *self)
{
  self->devices_by_id = g_hash_table_new_full (NULL, NULL,
                                               NULL,
                                               (GDestroyNotify) g_object_unref);
}
