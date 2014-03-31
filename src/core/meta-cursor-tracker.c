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

#include <X11/cursorfont.h>
#include <X11/extensions/Xfixes.h>
#include <X11/Xcursor/Xcursor.h>

#include "meta-cursor-tracker-private.h"
#include "screen-private.h"
#include "monitor-private.h"

#include "wayland/meta-wayland-private.h"

typedef struct {
  int ref_count;

  CoglTexture2D *texture;
  struct gbm_bo *bo;
  int hot_x, hot_y;
} MetaCursorReference;

struct _MetaCursorTracker {
  GObject parent_instance;

  MetaScreen *screen;

  gboolean is_showing;
  gboolean has_hw_cursor;

  /* The cursor tracker stores the cursor for the current grab
   * operation, the cursor for the window with pointer focus, and
   * the cursor for the root window, which contains either the
   * default arrow cursor or the 'busy' hourglass if we're launching
   * an app.
   *
   * We choose the first one available -- if there's a grab cursor,
   * we choose that cursor, if there's window cursor, we choose that,
   * otherwise we choose the root cursor.
   *
   * The displayed_cursor contains the chosen cursor.
   */
  MetaCursorReference *displayed_cursor;

  MetaCursorReference *grab_cursor;

  /* Wayland clients can set a NULL buffer as their cursor 
   * explicitly, which means that we shouldn't display anything.
   * So, we can't simply store a NULL in window_cursor to
   * determine an unset window cursor; we need an extra boolean.
   */
  gboolean has_window_cursor;
  MetaCursorReference *window_cursor;

  MetaCursorReference *root_cursor;

  MetaCursorReference *default_cursors[META_CURSOR_LAST];

  int current_x, current_y;
  MetaRectangle current_rect;
  MetaRectangle previous_rect;
  gboolean previous_is_valid;

