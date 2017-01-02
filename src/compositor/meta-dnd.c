/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2016 Hyungwon Hwang
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <gdk/gdkx.h>

#include "clutter/x11/clutter-x11.h"
#include "meta/meta-backend.h"
#include "compositor/compositor-private.h"
#include "core/display-private.h"
#include "backends/meta-dnd-private.h"
#include "meta/meta-dnd.h"

struct _MetaDndClass
{
  GObjectClass parent_class;
};

#ifdef HAVE_WAYLAND
#include "wayland/meta-wayland-private.h"
#include "wayland/meta-wayland-data-device.h"
#endif

typedef struct _MetaDndPrivate MetaDndPrivate;

struct _MetaDndPrivate
{
#ifdef HAVE_WAYLAND
  gulong handler_id[3];

  MetaCompositor *compositor;
  MetaWaylandCompositor *wl_compositor;
#else
  /* to avoid warnings (g_type_class_add_private: assertion `private_size > 0' failed) */
  gchar dummy;
#endif
};

struct _MetaDnd
{
  GObject parent;

  MetaDndPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (MetaDnd, meta_dnd, G_TYPE_OBJECT);

enum
{
  ENTER,
  POSITION_CHANGE,
  LEAVE,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
meta_dnd_class_init (MetaDndClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  signals[ENTER] =
    g_signal_new ("dnd-enter",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[POSITION_CHANGE] =
    g_signal_new ("dnd-position-change",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

  signals[LEAVE] =
    g_signal_new ("dnd-leave",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
meta_dnd_init (MetaDnd *dnd)
{
}

static void
meta_dnd_notify_dnd_enter (MetaDnd *dnd)
{
  g_signal_emit (dnd, signals[ENTER], 0);
}

static void
meta_dnd_notify_dnd_position_change (MetaDnd *dnd,
                                      int      x,
                                      int      y)
{
  g_signal_emit (dnd, signals[POSITION_CHANGE], 0, x, y);
}

static void
meta_dnd_notify_dnd_leave (MetaDnd *dnd)
{
  g_signal_emit (dnd, signals[LEAVE], 0);
}

/*
 * Process Xdnd events
 *
 * We pass the position and leave events to the plugin via a signal
 * where the actual drag & drop handling happens.
 *
 * http://www.freedesktop.org/wiki/Specifications/XDND
 */
gboolean
meta_dnd_handle_xdnd_event (MetaBackend    *backend,
                            MetaCompositor *compositor,
                            MetaDisplay    *display,
                            XEvent         *xev)
{
  MetaDnd *dnd = meta_backend_get_dnd (backend);
  Window output_window = compositor->output;

  if (xev->xany.type != ClientMessage)
    return FALSE;

  if (xev->xany.window != output_window &&
      xev->xany.window != clutter_x11_get_stage_window (CLUTTER_STAGE (compositor->stage)))
    return FALSE;

  if (xev->xclient.message_type == gdk_x11_get_xatom_by_name ("XdndPosition"))
    {
      XEvent xevent;
      Window src = xev->xclient.data.l[0];

      memset (&xevent, 0, sizeof(xevent));
      xevent.xany.type = ClientMessage;
      xevent.xany.display = display->xdisplay;
      xevent.xclient.window = src;
      xevent.xclient.message_type = gdk_x11_get_xatom_by_name ("XdndStatus");
      xevent.xclient.format = 32;
      xevent.xclient.data.l[0] = output_window;
      /* flags: bit 0: will we accept the drop? bit 1: do we want more position messages */
      xevent.xclient.data.l[1] = 2;
      xevent.xclient.data.l[4] = None;

      XSendEvent (display->xdisplay, src, False, 0, &xevent);

      meta_dnd_notify_dnd_position_change (dnd,
                                            (int)(xev->xclient.data.l[2] >> 16),
                                            (int)(xev->xclient.data.l[2] & 0xFFFF));

      return TRUE;
    }
  else if (xev->xclient.message_type == gdk_x11_get_xatom_by_name ("XdndLeave"))
    {
      meta_dnd_notify_dnd_leave (dnd);

      return TRUE;
    }
  else if (xev->xclient.message_type == gdk_x11_get_xatom_by_name ("XdndEnter"))
    {
      meta_dnd_notify_dnd_enter (dnd);

      return TRUE;
    }

    return FALSE;
}

#ifdef HAVE_WAYLAND
static void
meta_dnd_wayland_on_motion_event (ClutterActor *actor,
                                  ClutterEvent *event,
                                  MetaDnd      *dnd)
{
  MetaDndPrivate *priv = meta_dnd_get_instance_private (dnd);
  MetaWaylandDragGrab *current_grab;
  gfloat event_x, event_y;

  g_return_if_fail (event != NULL);

  clutter_event_get_coords (event, &event_x, &event_y);
  meta_dnd_notify_dnd_position_change (dnd, (int)event_x, (int)event_y);

  current_grab = meta_wayland_data_device_get_current_grab (&priv->wl_compositor->seat->data_device);
  if (current_grab)
    meta_wayland_drag_grab_update_feedback_actor (current_grab, event);
}

static void
meta_dnd_wayland_end_notify (ClutterActor *actor,
                             ClutterEvent *event,
                             MetaDnd      *dnd)
{
  MetaDndPrivate *priv = meta_dnd_get_instance_private (dnd);
  unsigned int i;

  meta_wayland_data_device_end_drag (&priv->wl_compositor->seat->data_device);

  for (i = 0; i < G_N_ELEMENTS (priv->handler_id); i++)
    {
      g_signal_handler_disconnect (priv->compositor->stage, priv->handler_id[i]);
      priv->handler_id[i] = 0;
    }

  priv->compositor = NULL;
  priv->wl_compositor = NULL;

  meta_dnd_notify_dnd_leave (dnd);
}

static void
meta_dnd_wayland_on_button_released (ClutterActor *actor,
                                     ClutterEvent *event,
                                     MetaDnd      *dnd)
{
  meta_dnd_wayland_end_notify (actor, event, dnd);
}

static void
meta_dnd_wayland_on_key_pressed (ClutterActor *actor,
                                 ClutterEvent *event,
                                 MetaDnd      *dnd)
{
  guint key = clutter_event_get_key_symbol (event);

  if (key != CLUTTER_KEY_Escape)
    return;

  meta_dnd_wayland_end_notify (actor, event, dnd);
}

void
meta_dnd_wayland_handle_begin_modal (MetaCompositor *compositor)
{
  MetaWaylandCompositor *wl_compositor = meta_wayland_compositor_get_default ();
  MetaDnd *dnd = meta_backend_get_dnd (meta_get_backend ());
  MetaDndPrivate *priv = meta_dnd_get_instance_private (dnd);

  if (priv->handler_id[0] == 0 &&
      meta_wayland_data_device_get_current_grab (&wl_compositor->seat->data_device) != NULL)
    {
      priv->compositor = compositor;
      priv->wl_compositor = wl_compositor;

      priv->handler_id[0] = g_signal_connect (compositor->stage,
                                              "motion-event",
                                              G_CALLBACK (meta_dnd_wayland_on_motion_event),
                                              dnd);

      priv->handler_id[1] = g_signal_connect (compositor->stage,
                                              "button-release-event",
                                              G_CALLBACK (meta_dnd_wayland_on_button_released),
                                              dnd);

      priv->handler_id[2] = g_signal_connect (compositor->stage,
                                              "key-press-event",
                                              G_CALLBACK (meta_dnd_wayland_on_key_pressed),
                                              dnd);

      meta_dnd_notify_dnd_enter (dnd);
    }
}
#endif
