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

#include "meta-window-wayland.h"

#include <meta/errors.h>
#include <errno.h>
#include <string.h> /* for strerror () */
#include "window-private.h"
#include "boxes-private.h"
#include "stack-tracker.h"
#include "meta-wayland-private.h"
#include "meta-wayland-surface.h"
#include "meta-wayland-xdg-shell.h"
#include "backends/meta-backend-private.h"
#include "backends/meta-logical-monitor.h"
#include "compositor/meta-surface-actor-wayland.h"
#include "backends/meta-backend-private.h"

struct _MetaWindowWayland
{
  MetaWindow parent;

  int geometry_scale;

  MetaWaylandSerial pending_configure_serial;
  gboolean has_pending_move;
  int pending_move_x;
  int pending_move_y;

  int last_sent_x;
  int last_sent_y;
  int last_sent_width;
  int last_sent_height;
};

struct _MetaWindowWaylandClass
{
  MetaWindowClass parent_class;
};

G_DEFINE_TYPE (MetaWindowWayland, meta_window_wayland, META_TYPE_WINDOW)

static int
get_window_geometry_scale_for_logical_monitor (MetaLogicalMonitor *logical_monitor)
{
  if (meta_is_stage_views_scaled ())
    return 1;
  else
    return meta_logical_monitor_get_scale (logical_monitor);
}

static void
meta_window_wayland_manage (MetaWindow *window)
{
  MetaWindowWayland *wl_window = META_WINDOW_WAYLAND (window);
  MetaDisplay *display = window->display;

  wl_window->geometry_scale =
    get_window_geometry_scale_for_logical_monitor (window->monitor);

  meta_display_register_wayland_window (display, window);

  {
    meta_stack_tracker_record_add (window->screen->stack_tracker,
                                   window->stamp,
                                   0);
  }

  meta_wayland_surface_window_managed (window->surface, window);
}

static void
meta_window_wayland_unmanage (MetaWindow *window)
{
  {
    meta_stack_tracker_record_remove (window->screen->stack_tracker,
                                      window->stamp,
                                      0);
  }

  meta_display_unregister_wayland_window (window->display, window);
}

static void
meta_window_wayland_ping (MetaWindow *window,
                          guint32     serial)
{
  meta_wayland_surface_ping (window->surface, serial);
}

static void
meta_window_wayland_delete (MetaWindow *window,
                            guint32     timestamp)
{
  meta_wayland_surface_delete (window->surface);
}

static void
meta_window_wayland_kill (MetaWindow *window)
{
  MetaWaylandSurface *surface = window->surface;
  struct wl_resource *resource = surface->resource;

  /* Send the client an unrecoverable error to kill the client. */
  wl_resource_post_error (resource,
                          WL_DISPLAY_ERROR_NO_MEMORY,
                          "User requested that we kill you. Sorry. Don't take it too personally.");
}

static void
meta_window_wayland_focus (MetaWindow *window,
                           guint32     timestamp)
{
  if (window->input)
    meta_display_set_input_focus_window (window->display,
                                         window,
                                         FALSE,
                                         timestamp);
}

static void
surface_state_changed (MetaWindow *window)
{
  MetaWindowWayland *wl_window = META_WINDOW_WAYLAND (window);

  /* don't send notify when the window is being unmanaged */
  if (window->unmanaging)
    return;

  meta_wayland_surface_configure_notify (window->surface,
                                         wl_window->last_sent_x,
                                         wl_window->last_sent_y,
                                         wl_window->last_sent_width,
                                         wl_window->last_sent_height,
                                         &wl_window->pending_configure_serial);
}

static void
meta_window_wayland_grab_op_began (MetaWindow *window,
                                   MetaGrabOp  op)
{
  if (meta_grab_op_is_resizing (op))
    surface_state_changed (window);

  META_WINDOW_CLASS (meta_window_wayland_parent_class)->grab_op_began (window, op);
}

static void
meta_window_wayland_grab_op_ended (MetaWindow *window,
                                   MetaGrabOp  op)
{
  if (meta_grab_op_is_resizing (op))
    surface_state_changed (window);

  META_WINDOW_CLASS (meta_window_wayland_parent_class)->grab_op_ended (window, op);
}

