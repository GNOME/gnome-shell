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

#include <config.h>
#include "iconcache.h"
#include "ui.h"
#include <meta/errors.h>

#include <X11/Xatom.h>

/* The icon-reading code is also in libwnck, please sync bugfixes */

static void
get_fallback_icons (MetaScreen     *screen,
                    GdkPixbuf     **iconp,
                    int             ideal_width,
                    int             ideal_height,
                    GdkPixbuf     **mini_iconp,
                    int             ideal_mini_width,
                    int             ideal_mini_height)
{
  /* we don't scale, should be fixed if we ever un-hardcode the icon
   * size
   */
  *iconp = meta_ui_get_default_window_icon (screen->ui);
  *mini_iconp = meta_ui_get_default_mini_icon (screen->ui);
}

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

static void
argbdata_to_pixdata (gulong *argb_data, int len, guchar **pixdata)
{
  guchar *p;
  int i;

  *pixdata = g_new (guchar, len * 4);
  p = *pixdata;

  /* One could speed this up a lot. */
  i = 0;
  while (i < len)
    {
      guint argb;
      guint rgba;

      argb = argb_data[i];
      rgba = (argb << 8) | (argb >> 24);

      *p = rgba >> 24;
      ++p;
      *p = (rgba >> 16) & 0xff;
      ++p;
      *p = (rgba >> 8) & 0xff;
      ++p;
      *p = rgba & 0xff;
      ++p;

      ++i;
    }
}

static gboolean
read_rgb_icon (MetaDisplay   *display,
               Window         xwindow,
               int            ideal_width,
               int            ideal_height,
               int            ideal_mini_width,
               int            ideal_mini_height,
               int           *width,
               int           *height,
               guchar       **pixdata,
               int           *mini_width,
               int           *mini_height,
               guchar       **mini_pixdata)
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

  meta_error_trap_push (display);
  type = None;
  data = NULL;
  result = XGetWindowProperty (display->xdisplay,
			       xwindow,
                               display->atom__NET_WM_ICON,
			       0, G_MAXLONG,
			       False, XA_CARDINAL, &type, &format, &nitems,
			       &bytes_after, &data);
  err = meta_error_trap_pop_with_return (display);

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

  *width = w;
  *height = h;

  *mini_width = mini_w;
  *mini_height = mini_h;

  argbdata_to_pixdata (best, w * h, pixdata);
  argbdata_to_pixdata (best_mini, mini_w * mini_h, mini_pixdata);

  XFree (data);

  return TRUE;
}

static void
free_pixels (guchar *pixels, gpointer data)
{
  g_free (pixels);
}

static void
get_pixmap_geometry (MetaDisplay *display,
                     Pixmap       pixmap,
                     int         *w,
                     int         *h,
                     int         *d)
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

  XGetGeometry (display->xdisplay,
                pixmap, &root_ignored, &x_ignored, &y_ignored,
                &width, &height, &border_width_ignored, &depth);

  if (w)
    *w = width;
  if (h)
    *h = height;
  if (d)
    *d = depth;
}

static void
apply_foreground_background (GdkPixbuf *pixbuf)
{
  int w, h;
  int i, j;
  guchar *pixels;
  int stride;

  w = gdk_pixbuf_get_width (pixbuf);
  h = gdk_pixbuf_get_height (pixbuf);
  pixels = gdk_pixbuf_get_pixels (pixbuf);
  stride = gdk_pixbuf_get_rowstride (pixbuf);

  i = 0;
  while (i < h)
    {
      j = 0;
      while (j < w)
        {
          guchar *p = pixels + i * stride + j * 4;
          if (p[3] == 0)
            p[0] = p[1] = p[2] =  0xff; /* white background */
          else
            p[0] = p[1] = p[2] = 0x00; /* black foreground */

          p[3] = 0xff;

          ++j;
        }

      ++i;
    }
}

