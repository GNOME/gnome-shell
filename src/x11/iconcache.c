/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter window icons */

/*
 * Copyright (C) 2002 Havoc Pennington
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
 */

#include "config.h"

#include "x11/meta-x11-display-private.h"
#include "iconcache.h"

#include <meta/errors.h>

#include <cairo.h>
#include <cairo-xlib.h>
#include <cairo-xlib-xrender.h>

#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>

static gboolean
find_largest_sizes (gulong *data,
                    gulong  nitems,
                    int    *width,
                    int    *height)
{
  *width = 0;
  *height = 0;

  while (nitems > 0)
    {
      int w, h;

      if (nitems < 3)
        return FALSE; /* no space for w, h */

      w = data[0];
      h = data[1];

      if (nitems < ((gulong)(w * h) + 2))
        return FALSE; /* not enough data */

      *width = MAX (w, *width);
      *height = MAX (h, *height);

      data += (w * h) + 2;
      nitems -= (w * h) + 2;
    }

  return TRUE;
}

static gboolean
find_best_size (gulong  *data,
                gulong   nitems,
                int      ideal_width,
                int      ideal_height,
                int     *width,
                int     *height,
                gulong **start)
{
  int best_w;
  int best_h;
  gulong *best_start;
  int max_width, max_height;

  *width = 0;
  *height = 0;
  *start = NULL;

  if (!find_largest_sizes (data, nitems, &max_width, &max_height))
    return FALSE;

  if (ideal_width < 0)
    ideal_width = max_width;
  if (ideal_height < 0)
    ideal_height = max_height;

  best_w = 0;
  best_h = 0;
  best_start = NULL;

  while (nitems > 0)
    {
      int w, h;
      gboolean replace;

      replace = FALSE;

      if (nitems < 3)
        return FALSE; /* no space for w, h */

      w = data[0];
      h = data[1];

      if (nitems < ((gulong)(w * h) + 2))
        break; /* not enough data */

      if (best_start == NULL)
        {
          replace = TRUE;
        }
      else
        {
          /* work with averages */
          const int ideal_size = (ideal_width + ideal_height) / 2;
          int best_size = (best_w + best_h) / 2;
          int this_size = (w + h) / 2;

          /* larger than desired is always better than smaller */
          if (best_size < ideal_size &&
              this_size >= ideal_size)
            replace = TRUE;
          /* if we have too small, pick anything bigger */
          else if (best_size < ideal_size &&
                   this_size > best_size)
            replace = TRUE;
          /* if we have too large, pick anything smaller
           * but still >= the ideal
           */
          else if (best_size > ideal_size &&
                   this_size >= ideal_size &&
                   this_size < best_size)
            replace = TRUE;
        }

      if (replace)
        {
          best_start = data + 2;
          best_w = w;
          best_h = h;
        }

      data += (w * h) + 2;
      nitems -= (w * h) + 2;
    }

  if (best_start)
    {
      *start = best_start;
      *width = best_w;
      *height = best_h;
      return TRUE;
    }
  else
    return FALSE;
}

static cairo_surface_t *
argbdata_to_surface (gulong *argb_data, int w, int h)
{
  cairo_surface_t *surface;
  int y, x, stride;
  uint32_t *data;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
  stride = cairo_image_surface_get_stride (surface) / sizeof (uint32_t);
  data = (uint32_t *) cairo_image_surface_get_data (surface);

  /* One could speed this up a lot. */
  for (y = 0; y < h; y++)
    {
      for (x = 0; x < w; x++)
        {
          uint32_t *p = &data[y * stride + x];
          gulong *d = &argb_data[y * w + x];
          *p = *d;
        }
    }

  cairo_surface_mark_dirty (surface);

  return surface;
}

static gboolean
read_rgb_icon (MetaX11Display   *x11_display,
               Window            xwindow,
               int               ideal_width,
               int               ideal_height,
               int               ideal_mini_width,
               int               ideal_mini_height,
               cairo_surface_t **icon,
               cairo_surface_t **mini_icon)
{
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  int result, err;
  guchar *data;
  gulong *best;
  int w, h;
  gulong *best_mini;
  int mini_w, mini_h;
  gulong *data_as_long;

  meta_error_trap_push (x11_display);
  type = None;
  data = NULL;
  result = XGetWindowProperty (x11_display->xdisplay,
			       xwindow,
                               x11_display->atom__NET_WM_ICON,
			       0, G_MAXLONG,
			       False, XA_CARDINAL, &type, &format, &nitems,
			       &bytes_after, &data);
  err = meta_error_trap_pop_with_return (x11_display);

  if (err != Success ||
      result != Success)
    return FALSE;

  if (type != XA_CARDINAL)
    {
      XFree (data);
      return FALSE;
    }

  data_as_long = (gulong *)data;

  if (!find_best_size (data_as_long, nitems,
                       ideal_width, ideal_height,
                       &w, &h, &best))
    {
      XFree (data);
      return FALSE;
    }

  if (!find_best_size (data_as_long, nitems,
                       ideal_mini_width, ideal_mini_height,
                       &mini_w, &mini_h, &best_mini))
    {
      XFree (data);
      return FALSE;
    }

  *icon = argbdata_to_surface (best, w, h);
  *mini_icon = argbdata_to_surface (best_mini, mini_w, mini_h);

  XFree (data);

  return TRUE;
}

