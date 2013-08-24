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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Author: Giovanni Campagna <gcampagn@redhat.com>
 */

/**
 * SECTION:cursor-tracker
 * @title: MetaCursorTracker
 * @short_description: Mutter cursor tracking helper
 */

#include <config.h>
#include <meta/main.h>
#include <meta/util.h>
#include <meta/errors.h>

#include <cogl/cogl.h>
#include <clutter/clutter.h>

#include <X11/extensions/Xfixes.h>

#include "meta-cursor-tracker-private.h"
#include "screen-private.h"

#ifdef HAVE_WAYLAND
#include "meta-wayland-private.h"
#endif

#define META_WAYLAND_DEFAULT_CURSOR_HOTSPOT_X 7
#define META_WAYLAND_DEFAULT_CURSOR_HOTSPOT_Y 4

struct _MetaCursorTracker {
  GObject parent_instance;

  MetaScreen *screen;

  gboolean is_showing;

  CoglTexture2D *sprite;
  int hot_x, hot_y;

  CoglTexture2D *root_cursor;
  int root_hot_x, root_hot_y;

  CoglTexture2D *default_cursor;

  int current_x, current_y;
  cairo_rectangle_int_t current_rect;
  cairo_rectangle_int_t previous_rect;
  gboolean previous_is_valid;

  CoglPipeline *pipeline;
};

struct _MetaCursorTrackerClass {
  GObjectClass parent_class;
};

G_DEFINE_TYPE (MetaCursorTracker, meta_cursor_tracker, G_TYPE_OBJECT);

enum {
    CURSOR_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

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

  if (self->sprite)
    cogl_object_unref (self->sprite);
  if (self->root_cursor)
    cogl_object_unref (self->root_cursor);
  if (self->default_cursor)
    cogl_object_unref (self->default_cursor);
  if (self->pipeline)
    cogl_object_unref (self->pipeline);

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

#ifdef HAVE_WAYLAND
static MetaCursorTracker *
make_wayland_cursor_tracker (MetaScreen *screen)
{
  MetaWaylandCompositor *compositor;
  CoglContext *ctx;
  MetaCursorTracker *self = g_object_new (META_TYPE_CURSOR_TRACKER, NULL);
  self->screen = screen;

  ctx = clutter_backend_get_cogl_context (clutter_get_default_backend ());
  self->pipeline = cogl_pipeline_new (ctx);

  compositor = meta_wayland_compositor_get_default ();
  compositor->seat->cursor_tracker = self;

  return self;
}
#endif

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

#ifdef HAVE_WAYLAND
  if (meta_is_wayland_compositor ())
    self = make_wayland_cursor_tracker (screen);
  else
#endif
    self = make_x11_cursor_tracker (screen);

  screen->cursor_tracker = self;
  return self;
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

  g_clear_pointer (&tracker->sprite, cogl_object_unref);
  g_signal_emit (tracker, signals[CURSOR_CHANGED], 0);

  return TRUE;
}

static void
ensure_xfixes_cursor (MetaCursorTracker *tracker)
{
  XFixesCursorImage *cursor_image;
  CoglTexture2D *sprite;
  guint8 *cursor_data;
  gboolean free_cursor_data;
  CoglContext *ctx;

  if (tracker->sprite)
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
                                          COGL_PIXEL_FORMAT_ANY,
                                          cursor_image->width * 4, /* stride */
                                          cursor_data,
                                          NULL);

  if (free_cursor_data)
    g_free (cursor_data);

  if (sprite != NULL)
    {
      tracker->sprite = sprite;
      tracker->hot_x = cursor_image->xhot;
      tracker->hot_y = cursor_image->yhot;
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

  return COGL_TEXTURE (tracker->sprite);
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

  if (x)
    *x = tracker->hot_x;
  if (y)
    *y = tracker->hot_y;
}

