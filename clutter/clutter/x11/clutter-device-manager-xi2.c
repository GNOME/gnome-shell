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

#include "clutter-build-config.h"

#include <stdint.h>

#include "clutter-device-manager-xi2.h"

#include "clutter-backend-x11.h"
#include "clutter-input-device-xi2.h"
#include "clutter-input-device-tool-xi2.h"
#include "clutter-virtual-input-device-x11.h"
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

enum {
  PAD_AXIS_FIRST  = 3, /* First axes are always x/y/pressure, ignored in pads */
  PAD_AXIS_STRIP1 = PAD_AXIS_FIRST,
  PAD_AXIS_STRIP2,
  PAD_AXIS_RING1,
  PAD_AXIS_RING2,
};

static Atom clutter_input_axis_atoms[N_AXIS_ATOMS] = { 0, };

static void clutter_event_translator_iface_init (ClutterEventTranslatorIface *iface);
static void clutter_event_extender_iface_init   (ClutterEventExtenderInterface *iface);

#define clutter_device_manager_xi2_get_type     _clutter_device_manager_xi2_get_type

G_DEFINE_TYPE_WITH_CODE (ClutterDeviceManagerXI2,
                         clutter_device_manager_xi2,
                         CLUTTER_TYPE_DEVICE_MANAGER,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_EVENT_TRANSLATOR,
                                                clutter_event_translator_iface_init)
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_EVENT_EXTENDER,
                                                clutter_event_extender_iface_init))

static void
clutter_device_manager_x11_copy_event_data (ClutterEventExtender *event_extender,
                                            const ClutterEvent   *src,
                                            ClutterEvent         *dest)
{
  gpointer event_x11;

  event_x11 = _clutter_event_get_platform_data (src);
  if (event_x11 != NULL)
    _clutter_event_set_platform_data (dest, _clutter_event_x11_copy (event_x11));
}

static void
clutter_device_manager_x11_free_event_data (ClutterEventExtender *event_extender,
                                            ClutterEvent         *event)
{
  gpointer event_x11;

  event_x11 = _clutter_event_get_platform_data (event);
  if (event_x11 != NULL)
    _clutter_event_x11_free (event_x11);
}

static void
clutter_event_extender_iface_init (ClutterEventExtenderInterface *iface)
{
  iface->copy_event_data = clutter_device_manager_x11_copy_event_data;
  iface->free_event_data = clutter_device_manager_x11_free_event_data;
}

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

static gboolean
is_touchpad_device (ClutterBackendX11 *backend_x11,
                    XIDeviceInfo      *info)
{
  gulong nitems, bytes_after;
  guint32 *data = NULL;
  int rc, format;
  Atom type;
  Atom prop;

  prop = XInternAtom (backend_x11->xdpy, "libinput Tapping Enabled", True);
  if (prop == None)
    return FALSE;

  clutter_x11_trap_x_errors ();
  rc = XIGetProperty (backend_x11->xdpy,
                      info->deviceid,
                      prop,
                      0, 1, False, XA_INTEGER, &type, &format, &nitems, &bytes_after,
                      (guchar **) &data);
  clutter_x11_untrap_x_errors ();

  /* We don't care about the data */
  XFree (data);

  if (rc != Success || type != XA_INTEGER || format != 8 || nitems != 1)
    return FALSE;

  return TRUE;
}

static gboolean
get_device_ids (ClutterBackendX11  *backend_x11,
                XIDeviceInfo       *info,
                gchar             **vendor_id,
                gchar             **product_id)
{
  gulong nitems, bytes_after;
  guint32 *data = NULL;
  int rc, format;
  Atom type;

  clutter_x11_trap_x_errors ();
  rc = XIGetProperty (backend_x11->xdpy,
                      info->deviceid,
                      XInternAtom (backend_x11->xdpy, "Device Product ID", False),
                      0, 2, False, XA_INTEGER, &type, &format, &nitems, &bytes_after,
                      (guchar **) &data);
  clutter_x11_untrap_x_errors ();

  if (rc != Success || type != XA_INTEGER || format != 32 || nitems != 2)
    {
      XFree (data);
      return FALSE;
    }

  if (vendor_id)
    *vendor_id = g_strdup_printf ("%.4x", data[0]);
  if (product_id)
    *product_id = g_strdup_printf ("%.4x", data[1]);

  XFree (data);

  return TRUE;
}

