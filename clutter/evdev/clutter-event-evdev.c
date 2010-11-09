/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2006-2007 OpenedHand
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
 * Author: Damien Lespiau <damien.lespiau@intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <linux/input.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <gudev/gudev.h>

#include "clutter-backend.h"
#include "clutter-debug.h"
#include "clutter-device-manager.h"
#include "clutter-event.h"
#include "clutter-input-device-evdev.h"
#include "clutter-main.h"
#include "clutter-private.h"
#include "clutter-xkb-utils.h"

/* List of all the event sources */
static GSList *event_sources = NULL;

const char *option_xkb_layout = "us";
const char *option_xkb_variant = "";
const char *option_xkb_options = "";

/*
 * ClutterEventSource for reading input devices
 */

typedef struct _ClutterEventSource  ClutterEventSource;

struct _ClutterEventSource
{
  GSource source;

  ClutterInputDeviceEvdev *device;    /* back pointer to the evdev device */
  GPollFD event_poll_fd;              /* file descriptor of the /dev node */
  struct xkb_desc *xkb;               /* compiled xkb keymap */
  uint32_t modifier_state;            /* remember the modifier state */
};

static gboolean
clutter_event_prepare (GSource *source,
                       gint    *timeout)
{
  gboolean retval;

  clutter_threads_enter ();

  *timeout = -1;
  retval = clutter_events_pending ();

  clutter_threads_leave ();

  return retval;
}

static gboolean
clutter_event_check (GSource *source)
{
  ClutterEventSource *event_source = (ClutterEventSource *) source;
  gboolean retval;

  clutter_threads_enter ();

  retval = ((event_source->event_poll_fd.revents & G_IO_IN) ||
            clutter_events_pending ());

  clutter_threads_leave ();

  return retval;
}

static gboolean
clutter_event_dispatch (GSource     *g_source,
                        GSourceFunc  callback,
                        gpointer     user_data)
{
  ClutterEventSource *source = (ClutterEventSource *) g_source;
  ClutterInputDevice *input_device = (ClutterInputDevice *) source->device;
  ClutterMainContext *clutter_context;
  struct input_event ev[8];
  ClutterEvent *event = NULL;
  ClutterStage *stage;
  gint len, i;

  clutter_threads_enter ();

  clutter_context = _clutter_context_get_default ();
  stage = CLUTTER_STAGE (clutter_stage_get_default ());

  /* Don't queue more events if we haven't finished handling the previous batch
   */
  if (!clutter_events_pending ())
    {
       len = read (source->event_poll_fd.fd, &ev, sizeof (ev));
       if (len < 0 || len % sizeof (ev[0]) != 0)
       {
         if (errno != EAGAIN)
           {
             g_warning ("Could not read device (%d)", errno);
           }
         goto out;
       }

       for (i = 0; i < len / sizeof (ev[0]); i++)
         {
           struct input_event *e = &ev[i];
           uint32_t _time;

           _time = e->time.tv_sec * 1000 + e->time.tv_usec / 1000;

           switch (e->type)
             {
             case EV_KEY:

               /* don't repeat mouse buttons */
               if (e->code >= BTN_MOUSE && e->code < KEY_OK)
                 if (e->value == 2)
                   continue;

               event =
                 _clutter_key_event_new_from_evdev (input_device,
                                                    stage,
                                                    source->xkb,
                                                    _time, e->code, e->value,
                                                    &source->modifier_state);

               break;
             case EV_SYN:
               /* Nothing to do here? */
               break;
             case EV_MSC:
               /* Nothing to do here? */
               break;
             case EV_ABS:
             case EV_REL:
             default:
               g_warning ("Unhandled event of type %d", e->type);
               break;
             }

           if (event)
             {
               g_queue_push_head (clutter_context->events_queue, event);
               event = NULL;
             }
         }
    }

  /* Pop an event off the queue if any */
  event = clutter_event_get ();

  if (event)
    {
      /* forward the event into clutter for emission etc. */
      clutter_do_event (event);
      clutter_event_free (event);
    }

out:
  clutter_threads_leave ();

  return TRUE;
}
static GSourceFuncs event_funcs = {
  clutter_event_prepare,
  clutter_event_check,
  clutter_event_dispatch,
  NULL
};

