/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* 
 * Copyright 2013 Red Hat, Inc.
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
 * Author: Giovanni Campagna <gcampagn@redhat.com>
 */

/**
 * SECTION:cursor-tracker
 * @title: MetaCursorTracker
 * @short_description: Mutter cursor tracking helper. Originally only
 *                     tracking the cursor image, now more of a "core
 *                     pointer abstraction"
 */

#include <config.h>
#include <string.h>
#include <meta/main.h>
#include <meta/util.h>
#include <meta/errors.h>

#include <cogl/cogl.h>
#include <cogl/cogl-wayland-server.h>
#include <clutter/clutter.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "meta-cursor-private.h"
#include "meta-cursor-tracker-private.h"
#include "screen-private.h"

#include "wayland/meta-wayland-private.h"

G_DEFINE_TYPE (MetaCursorTracker, meta_cursor_tracker, G_TYPE_OBJECT);

enum {
    CURSOR_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static MetaCursorReference *
get_displayed_cursor (MetaCursorTracker *tracker)
{
  if (!tracker->is_showing)
    return NULL;

  if (tracker->grab_cursor)
    return tracker->grab_cursor;

  if (tracker->has_window_cursor)
    return tracker->window_cursor;

  return tracker->root_cursor;
}

static void
update_displayed_cursor (MetaCursorTracker *tracker)
{
  meta_cursor_renderer_set_cursor (tracker->renderer, tracker->displayed_cursor);
}

static void
sync_cursor (MetaCursorTracker *tracker)
{
  MetaCursorReference *displayed_cursor = get_displayed_cursor (tracker);

  if (tracker->displayed_cursor == displayed_cursor)
    return;

  g_clear_pointer (&tracker->displayed_cursor, meta_cursor_reference_unref);
  if (displayed_cursor)
    tracker->displayed_cursor = meta_cursor_reference_ref (displayed_cursor);

  update_displayed_cursor (tracker);
  g_signal_emit (tracker, signals[CURSOR_CHANGED], 0);
}

static void
meta_cursor_tracker_init (MetaCursorTracker *self)
{
  /* (JS) Best (?) that can be assumed since XFixes doesn't provide a way of
     detecting if the system mouse cursor is showing or not.

     On wayland we start with the cursor showing
  */
  self->is_showing = TRUE;
}

static void
meta_cursor_tracker_finalize (GObject *object)
{
  MetaCursorTracker *self = META_CURSOR_TRACKER (object);

  if (self->displayed_cursor)
    meta_cursor_reference_unref (self->displayed_cursor);
  if (self->root_cursor)
    meta_cursor_reference_unref (self->root_cursor);

  G_OBJECT_CLASS (meta_cursor_tracker_parent_class)->finalize (object);
}

static void
meta_cursor_tracker_class_init (MetaCursorTrackerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meta_cursor_tracker_finalize;

  signals[CURSOR_CHANGED] = g_signal_new ("cursor-changed",
                                          G_TYPE_FROM_CLASS (klass),
                                          G_SIGNAL_RUN_LAST,
                                          0,
                                          NULL, NULL, NULL,
                                          G_TYPE_NONE, 0);
}

static MetaCursorTracker *
make_wayland_cursor_tracker (MetaScreen *screen)
{
  MetaWaylandCompositor *compositor;
  MetaCursorTracker *self;

  self = g_object_new (META_TYPE_CURSOR_TRACKER, NULL);
  self->screen = screen;
  self->renderer = meta_cursor_renderer_new ();

  compositor = meta_wayland_compositor_get_default ();
  compositor->seat->pointer.cursor_tracker = self;
  meta_cursor_tracker_update_position (self, 0, 0);

  return self;
}

static MetaCursorTracker *
make_x11_cursor_tracker (MetaScreen *screen)
{
  MetaCursorTracker *self = g_object_new (META_TYPE_CURSOR_TRACKER, NULL);

  self->screen = screen;
  self->renderer = meta_cursor_renderer_new ();

  XFixesSelectCursorInput (screen->display->xdisplay,
                           screen->xroot,
                           XFixesDisplayCursorNotifyMask);

  return self;
}

static MetaCursorTracker *
meta_cursor_tracker_new (MetaScreen *screen)
{
  if (meta_is_wayland_compositor ())
    return make_wayland_cursor_tracker (screen);
  else
    return make_x11_cursor_tracker (screen);
}

static MetaCursorTracker *_cursor_tracker;

/**
 * meta_cursor_tracker_get_for_screen:
 * @screen: the #MetaScreen
 *
 * Retrieves the cursor tracker object for @screen.
 *
 * Returns: (transfer none):
 */
MetaCursorTracker *
meta_cursor_tracker_get_for_screen (MetaScreen *screen)
{
  if (!_cursor_tracker)
    _cursor_tracker = meta_cursor_tracker_new (screen);

  return _cursor_tracker;
}

static void
set_window_cursor (MetaCursorTracker   *tracker,
                   gboolean             has_cursor,
                   MetaCursorReference *cursor)
{
  g_clear_pointer (&tracker->window_cursor, meta_cursor_reference_unref);
  if (cursor)
    tracker->window_cursor = meta_cursor_reference_ref (cursor);
  tracker->has_window_cursor = has_cursor;
  sync_cursor (tracker);
}

gboolean
meta_cursor_tracker_handle_xevent (MetaCursorTracker *tracker,
                                   XEvent            *xevent)
{
  XFixesCursorNotifyEvent *notify_event;

  if (meta_is_wayland_compositor ())
    return FALSE;

  if (xevent->xany.type != tracker->screen->display->xfixes_event_base + XFixesCursorNotify)
    return FALSE;

  notify_event = (XFixesCursorNotifyEvent *)xevent;
  if (notify_event->subtype != XFixesDisplayCursorNotify)
    return FALSE;

  set_window_cursor (tracker, FALSE, NULL);

  return TRUE;
}

static MetaCursorReference *
meta_cursor_reference_take_texture (CoglTexture2D *texture,
                                    int            hot_x,
                                    int            hot_y)
{
  MetaCursorReference *self;

  self = g_slice_new0 (MetaCursorReference);
  self->ref_count = 1;
  self->image.texture = texture;
  self->image.hot_x = hot_x;
  self->image.hot_y = hot_y;

  return self;
}

static void
ensure_xfixes_cursor (MetaCursorTracker *tracker)
{
  XFixesCursorImage *cursor_image;
  CoglTexture2D *sprite;
  guint8 *cursor_data;
  gboolean free_cursor_data;
  CoglContext *ctx;

  if (tracker->has_window_cursor)
    return;

  cursor_image = XFixesGetCursorImage (tracker->screen->display->xdisplay);
  if (!cursor_image)
    return;

  /* Like all X APIs, XFixesGetCursorImage() returns arrays of 32-bit
   * quantities as arrays of long; we need to convert on 64 bit */
  if (sizeof(long) == 4)
    {
      cursor_data = (guint8 *)cursor_image->pixels;
      free_cursor_data = FALSE;
    }
  else
    {
      int i, j;
      guint32 *cursor_words;
      gulong *p;
      guint32 *q;

      cursor_words = g_new (guint32, cursor_image->width * cursor_image->height);
      cursor_data = (guint8 *)cursor_words;

      p = cursor_image->pixels;
      q = cursor_words;
      for (j = 0; j < cursor_image->height; j++)
        for (i = 0; i < cursor_image->width; i++)
          *(q++) = *(p++);

      free_cursor_data = TRUE;
    }

  ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());
  sprite = cogl_texture_2d_new_from_data (ctx,
                                          cursor_image->width,
                                          cursor_image->height,
                                          CLUTTER_CAIRO_FORMAT_ARGB32,
                                          cursor_image->width * 4, /* stride */
                                          cursor_data,
                                          NULL);