static gchar *
get_device_node_path (ClutterBackendX11  *backend_x11,
                      XIDeviceInfo       *info)
{
  gulong nitems, bytes_after;
  guchar *data;
  int rc, format;
  Atom prop, type;
  gchar *node_path;

  prop = XInternAtom (backend_x11->xdpy, "Device Node", False);
  if (prop == None)
    return NULL;

  clutter_x11_trap_x_errors ();

  rc = XIGetProperty (backend_x11->xdpy,
                      info->deviceid, prop, 0, 1024, False,
                      XA_STRING, &type, &format, &nitems, &bytes_after,
                      (guchar **) &data);

  if (clutter_x11_untrap_x_errors ())
    return NULL;

  if (rc != Success || type != XA_STRING || format != 8)
    {
      XFree (data);
      return FALSE;
    }

  node_path = g_strdup ((char *) data);
  XFree (data);

  return node_path;
}

static void
get_pad_features (XIDeviceInfo *info,
                  guint        *n_rings,
                  guint        *n_strips)
{
  gint i, rings = 0, strips = 0;

  for (i = PAD_AXIS_FIRST; i < info->num_classes; i++)
    {
      XIValuatorClassInfo *valuator = (XIValuatorClassInfo*) info->classes[i];
      int axis = valuator->number;

      if (valuator->type != XIValuatorClass)
        continue;
      if (valuator->max <= 1)
        continue;

      /* Ring/strip axes are fixed in pad devices as handled by the
       * wacom driver. Match those to detect pad features.
       */
      if (axis == PAD_AXIS_STRIP1 || axis == PAD_AXIS_STRIP2)
        strips++;
      else if (axis == PAD_AXIS_RING1 || axis == PAD_AXIS_RING2)
        rings++;
    }

  *n_rings = rings;
  *n_strips = strips;
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
  guint num_touches = 0, num_rings = 0, num_strips = 0;
  gchar *vendor_id = NULL, *product_id = NULL, *node_path = NULL;

  if (info->use == XIMasterKeyboard || info->use == XISlaveKeyboard)
    {
      source = CLUTTER_KEYBOARD_DEVICE;
    }
  else if (is_touchpad_device (backend_x11, info))
    {
      source = CLUTTER_TOUCHPAD_DEVICE;
    }
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
      else if (strstr (name, " pad") != NULL)
        source = CLUTTER_PAD_DEVICE;
      else if (strstr (name, "wacom") != NULL || strstr (name, "pen") != NULL)
        source = CLUTTER_PEN_DEVICE;
      else if (strstr (name, "touchpad") != NULL)
        source = CLUTTER_TOUCHPAD_DEVICE;
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

  if (info->use != XIMasterKeyboard &&
      info->use != XIMasterPointer)
    {
      get_device_ids (backend_x11, info, &vendor_id, &product_id);
      node_path = get_device_node_path (backend_x11, info);
    }

  if (source == CLUTTER_PAD_DEVICE)
    {
      is_enabled = TRUE;
      get_pad_features (info, &num_rings, &num_strips);
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
                         "vendor-id", vendor_id,
                         "product-id", product_id,
                         "device-node", node_path,
                         "n-rings", num_rings,
                         "n-strips", num_strips,
                         NULL);

  translate_device_classes (backend_x11->xdpy, retval,
                            info->classes,
                            info->num_classes);
  g_free (vendor_id);
  g_free (product_id);

  CLUTTER_NOTE (BACKEND, "Created device '%s' (id: %d, has-cursor: %s)",
                info->name,
                info->deviceid,
                info->use == XIMasterPointer ? "yes" : "no");

  return retval;
}

static void
pad_passive_button_grab (ClutterInputDevice *device)
{
  XIGrabModifiers xi_grab_mods = { XIAnyModifier, };
  XIEventMask xi_event_mask;
  gint device_id, rc;

  device_id = clutter_input_device_get_device_id (device);

  xi_event_mask.deviceid = device_id;
  xi_event_mask.mask_len = XIMaskLen (XI_LASTEVENT);
  xi_event_mask.mask = g_new0 (unsigned char, xi_event_mask.mask_len);

  XISetMask (xi_event_mask.mask, XI_Motion);
  XISetMask (xi_event_mask.mask, XI_ButtonPress);
  XISetMask (xi_event_mask.mask, XI_ButtonRelease);

  clutter_x11_trap_x_errors ();
  rc = XIGrabButton (clutter_x11_get_default_display (),
                     device_id, XIAnyButton,
                     clutter_x11_get_root_window (), None,
                     XIGrabModeSync, XIGrabModeSync,
                     True, &xi_event_mask, 1, &xi_grab_mods);
  if (rc != 0)
    {
      g_warning ("Could not passively grab pad device: %s",
                 clutter_input_device_get_device_name (device));
    }
  else
    {
      XIAllowEvents (clutter_x11_get_default_display (),
                     device_id, XIAsyncDevice,
                     CLUTTER_CURRENT_TIME);
    }

  clutter_x11_untrap_x_errors ();

  g_free (xi_event_mask.mask);
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
                        device);

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

  if (clutter_input_device_get_device_type (device) == CLUTTER_PAD_DEVICE)
    pad_passive_button_grab (device);

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
      if (ev->info[i].flags & XIDeviceEnabled &&
          !g_hash_table_lookup (manager_xi2->devices_by_id,
                                GINT_TO_POINTER (ev->info[i].deviceid)))
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