static void
meta_window_wayland_move_resize_internal (MetaWindow                *window,
                                          int                        gravity,
                                          MetaRectangle              unconstrained_rect,
                                          MetaRectangle              constrained_rect,
                                          MetaMoveResizeFlags        flags,
                                          MetaMoveResizeResultFlags *result)
{
  MetaWindowWayland *wl_window = META_WINDOW_WAYLAND (window);
  gboolean can_move_now;
  int configured_x;
  int configured_y;
  int configured_width;
  int configured_height;
  int geometry_scale;

  g_assert (window->frame == NULL);

  /* don't do anything if we're dropping the window, see #751847 */
  if (window->unmanaging)
    return;

  configured_x = constrained_rect.x;
  configured_y = constrained_rect.y;

  /* The scale the window is drawn in might change depending on what monitor it
   * is mainly on. Scale the configured rectangle to be in logical pixel
   * coordinate space so that we can have a scale independent size to pass
   * to the Wayland surface. */
  geometry_scale = meta_window_wayland_get_geometry_scale (window);
  configured_width = constrained_rect.width / geometry_scale;
  configured_height = constrained_rect.height / geometry_scale;

  /* For wayland clients, the size is completely determined by the client,
   * and while this allows to avoid some trickery with frames and the resulting
   * lagging, we also need to insist a bit when the constraints would apply
   * a different size than the client decides.
   *
   * Note that this is not generally a problem for normal toplevel windows (the
   * constraints don't see the size hints, or just change the position), but
   * it can be for maximized or fullscreen.
   */

  if (flags & META_MOVE_RESIZE_WAYLAND_RESIZE)
    {
      /* This is a call to wl_surface_commit(), ignore the constrained_rect and
       * update the real client size to match the buffer size.
       */

      if (window->rect.width != unconstrained_rect.width ||
          window->rect.height != unconstrained_rect.height)
        {
          *result |= META_MOVE_RESIZE_RESULT_RESIZED;
          window->rect.width = unconstrained_rect.width;
          window->rect.height = unconstrained_rect.height;
        }

      /* This is a commit of an attach. We should move the window to match the
       * new position the client wants. */
      can_move_now = TRUE;
    }
  else
    {
      /* If the size changed, or the state changed, then we have to wait until
       * the client acks our configure before moving the window. */
      if (constrained_rect.width != window->rect.width ||
          constrained_rect.height != window->rect.height ||
          (flags & META_MOVE_RESIZE_STATE_CHANGED))
        {
          /* If the constrained size is 1x1 and the unconstrained size is 0x0
           * it means that we are trying to resize a window where the client has
           * not yet committed a buffer. The 1x1 constrained size is a result of
           * how the constraints code works. Lets avoid trying to have the
           * client configure itself to draw on a 1x1 surface.
           *
           * We cannot guard against only an empty unconstrained_rect here,
           * because the client may have created a xdg surface without a buffer
           * attached and asked it to be maximized. In such case we should let
           * it know about the expected window geometry of a maximized window,
           * even though there is currently no buffer attached. */
          if (unconstrained_rect.width == 0 &&
              unconstrained_rect.height == 0 &&
              constrained_rect.width == 1 &&
              constrained_rect.height == 1)
            return;

          meta_wayland_surface_configure_notify (window->surface,
                                                 configured_x,
                                                 configured_y,
                                                 configured_width,
                                                 configured_height,
                                                 &wl_window->pending_configure_serial);

          /* We need to wait until the resize completes before we can move */
          can_move_now = FALSE;
        }
      else
        {
          /* We're just moving the window, so we don't need to wait for a configure
           * and then ack to simply move the window. */
          can_move_now = TRUE;
        }
    }

  wl_window->last_sent_x = configured_x;
  wl_window->last_sent_y = configured_y;
  wl_window->last_sent_width = configured_width;
  wl_window->last_sent_height = configured_height;

  if (can_move_now)
    {
      int new_x = constrained_rect.x;
      int new_y = constrained_rect.y;

      if (new_x != window->rect.x || new_y != window->rect.y)
        {
          *result |= META_MOVE_RESIZE_RESULT_MOVED;
          window->rect.x = new_x;
          window->rect.y = new_y;
        }

      int new_buffer_x = new_x - window->custom_frame_extents.left;
      int new_buffer_y = new_y - window->custom_frame_extents.top;

      if (new_buffer_x != window->buffer_rect.x || new_buffer_y != window->buffer_rect.y)
        {
          *result |= META_MOVE_RESIZE_RESULT_MOVED;
          window->buffer_rect.x = new_buffer_x;
          window->buffer_rect.y = new_buffer_y;
        }
    }
  else
    {
      int new_x = constrained_rect.x;
      int new_y = constrained_rect.y;

      if (new_x != window->rect.x || new_y != window->rect.y)
        {
          wl_window->has_pending_move = TRUE;
          wl_window->pending_move_x = new_x;
          wl_window->pending_move_y = new_y;
        }
    }
}