  if (free_cursor_data)
    g_free (cursor_data);

  if (sprite != NULL)
    {
      MetaCursorReference *cursor = meta_cursor_reference_take_texture (sprite,
                                                                        cursor_image->xhot,
                                                                        cursor_image->yhot);
      set_window_cursor (tracker, TRUE, cursor);
    }
  XFree (cursor_image);
}

/**
 * meta_cursor_tracker_get_sprite:
 *
 * Returns: (transfer none):
 */
CoglTexture *
meta_cursor_tracker_get_sprite (MetaCursorTracker *tracker)
{
  g_return_val_if_fail (META_IS_CURSOR_TRACKER (tracker), NULL);

  if (!meta_is_wayland_compositor ())
    ensure_xfixes_cursor (tracker);

  if (tracker->displayed_cursor)
    return meta_cursor_reference_get_cogl_texture (tracker->displayed_cursor, NULL, NULL);
  else
    return NULL;
}

/**
 * meta_cursor_tracker_get_hot:
 * @tracker:
 * @x: (out):
 * @y: (out):
 *
 */
void
meta_cursor_tracker_get_hot (MetaCursorTracker *tracker,
                             int               *x,
                             int               *y)
{
  g_return_if_fail (META_IS_CURSOR_TRACKER (tracker));

  if (!meta_is_wayland_compositor ())
    ensure_xfixes_cursor (tracker);

  if (tracker->displayed_cursor)
    meta_cursor_reference_get_cogl_texture (tracker->displayed_cursor, x, y);
  else
    {
      if (x)
        *x = 0;
      if (y)
        *y = 0;
    }
}