static GdkPixbuf*
apply_mask (GdkPixbuf *pixbuf,
            GdkPixbuf *mask)
{
  int w, h;
  int i, j;
  GdkPixbuf *with_alpha;
  guchar *src;
  guchar *dest;
  int src_stride;
  int dest_stride;

  w = MIN (gdk_pixbuf_get_width (mask), gdk_pixbuf_get_width (pixbuf));
  h = MIN (gdk_pixbuf_get_height (mask), gdk_pixbuf_get_height (pixbuf));

  with_alpha = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);

  dest = gdk_pixbuf_get_pixels (with_alpha);
  src = gdk_pixbuf_get_pixels (mask);

  dest_stride = gdk_pixbuf_get_rowstride (with_alpha);
  src_stride = gdk_pixbuf_get_rowstride (mask);

  i = 0;
  while (i < h)
    {
      j = 0;
      while (j < w)
        {
          guchar *s = src + i * src_stride + j * 4;
          guchar *d = dest + i * dest_stride + j * 4;

          d[3] = s[3];

          ++j;
        }

      ++i;
    }

  return with_alpha;
}

static gboolean
try_pixmap_and_mask (MetaDisplay *display,
                     Pixmap       src_pixmap,
                     Pixmap       src_mask,
                     GdkPixbuf  **iconp,
                     int          ideal_width,
                     int          ideal_height,
                     GdkPixbuf  **mini_iconp,
                     int          ideal_mini_width,
                     int          ideal_mini_height)
{
  GdkPixbuf *unscaled = NULL;
  GdkPixbuf *mask = NULL;
  int w, h, d;

  if (src_pixmap == None)
    return FALSE;

  meta_error_trap_push (display);

  get_pixmap_geometry (display, src_pixmap, &w, &h, &d);

  unscaled = meta_gdk_pixbuf_get_from_pixmap (src_pixmap,
                                              0, 0,
                                              w, h);

  /* A depth 1 pixmap has 0 background, and 1 foreground, but
   * cairo and meta_gdk_pixbuf_get_from_pixmap consider it
   * to be 0 transparent, 1 opaque */
  if (d == 1)
    apply_foreground_background (unscaled);

  if (unscaled && src_mask != None)
    {
      get_pixmap_geometry (display, src_mask, &w, &h, &d);
      if (d == 1)
        mask = meta_gdk_pixbuf_get_from_pixmap (src_mask,
                                                0, 0,
                                                w, h);
    }

  meta_error_trap_pop (display);

  if (mask)
    {
      GdkPixbuf *masked;

      masked = apply_mask (unscaled, mask);
      g_object_unref (G_OBJECT (unscaled));
      unscaled = masked;

      g_object_unref (G_OBJECT (mask));
      mask = NULL;
    }

  if (unscaled)
    {
      *iconp =
        gdk_pixbuf_scale_simple (unscaled,
                                 ideal_width > 0 ? ideal_width :
                                 gdk_pixbuf_get_width (unscaled),
                                 ideal_height > 0 ? ideal_height :
                                 gdk_pixbuf_get_height (unscaled),
                                 GDK_INTERP_BILINEAR);
      *mini_iconp =
        gdk_pixbuf_scale_simple (unscaled,
                                 ideal_mini_width > 0 ? ideal_mini_width :
                                 gdk_pixbuf_get_width (unscaled),
                                 ideal_mini_height > 0 ? ideal_mini_height :
                                 gdk_pixbuf_get_height (unscaled),
                                 GDK_INTERP_BILINEAR);

      g_object_unref (G_OBJECT (unscaled));
      
      if (*iconp && *mini_iconp)
        return TRUE;
      else
        {
          if (*iconp)
            g_object_unref (G_OBJECT (*iconp));
          if (*mini_iconp)
            g_object_unref (G_OBJECT (*mini_iconp));
          return FALSE;
        }
    }
  else
    return FALSE;
}

static void
get_kwm_win_icon (MetaDisplay *display,
                  Window       xwindow,
                  Pixmap      *pixmap,
                  Pixmap      *mask)
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

  meta_error_trap_push (display);
  icons = NULL;
  result = XGetWindowProperty (display->xdisplay, xwindow,
                               display->atom__KWM_WIN_ICON,
			       0, G_MAXLONG,
			       False,
                               display->atom__KWM_WIN_ICON,
			       &type, &format, &nitems,
			       &bytes_after, &data);
  icons = (Pixmap *)data;

  err = meta_error_trap_pop_with_return (display);
  if (err != Success ||
      result != Success)
    return;

  if (type != display->atom__KWM_WIN_ICON)
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
#if 0
  icon_cache->icon = NULL;
  icon_cache->mini_icon = NULL;
  icon_cache->ideal_width = -1; /* won't be a legit width */
  icon_cache->ideal_height = -1;
  icon_cache->ideal_mini_width = -1;
  icon_cache->ideal_mini_height = -1;
