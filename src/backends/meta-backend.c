/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "config.h"

#include "meta-backend.h"
#include <meta/main.h>

#include <gdk/gdkx.h>
#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>

#include "backends/native/meta-weston-launch.h"
#include <meta/util.h>

/* Mutter is responsible for pulling events off the X queue, so Clutter
 * doesn't need (and shouldn't) run its normal event source which polls
 * the X fd, but we do have to deal with dispatching events that accumulate
 * in the clutter queue. This happens, for example, when clutter generate
 * enter/leave events on mouse motion - several events are queued in the
 * clutter queue but only one dispatched. It could also happen because of
 * explicit calls to clutter_event_put(). We add a very simple custom
 * event loop source which is simply responsible for pulling events off
 * of the queue and dispatching them before we block for new events.
 */

static gboolean
event_prepare (GSource    *source,
               gint       *timeout_)
{
  *timeout_ = -1;

  return clutter_events_pending ();
}

static gboolean
event_check (GSource *source)
{
  return clutter_events_pending ();
}

static gboolean
event_dispatch (GSource    *source,
                GSourceFunc callback,
                gpointer    user_data)
{
  ClutterEvent *event = clutter_event_get ();

  if (event)
    {
      clutter_do_event (event);
      clutter_event_free (event);
    }

  return TRUE;
}

static GSourceFuncs event_funcs = {
  event_prepare,
  event_check,
  event_dispatch
};

static MetaLauncher *launcher;

void
meta_clutter_init (void)
{
  GSource *source;

  /* When running as an X11 compositor, we install our own event filter and
   * pass events to Clutter explicitly, so we need to prevent Clutter from
   * handling our events.
   *
   * However, when running as a Wayland compostior under X11 nested, Clutter
   * Clutter needs to see events related to its own window. We need to
   * eventually replace this with a proper frontend / backend split: Clutter
   * under nested is connecting to the "host X server" to get its events it
   * needs to put up a window, and GTK+ is connecting to the "inner X server".
   * The two would the same in the X11 compositor case, but not when running
   * XWayland as a Wayland compositor.
   */
  if (!meta_is_wayland_compositor ())
    {
      clutter_x11_set_display (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
      clutter_x11_disable_event_retrieval ();
    }

  /* If we're running on bare metal, we're a display server,
   * so start talking to weston-launch. */
#if defined(CLUTTER_WINDOWING_EGL)
  if (clutter_check_windowing_backend (CLUTTER_WINDOWING_EGL))
    launcher = meta_launcher_new ();
#endif

  if (clutter_init (NULL, NULL) != CLUTTER_INIT_SUCCESS)
    g_error ("Unable to initialize Clutter.\n");

  source = g_source_new (&event_funcs, sizeof (GSource));
  g_source_attach (source, NULL);
  g_source_unref (source);
}

gboolean
meta_activate_vt (int vt, GError **error)
{
  if (launcher)
    return meta_launcher_activate_vt (launcher, vt, error);
  else
    {
      g_debug ("Ignoring VT switch keybinding, not running as display server");
      return TRUE;
    }
}

/**
 * meta_activate_session:
 *
 * Tells mutter to activate the session. When mutter is a
 * Wayland compositor, this tells logind to switch over to
 * the new session.
 */
gboolean
meta_activate_session (void)
{
  GError *error = NULL;

  if (!meta_launcher_activate_vt (launcher, -1, &error))
    {
      g_warning ("Could not activate session: %s\n", error->message);
      g_error_free (error);
      return FALSE;
    }

  return TRUE;
}