static void
get_pixmap_geometry (MetaX11Display *x11_display,
                     Pixmap          pixmap,
                     int            *w,
                     int            *h,
                     int            *d)
{
  Window root_ignored;
  int x_ignored, y_ignored;
  guint width, height;
  guint border_width_ignored;
  guint depth;

  if (w)
    *w = 1;
  if (h)
    *h = 1;
  if (d)
    *d = 1;

  XGetGeometry (x11_display->xdisplay,
                pixmap, &root_ignored, &x_ignored, &y_ignored,
                &width, &height, &border_width_ignored, &depth);

  if (w)
    *w = width;
  if (h)
    *h = height;
  if (d)
    *d = depth;
}

static int
standard_pict_format_for_depth (int depth)
{
  switch (depth)
    {
    case 1:
      return PictStandardA1;
    case 24:
      return PictStandardRGB24;
    case 32:
      return PictStandardARGB32;
    default:
      g_assert_not_reached ();
    }
}

static XRenderPictFormat *
pict_format_for_depth (Display *xdisplay, int depth)
{
  return XRenderFindStandardFormat (xdisplay, standard_pict_format_for_depth (depth));
}

static cairo_surface_t *
surface_from_pixmap (Display *xdisplay, Pixmap xpixmap,
                     int width, int height)
{
  Window root_return;
  int x_ret, y_ret;
  unsigned int w_ret, h_ret, bw_ret, depth_ret;

  if (!XGetGeometry (xdisplay, xpixmap, &root_return,
                     &x_ret, &y_ret, &w_ret, &h_ret, &bw_ret, &depth_ret))
    return NULL;

  return cairo_xlib_surface_create_with_xrender_format (xdisplay, xpixmap, DefaultScreenOfDisplay (xdisplay),
                                                        pict_format_for_depth (xdisplay, depth_ret), w_ret, h_ret);
}