#endif
  icon_cache->want_fallback = TRUE;
  icon_cache->wm_hints_dirty = TRUE;
  icon_cache->kwm_win_icon_dirty = TRUE;
  icon_cache->net_wm_icon_dirty = TRUE;
}

static void
clear_icon_cache (MetaIconCache *icon_cache,
                  gboolean       dirty_all)
{
#if 0
  if (icon_cache->icon)
    g_object_unref (G_OBJECT (icon_cache->icon));
  icon_cache->icon = NULL;

  if (icon_cache->mini_icon)
    g_object_unref (G_OBJECT (icon_cache->mini_icon));
  icon_cache->mini_icon = NULL;
#endif
  
  icon_cache->origin = USING_NO_ICON;

  if (dirty_all)
    {
      icon_cache->wm_hints_dirty = TRUE;
      icon_cache->kwm_win_icon_dirty = TRUE;
      icon_cache->net_wm_icon_dirty = TRUE;
    }
}

void
meta_icon_cache_free (MetaIconCache *icon_cache)
{
  clear_icon_cache (icon_cache, FALSE);
}

void
meta_icon_cache_property_changed (MetaIconCache *icon_cache,
                                  MetaDisplay   *display,
                                  Atom           atom)
{
  if (atom == display->atom__NET_WM_ICON)
    icon_cache->net_wm_icon_dirty = TRUE;
  else if (atom == display->atom__KWM_WIN_ICON)
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
  else if (icon_cache->origin < USING_FALLBACK_ICON &&
           icon_cache->want_fallback)
    return TRUE;
  else if (icon_cache->origin == USING_NO_ICON)
    return TRUE;
  else if (icon_cache->origin == USING_FALLBACK_ICON &&
           !icon_cache->want_fallback)
    return TRUE;
  else
    return FALSE;
}

static void
replace_cache (MetaIconCache *icon_cache,
               IconOrigin     origin,
               GdkPixbuf     *new_icon,
               GdkPixbuf     *new_mini_icon)
{
  clear_icon_cache (icon_cache, FALSE);

  icon_cache->origin = origin;

#if 0
  if (new_icon)
    g_object_ref (G_OBJECT (new_icon));

  icon_cache->icon = new_icon;

  if (new_mini_icon)
    g_object_ref (G_OBJECT (new_mini_icon));

  icon_cache->mini_icon = new_mini_icon;
#endif
}

static GdkPixbuf*
scaled_from_pixdata (guchar *pixdata,
                     int     w,
                     int     h,
                     int     new_w,
                     int     new_h)
{
  GdkPixbuf *src;
  GdkPixbuf *dest;
  
  src = gdk_pixbuf_new_from_data (pixdata,
                                  GDK_COLORSPACE_RGB,
                                  TRUE,
                                  8,
                                  w, h, w * 4,
                                  free_pixels, 
                                  NULL);

  if (src == NULL)
    return NULL;

  if (w != h)
    {
      GdkPixbuf *tmp;
      int size;

      size = MAX (w, h);
      
      tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, size, size);

      if (tmp)
	{
	  gdk_pixbuf_fill (tmp, 0);
	  gdk_pixbuf_copy_area (src, 0, 0, w, h,
				tmp,
				(size - w) / 2, (size - h) / 2);
	  
	  g_object_unref (src);
	  src = tmp;
	}
    }
  
  if (w != new_w || h != new_h)
    {
      dest = gdk_pixbuf_scale_simple (src, new_w, new_h, GDK_INTERP_BILINEAR);
      
      g_object_unref (G_OBJECT (src));
    }
  else
    {
      dest = src;
    }

  return dest;
}

