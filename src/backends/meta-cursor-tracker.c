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
#include <gbm.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "meta-cursor-private.h"
#include "meta-cursor-tracker-private.h"
#include "screen-private.h"
#include "meta-monitor-manager.h"

#include "wayland/meta-wayland-private.h"

G_DEFINE_TYPE (MetaCursorTracker, meta_cursor_tracker, G_TYPE_OBJECT);

enum {
    CURSOR_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void meta_cursor_tracker_set_crtc_has_hw_cursor (MetaCursorTracker *tracker,
                                                        MetaCRTC          *crtc,
                                                        gboolean           has_hw_cursor);
static void sync_cursor (MetaCursorTracker *tracker);

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

  if (self->pipeline)
    cogl_object_unref (self->pipeline);
  if (self->gbm)
    gbm_device_destroy (self->gbm);

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

static void
on_monitors_changed (MetaMonitorManager *monitors,
                     MetaCursorTracker  *tracker)
{
  MetaCRTC *crtcs;
  unsigned int i, n_crtcs;

  if (!tracker->has_hw_cursor)
    return;

  /* Go through the new list of monitors, find out where the cursor is */
  meta_monitor_manager_get_resources (monitors, NULL, NULL, &crtcs, &n_crtcs, NULL, NULL);

  for (i = 0; i < n_crtcs; i++)
    {
      MetaRectangle *rect = &crtcs[i].rect;
      gboolean has;

      has = meta_rectangle_overlap (&tracker->current_rect, rect);

      /* Need to do it unconditionally here, our tracking is
         wrong because we reloaded the CRTCs */
      meta_cursor_tracker_set_crtc_has_hw_cursor (tracker, &crtcs[i], has);
    }
}

static MetaCursorTracker *
make_wayland_cursor_tracker (MetaScreen *screen)
{
  MetaWaylandCompositor *compositor;
  CoglContext *ctx;
  MetaMonitorManager *monitors;
  MetaCursorTracker *self;

  self = g_object_new (META_TYPE_CURSOR_TRACKER, NULL);
  self->screen = screen;

  ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());
  self->pipeline = cogl_pipeline_new (ctx);

  compositor = meta_wayland_compositor_get_default ();
  compositor->seat->pointer.cursor_tracker = self;
  meta_cursor_tracker_update_position (self,
                                       wl_fixed_to_int (compositor->seat->pointer.x),
                                       wl_fixed_to_int (compositor->seat->pointer.y));

#if defined(CLUTTER_WINDOWING_EGL)
  if (clutter_check_windowing_backend (CLUTTER_WINDOWING_EGL))
    {
      CoglRenderer *cogl_renderer = cogl_display_get_renderer (cogl_context_get_display (ctx));
      self->drm_fd = cogl_kms_renderer_get_kms_fd (cogl_renderer);
      self->gbm = gbm_create_device (self->drm_fd);
    }
#endif

  monitors = meta_monitor_manager_get ();
  g_signal_connect_object (monitors, "monitors-changed",
                           G_CALLBACK (on_monitors_changed), self, 0);

  return self;
}

static MetaCursorTracker *
make_x11_cursor_tracker (MetaScreen *screen)
{
  MetaCursorTracker *self = g_object_new (META_TYPE_CURSOR_TRACKER, NULL);
  self->screen = screen;

  XFixesSelectCursorInput (screen->display->xdisplay,
                           screen->xroot,
                           XFixesDisplayCursorNotifyMask);

  return self;
}

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
  MetaCursorTracker *self;

  if (screen->cursor_tracker)
    return screen->cursor_tracker;

  if (meta_is_wayland_compositor ())
    self = make_wayland_cursor_tracker (screen);
  else
    self = make_x11_cursor_tracker (screen);

  screen->cursor_tracker = self;
  return self;
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

static gboolean
should_have_hw_cursor (MetaCursorTracker *tracker)
{
  if (tracker->displayed_cursor)
    return (meta_cursor_reference_get_gbm_bo (tracker->displayed_cursor, NULL, NULL) != NULL);
  else
    return FALSE;
}

static void
update_hw_cursor (MetaCursorTracker *tracker)
{
  MetaMonitorManager *monitors;
  MetaCRTC *crtcs;
  unsigned int i, n_crtcs;
  gboolean enabled;

  enabled = should_have_hw_cursor (tracker);
  tracker->has_hw_cursor = enabled;

  monitors = meta_monitor_manager_get ();
  meta_monitor_manager_get_resources (monitors, NULL, NULL, &crtcs, &n_crtcs, NULL, NULL);

  for (i = 0; i < n_crtcs; i++)
    {
      MetaRectangle *rect = &crtcs[i].rect;
      gboolean has;

      has = enabled && meta_rectangle_overlap (&tracker->current_rect, rect);

      if (has || crtcs[i].has_hw_cursor)
        meta_cursor_tracker_set_crtc_has_hw_cursor (tracker, &crtcs[i], has);
    }
}

