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

#include <X11/cursorfont.h>
#include <X11/extensions/Xfixes.h>
#include <X11/Xcursor/Xcursor.h>

#include "meta-cursor-tracker-private.h"
#include "screen-private.h"
#include "meta-wayland-private.h"
#include "monitor-private.h"

#define META_WAYLAND_DEFAULT_CURSOR_HOTSPOT_X 7
#define META_WAYLAND_DEFAULT_CURSOR_HOTSPOT_Y 4

typedef struct {
  CoglTexture2D *texture;
  struct gbm_bo *bo;
  int hot_x, hot_y;

  int ref_count;
} MetaCursorReference;

struct _MetaCursorTracker {
  GObject parent_instance;

  MetaScreen *screen;

  gboolean is_showing;
  gboolean has_cursor;
  gboolean has_hw_cursor;

  MetaCursorReference *sprite;
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

static void meta_cursor_tracker_set_sprite (MetaCursorTracker   *tracker,
                                            MetaCursorReference *sprite);

static void meta_cursor_tracker_set_crtc_has_hw_cursor (MetaCursorTracker *tracker,
                                                        MetaCRTC          *crtc,
                                                        gboolean           has_hw_cursor);

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
                                                 COGL_PIXEL_FORMAT_ANY,
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
  CoglPixelFormat cogl_format, cogl_internal_format;
  struct wl_shm_buffer *shm_buffer;
  uint32_t gbm_format;

  self = g_slice_new0 (MetaCursorReference);
  self->ref_count = 1;
 
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
            cogl_internal_format = COGL_PIXEL_FORMAT_ANY;
            gbm_format = GBM_FORMAT_ARGB8888;
            break;
          case WL_SHM_FORMAT_XRGB8888:
            cogl_format = COGL_PIXEL_FORMAT_ARGB_8888;
            cogl_internal_format = COGL_PIXEL_FORMAT_RGB_888;
            gbm_format = GBM_FORMAT_XRGB8888;
            break;
#else
          case WL_SHM_FORMAT_ARGB8888:
            cogl_format = COGL_PIXEL_FORMAT_BGRA_8888_PRE;
            cogl_internal_format = COGL_PIXEL_FORMAT_ANY;
            gbm_format = GBM_FORMAT_ARGB8888;
            break;
          case WL_SHM_FORMAT_XRGB8888:
            cogl_format = COGL_PIXEL_FORMAT_BGRA_8888;
            cogl_internal_format = COGL_PIXEL_FORMAT_BGR_888;
            gbm_format = GBM_FORMAT_XRGB8888;
            break;
#endif
          default:
            g_warn_if_reached ();
            cogl_format = COGL_PIXEL_FORMAT_ARGB_8888;
            cogl_internal_format = COGL_PIXEL_FORMAT_ANY;
            gbm_format = GBM_FORMAT_ARGB8888;
        }

      self->texture = cogl_texture_2d_new_from_data (cogl_context,
                                                     width, height,
                                                     cogl_format,
                                                     cogl_internal_format,
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
          cogl_format = cogl_texture_get_format (COGL_TEXTURE (self->texture));
          switch (cogl_format)
            {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
            case COGL_PIXEL_FORMAT_ARGB_8888_PRE:
            case COGL_PIXEL_FORMAT_ARGB_8888:
              gbm_format = GBM_FORMAT_BGRA8888;
              break;
            case COGL_PIXEL_FORMAT_BGRA_8888_PRE:
            case COGL_PIXEL_FORMAT_BGRA_8888:
              break;
            case COGL_PIXEL_FORMAT_RGB_888:
              break;
#else
            case COGL_PIXEL_FORMAT_ARGB_8888_PRE:
            case COGL_PIXEL_FORMAT_ARGB_8888:
              gbm_format = GBM_FORMAT_ARGB8888;
              break;
            case COGL_PIXEL_FORMAT_BGRA_8888_PRE:
            case COGL_PIXEL_FORMAT_BGRA_8888:
              gbm_format = GBM_FORMAT_BGRA8888;
              break;
            case COGL_PIXEL_FORMAT_RGB_888:
              gbm_format = GBM_FORMAT_RGB888;
              break;
#endif
            default:
              meta_warning ("Unknown cogl format %d\n", cogl_format);
              return self;
            }

          if (gbm_device_is_format_supported (tracker->gbm, gbm_format,
                                              GBM_BO_USE_CURSOR_64X64))
            {
              self->bo = gbm_bo_import (tracker->gbm, GBM_BO_IMPORT_WL_BUFFER,
                                        buffer, GBM_BO_USE_CURSOR_64X64);
              if (!self->bo)
                meta_warning ("Importing HW cursor from wl_buffer failed\n");
            }
          else
            meta_warning ("HW cursor for format %d not supported\n", gbm_format);
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

  if (self->sprite)
    meta_cursor_reference_unref (self->sprite);
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

  self->drm_fd = compositor->drm_fd;
  if (self->drm_fd >= 0)
    self->gbm = gbm_create_device (compositor->drm_fd);

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

  g_clear_pointer (&tracker->sprite, meta_cursor_reference_unref);
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
      tracker->sprite = meta_cursor_reference_take_texture (sprite);
      tracker->sprite->hot_x = cursor_image->xhot;
      tracker->sprite->hot_y = cursor_image->yhot;
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

  if (tracker->sprite)
    return COGL_TEXTURE (tracker->sprite->texture);
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

  if (tracker->sprite)
    {
      if (x)
        *x = tracker->sprite->hot_x;
      if (y)
        *y = tracker->sprite->hot_y;
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
  if (tracker->default_cursors[cursor])
    return tracker->default_cursors[cursor];

  tracker->default_cursors[cursor] = meta_cursor_reference_from_theme (tracker, cursor);
  if (!tracker->default_cursors[cursor])
    meta_warning ("Failed to load cursor from theme\n");

  return tracker->default_cursors[cursor];
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
      MetaCursorReference *ref;

      ref = ensure_wayland_cursor (tracker, cursor);

      g_clear_pointer (&tracker->root_cursor, meta_cursor_reference_unref);
      tracker->root_cursor = meta_cursor_reference_ref (ref);
    }
}