  CoglPipeline *pipeline;
  int drm_fd;
  struct gbm_device *gbm;
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

static void meta_cursor_tracker_set_crtc_has_hw_cursor (MetaCursorTracker *tracker,
                                                        MetaCRTC          *crtc,
                                                        gboolean           has_hw_cursor);
static void sync_cursor (MetaCursorTracker *tracker);

static MetaCursorReference *
meta_cursor_reference_ref (MetaCursorReference *self)
{
  g_assert (self->ref_count > 0);
  self->ref_count++;

  return self;
}

static void
meta_cursor_reference_unref (MetaCursorReference *self)
{
  self->ref_count--;

  if (self->ref_count == 0)
    {
      cogl_object_unref (self->texture);
      if (self->bo)
        gbm_bo_destroy (self->bo);

      g_slice_free (MetaCursorReference, self);
    }
}

static void
translate_meta_cursor (MetaCursor   cursor,
                       guint       *glyph_out,
                       const char **name_out)
{
  guint glyph = XC_num_glyphs;
  const char *name = NULL;

  switch (cursor)
    {
    case META_CURSOR_DEFAULT:
      glyph = XC_left_ptr;
      break;
    case META_CURSOR_NORTH_RESIZE:
      glyph = XC_top_side;
      break;
    case META_CURSOR_SOUTH_RESIZE:
      glyph = XC_bottom_side;
      break;
    case META_CURSOR_WEST_RESIZE:
      glyph = XC_left_side;
      break;
    case META_CURSOR_EAST_RESIZE:
      glyph = XC_right_side;
      break;
    case META_CURSOR_SE_RESIZE:
      glyph = XC_bottom_right_corner;
      break;
    case META_CURSOR_SW_RESIZE:
      glyph = XC_bottom_left_corner;
      break;
    case META_CURSOR_NE_RESIZE:
      glyph = XC_top_right_corner;
      break;
    case META_CURSOR_NW_RESIZE:
      glyph = XC_top_left_corner;
      break;
    case META_CURSOR_MOVE_OR_RESIZE_WINDOW:
      glyph = XC_fleur;
      break;
    case META_CURSOR_BUSY:
      glyph = XC_watch;
      break;
    case META_CURSOR_DND_IN_DRAG:
      name = "dnd-none";
      break;
    case META_CURSOR_DND_MOVE:
      name = "dnd-move";
      break;
    case META_CURSOR_DND_COPY:
      name = "dnd-copy";
      break;
    case META_CURSOR_DND_UNSUPPORTED_TARGET:
      name = "dnd-none";
      break;
    case META_CURSOR_POINTING_HAND:
      glyph = XC_hand2;
      break;
    case META_CURSOR_CROSSHAIR:
      glyph = XC_crosshair;
      break;
    case META_CURSOR_IBEAM:
      glyph = XC_xterm;
      break;

    default:
      g_assert_not_reached ();
      glyph = 0; /* silence compiler */
      break;
    }

  *glyph_out = glyph;
  *name_out = name;
}

static Cursor
load_cursor_on_server (MetaDisplay *display,
                       MetaCursor   cursor)
{
  Cursor xcursor;
  guint glyph;
  const char *name;

  translate_meta_cursor (cursor, &glyph, &name);

  if (name != NULL)
    xcursor = XcursorLibraryLoadCursor (display->xdisplay, name);
  else
    xcursor = XCreateFontCursor (display->xdisplay, glyph);

  return xcursor;
}

Cursor
meta_display_create_x_cursor (MetaDisplay *display,
                              MetaCursor cursor)
{
  return load_cursor_on_server (display, cursor);
}

static XcursorImage *
load_cursor_on_client (MetaDisplay *display,
                       MetaCursor   cursor)
{
  XcursorImage *image;
  guint glyph;
  const char *name;
  const char *theme = XcursorGetTheme (display->xdisplay);
  int size = XcursorGetDefaultSize (display->xdisplay);

  translate_meta_cursor (cursor, &glyph, &name);

  if (name != NULL)
    image = XcursorLibraryLoadImage (name, theme, size);
  else
    image = XcursorShapeLoadImage (glyph, theme, size);

  return image;
}

static MetaCursorReference *
meta_cursor_reference_from_theme (MetaCursorTracker  *tracker,
                                  MetaCursor          cursor)
{
  XcursorImage *image;
  int width, height, rowstride;
  CoglPixelFormat cogl_format;
  uint32_t gbm_format;
  ClutterBackend *clutter_backend;
  CoglContext *cogl_context;
  MetaCursorReference *self;

  image = load_cursor_on_client (tracker->screen->display, cursor);
  if (!image)
    return NULL;

  width           = image->width;
  height          = image->height;
  rowstride       = width * 4;

  gbm_format = GBM_FORMAT_ARGB8888;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  cogl_format = COGL_PIXEL_FORMAT_BGRA_8888;
#else
  cogl_format = COGL_PIXEL_FORMAT_ARGB_8888;
#endif

  self = g_slice_new0 (MetaCursorReference);
  self->ref_count = 1;
  self->hot_x = image->xhot;
  self->hot_y = image->yhot;

  clutter_backend = clutter_get_default_backend ();
  cogl_context = clutter_backend_get_cogl_context (clutter_backend);
  self->texture = cogl_texture_2d_new_from_data (cogl_context,
                                                 width, height,
                                                 cogl_format,
                                                 rowstride,
                                                 (uint8_t*)image->pixels,
                                                 NULL);

  if (tracker->gbm)
    {
      if (width > 64 || height > 64)
        {
          meta_warning ("Invalid theme cursor size (must be at most 64x64)\n");
          goto out;
        }

      if (gbm_device_is_format_supported (tracker->gbm, gbm_format,
                                          GBM_BO_USE_CURSOR_64X64 | GBM_BO_USE_WRITE))
        {
          uint32_t buf[64 * 64];
          int i;

          self->bo = gbm_bo_create (tracker->gbm, 64, 64,
                                    gbm_format, GBM_BO_USE_CURSOR_64X64 | GBM_BO_USE_WRITE);

          memset (buf, 0, sizeof(buf));
          for (i = 0; i < height; i++)
            memcpy (buf + i * 64, image->pixels + i * width, width * 4);

          gbm_bo_write (self->bo, buf, 64 * 64 * 4);
        }
      else
        meta_warning ("HW cursor for format %d not supported\n", gbm_format);
    }

 out:
  XcursorImageDestroy (image);

  return self;
}

static MetaCursorReference *
meta_cursor_reference_take_texture (CoglTexture2D *texture)
{
  MetaCursorReference *self;

  self = g_slice_new0 (MetaCursorReference);
  self->ref_count = 1;

  self->texture = texture;

  return self;
}

static MetaCursorReference *
meta_cursor_reference_from_buffer (MetaCursorTracker  *tracker,
                                   struct wl_resource *buffer,
                                   int                 hot_x,
                                   int                 hot_y)
{
  ClutterBackend *backend;
  CoglContext *cogl_context;
  MetaCursorReference *self;
  CoglPixelFormat cogl_format;
  struct wl_shm_buffer *shm_buffer;
  uint32_t gbm_format;

  self = g_slice_new0 (MetaCursorReference);
  self->ref_count = 1;
  self->hot_x = hot_x;
  self->hot_y = hot_y;

  backend = clutter_get_default_backend ();
  cogl_context = clutter_backend_get_cogl_context (backend);

  shm_buffer = wl_shm_buffer_get (buffer);
  if (shm_buffer)
    {
      int rowstride = wl_shm_buffer_get_stride (shm_buffer);
      int width = wl_shm_buffer_get_width (shm_buffer);
      int height = wl_shm_buffer_get_height (shm_buffer);

      switch (wl_shm_buffer_get_format (shm_buffer))
        {
#if G_BYTE_ORDER == G_BIG_ENDIAN
          case WL_SHM_FORMAT_ARGB8888:
            cogl_format = COGL_PIXEL_FORMAT_ARGB_8888_PRE;
            gbm_format = GBM_FORMAT_ARGB8888;
            break;
          case WL_SHM_FORMAT_XRGB8888:
            cogl_format = COGL_PIXEL_FORMAT_ARGB_8888;
            gbm_format = GBM_FORMAT_XRGB8888;
            break;
#else
          case WL_SHM_FORMAT_ARGB8888:
            cogl_format = COGL_PIXEL_FORMAT_BGRA_8888_PRE;
            gbm_format = GBM_FORMAT_ARGB8888;
            break;
          case WL_SHM_FORMAT_XRGB8888:
            cogl_format = COGL_PIXEL_FORMAT_BGRA_8888;
            gbm_format = GBM_FORMAT_XRGB8888;
            break;
#endif
          default:
            g_warn_if_reached ();
            cogl_format = COGL_PIXEL_FORMAT_ARGB_8888;
            gbm_format = GBM_FORMAT_ARGB8888;
        }

      self->texture = cogl_texture_2d_new_from_data (cogl_context,
                                                     width, height,
                                                     cogl_format,
                                                     rowstride,
                                                     wl_shm_buffer_get_data (shm_buffer),
                                                     NULL);

      if (width > 64 || height > 64)
        {
          meta_warning ("Invalid cursor size (must be at most 64x64), falling back to software (GL) cursors\n");
          return self;
        }

      if (tracker->gbm)
        {
          if (gbm_device_is_format_supported (tracker->gbm, gbm_format,
                                              GBM_BO_USE_CURSOR_64X64 | GBM_BO_USE_WRITE))
            {
              uint8_t *data;
              uint8_t buf[4 * 64 * 64];
              int i;

              self->bo = gbm_bo_create (tracker->gbm, 64, 64,
                                        gbm_format, GBM_BO_USE_CURSOR_64X64 | GBM_BO_USE_WRITE);

              data = wl_shm_buffer_get_data (shm_buffer);
              memset (buf, 0, sizeof(buf));
              for (i = 0; i < height; i++)
                memcpy (buf + i * 4 * 64, data + i * rowstride, 4 * width);

              gbm_bo_write (self->bo, buf, 64 * 64 * 4);
            }
          else
            meta_warning ("HW cursor for format %d not supported\n", gbm_format);
        }
    }
  else
    {
      int width, height;

      self->texture = cogl_wayland_texture_2d_new_from_buffer (cogl_context, buffer, NULL);
      width = cogl_texture_get_width (COGL_TEXTURE (self->texture));
      height = cogl_texture_get_height (COGL_TEXTURE (self->texture));

      /* HW cursors must be 64x64, but 64x64 is huge, and no cursor theme actually uses
         that, so themed cursors must be padded with transparent pixels to fill the
         overlay. This is trivial if we have CPU access to the data, but it's not
         possible if the buffer is in GPU memory (and possibly tiled too), so if we
         don't get the right size, we fallback to GL.
      */
      if (width != 64 || height != 64)
        {
          meta_warning ("Invalid cursor size (must be 64x64), falling back to software (GL) cursors\n");
          return self;
        }

      if (tracker->gbm)
        {
          self->bo = gbm_bo_import (tracker->gbm, GBM_BO_IMPORT_WL_BUFFER,
                                    buffer, GBM_BO_USE_CURSOR_64X64);
          if (!self->bo)
            meta_warning ("Importing HW cursor from wl_buffer failed\n");
        }
     }

  return self;
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
  int i;

  if (self->displayed_cursor)
    meta_cursor_reference_unref (self->displayed_cursor);
  if (self->root_cursor)
    meta_cursor_reference_unref (self->root_cursor);

  for (i = 0; i < META_CURSOR_LAST; i++)
    if (self->default_cursors[i])
      meta_cursor_reference_unref (self->default_cursors[i]);

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
  compositor->seat->cursor_tracker = self;
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
      MetaCursorReference *cursor = meta_cursor_reference_take_texture (sprite);
      cursor->hot_x = cursor_image->xhot;
      cursor->hot_y = cursor_image->yhot;

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
    return COGL_TEXTURE (tracker->displayed_cursor->texture);
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
    {
      MetaCursorReference *displayed_cursor = tracker->displayed_cursor;
      if (x)
        *x = displayed_cursor->hot_x;
      if (y)
        *y = displayed_cursor->hot_y;
    }
  else
    {
      if (x)
        *x = 0;
      if (y)
        *y = 0;
    }
}

static MetaCursorReference *
ensure_wayland_cursor (MetaCursorTracker *tracker,
                       MetaCursor         cursor)
{
  if (!tracker->default_cursors[cursor])
    {
      tracker->default_cursors[cursor] = meta_cursor_reference_from_theme (tracker, cursor);
      if (!tracker->default_cursors[cursor])
        meta_warning ("Failed to load cursor from theme\n");
    }

  return meta_cursor_reference_ref (tracker->default_cursors[cursor]);
}

void
meta_cursor_tracker_set_grab_cursor (MetaCursorTracker *tracker,
                                     MetaCursor         cursor)
{
  g_clear_pointer (&tracker->grab_cursor, meta_cursor_reference_unref);
  if (cursor != META_CURSOR_DEFAULT)
    tracker->grab_cursor = ensure_wayland_cursor (tracker, cursor);
  sync_cursor (tracker);
}

void
meta_cursor_tracker_set_window_cursor (MetaCursorTracker  *tracker,
                                       struct wl_resource *buffer,
                                       int                 hot_x,
                                       int                 hot_y)
{
  MetaCursorReference *cursor;

  if (buffer)
    cursor = meta_cursor_reference_from_buffer (tracker, buffer, hot_x, hot_y);
  else
    cursor = NULL;

  set_window_cursor (tracker, TRUE, cursor);
}

void
meta_cursor_tracker_unset_window_cursor (MetaCursorTracker *tracker)
{
  set_window_cursor (tracker, FALSE, NULL);
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
      g_clear_pointer (&tracker->root_cursor, meta_cursor_reference_unref);
      tracker->root_cursor = ensure_wayland_cursor (tracker, cursor);
      sync_cursor (tracker);
    }
}