static gboolean
translate_pad_axis (ClutterInputDevice *device,
                    XIValuatorState    *valuators,
                    ClutterEventType   *evtype,
                    guint              *number,
                    gdouble            *value)
{
  double *values;
  gint i;

  values = valuators->values;

  for (i = PAD_AXIS_FIRST; i < valuators->mask_len * 8; i++)
    {
      gdouble val;
      guint axis_number = 0;

      if (!XIMaskIsSet (valuators->mask, i))
        continue;

      val = *values++;
      if (val <= 0)
        continue;

      _clutter_input_device_translate_axis (device, i, val, value);

      if (i == PAD_AXIS_RING1 || i == PAD_AXIS_RING2)
        {
          *evtype = CLUTTER_PAD_RING;
          (*value) *= 360.0;
        }
      else if (i == PAD_AXIS_STRIP1 || i == PAD_AXIS_STRIP2)
        {
          *evtype = CLUTTER_PAD_STRIP;
        }
      else
        continue;

      if (i == PAD_AXIS_STRIP2 || i == PAD_AXIS_RING2)
        axis_number++;

      *number = axis_number;
      return TRUE;
    }

  return FALSE;
}

static void
translate_coords (ClutterStageX11 *stage_x11,
                  gdouble          event_x,
                  gdouble          event_y,
                  gfloat          *x_out,
                  gfloat          *y_out)
{
  ClutterStageCogl *stage_cogl = CLUTTER_STAGE_COGL (stage_x11);
  ClutterActor *stage = CLUTTER_ACTOR (stage_cogl->wrapper);
  gfloat stage_width;
  gfloat stage_height;

  clutter_actor_get_size (stage, &stage_width, &stage_height);

  *x_out = CLAMP (event_x, 0, stage_width);
  *y_out = CLAMP (event_y, 0, stage_height);
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

static guint
device_get_tool_serial (ClutterBackendX11  *backend_x11,
                        ClutterInputDevice *device)
{
  gulong nitems, bytes_after;
  guint32 *data = NULL;
  guint serial_id = 0;
  int rc, format;
  Atom type;
  Atom prop;

  prop = XInternAtom (backend_x11->xdpy, "Wacom Serial IDs", True);
  if (prop == None)
    return 0;

  clutter_x11_trap_x_errors ();
  rc = XIGetProperty (backend_x11->xdpy,
                      clutter_input_device_get_device_id (device),
                      prop, 0, 4, FALSE, XA_INTEGER, &type, &format, &nitems, &bytes_after,
                      (guchar **) &data);
  clutter_x11_untrap_x_errors ();

  if (rc == Success && type == XA_INTEGER && format == 32 && nitems >= 4)
    serial_id = data[3];

  XFree (data);

  return serial_id;
}

static void
handle_property_event (ClutterDeviceManagerXI2 *manager_xi2,
                       XIEvent                 *event)
{
  XIPropertyEvent *xev = (XIPropertyEvent *) event;
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (clutter_get_default_backend ());
  Atom serial_ids_prop = XInternAtom (backend_x11->xdpy, "Wacom Serial IDs", True);
  ClutterInputDevice *device;

  device = g_hash_table_lookup (manager_xi2->devices_by_id,
                                GINT_TO_POINTER (xev->deviceid));
  if (!device)
    return;

  if (xev->property == serial_ids_prop)
    {
      ClutterInputDeviceTool *tool = NULL;
      ClutterInputDeviceToolType type;
      guint serial_id;

      serial_id = device_get_tool_serial (backend_x11, device);

      if (serial_id != 0)
        {
          tool = g_hash_table_lookup (manager_xi2->tools_by_serial,
                                      GUINT_TO_POINTER (serial_id));
          if (!tool)
            {
              type = clutter_input_device_get_device_type (device) == CLUTTER_ERASER_DEVICE ?
                CLUTTER_INPUT_DEVICE_TOOL_ERASER : CLUTTER_INPUT_DEVICE_TOOL_PEN;
              tool = clutter_input_device_tool_xi2_new (serial_id, type);
              g_hash_table_insert (manager_xi2->tools_by_serial,
                                   GUINT_TO_POINTER (serial_id),
                                   tool);
            }
        }

      clutter_input_device_xi2_update_tool (device, tool);
      g_signal_emit_by_name (manager_xi2, "tool-changed", device, tool);
    }
}

static gboolean
translate_pad_event (ClutterEvent       *event,
                     XIDeviceEvent      *xev,
                     ClutterInputDevice *device)
{
  gdouble value;
  guint number;

  if (!translate_pad_axis (device, &xev->valuators,
                           &event->any.type,
                           &number, &value))
    return FALSE;

  /* When touching a ring/strip a first XI_Motion event
   * is generated. Use it to reset the pad state, so
   * later events actually have a directionality.
   */
  if (xev->evtype == XI_Motion)
    value = -1;

  if (event->any.type == CLUTTER_PAD_RING)
    {
      event->pad_ring.ring_number = number;
      event->pad_ring.angle = value;
    }
  else
    {
      event->pad_strip.strip_number = number;
      event->pad_strip.value = value;
    }

  event->any.time = xev->time;
  clutter_event_set_device (event, device);
  clutter_event_set_source_device (event, device);

  CLUTTER_NOTE (EVENT,
                "%s: win:0x%x, device:%d '%s', time:%d "
                "(value:%f)",
                event->any.type == CLUTTER_PAD_RING
                ? "pad ring  "
                : "pad strip",
                (unsigned int) xev->event,
                device->id,
                device->device_name,
                event->any.time, value);
  return TRUE;
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
        xi_event->evtype == XI_DeviceChanged ||
        xi_event->evtype == XI_PropertyEvent))
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

	if (clutter_input_device_get_device_type (source_device) == CLUTTER_PAD_DEVICE)
          {
            /* We got these events because of the passive button grab */
            XIAllowEvents (clutter_x11_get_default_display (),
                           xev->sourceid,
                           XIAsyncDevice,
                           xev->time);

            event->any.stage = stage;

            if (xev->detail >= 4 && xev->detail <= 7)
              {
                retval = CLUTTER_TRANSLATE_REMOVE;

                if (xi_event->evtype == XI_ButtonPress &&
                    translate_pad_event (event, xev, source_device))
                  retval = CLUTTER_TRANSLATE_QUEUE;

                break;
              }

            event->any.type =
              (xi_event->evtype == XI_ButtonPress) ? CLUTTER_PAD_BUTTON_PRESS
                                                   : CLUTTER_PAD_BUTTON_RELEASE;
            event->any.time = xev->time;

            /* The 4-7 button range is taken as non-existent on pad devices,
             * let the buttons above that take over this range.
             */
            if (xev->detail > 7)
              xev->detail -= 4;

            /* Pad buttons are 0-indexed */
            event->pad_button.button = xev->detail - 1;
            clutter_event_set_device (event, device);
            clutter_event_set_source_device (event, source_device);

            CLUTTER_NOTE (EVENT,
                          "%s: win:0x%x, device:%d '%s', time:%d "
                          "(button:%d)",
                          event->any.type == CLUTTER_BUTTON_PRESS
                            ? "pad button press  "
                            : "pad button release",
                          (unsigned int) stage_x11->xwin,
                          device->id,
                          device->device_name,
                          event->any.time,
                          event->pad_button.button);

            retval = CLUTTER_TRANSLATE_QUEUE;
            break;
          }

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
            translate_coords (stage_x11, xev->event_x, xev->event_y, &event->scroll.x, &event->scroll.y);
	    _clutter_input_device_xi2_translate_state (event,
						       &xev->mods,
						       &xev->buttons,
						       &xev->group);

            clutter_event_set_source_device (event, source_device);
            clutter_event_set_device (event, device);

            event->scroll.axes = translate_axes (event->scroll.device,
                                                 event->scroll.x,
                                                 event->scroll.y,
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
            translate_coords (stage_x11, xev->event_x, xev->event_y, &event->button.x, &event->button.y);
            event->button.button = xev->detail;
	    _clutter_input_device_xi2_translate_state (event,
						       &xev->mods,
						       &xev->buttons,
						       &xev->group);

            clutter_event_set_source_device (event, source_device);
            clutter_event_set_device (event, device);
            clutter_event_set_device_tool (event,
                                           clutter_input_device_xi2_get_current_tool (source_device));

            event->button.axes = translate_axes (event->button.device,
                                                 event->button.x,
                                                 event->button.y,
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

        if (clutter_input_device_get_device_type (source_device) == CLUTTER_PAD_DEVICE)
          {
            event->any.stage = stage;

            if (translate_pad_event (event, xev, source_device))
              retval = CLUTTER_TRANSLATE_QUEUE;
            break;
          }

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
            translate_coords (stage_x11, xev->event_x, xev->event_y, &event->scroll.x, &event->scroll.y);
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
        translate_coords (stage_x11, xev->event_x, xev->event_y, &event->motion.x, &event->motion.y);
	_clutter_input_device_xi2_translate_state (event,
						   &xev->mods,
						   &xev->buttons,
						   &xev->group);

        clutter_event_set_source_device (event, source_device);
        clutter_event_set_device (event, device);
        clutter_event_set_device_tool (event,
                                       clutter_input_device_xi2_get_current_tool (source_device));

        event->motion.axes = translate_axes (event->motion.device,
                                             event->motion.x,
                                             event->motion.y,
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
      {
        XIDeviceEvent *xev = (XIDeviceEvent *) xi_event;
        device = g_hash_table_lookup (manager_xi2->devices_by_id,
                                      GINT_TO_POINTER (xev->deviceid));
        if (!_clutter_input_device_get_stage (device))
          _clutter_input_device_set_stage (device, stage);
      }
      /* Fall through */
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
        translate_coords (stage_x11, xev->event_x, xev->event_y, &event->touch.x, &event->touch.y);
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
        translate_coords (stage_x11, xev->event_x, xev->event_y, &event->touch.x, &event->touch.y);

        clutter_event_set_source_device (event, source_device);

        device = g_hash_table_lookup (manager_xi2->devices_by_id,
                                      GINT_TO_POINTER (xev->deviceid));
        clutter_event_set_device (event, device);

        event->touch.axes = translate_axes (event->touch.device,
                                            event->motion.x,
                                            event->motion.y,
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
            translate_coords (stage_x11, xev->event_x, xev->event_y, &event->crossing.x, &event->crossing.y);

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
            translate_coords (stage_x11, xev->event_x, xev->event_y, &event->crossing.x, &event->crossing.y);

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
    case XI_PropertyEvent:
      handle_property_event (manager_xi2, xi_event);
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
  ClutterInputDevice *pointer = NULL;
  GList *l;

  for (l = manager_xi2->master_devices; l != NULL ; l = l->next)
    {
      ClutterInputDevice *device = l->data;
      if (clutter_input_device_get_device_type (device) == CLUTTER_POINTER_DEVICE)
        {
          pointer = device;
          break;
        }
    }

  if (pointer == NULL)
    return NULL;

  switch (device_type)
    {
    case CLUTTER_POINTER_DEVICE:
      return pointer;

    case CLUTTER_KEYBOARD_DEVICE:
      return clutter_input_device_get_associated_device (pointer);

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

      if (!xi_device->enabled)
        continue;

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
  XISetMask (mask, XI_PropertyEvent);

  event_mask.deviceid = XIAllDevices;
  event_mask.mask_len = sizeof (mask);
  event_mask.mask = mask;

  clutter_device_manager_xi2_select_events (manager,
                                            clutter_x11_get_root_window (),
                                            &event_mask);

  XSync (backend_x11->xdpy, False);

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

static ClutterVirtualInputDevice *
clutter_device_manager_xi2_create_virtual_device (ClutterDeviceManager   *manager,
                                                  ClutterInputDeviceType  device_type)
{
  return g_object_new (CLUTTER_TYPE_VIRTUAL_INPUT_DEVICE_X11,
                       "device-manager", manager,
                       "device-type", device_type,
                       NULL);
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
  manager_class->create_virtual_device = clutter_device_manager_xi2_create_virtual_device;
}

static void
clutter_device_manager_xi2_init (ClutterDeviceManagerXI2 *self)
{
  self->devices_by_id = g_hash_table_new_full (NULL, NULL,
                                               NULL,
                                               (GDestroyNotify) g_object_unref);
  self->tools_by_serial = g_hash_table_new_full (NULL, NULL, NULL,
                                                 (GDestroyNotify) g_object_unref);
}
