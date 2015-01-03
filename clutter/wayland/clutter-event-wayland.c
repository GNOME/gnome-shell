/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2010  Intel Corporation.
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

 * Authors:
 *  Matthew Allum
 *  Robert Bragg
 *  Kristian Høgsberg
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <stdlib.h>
#include <wayland-client.h>

#include "clutter-event.h"
#include "clutter-main.h"
#include "clutter-private.h"

#include "clutter-event-wayland.h"

typedef struct _ClutterEventSourceWayland
{
  GSource source;
  GPollFD pfd;
  struct wl_display *display;
} ClutterEventSourceWayland;

static gboolean
clutter_event_source_wayland_prepare (GSource *base, gint *timeout)
{
  ClutterEventSourceWayland *source = (ClutterEventSourceWayland *) base;
  gboolean retval;

  _clutter_threads_acquire_lock ();

  *timeout = -1;

  /* We have to add/remove the GPollFD if we want to update our
   * poll event mask dynamically.  Instead, let's just flush all
   * writes on idle */
  wl_display_flush (source->display);

  retval = clutter_events_pending ();

  _clutter_threads_release_lock ();

  return retval;
}

static gboolean
clutter_event_source_wayland_check (GSource *base)
{
  ClutterEventSourceWayland *source = (ClutterEventSourceWayland *) base;
  gboolean retval;

  _clutter_threads_acquire_lock ();

  retval = clutter_events_pending () || source->pfd.revents;

  _clutter_threads_release_lock ();

  return retval;
}

static gboolean
clutter_event_source_wayland_dispatch (GSource *base,
				       GSourceFunc callback,
				       gpointer data)
{
  ClutterEventSourceWayland *source = (ClutterEventSourceWayland *) base;
  ClutterEvent *event;

  _clutter_threads_acquire_lock ();

  if (source->pfd.revents)
    {
      wl_display_dispatch (source->display);
      source->pfd.revents = 0;
    }

  event = clutter_event_get ();

  if (event)
    {
      /* forward the event into clutter for emission etc. */
      clutter_do_event (event);
      clutter_event_free (event);
    }

  _clutter_threads_release_lock ();

  return TRUE;
}

static GSourceFuncs clutter_event_source_wayland_funcs = {
    clutter_event_source_wayland_prepare,
    clutter_event_source_wayland_check,
    clutter_event_source_wayland_dispatch,
    NULL
};


GSource *
_clutter_event_source_wayland_new (struct wl_display *display)
{
  ClutterEventSourceWayland *source;

  source = (ClutterEventSourceWayland *)
    g_source_new (&clutter_event_source_wayland_funcs,
                  sizeof (ClutterEventSourceWayland));
  source->display = display;
  source->pfd.fd =
    wl_display_get_fd (display);
  source->pfd.events = G_IO_IN | G_IO_ERR;
  g_source_add_poll (&source->source, &source->pfd);

  return &source->source;
}