static void
update_hw_cursor (MetaCursorTracker *tracker)
{
  MetaMonitorManager *monitors;
  MetaCRTC *crtcs;
  unsigned int i, n_crtcs;
  gboolean enabled;

  enabled = tracker->displayed_cursor && tracker->displayed_cursor->bo != NULL;
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
sync_displayed_cursor (MetaCursorTracker *tracker)
{
  MetaCursorReference *displayed_cursor = get_displayed_cursor (tracker);

  if (tracker->displayed_cursor == displayed_cursor)
    return;

  g_clear_pointer (&tracker->displayed_cursor, meta_cursor_reference_unref);
  if (displayed_cursor)
    tracker->displayed_cursor = meta_cursor_reference_ref (displayed_cursor);

  if (meta_is_wayland_compositor ())
    {
      if (displayed_cursor)
        cogl_pipeline_set_layer_texture (tracker->pipeline, 0, COGL_TEXTURE (displayed_cursor->texture));
      else
        cogl_pipeline_set_layer_texture (tracker->pipeline, 0, NULL);

      update_hw_cursor (tracker);
    }

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
      tracker->current_rect.x = tracker->current_x - displayed_cursor->hot_x;
      tracker->current_rect.y = tracker->current_y - displayed_cursor->hot_y;
      tracker->current_rect.width = cogl_texture_get_width (COGL_TEXTURE (displayed_cursor->texture));
      tracker->current_rect.height = cogl_texture_get_height (COGL_TEXTURE (displayed_cursor->texture));
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
      union gbm_bo_handle handle;
      int width, height;
      int hot_x, hot_y;

      handle = gbm_bo_get_handle (displayed_cursor->bo);
      width = gbm_bo_get_width (displayed_cursor->bo);
      height = gbm_bo_get_height (displayed_cursor->bo);
      hot_x = displayed_cursor->hot_x;
      hot_y = displayed_cursor->hot_y;

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