static void
scale_size (int  *width,
            int  *height,
            float scale)
{
  if (*width < G_MAXINT)
    {
      float new_width = (*width * scale);
      *width = (int) MIN (new_width, G_MAXINT);
    }

  if (*height < G_MAXINT)
    {
      float new_height = (*height * scale);
      *height = (int) MIN (new_height, G_MAXINT);
    }
}

static void
scale_rect_size (MetaRectangle *rect,
                 float          scale)
{
  scale_size (&rect->width, &rect->height, scale);
}

static void
meta_window_wayland_update_main_monitor (MetaWindow *window,
                                         gboolean    user_op)
{
  MetaBackend *backend = meta_get_backend ();
  MetaMonitorManager *monitor_manager =
    meta_backend_get_monitor_manager (backend);
  MetaWindow *toplevel_window;
  MetaLogicalMonitor *from;
  MetaLogicalMonitor *to;
  MetaLogicalMonitor *scaled_new;
  float from_scale, to_scale;
  float scale;
  MetaRectangle rect;

  from = window->monitor;

  /* If the window is not a toplevel window (i.e. it's a popup window) just use
   * the monitor of the toplevel. */
  toplevel_window = meta_wayland_surface_get_toplevel_window (window->surface);
  if (toplevel_window != window)
    {
      meta_window_update_monitor (toplevel_window, user_op);
      window->monitor = toplevel_window->monitor;
      return;
    }

  /* Require both the current and the new monitor would be the new main monitor,
   * even given the resulting scale the window would end up having. This is
   * needed to avoid jumping back and forth between the new and the old, since
   * changing main monitor may cause the window to be resized so that it no
   * longer have that same new main monitor. */
  to = meta_window_calculate_main_logical_monitor (window);

  if (from == to)
    return;

  if (from == NULL || to == NULL)
    {
      window->monitor = to;
      return;
    }

  from_scale = meta_logical_monitor_get_scale (from);
  to_scale = meta_logical_monitor_get_scale (to);

  if (from_scale == to_scale)
    {
      window->monitor = to;
      return;
    }

  if (meta_is_stage_views_scaled ())
    {
      window->monitor = to;
      return;
    }

  /* To avoid a window alternating between two main monitors because scaling
   * changes the main monitor, wait until both the current and the new scale
   * will result in the same main monitor. */
  scale = to_scale / from_scale;
  rect = window->rect;
  scale_rect_size (&rect, scale);
  scaled_new =
    meta_monitor_manager_get_logical_monitor_from_rect (monitor_manager, &rect);
  if (to != scaled_new)
    return;

  window->monitor = to;
}

static void
meta_window_wayland_main_monitor_changed (MetaWindow               *window,
                                          const MetaLogicalMonitor *old)
{
  MetaWindowWayland *wl_window = META_WINDOW_WAYLAND (window);
  int old_geometry_scale = wl_window->geometry_scale;
  int geometry_scale;
  float scale_factor;
  MetaWaylandSurface *surface;

  if (!window->monitor)
    return;

  geometry_scale = meta_window_wayland_get_geometry_scale (window);

  /* This function makes sure that window geometry, window actor geometry and
   * surface actor geometry gets set according the old and current main monitor
   * scale. If there either is no past or current main monitor, or if the scale
   * didn't change, there is nothing to do. */
  if (old == NULL ||
      window->monitor == NULL ||
      old_geometry_scale == geometry_scale)
    return;

  /* MetaWindow keeps its rectangles in the physical pixel coordinate space.
   * When the main monitor of a window changes, it can cause the corresponding
   * window surfaces to be scaled given the monitor scale, so we need to scale
   * the rectangles in MetaWindow accordingly. */

  scale_factor = (float) geometry_scale / old_geometry_scale;

  /* Window size. */
  scale_rect_size (&window->rect, scale_factor);
  scale_rect_size (&window->unconstrained_rect, scale_factor);
  scale_rect_size (&window->saved_rect, scale_factor);
  scale_size (&window->size_hints.min_width, &window->size_hints.min_height, scale_factor);
  scale_size (&window->size_hints.max_width, &window->size_hints.max_height, scale_factor);

  /* Window geometry offset (XXX: Need a better place, see
   * meta_window_wayland_move_resize). */
  window->custom_frame_extents.left =
    (int)(scale_factor * window->custom_frame_extents.left);
  window->custom_frame_extents.top =
    (int)(scale_factor * window->custom_frame_extents.top);

  /* Buffer rect. */
  scale_rect_size (&window->buffer_rect, scale_factor);
  window->buffer_rect.x =
    window->rect.x - window->custom_frame_extents.left;
  window->buffer_rect.y =
    window->rect.y - window->custom_frame_extents.top;

  meta_compositor_sync_window_geometry (window->display->compositor,
                                        window,
                                        TRUE);

  /* The surface actor needs to update the scale recursively for itself and all
   * its subsurfaces */
  surface = window->surface;
  if (surface)
    {
      MetaSurfaceActorWayland *actor =
        META_SURFACE_ACTOR_WAYLAND (surface->surface_actor);

      meta_surface_actor_wayland_sync_state_recursive (actor);
    }

  wl_window->geometry_scale = geometry_scale;

  meta_window_emit_size_changed (window);
}

