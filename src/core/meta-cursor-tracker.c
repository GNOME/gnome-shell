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

#include <gdk/gdk.h>

#include <X11/cursorfont.h>
#include <X11/extensions/Xfixes.h>
#include <X11/Xcursor/Xcursor.h>

#include "meta-cursor-tracker-private.h"
#include "screen-private.h"

#define META_WAYLAND_DEFAULT_CURSOR_HOTSPOT_X 7
#define META_WAYLAND_DEFAULT_CURSOR_HOTSPOT_Y 4

struct _MetaCursorTracker {
  GObject parent_instance;

  MetaScreen *screen;

  gboolean is_showing;

  CoglTexture2D *sprite;
  int hot_x, hot_y;
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

static void
meta_cursor_tracker_init (MetaCursorTracker *self)
{
  /* (JS) Best (?) that can be assumed since XFixes doesn't provide a way of
   * detecting if the system mouse cursor is showing or not.
   *
   * On wayland we start with the cursor showing
   */
  self->is_showing = TRUE;
}

static void
meta_cursor_tracker_finalize (GObject *object)
{
  MetaCursorTracker *self = META_CURSOR_TRACKER (object);

  if (self->sprite)
    cogl_object_unref (self->sprite);

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

  self = make_x11_cursor_tracker (screen);

  screen->cursor_tracker = self;
  return self;
}

gboolean
meta_cursor_tracker_handle_xevent (MetaCursorTracker *tracker,
                                   XEvent            *xevent)
{
  XFixesCursorNotifyEvent *notify_event;

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

  ensure_xfixes_cursor (tracker);

  if (x)
    *x = tracker->hot_x;
  if (y)
    *y = tracker->hot_y;
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
}

void
meta_cursor_tracker_get_pointer (MetaCursorTracker   *tracker,
                                 int                 *x,
                                 int                 *y,
                                 ClutterModifierType *mods)
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

void
meta_cursor_tracker_set_pointer_visible (MetaCursorTracker *tracker,
                                         gboolean           visible)
{
  if (visible == tracker->is_showing)
    return;
  tracker->is_showing = visible;

  if (visible)
    XFixesShowCursor (tracker->screen->display->xdisplay,
                      tracker->screen->xroot);
  else
    XFixesHideCursor (tracker->screen->display->xdisplay,
                      tracker->screen->xroot);
}