void
meta_cursor_tracker_set_grab_cursor (MetaCursorTracker   *tracker,
                                     MetaCursorReference *cursor)
{
  g_clear_pointer (&tracker->grab_cursor, meta_cursor_reference_unref);
  if (cursor)
    tracker->grab_cursor = meta_cursor_reference_ref (cursor);

  sync_cursor (tracker);
}

void
meta_cursor_tracker_set_window_cursor (MetaCursorTracker   *tracker,
                                       MetaCursorReference *cursor)
{
  set_window_cursor (tracker, TRUE, cursor);
}

void
meta_cursor_tracker_unset_window_cursor (MetaCursorTracker *tracker)
{
  set_window_cursor (tracker, FALSE, NULL);
}

void
meta_cursor_tracker_set_root_cursor (MetaCursorTracker   *tracker,
                                     MetaCursorReference *cursor)
{
  g_clear_pointer (&tracker->root_cursor, meta_cursor_reference_unref);
  if (cursor)
    tracker->root_cursor = meta_cursor_reference_ref (cursor);

  sync_cursor (tracker);
}

void
meta_cursor_tracker_update_position (MetaCursorTracker *tracker,
                                     int                new_x,
                                     int                new_y)
{
  g_assert (meta_is_wayland_compositor ());

  meta_cursor_renderer_set_position (tracker->renderer, new_x, new_y);
}

static void
get_pointer_position_gdk (int         *x,
                          int         *y,
                          int         *mods)
{
  GdkDeviceManager *gmanager;
  GdkDevice *gdevice;
  GdkScreen *gscreen;

  gmanager = gdk_display_get_device_manager (gdk_display_get_default ());
  gdevice = gdk_x11_device_manager_lookup (gmanager, META_VIRTUAL_CORE_POINTER_ID);

  gdk_device_get_position (gdevice, &gscreen, x, y);
  if (mods)
    gdk_device_get_state (gdevice,
                          gdk_screen_get_root_window (gscreen),
                          NULL, (GdkModifierType*)mods);
}

static void
get_pointer_position_clutter (int         *x,
                              int         *y,
                              int         *mods)
{
  ClutterDeviceManager *cmanager;
  ClutterInputDevice *cdevice;
  ClutterPoint point;

  cmanager = clutter_device_manager_get_default ();
  cdevice = clutter_device_manager_get_core_device (cmanager, CLUTTER_POINTER_DEVICE);

  clutter_input_device_get_coords (cdevice, NULL, &point);
  if (x)
    *x = point.x;
  if (y)
    *y = point.y;
  if (mods)
    *mods = clutter_input_device_get_modifier_state (cdevice);
}

void
meta_cursor_tracker_get_pointer (MetaCursorTracker   *tracker,
                                 int                 *x,
                                 int                 *y,
                                 ClutterModifierType *mods)
{
  /* We can't use the clutter interface when not running as a wayland compositor,
     because we need to query the server, rather than using the last cached value.
     OTOH, on wayland we can't use GDK, because that only sees the events
     we forward to xwayland.
  */
  if (meta_is_wayland_compositor ())
    get_pointer_position_clutter (x, y, (int*)mods);
  else
    get_pointer_position_gdk (x, y, (int*)mods);
}

void
meta_cursor_tracker_set_pointer_visible (MetaCursorTracker *tracker,
                                         gboolean           visible)
{
  if (visible == tracker->is_showing)
    return;
  tracker->is_showing = visible;

  if (meta_is_wayland_compositor ())
    {
      sync_cursor (tracker);
    }
  else
    {
      if (visible)
        XFixesShowCursor (tracker->screen->display->xdisplay,
                          tracker->screen->xroot);
      else
        XFixesHideCursor (tracker->screen->display->xdisplay,
                          tracker->screen->xroot);
    }
}

void
meta_cursor_tracker_force_update (MetaCursorTracker *tracker)
{
  g_assert (meta_is_wayland_compositor ());

  meta_cursor_renderer_force_update (tracker->renderer);
}

struct gbm_device *
meta_cursor_tracker_get_gbm_device (MetaCursorTracker *tracker)
{
  return meta_cursor_renderer_get_gbm_device (tracker->renderer);
}