void
meta_cursor_tracker_revert_root (MetaCursorTracker *tracker)
{
  meta_cursor_tracker_set_sprite (tracker, tracker->root_cursor);
}

static void
update_hw_cursor (MetaCursorTracker *tracker)
{
  MetaMonitorManager *monitors;
  MetaCRTC *crtcs;
  unsigned int i, n_crtcs;
  gboolean enabled;

  enabled = tracker->has_cursor && tracker->sprite->bo != NULL;
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

void
meta_cursor_tracker_set_buffer (MetaCursorTracker  *tracker,
                                struct wl_resource *buffer,
                                int                 hot_x,
                                int                 hot_y)
{
  MetaCursorReference *new_cursor;

  if (buffer)
    {
      new_cursor = meta_cursor_reference_from_buffer (tracker, buffer, hot_x, hot_y);
      meta_cursor_tracker_set_sprite (tracker, new_cursor);
      meta_cursor_reference_unref (new_cursor);
    }
  else
    meta_cursor_tracker_set_sprite (tracker, NULL);
}

static void
meta_cursor_tracker_set_sprite (MetaCursorTracker   *tracker,
                                MetaCursorReference *sprite)
{
  g_assert (meta_is_wayland_compositor ());

  if (sprite == tracker->sprite)
    return;

  g_clear_pointer (&tracker->sprite, meta_cursor_reference_unref);

  if (sprite)
    {
      tracker->sprite = meta_cursor_reference_ref (sprite);
      cogl_pipeline_set_layer_texture (tracker->pipeline, 0, COGL_TEXTURE (tracker->sprite->texture));
    }
  else
    cogl_pipeline_set_layer_texture (tracker->pipeline, 0, NULL);

  tracker->has_cursor = tracker->sprite != NULL && tracker->is_showing;
  update_hw_cursor (tracker);

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

  if (tracker->sprite)
    {
      tracker->current_rect.x = tracker->current_x - tracker->sprite->hot_x;
      tracker->current_rect.y = tracker->current_y - tracker->sprite->hot_y;
      tracker->current_rect.width = cogl_texture_get_width (COGL_TEXTURE (tracker->sprite->texture));
      tracker->current_rect.height = cogl_texture_get_height (COGL_TEXTURE (tracker->sprite->texture));
    }
  else
    {
      tracker->current_rect.x = 0;
      tracker->current_rect.y = 0;
      tracker->current_rect.width = 0;
      tracker->current_rect.height = 0;
    }

  if (tracker->has_hw_cursor)
    move_hw_cursor (tracker);
}

void
meta_cursor_tracker_paint (MetaCursorTracker *tracker)
{
  g_assert (meta_is_wayland_compositor ());

  if (tracker->has_hw_cursor || !tracker->has_cursor)
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

void
meta_cursor_tracker_queue_redraw (MetaCursorTracker *tracker,
                                  ClutterActor      *stage)
{
  cairo_rectangle_int_t clip;

  g_assert (meta_is_wayland_compositor ());

  if (tracker->previous_is_valid)
    {
      cairo_rectangle_int_t clip = {
        .x = tracker->previous_rect.x,
        .y = tracker->previous_rect.y,
        .width = tracker->previous_rect.width,
        .height = tracker->previous_rect.height
      };
      clutter_actor_queue_redraw_with_clip (stage, &clip);
      tracker->previous_is_valid = FALSE;
    }

  if (tracker->has_hw_cursor || !tracker->has_cursor)
    return;

  clip.x = tracker->current_rect.x;
  clip.y = tracker->current_rect.y;
  clip.width = tracker->current_rect.width;
  clip.height = tracker->current_rect.height;
  clutter_actor_queue_redraw_with_clip (stage, &clip);
}

static void
meta_cursor_tracker_set_crtc_has_hw_cursor (MetaCursorTracker *tracker,
                                            MetaCRTC          *crtc,
                                            gboolean           has)
{
  if (has)
    {
      union gbm_bo_handle handle;
      int width, height;
      int hot_x, hot_y;

      handle = gbm_bo_get_handle (tracker->sprite->bo);
      width = gbm_bo_get_width (tracker->sprite->bo);
      height = gbm_bo_get_height (tracker->sprite->bo);
      hot_x = tracker->sprite->hot_x;
      hot_y = tracker->sprite->hot_y;

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
  gdevice = gdk_device_manager_get_client_pointer (gmanager);

  gdk_device_get_position (gdevice, &gscreen, x, y);
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
  *x = point.x;
  *y = point.y;
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
      MetaWaylandCompositor *compositor;

      compositor = meta_wayland_compositor_get_default ();

      tracker->has_cursor = tracker->sprite != NULL && visible;
      update_hw_cursor (tracker);
      meta_cursor_tracker_queue_redraw (tracker, compositor->stage);
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