static gboolean
try_pixmap_and_mask (MetaX11Display   *x11_display,
                     Pixmap            src_pixmap,
                     Pixmap            src_mask,
                     cairo_surface_t **iconp)
{
  Display *xdisplay = x11_display->xdisplay;
  cairo_surface_t *icon, *mask = NULL;
  int w, h, d;

  if (src_pixmap == None)
    return FALSE;

  meta_error_trap_push (x11_display);

  get_pixmap_geometry (x11_display, src_pixmap, &w, &h, &d);
  icon = surface_from_pixmap (xdisplay, src_pixmap, w, h);

  if (icon && src_mask != None)
    {
      get_pixmap_geometry (x11_display, src_mask, &w, &h, &d);

      if (d == 1)
        mask = surface_from_pixmap (xdisplay, src_mask, w, h);
    }

  meta_error_trap_pop (x11_display);

  if (icon && mask)
    {
      cairo_surface_t *masked;
      cairo_t *cr;

      masked = cairo_surface_create_similar_image (icon,
                                                   CAIRO_FORMAT_ARGB32,
                                                   cairo_xlib_surface_get_width (icon),
                                                   cairo_xlib_surface_get_height (icon));
      cr = cairo_create (masked);

      cairo_set_source_surface (cr, icon, 0, 0);
      cairo_mask_surface (cr, mask, 0, 0);

      cairo_destroy (cr);
      cairo_surface_destroy (icon);
      cairo_surface_destroy (mask);

      icon = masked;
    }

  if (icon)
    {
      *iconp = icon;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static void
get_kwm_win_icon (MetaX11Display *x11_display,
                  Window          xwindow,
                  Pixmap         *pixmap,
                  Pixmap         *mask)
{
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  guchar *data;
  Pixmap *icons;
  int err, result;

  *pixmap = None;
  *mask = None;

  meta_error_trap_push (x11_display);
  icons = NULL;
  result = XGetWindowProperty (x11_display->xdisplay, xwindow,
                               x11_display->atom__KWM_WIN_ICON,
			       0, G_MAXLONG,
			       False,
                               x11_display->atom__KWM_WIN_ICON,
			       &type, &format, &nitems,
			       &bytes_after, &data);
  icons = (Pixmap *)data;

  err = meta_error_trap_pop_with_return (x11_display);
  if (err != Success ||
      result != Success)
    return;

  if (type != x11_display->atom__KWM_WIN_ICON)
    {
      XFree (icons);
      return;
    }

  *pixmap = icons[0];
  *mask = icons[1];

  XFree (icons);

  return;
}

void
meta_icon_cache_init (MetaIconCache *icon_cache)
{
  g_return_if_fail (icon_cache != NULL);

  icon_cache->origin = USING_NO_ICON;
  icon_cache->prev_pixmap = None;
  icon_cache->prev_mask = None;
  icon_cache->wm_hints_dirty = TRUE;
  icon_cache->kwm_win_icon_dirty = TRUE;
  icon_cache->net_wm_icon_dirty = TRUE;
}

void
meta_icon_cache_property_changed (MetaIconCache  *icon_cache,
                                  MetaX11Display *x11_display,
                                  Atom            atom)
{
  if (atom == x11_display->atom__NET_WM_ICON)
    icon_cache->net_wm_icon_dirty = TRUE;
  else if (atom == x11_display->atom__KWM_WIN_ICON)
    icon_cache->kwm_win_icon_dirty = TRUE;
  else if (atom == XA_WM_HINTS)
    icon_cache->wm_hints_dirty = TRUE;
}

gboolean
meta_icon_cache_get_icon_invalidated (MetaIconCache *icon_cache)
{
  if (icon_cache->origin <= USING_KWM_WIN_ICON &&
      icon_cache->kwm_win_icon_dirty)
    return TRUE;
  else if (icon_cache->origin <= USING_WM_HINTS &&
           icon_cache->wm_hints_dirty)
    return TRUE;
  else if (icon_cache->origin <= USING_NET_WM_ICON &&
           icon_cache->net_wm_icon_dirty)
    return TRUE;
  else if (icon_cache->origin < USING_FALLBACK_ICON)
    return TRUE;
  else
    return FALSE;
}

gboolean
meta_read_icons (MetaX11Display   *x11_display,
                 Window            xwindow,
                 MetaIconCache    *icon_cache,
                 Pixmap            wm_hints_pixmap,
                 Pixmap            wm_hints_mask,
                 cairo_surface_t **iconp,
                 int               ideal_width,
                 int               ideal_height,
                 cairo_surface_t **mini_iconp,
                 int               ideal_mini_width,
                 int               ideal_mini_height)
{
  /* Return value is whether the icon changed */

  g_return_val_if_fail (icon_cache != NULL, FALSE);

  *iconp = NULL;
  *mini_iconp = NULL;

  if (!meta_icon_cache_get_icon_invalidated (icon_cache))
    return FALSE; /* we have no new info to use */

  /* Our algorithm here assumes that we can't have for example origin
   * < USING_NET_WM_ICON and icon_cache->net_wm_icon_dirty == FALSE
   * unless we have tried to read NET_WM_ICON.
   *
   * Put another way, if an icon origin is not dirty, then we have
   * tried to read it at the current size. If it is dirty, then
   * we haven't done that since the last change.
   */

  if (icon_cache->origin <= USING_NET_WM_ICON &&
      icon_cache->net_wm_icon_dirty)
    {
      icon_cache->net_wm_icon_dirty = FALSE;

      if (read_rgb_icon (x11_display, xwindow,
                         ideal_width, ideal_height,
                         ideal_mini_width, ideal_mini_height,
                         iconp, mini_iconp))
        {
          icon_cache->origin = USING_NET_WM_ICON;
          return TRUE;
        }
    }

  if (icon_cache->origin <= USING_WM_HINTS &&
      icon_cache->wm_hints_dirty)
    {
      Pixmap pixmap;
      Pixmap mask;

      icon_cache->wm_hints_dirty = FALSE;

      pixmap = wm_hints_pixmap;
      mask = wm_hints_mask;

      /* We won't update if pixmap is unchanged;
       * avoids a get_from_drawable() on every geometry
       * hints change
       */
      if ((pixmap != icon_cache->prev_pixmap ||
           mask != icon_cache->prev_mask) &&
          pixmap != None)
        {
          if (try_pixmap_and_mask (x11_display, pixmap, mask, iconp))
            {
              *mini_iconp = cairo_surface_reference (*iconp);
              icon_cache->prev_pixmap = pixmap;
              icon_cache->prev_mask = mask;
              icon_cache->origin = USING_WM_HINTS;
              return TRUE;
            }
        }
    }

  if (icon_cache->origin <= USING_KWM_WIN_ICON &&
      icon_cache->kwm_win_icon_dirty)
    {
      Pixmap pixmap;
      Pixmap mask;

      icon_cache->kwm_win_icon_dirty = FALSE;

      get_kwm_win_icon (x11_display, xwindow, &pixmap, &mask);

      if ((pixmap != icon_cache->prev_pixmap ||
           mask != icon_cache->prev_mask) &&
          pixmap != None)
        {
          if (try_pixmap_and_mask (x11_display, pixmap, mask, iconp))
            {
              *mini_iconp = cairo_surface_reference (*iconp);
              icon_cache->prev_pixmap = pixmap;
              icon_cache->prev_mask = mask;
              icon_cache->origin = USING_KWM_WIN_ICON;
              return TRUE;
            }
        }
    }

  if (icon_cache->origin < USING_FALLBACK_ICON)
    {
      icon_cache->origin = USING_FALLBACK_ICON;
      *iconp = NULL;
      *mini_iconp = NULL;
      return TRUE;
    }

  /* found nothing new */
  return FALSE;
}