static void
ensure_wayland_cursor (MetaCursorTracker *tracker)
{
  CoglBitmap *bitmap;
  char *filename;

  if (tracker->default_cursor)
    return;

  filename = g_build_filename (MUTTER_PKGDATADIR,
                               "cursors/left_ptr.png",
                               NULL);

  bitmap = cogl_bitmap_new_from_file (filename, NULL);
  tracker->default_cursor = cogl_texture_2d_new_from_bitmap (bitmap,
                                                             COGL_PIXEL_FORMAT_ANY,
                                                             NULL);

  cogl_object_unref (bitmap);
  g_free (filename);
}

void
meta_cursor_tracker_set_root_cursor (MetaCursorTracker *tracker,
                                     MetaCursor         cursor)
{
  Cursor xcursor;
  MetaDisplay *display = tracker->screen->display;

  /* First create a cursor for X11 applications that don't specify their own */
  xcursor = meta_display_create_x_cursor (display, cursor);

  XDefineCursor (display->xdisplay, tracker->screen->xroot, xcursor);
  XFlush (display->xdisplay);
  XFreeCursor (display->xdisplay, xcursor);

  /* Now update the real root cursor */
  if (meta_is_wayland_compositor ())
    {
      /* FIXME! We need to load all the other cursors too */
      ensure_wayland_cursor (tracker);

      g_clear_pointer (&tracker->root_cursor, cogl_object_unref);
      tracker->root_cursor = cogl_object_ref (tracker->default_cursor);
      tracker->root_hot_x = META_WAYLAND_DEFAULT_CURSOR_HOTSPOT_X;
      tracker->root_hot_y = META_WAYLAND_DEFAULT_CURSOR_HOTSPOT_Y;
    }
}

void
meta_cursor_tracker_revert_root (MetaCursorTracker *tracker)
{
  meta_cursor_tracker_set_sprite (tracker,
                                  tracker->root_cursor,
                                  tracker->root_hot_x,
                                  tracker->root_hot_y);
}

void
meta_cursor_tracker_set_sprite (MetaCursorTracker *tracker,
                                CoglTexture2D     *sprite,
                                int                hot_x,
                                int                hot_y)
{
  g_assert (meta_is_wayland_compositor ());

  g_clear_pointer (&tracker->sprite, cogl_object_unref);

  if (sprite)
    {
      tracker->sprite = cogl_object_ref (sprite);
      tracker->hot_x = hot_x;
      tracker->hot_y = hot_y;
      cogl_pipeline_set_layer_texture (tracker->pipeline, 0, COGL_TEXTURE (tracker->sprite));
    }
  else
    cogl_pipeline_set_layer_texture (tracker->pipeline, 0, NULL);

  g_signal_emit (tracker, signals[CURSOR_CHANGED], 0);

  meta_cursor_tracker_update_position (tracker, tracker->current_x, tracker->current_y);
}

void
meta_cursor_tracker_update_position (MetaCursorTracker *tracker,
                                     int                new_x,
                                     int                new_y)
{
  g_assert (meta_is_wayland_compositor ());

  tracker->current_x = new_x;
  tracker->current_y = new_y;
  tracker->current_rect.x = tracker->current_x - tracker->hot_x;
  tracker->current_rect.y = tracker->current_y - tracker->hot_y;

  if (tracker->sprite)
    {
      tracker->current_rect.width = cogl_texture_get_width (COGL_TEXTURE (tracker->sprite));
      tracker->current_rect.height = cogl_texture_get_height (COGL_TEXTURE (tracker->sprite));
    }
  else
    {
      tracker->current_rect.width = 0;
      tracker->current_rect.height = 0;
    }
}

void
meta_cursor_tracker_paint (MetaCursorTracker *tracker)
{
  g_assert (meta_is_wayland_compositor ());

  if (tracker->sprite == NULL)
    return;

  /* FIXME: try to use a DRM cursor when possible */
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

void
meta_cursor_tracker_queue_redraw (MetaCursorTracker *tracker,
                                  ClutterActor      *stage)
{
  g_assert (meta_is_wayland_compositor ());

  if (tracker->previous_is_valid)
    {
      clutter_actor_queue_redraw_with_clip (stage, &tracker->previous_rect);
      tracker->previous_is_valid = FALSE;
    }

  if (tracker->sprite == NULL)
    return;

  clutter_actor_queue_redraw_with_clip (stage, &tracker->current_rect);
}