static void
move_hw_cursor (MetaCursorTracker *tracker)
{
  MetaMonitorManager *monitors;
  MetaCRTC *crtcs;
  unsigned int i, n_crtcs;

  monitors = meta_monitor_manager_get ();
  meta_monitor_manager_get_resources (monitors, NULL, NULL, &crtcs, &n_crtcs, NULL, NULL);

  g_assert (tracker->has_hw_cursor);

  for (i = 0; i < n_crtcs; i++)
    {
      MetaRectangle *rect = &crtcs[i].rect;
      gboolean has;

      has = meta_rectangle_overlap (&tracker->current_rect, rect);

      if (has != crtcs[i].has_hw_cursor)
        meta_cursor_tracker_set_crtc_has_hw_cursor (tracker, &crtcs[i], has);
      if (has)
        drmModeMoveCursor (tracker->drm_fd, crtcs[i].crtc_id,
                           tracker->current_rect.x - rect->x,
                           tracker->current_rect.y - rect->y);
    }
}

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
  if (meta_is_wayland_compositor ())
    {
      if (tracker->displayed_cursor)
        {
          CoglTexture *texture = meta_cursor_reference_get_cogl_texture (tracker->displayed_cursor, NULL, NULL);
          cogl_pipeline_set_layer_texture (tracker->pipeline, 0, texture);
        }
      else
        cogl_pipeline_set_layer_texture (tracker->pipeline, 0, NULL);

      update_hw_cursor (tracker);
    }
}

static void
sync_displayed_cursor (MetaCursorTracker *tracker)
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
meta_cursor_tracker_queue_redraw (MetaCursorTracker *tracker)
{
  MetaWaylandCompositor *compositor = meta_wayland_compositor_get_default ();
  ClutterActor *stage = compositor->stage;
  cairo_rectangle_int_t clip;

  g_assert (meta_is_wayland_compositor ());

  /* Clear the location the cursor was at before, if we need to. */
  if (tracker->previous_is_valid)
    {
      clip.x = tracker->previous_rect.x;
      clip.y = tracker->previous_rect.y;
      clip.width = tracker->previous_rect.width;
      clip.height = tracker->previous_rect.height;
      clutter_actor_queue_redraw_with_clip (stage, &clip);
      tracker->previous_is_valid = FALSE;
    }

  if (tracker->has_hw_cursor || !tracker->displayed_cursor)
    return;

  clip.x = tracker->current_rect.x;
  clip.y = tracker->current_rect.y;
  clip.width = tracker->current_rect.width;
  clip.height = tracker->current_rect.height;
  clutter_actor_queue_redraw_with_clip (stage, &clip);
}

static void
sync_cursor (MetaCursorTracker *tracker)
{
  MetaCursorReference *displayed_cursor;

  sync_displayed_cursor (tracker);
  displayed_cursor = tracker->displayed_cursor;

  if (displayed_cursor)
    {
      CoglTexture *texture;
      int hot_x, hot_y;

      texture = meta_cursor_reference_get_cogl_texture (displayed_cursor, &hot_x, &hot_y);

      tracker->current_rect.x = tracker->current_x - hot_x;
      tracker->current_rect.y = tracker->current_y - hot_y;
      tracker->current_rect.width = cogl_texture_get_width (COGL_TEXTURE (texture));
      tracker->current_rect.height = cogl_texture_get_height (COGL_TEXTURE (texture));
    }
  else
    {
      tracker->current_rect.x = 0;
      tracker->current_rect.y = 0;
      tracker->current_rect.width = 0;
      tracker->current_rect.height = 0;
    }

  if (meta_is_wayland_compositor ())
    {
      if (tracker->has_hw_cursor)
        move_hw_cursor (tracker);
      else
        meta_cursor_tracker_queue_redraw (tracker);
    }
}

void
meta_cursor_tracker_update_position (MetaCursorTracker *tracker,
                                     int                new_x,
                                     int                new_y)
{
  g_assert (meta_is_wayland_compositor ());

  tracker->current_x = new_x;
  tracker->current_y = new_y;

  sync_cursor (tracker);
}

void
meta_cursor_tracker_paint (MetaCursorTracker *tracker)
{
  g_assert (meta_is_wayland_compositor ());

  if (tracker->has_hw_cursor || !tracker->displayed_cursor)
    return;

  cogl_framebuffer_draw_rectangle (cogl_get_draw_framebuffer (),
                                   tracker->pipeline,
                                   tracker->current_rect.x,
                                   tracker->current_rect.y,
                                   tracker->current_rect.x +
                                   tracker->current_rect.width,
                                   tracker->current_rect.y +
                                   tracker->current_rect.height);

  tracker->previous_rect = tracker->current_rect;
  tracker->previous_is_valid = TRUE;
}

static void
meta_cursor_tracker_set_crtc_has_hw_cursor (MetaCursorTracker *tracker,
                                            MetaCRTC          *crtc,
                                            gboolean           has)
{
  if (has)
    {
      MetaCursorReference *displayed_cursor = tracker->displayed_cursor;
      struct gbm_bo *bo;
      union gbm_bo_handle handle;
      int width, height;
      int hot_x, hot_y;

      bo = meta_cursor_reference_get_gbm_bo (displayed_cursor, &hot_x, &hot_y);

      handle = gbm_bo_get_handle (bo);
      width = gbm_bo_get_width (bo);
      height = gbm_bo_get_height (bo);

      drmModeSetCursor2 (tracker->drm_fd, crtc->crtc_id, handle.u32,
                         width, height, hot_x, hot_y);
      crtc->has_hw_cursor = TRUE;
    }
  else
    {
      drmModeSetCursor2 (tracker->drm_fd, crtc->crtc_id, 0, 0, 0, 0, 0);
      crtc->has_hw_cursor = FALSE;
    }
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

  update_hw_cursor (tracker);
  sync_cursor (tracker);
}