static uint32_t
meta_window_wayland_get_client_pid (MetaWindow *window)
{
  MetaWaylandSurface *surface = window->surface;
  struct wl_resource *resource = surface->resource;
  pid_t pid;

  wl_client_get_credentials (wl_resource_get_client (resource), &pid, NULL, NULL);
  return (uint32_t)pid;
}

static void
appears_focused_changed (GObject    *object,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
  MetaWindow *window = META_WINDOW (object);
  surface_state_changed (window);
}

static void
meta_window_wayland_init (MetaWindowWayland *wl_window)
{
  MetaWindow *window = META_WINDOW (wl_window);

  wl_window->geometry_scale = 1;

  g_signal_connect (window, "notify::appears-focused",
                    G_CALLBACK (appears_focused_changed), NULL);
}

static void
meta_window_wayland_force_restore_shortcuts (MetaWindow         *window,
                                             ClutterInputDevice *source)
{
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();

  meta_wayland_compositor_restore_shortcuts (compositor, source);
}

static gboolean
meta_window_wayland_shortcuts_inhibited (MetaWindow         *window,
                                         ClutterInputDevice *source)
{
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();

  return meta_wayland_compositor_is_shortcuts_inhibited (compositor, source);
}

static void
meta_window_wayland_class_init (MetaWindowWaylandClass *klass)
{
  MetaWindowClass *window_class = META_WINDOW_CLASS (klass);

  window_class->manage = meta_window_wayland_manage;
  window_class->unmanage = meta_window_wayland_unmanage;
  window_class->ping = meta_window_wayland_ping;
  window_class->delete = meta_window_wayland_delete;
  window_class->kill = meta_window_wayland_kill;
  window_class->focus = meta_window_wayland_focus;
  window_class->grab_op_began = meta_window_wayland_grab_op_began;
  window_class->grab_op_ended = meta_window_wayland_grab_op_ended;
  window_class->move_resize_internal = meta_window_wayland_move_resize_internal;
  window_class->update_main_monitor = meta_window_wayland_update_main_monitor;
  window_class->main_monitor_changed = meta_window_wayland_main_monitor_changed;
  window_class->get_client_pid = meta_window_wayland_get_client_pid;
  window_class->force_restore_shortcuts = meta_window_wayland_force_restore_shortcuts;
  window_class->shortcuts_inhibited = meta_window_wayland_shortcuts_inhibited;
}

MetaWindow *
meta_window_wayland_new (MetaDisplay        *display,
                         MetaWaylandSurface *surface)
{
  XWindowAttributes attrs = { 0 };
  MetaScreen *scr = display->screen;
  MetaWindow *window;

  /*
   * Set attributes used by _meta_window_shared_new, don't bother trying to fake
   * X11 window attributes with the rest, since they'll be ignored anyway.
   */
  attrs.x = 0;
  attrs.y = 0;
  attrs.width = 0;
  attrs.height = 0;
  attrs.depth = 24;
  attrs.visual = NULL;
  attrs.map_state = IsUnmapped;
  attrs.override_redirect = False;

  /* XXX: Note: In the Wayland case we currently still trap X errors while
   * creating a MetaWindow because we will still be making various redundant
   * X requests (passing a window xid of None) until we thoroughly audit all
   * the code to make sure it knows about non X based clients...
   */
  meta_error_trap_push (display); /* Push a trap over all of window
                                   * creation, to reduce XSync() calls
                                   */

  window = _meta_window_shared_new (display,
                                    scr,
                                    META_WINDOW_CLIENT_TYPE_WAYLAND,
                                    surface,
                                    None,
                                    WithdrawnState,
                                    META_COMP_EFFECT_CREATE,
                                    &attrs);
  window->can_ping = TRUE;

  meta_error_trap_pop (display); /* pop the XSync()-reducing trap */

  return window;
}