static GSource *
clutter_event_source_new (ClutterInputDeviceEvdev *input_device)
{
  GSource *source = g_source_new (&event_funcs, sizeof (ClutterEventSource));
  ClutterEventSource *event_source = (ClutterEventSource *) source;
  const gchar *node_path;
  gint fd;

  /* grab the udev input device node and open it */
  node_path = _clutter_input_device_evdev_get_device_path (input_device);

  CLUTTER_NOTE (EVENT, "Creating GSource for device %s", node_path);

  fd = open (node_path, O_RDONLY | O_NONBLOCK);
  if (fd < 0)
    {
      g_warning ("Could not open device %s: %s", node_path, strerror (errno));
      return NULL;
    }

  /* setup the source */
  event_source->device = input_device;
  event_source->event_poll_fd.fd = fd;
  event_source->event_poll_fd.events = G_IO_IN;

  /* create the xkb description */
  event_source->xkb = _clutter_xkb_desc_new (NULL,
                                             option_xkb_layout,
                                             option_xkb_variant,
                                             option_xkb_options);
  if (G_UNLIKELY (event_source->xkb == NULL))
    {
      g_warning ("Could not compile keymap %s:%s:%s", option_xkb_layout,
                 option_xkb_variant, option_xkb_options);
      close (fd);
      g_source_unref (source);
      return NULL;
    }

  /* and finally configure and attach the GSource */
  g_source_set_priority (source, CLUTTER_PRIORITY_EVENTS);
  g_source_add_poll (source, &event_source->event_poll_fd);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

  return source;
}

static void
clutter_event_source_free (ClutterEventSource *source)
{
  GSource *g_source = (GSource *) source;
  const gchar *node_path;

  node_path = _clutter_input_device_evdev_get_device_path (source->device);

  CLUTTER_NOTE (EVENT, "Removing GSource for device %s", node_path);

  /* ignore the return value of close, it's not like we can do something
   * about it */
  close (source->event_poll_fd.fd);

  g_source_destroy (g_source);
  g_source_unref (g_source);
}

static void
_clutter_event_evdev_add_source (ClutterInputDeviceEvdev *input_device)
{
  GSource *source;

  source = clutter_event_source_new (input_device);
  if (G_LIKELY (source))
    event_sources = g_slist_prepend (event_sources, source);
}

static void
_clutter_event_evdev_remove_source (ClutterEventSource *source)
{
  clutter_event_source_free (source);
  event_sources = g_slist_remove (event_sources, source);
}

static void
on_device_added (ClutterDeviceManager *manager,
                 ClutterInputDevice   *device,
                 gpointer              data)
{
  ClutterInputDeviceEvdev *input_device = CLUTTER_INPUT_DEVICE_EVDEV (device);

  _clutter_event_evdev_add_source (input_device);
}

static ClutterEventSource *
find_source_by_device (ClutterInputDevice *device)
{
  GSList *l;

  for (l = event_sources; l; l = g_slist_next (l))
    {
      ClutterEventSource *source = l->data;

      if (source->device == (ClutterInputDeviceEvdev *) device)
        return source;
    }

  return NULL;
}

static void
on_device_removed (ClutterDeviceManager *manager,
                   ClutterInputDevice   *device,
                   gpointer              data)
{
  ClutterEventSource *source;

  source = find_source_by_device (device);
  if (G_UNLIKELY (source == NULL))
    {
      g_warning ("Trying to remove a device without a source installed ?!");
      return;
    }

  _clutter_event_evdev_remove_source (source);
}

void
_clutter_events_evdev_init (ClutterBackend *backend)
{
  ClutterDeviceManager *device_manager;
  GSList *devices, *l;

  CLUTTER_NOTE (EVENT, "Initializing evdev backend");

  /* Of course, we assume that the singleton will be a
   * ClutterDeviceManagerEvdev */
  device_manager = clutter_device_manager_get_default ();

  devices = clutter_device_manager_list_devices (device_manager);
  for (l = devices; l; l = g_slist_next (l))
    {
      ClutterInputDeviceEvdev *input_device = l->data;

      _clutter_event_evdev_add_source (input_device);
    }

  /* make sure to add/remove sources when devices are added/removed */
  g_signal_connect (device_manager, "device-added",
                    G_CALLBACK (on_device_added), NULL);
  g_signal_connect (device_manager, "device-removed",
                    G_CALLBACK (on_device_removed), NULL);
}

void
_clutter_events_evdev_uninit (ClutterBackend *backend)
{
  GSList *l;

  for (l = event_sources; l; l = g_slist_next (l))
    {
      ClutterEventSource *source = l->data;

      clutter_event_source_free (source);
    }
  g_slist_free (event_sources);
}