gboolean
meta_read_icons (MetaScreen     *screen,
                 Window          xwindow,
                 MetaIconCache  *icon_cache,
                 Pixmap          wm_hints_pixmap,
                 Pixmap          wm_hints_mask,
                 GdkPixbuf     **iconp,
                 int             ideal_width,
                 int             ideal_height,
                 GdkPixbuf     **mini_iconp,
                 int             ideal_mini_width,
                 int             ideal_mini_height)
{
  guchar *pixdata;
  int w, h;
  guchar *mini_pixdata;
  int mini_w, mini_h;
  Pixmap pixmap;
  Pixmap mask;

  /* Return value is whether the icon changed */

  g_return_val_if_fail (icon_cache != NULL, FALSE);

  *iconp = NULL;
  *mini_iconp = NULL;

#if 0
  if (ideal_width != icon_cache->ideal_width ||
      ideal_height != icon_cache->ideal_height ||
      ideal_mini_width != icon_cache->ideal_mini_width ||
      ideal_mini_height != icon_cache->ideal_mini_height)
    clear_icon_cache (icon_cache, TRUE);

  icon_cache->ideal_width = ideal_width;
  icon_cache->ideal_height = ideal_height;
  icon_cache->ideal_mini_width = ideal_mini_width;
  icon_cache->ideal_mini_height = ideal_mini_height;
#endif
  
  if (!meta_icon_cache_get_icon_invalidated (icon_cache))
    return FALSE; /* we have no new info to use */

  pixdata = NULL;

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

      if (read_rgb_icon (screen->display, xwindow,
                         ideal_width, ideal_height,
                         ideal_mini_width, ideal_mini_height,
                         &w, &h, &pixdata,
                         &mini_w, &mini_h, &mini_pixdata))
        {
          *iconp = scaled_from_pixdata (pixdata, w, h,
                                        ideal_width, ideal_height);

          *mini_iconp = scaled_from_pixdata (mini_pixdata, mini_w, mini_h,
                                             ideal_mini_width, ideal_mini_height);

          if (*iconp && *mini_iconp)
            {
              replace_cache (icon_cache, USING_NET_WM_ICON,
                             *iconp, *mini_iconp);
              
              return TRUE;
            }
          else
            {
              if (*iconp)
                g_object_unref (G_OBJECT (*iconp));
              if (*mini_iconp)
                g_object_unref (G_OBJECT (*mini_iconp));
            }
        }
    }

  if (icon_cache->origin <= USING_WM_HINTS &&
      icon_cache->wm_hints_dirty)
    {
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
          if (try_pixmap_and_mask (screen->display,
                                   pixmap, mask,
                                   iconp, ideal_width, ideal_height,
                                   mini_iconp, ideal_mini_width, ideal_mini_height))
            {
              icon_cache->prev_pixmap = pixmap;
              icon_cache->prev_mask = mask;

              replace_cache (icon_cache, USING_WM_HINTS,
                             *iconp, *mini_iconp);

              return TRUE;
            }
        }
    }

  if (icon_cache->origin <= USING_KWM_WIN_ICON &&
      icon_cache->kwm_win_icon_dirty)
    {
      icon_cache->kwm_win_icon_dirty = FALSE;

      get_kwm_win_icon (screen->display, xwindow, &pixmap, &mask);

      if ((pixmap != icon_cache->prev_pixmap ||
           mask != icon_cache->prev_mask) &&
          pixmap != None)
        {
          if (try_pixmap_and_mask (screen->display, pixmap, mask,
                                   iconp, ideal_width, ideal_height,
                                   mini_iconp, ideal_mini_width, ideal_mini_height))
            {
              icon_cache->prev_pixmap = pixmap;
              icon_cache->prev_mask = mask;

              replace_cache (icon_cache, USING_KWM_WIN_ICON,
                             *iconp, *mini_iconp);

              return TRUE;
            }
        }
    }

  if (icon_cache->want_fallback &&
      icon_cache->origin < USING_FALLBACK_ICON)
    {
      get_fallback_icons (screen,
                          iconp,
                          ideal_width,
                          ideal_height,
                          mini_iconp,
                          ideal_mini_width,
                          ideal_mini_height);

      replace_cache (icon_cache, USING_FALLBACK_ICON,
                     *iconp, *mini_iconp);
      
      return TRUE;
    }

  if (!icon_cache->want_fallback &&
      icon_cache->origin == USING_FALLBACK_ICON)
    {
      /* Get rid of current icon */
      clear_icon_cache (icon_cache, FALSE);

      return TRUE;
    }

  /* found nothing new */
  return FALSE;
}