static gboolean
should_do_pending_move (MetaWindowWayland *wl_window,
                        MetaWaylandSerial *acked_configure_serial)
{
  if (!wl_window->has_pending_move)
    return FALSE;

  if (wl_window->pending_configure_serial.set)
    {
      /* If we're waiting for a configure and this isn't an ACK for
       * any configure, then fizzle it out. */
      if (!acked_configure_serial->set)
        return FALSE;

      /* If we're waiting for a configure and this isn't an ACK for
       * the configure we're waiting for, then fizzle it out. */
      if (acked_configure_serial->value != wl_window->pending_configure_serial.value)
        return FALSE;
    }

  return TRUE;
}

int
meta_window_wayland_get_geometry_scale (MetaWindow *window)
{
  return get_window_geometry_scale_for_logical_monitor (window->monitor);
}

/**
 * meta_window_move_resize_wayland:
 *
 * Complete a resize operation from a wayland client.
 */
void
meta_window_wayland_move_resize (MetaWindow        *window,
                                 MetaWaylandSerial *acked_configure_serial,
                                 MetaRectangle      new_geom,
                                 int                dx,
                                 int                dy)
{
  MetaWindowWayland *wl_window = META_WINDOW_WAYLAND (window);
  int geometry_scale;
  int gravity;
  MetaRectangle rect;
  MetaMoveResizeFlags flags;

  /* new_geom is in the logical pixel coordinate space, but MetaWindow wants its
   * rects to represent what in turn will end up on the stage, i.e. we need to
   * scale new_geom to physical pixels given what buffer scale and texture scale
   * is in use. */

  geometry_scale = meta_window_wayland_get_geometry_scale (window);
  new_geom.x *= geometry_scale;
  new_geom.y *= geometry_scale;
  new_geom.width *= geometry_scale;
  new_geom.height *= geometry_scale;

  /* The (dx, dy) offset is also in logical pixel coordinate space and needs
   * to be scaled in the same way as new_geom. */
  dx *= geometry_scale;
  dy *= geometry_scale;

  /* XXX: Find a better place to store the window geometry offsets. */
  window->custom_frame_extents.left = new_geom.x;
  window->custom_frame_extents.top = new_geom.y;

  flags = META_MOVE_RESIZE_WAYLAND_RESIZE;

  /* x/y are ignored when we're doing interactive resizing */
  if (!meta_grab_op_is_resizing (window->display->grab_op))
    {
      if (wl_window->has_pending_move && should_do_pending_move (wl_window, acked_configure_serial))
        {
          rect.x = wl_window->pending_move_x;
          rect.y = wl_window->pending_move_y;
          wl_window->has_pending_move = FALSE;
          flags |= META_MOVE_RESIZE_MOVE_ACTION;
        }
      else
        {
          rect.x = window->rect.x;
          rect.y = window->rect.y;
        }

      if (dx != 0 || dy != 0)
        {
          rect.x += dx;
          rect.y += dy;
          flags |= META_MOVE_RESIZE_MOVE_ACTION;
        }
    }

  wl_window->pending_configure_serial.set = FALSE;

  rect.width = new_geom.width;
  rect.height = new_geom.height;

  if (rect.width != window->rect.width || rect.height != window->rect.height)
    flags |= META_MOVE_RESIZE_RESIZE_ACTION;

  gravity = meta_resize_gravity_from_grab_op (window->display->grab_op);
  meta_window_move_resize_internal (window, flags, gravity, rect);
}

void
meta_window_wayland_place_relative_to (MetaWindow *window,
                                       MetaWindow *other,
                                       int         x,
                                       int         y)
{
  int geometry_scale;

  /* If there is no monitor, we can't position the window reliably. */
  if (!other->monitor)
    return;

  geometry_scale = meta_window_wayland_get_geometry_scale (other);
  meta_window_move_frame (window, FALSE,
                          other->buffer_rect.x + (x * geometry_scale),
                          other->buffer_rect.y + (y * geometry_scale));
  window->placed = TRUE;
}

