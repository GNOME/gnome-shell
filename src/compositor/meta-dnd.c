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

struct _MetaDnd
{
  GObject parent;
};

G_DEFINE_TYPE (MetaDnd, meta_dnd, G_TYPE_OBJECT);

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