void
meta_window_place_with_placement_rule (MetaWindow        *window,
                                       MetaPlacementRule *placement_rule)
{
  g_clear_pointer (&window->placement_rule, g_free);
  window->placement_rule = g_new0 (MetaPlacementRule, 1);
  *window->placement_rule = *placement_rule;

  window->unconstrained_rect.width = placement_rule->width;
  window->unconstrained_rect.height = placement_rule->height;
  meta_window_force_placement (window);
}

void
meta_window_wayland_set_min_size (MetaWindow *window,
                                  int         width,
                                  int         height)
{
  gint64 new_width, new_height;
  float scale;

  meta_topic (META_DEBUG_GEOMETRY, "Window %s sets min size %d x %d\n",
              window->desc, width, height);

  if (width == 0 && height == 0)
    {
      window->size_hints.min_width = 0;
      window->size_hints.min_height = 0;
      window->size_hints.flags &= ~PMinSize;

      return;
    }

  scale = (float) meta_window_wayland_get_geometry_scale (window);
  scale_size (&width, &height, scale);

  new_width = width + (window->custom_frame_extents.left +
                       window->custom_frame_extents.right);
  new_height = height + (window->custom_frame_extents.top +
                         window->custom_frame_extents.bottom);

  window->size_hints.min_width = (int) MIN (new_width, G_MAXINT);
  window->size_hints.min_height = (int) MIN (new_height, G_MAXINT);
  window->size_hints.flags |= PMinSize;
}

void
meta_window_wayland_set_max_size (MetaWindow *window,
                                  int         width,
                                  int         height)

{
  gint64 new_width, new_height;
  float scale;

  meta_topic (META_DEBUG_GEOMETRY, "Window %s sets max size %d x %d\n",
              window->desc, width, height);

  if (width == 0 && height == 0)
    {
      window->size_hints.max_width = G_MAXINT;
      window->size_hints.max_height = G_MAXINT;
      window->size_hints.flags &= ~PMaxSize;

      return;
    }

  scale = (float) meta_window_wayland_get_geometry_scale (window);
  scale_size (&width, &height, scale);

  new_width = width + (window->custom_frame_extents.left +
                       window->custom_frame_extents.right);
  new_height = height + (window->custom_frame_extents.top +
                         window->custom_frame_extents.bottom);

  window->size_hints.max_width = (int) ((new_width > 0 && new_width < G_MAXINT) ?
                                        new_width : G_MAXINT);
  window->size_hints.max_height = (int)  ((new_height > 0 && new_height < G_MAXINT) ?
                                          new_height : G_MAXINT);
  window->size_hints.flags |= PMaxSize;
}

void
meta_window_wayland_get_min_size (MetaWindow *window,
                                  int        *width,
                                  int        *height)
{
  gint64 current_width, current_height;
  float scale;

  if (!(window->size_hints.flags & PMinSize))
    {
      /* Zero means unlimited */
      *width = 0;
      *height = 0;

      return;
    }

  current_width = window->size_hints.min_width -
                  (window->custom_frame_extents.left +
                   window->custom_frame_extents.right);
  current_height = window->size_hints.min_height -
                   (window->custom_frame_extents.top +
                    window->custom_frame_extents.bottom);

  *width = MAX (current_width, 0);
  *height = MAX (current_height, 0);

  scale = 1.0 / (float) meta_window_wayland_get_geometry_scale (window);
  scale_size (width, height, scale);
}

void
meta_window_wayland_get_max_size (MetaWindow *window,
                                  int        *width,
                                  int        *height)
{
  gint64 current_width = 0;
  gint64 current_height = 0;
  float scale;

  if (!(window->size_hints.flags & PMaxSize))
    {
      /* Zero means unlimited */
      *width = 0;
      *height = 0;

      return;
    }

  if (window->size_hints.max_width < G_MAXINT)
    current_width = window->size_hints.max_width -
                    (window->custom_frame_extents.left +
                     window->custom_frame_extents.right);

  if (window->size_hints.max_height < G_MAXINT)
    current_height = window->size_hints.max_height -
                     (window->custom_frame_extents.top +
                      window->custom_frame_extents.bottom);

  *width = CLAMP (current_width, 0, G_MAXINT);
  *height = CLAMP (current_height, 0, G_MAXINT);

  scale = 1.0 / (float) meta_window_wayland_get_geometry_scale (window);
  scale_size (width, height, scale);
}

