/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* 
 * Copyright (C) 2007 Iain Holmes
 * Based on xcompmgr - (c) 2003 Keith Packard
 *          xfwm4    - (c) 2005-2007 Olivier Fourdan
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
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include <gdk/gdk.h>

#include "display.h"
#include "screen.h"
#include "frame.h"
#include "errors.h"
#include "compositor.h"
#include "xprops.h"
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrender.h>

#if COMPOSITE_MAJOR > 0 || COMPOSITE_MINOR >= 2
#define HAVE_NAME_WINDOW_PIXMAP 1
#endif

#if COMPOSITE_MAJOR > 0 || COMPOSITE_MINOR >= 3
#define HAVE_COW 1
#else
/* Don't have a cow man...HAAHAAHAA */
#endif

#define USE_IDLE_REPAINT 1

typedef enum _MetaCompWindowType
{
  META_COMP_WINDOW_NORMAL,
  META_COMP_WINDOW_DND,
  META_COMP_WINDOW_DESKTOP,
  META_COMP_WINDOW_DOCK
} MetaCompWindowType;

struct _MetaCompositor 
{
  MetaDisplay *display;

  Atom atom_x_root_pixmap;
  Atom atom_x_set_root;
  Atom atom_net_wm_window_opacity;
  Atom atom_net_wm_window_type_dnd;

#ifdef USE_IDLE_REPAINT
  guint repaint_id;
#endif
  guint enabled : 1;
  guint show_redraw : 1;
  guint debug : 1;
};

typedef struct _conv 
{
  int size;
  double *data;
} conv;

typedef struct _MetaCompScreen 
{
  MetaScreen *screen;
  GList *windows;
  GHashTable *windows_by_xid;

  Window output;

  gboolean have_shadows;
  conv *gaussian_map;
  guchar *shadow_corner;
  guchar *shadow_top;

  Picture root_picture;
  Picture root_buffer;
  Picture black_picture;
  Picture trans_black_picture;
  Picture root_tile;
  XserverRegion all_damage;

  guint overlays;
  gboolean compositor_active;
  gboolean clip_changed;

  GSList *dock_windows;
} MetaCompScreen;

typedef struct _MetaCompWindow 
{
  MetaScreen *screen;
  MetaWindow *window; /* May be NULL if this window isn't managed by Metacity */
  Window id;
  XWindowAttributes attrs;

#ifdef HAVE_NAME_WINDOW_PIXMAP
  Pixmap back_pixmap;

  /* When the window is shaded back_pixmap will be replaced with the pixmap
     for the shaded window. This is a copy of the original unshaded window
     so that we can still see what the window looked like when it is needed 
     for the _get_window_pixmap function */
  Pixmap shaded_back_pixmap;
#endif

  int mode;

  gboolean damaged;
  gboolean shaped;
  gboolean needs_shadow;

  MetaCompWindowType type;

  Damage damage;
  Picture picture;
  Picture alpha_pict;
  Picture shadow_pict;

  XserverRegion border_size;
  XserverRegion extents;

  Picture shadow;
  int shadow_dx;
  int shadow_dy;
  int shadow_width;
  int shadow_height;

  guint opacity;

  XserverRegion border_clip;

  gboolean updates_frozen;
  gboolean update_pending;
} MetaCompWindow;

#define OPAQUE 0xffffffff

#define WINDOW_SOLID 0
#define WINDOW_ARGB 1

#define SHADOW_RADIUS 6.0
#define SHADOW_OFFSET_X (SHADOW_RADIUS * -3 / 2)
#define SHADOW_OFFSET_Y (SHADOW_RADIUS * -5 / 4)

#define SHADOW_OPACITY 0.66
 
#define TRANS_OPACITY 0.75

/* Gaussian stuff for creating the shadows */
static double
gaussian (double r,
          double x,
          double y)
{
  return ((1 / (sqrt (2 * G_PI * r))) *
          exp ((- (x * x + y * y)) / (2 * r * r)));
}

static conv *
make_gaussian_map (double r)
{
  conv *c;
  int size, centre;
  int x, y;
  double t, g;

  size = ((int) ceil ((r * 3)) + 1) & ~1;
  centre = size / 2;
  c = g_malloc (sizeof (conv) + size * size * sizeof (double));
  c->size = size;
  c->data = (double *) (c + 1);
  t = 0.0;

  for (y = 0; y < size; y++) 
    {
      for (x = 0; x < size; x++) 
        {
          g = gaussian (r, (double) (x - centre), (double) (y - centre));
          t += g;
          c->data[y * size + x] = g;
        }
    }
  
  for (y = 0; y < size; y++) 
    {
      for (x = 0; x < size; x++) 
        {
          c->data[y * size + x] /= t;
        }
    }
  
  return c;
}

static void
dump_xserver_region (const char   *location, 
                     MetaDisplay  *display,
                     XserverRegion region)
{
  MetaCompositor *compositor = display->compositor;
  Display *dpy = display->xdisplay; 
  int nrects;
  XRectangle *rects;
  XRectangle bounds;

  if (!compositor->debug)
    return;

  if (region)
    {
      rects = XFixesFetchRegionAndBounds (dpy, region, &nrects, &bounds);
      if (nrects > 0)
        {
          int i;
          fprintf (stderr, "%s (XSR): %d rects, bounds: %d,%d (%d,%d)\n",
                   location, nrects, bounds.x, bounds.y, bounds.width, bounds.height);
          for (i = 1; i < nrects; i++)
            fprintf (stderr, "\t%d,%d (%d,%d)\n",
                     rects[i].x, rects[i].y, rects[i].width, rects[i].height);
        }
      else
        fprintf (stderr, "%s (XSR): empty\n", location);
      XFree (rects);
    }
  else
    fprintf (stderr, "%s (XSR): null\n", location);
}

/*
* A picture will help
*
*      -center   0                width  width+center
*  -center +-----+-------------------+-----+
*          |     |                   |     |
*          |     |                   |     |
*        0 +-----+-------------------+-----+
*          |     |                   |     |
*          |     |                   |     |
*          |     |                   |     |
*   height +-----+-------------------+-----+
*          |     |                   |     |
* height+  |     |                   |     |
*  center  +-----+-------------------+-----+
*/
static guchar
sum_gaussian (conv          *map,
              double         opacity,
              int            x,
              int            y,
              int            width,
              int            height)
{
  double *g_data, *g_line;
  double v;
  int fx, fy;
  int fx_start, fx_end;
  int fy_start, fy_end;
  int g_size, centre;

  g_line = map->data;
  g_size = map->size;
  centre = g_size / 2;
  fx_start = centre - x;
  if (fx_start < 0) 
    fx_start = 0;

  fx_end = width + centre - x;
  if (fx_end > g_size) 
    fx_end = g_size;

  fy_start = centre - y;
  if (fy_start < 0)
    fy_start = 0;

  fy_end = height + centre - y;
  if (fy_end > g_size) 
    fy_end = g_size;

  g_line = g_line + fy_start * g_size + fx_start;

  v = 0.0;
  for (fy = fy_start; fy < fy_end; fy++) 
    {
      g_data = g_line;
      g_line += g_size;
      
      for (fx = fx_start; fx < fx_end; fx++)
        v += *g_data++;
    }
  
  if (v > 1.0)
    v = 1.0;
  
  return ((guchar) (v * opacity * 255.0));
}

/* precompute shadow corners and sides to save time for large windows */
static void
presum_gaussian (MetaCompScreen *info)
{
  int centre;
  int opacity, x, y;
  int msize;
  conv *map;

  map = info->gaussian_map;
  msize = map->size;
  centre = map->size / 2;

  if (info->shadow_corner)
    g_free (info->shadow_corner);
  if (info->shadow_top)
    g_free (info->shadow_top);

  info->shadow_corner = (guchar *)(g_malloc ((msize + 1) * (msize + 1) * 26));
  info->shadow_top = (guchar *) (g_malloc ((msize + 1) * 26));
  
  for (x = 0; x <= msize; x++) 
    {
      
      info->shadow_top[25 * (msize + 1) + x] =
        sum_gaussian (map, 1, x - centre, centre, msize * 2, msize * 2);
      for (opacity = 0; opacity < 25; opacity++) 
        {
          info->shadow_top[opacity * (msize + 1) + x] =
            info->shadow_top[25 * (msize + 1) + x] * opacity / 25;
        }
      
      for (y = 0; y <= x; y++) 
        {
          info->shadow_corner[25 * (msize + 1) * (msize + 1) 
                              + y * (msize + 1) 
                              + x]
            = sum_gaussian (map, 1, x - centre, y - centre,
                            msize * 2, msize * 2);
          
          info->shadow_corner[25 * (msize + 1) * (msize + 1) 
                              + x * (msize + 1) + y] =
            info->shadow_corner[25 * (msize + 1) * (msize + 1) 
                                + y * (msize + 1) + x];
          
          for (opacity = 0; opacity < 25; opacity++) 
            {
              info->shadow_corner[opacity * (msize + 1) * (msize + 1) 
                                  + y * (msize + 1) + x]
                = info->shadow_corner[opacity * (msize + 1) * (msize + 1) 
                                      + x * (msize + 1) + y]
                = info->shadow_corner[25 * (msize + 1) * (msize + 1) 
                                      + y * (msize + 1) + x] * opacity / 25;
            }
        }
    }
}

static XImage *
make_shadow (MetaDisplay *display,
             MetaScreen  *screen,
             double       opacity,
             int          width,
             int          height)
{
  MetaCompScreen *info = screen->compositor_data;
  XImage *ximage;
  guchar *data;
  int msize = info->gaussian_map->size;
  int ylimit, xlimit;
  int swidth = width + msize;
  int sheight = height + msize;
  int centre = msize / 2;
  int x, y;
  guchar d;
  int x_diff;
  int opacity_int = (int)(opacity * 25);

  data = g_malloc (swidth * sheight * sizeof (guchar));

  ximage = XCreateImage (display->xdisplay, 
                         DefaultVisual (display->xdisplay, screen->number),
                         8, ZPixmap, 0, (char *) data,
                         swidth, sheight, 8, swidth * sizeof (guchar));
  if (!ximage) 
    {
      g_free (data);
      return NULL;
    }

  /*
   * Build the gaussian in sections
   */

  /*
   * centre (fill the complete data array
   */
  if (msize > 0)
    d = info->shadow_top[opacity_int * (msize + 1) + msize];
  else
    d = sum_gaussian (info->gaussian_map, opacity, centre, 
                      centre, width, height);
  memset (data, d, sheight * swidth);

  /*
   * corners
   */
  ylimit = msize;
  if (ylimit > sheight / 2)
    ylimit = (sheight + 1) / 2;

  xlimit = msize;
  if (xlimit > swidth / 2)
    xlimit = (swidth + 1) / 2;

  for (y = 0; y < ylimit; y++) 
    {
      for (x = 0; x < xlimit; x++) 
        {
          
          if (xlimit == msize && ylimit == msize)
            d = info->shadow_corner[opacity_int * (msize + 1) * (msize + 1) + y * (msize + 1) + x]; 
          else
            d = sum_gaussian (info->gaussian_map, opacity, x - centre, 
                              y - centre, width, height);
          
          data[y * swidth + x] = d;
          data[(sheight - y - 1) * swidth + x] = d;
          data[(sheight - y - 1) * swidth + (swidth - x - 1)] = d;
          data[y * swidth + (swidth - x - 1)] = d;
        }
    }
  
  /* top/bottom */
  x_diff = swidth - (msize * 2);
  if (x_diff > 0 && ylimit > 0) 
    {
      for (y = 0; y < ylimit; y++) 
        {
          if (ylimit == msize)
            d = info->shadow_top[opacity_int * (msize + 1) + y];
          else
            d = sum_gaussian (info->gaussian_map, opacity, centre, 
                              y - centre, width, height);

          memset (&data[y * swidth + msize], d, x_diff);
          memset (&data[(sheight - y - 1) * swidth + msize], d, x_diff);
        }
    }
  
  /*
   * sides
   */
  for (x = 0; x < xlimit; x++) 
    {
      if (xlimit == msize)
        d = info->shadow_top[opacity_int * (msize + 1) + x];
      else
        d = sum_gaussian (info->gaussian_map, opacity, x - centre, 
                        centre, width, height);
    
      for (y = msize; y < sheight - msize; y++) 
        {
          data[y * swidth + x] = d;
          data[y * swidth + (swidth - x - 1)] = d;
        }
    }
  
  return ximage;
}

static Picture
shadow_picture (MetaDisplay *display,
                MetaScreen  *screen,
                double       opacity,
                Picture      alpha_pict,
                int          width,
                int          height,
                int         *wp,
                int         *hp)
{
  XImage *shadow_image;
  Pixmap shadow_pixmap;
  Picture shadow_picture;
  GC gc;

  shadow_image = make_shadow (display, screen, opacity, width, height);
  if (!shadow_image)
    return None;

  shadow_pixmap = XCreatePixmap (display->xdisplay, screen->xroot,
                                 shadow_image->width, shadow_image->height, 8);
  if (!shadow_pixmap) 
    {
      XDestroyImage (shadow_image);
      return None;
    }

  shadow_picture = XRenderCreatePicture (display->xdisplay, shadow_pixmap,
                                         XRenderFindStandardFormat (display->xdisplay, PictStandardA8),
                                         0, 0);
  if (!shadow_picture) 
    {
      XDestroyImage (shadow_image);
      XFreePixmap (display->xdisplay, shadow_pixmap);
      return None;
    }
  
  gc = XCreateGC (display->xdisplay, shadow_pixmap, 0, 0);
  if (!gc) 
    {
      XDestroyImage (shadow_image);
      XFreePixmap (display->xdisplay, shadow_pixmap);
      XRenderFreePicture (display->xdisplay, shadow_picture);
      return None;
    }

  XPutImage (display->xdisplay, shadow_pixmap, gc, shadow_image, 0, 0, 0, 0,
             shadow_image->width, shadow_image->height);
  *wp = shadow_image->width;
  *hp = shadow_image->height;
  
  XFreeGC (display->xdisplay, gc);
  XDestroyImage (shadow_image);
  XFreePixmap (display->xdisplay, shadow_pixmap);
  
  return shadow_picture;
}

static MetaCompWindow *
find_window_for_screen (MetaScreen *screen,
                        Window      xwindow)
{
  MetaCompScreen *info = screen->compositor_data;

  if (info == NULL)
    return NULL;
  
  return g_hash_table_lookup (info->windows_by_xid, (gpointer) xwindow);
}

static MetaCompWindow *
find_window_in_display (MetaDisplay *display,
                        Window       xwindow)
{
  GSList *index;

  for (index = display->screens; index; index = index->next) 
    {
      MetaCompWindow *cw = find_window_for_screen (index->data, xwindow);

      if (cw != NULL)
        return cw;
    }
  
  return NULL;
}

static Picture
solid_picture (MetaDisplay *display,
               MetaScreen  *screen,
               gboolean     argb,
               double       a,
               double       r,
               double       g,
               double       b)
{
  Pixmap pixmap;
  Picture picture;
  XRenderPictureAttributes pa;
  XRenderPictFormat *render_format;
  XRenderColor c;

  render_format = XRenderFindStandardFormat (display->xdisplay,
                                             argb ? PictStandardARGB32 : PictStandardA8);

  pixmap = XCreatePixmap (display->xdisplay, screen->xroot, 
                          1, 1, argb ? 32 : 8);
  g_return_val_if_fail (pixmap != None, None);

  pa.repeat = TRUE;
  picture = XRenderCreatePicture (display->xdisplay, pixmap, render_format,
                                  CPRepeat, &pa);
  if (picture == None) 
    {
      XFreePixmap (display->xdisplay, pixmap);
      g_warning ("(picture != None) failed");
      return None;
    }

  c.alpha = a * 0xffff;
  c.red = r * 0xffff;
  c.green = g * 0xffff;
  c.blue = b * 0xffff;
  
  XRenderFillRectangle (display->xdisplay, PictOpSrc, picture, &c, 0, 0, 1, 1);
  XFreePixmap (display->xdisplay, pixmap);
  
  return picture;
}

static Picture
root_tile (MetaScreen *screen)
{
  MetaDisplay *display = screen->display;
  Picture picture;
  Pixmap pixmap;
  gboolean fill = FALSE;
  XRenderPictureAttributes pa;
  XRenderPictFormat *format;
  int p;
  Atom background_atoms[2];
  Atom pixmap_atom;

  pixmap = None;
  background_atoms[0] = display->compositor->atom_x_root_pixmap;
  background_atoms[1] = display->compositor->atom_x_set_root;

  pixmap_atom = XInternAtom (display->xdisplay, "PIXMAP", False);
  for (p = 0; p < 2; p++) 
    {
      Atom actual_type;
      int actual_format;
      gulong nitems, bytes_after;
      guchar *prop;
      
      if (XGetWindowProperty (display->xdisplay, screen->xroot, 
                              background_atoms[p],
                              0, 4, FALSE, AnyPropertyType,
                              &actual_type, &actual_format, 
                              &nitems, &bytes_after, &prop) == Success)
        {
          if (actual_type == pixmap_atom &&
              actual_format == 32 &&
              nitems == 1) 
            {
              memcpy (&pixmap, prop, 4);
              XFree (prop);
              fill = FALSE;
              break;
            }
        } 
    }

  if (!pixmap) 
    {
      pixmap = XCreatePixmap (display->xdisplay, screen->xroot, 1, 1, 
                              DefaultDepth (display->xdisplay, screen->number));
      g_return_val_if_fail (pixmap != None, None);
      fill = TRUE;
    }
  
  pa.repeat = TRUE;
  format = XRenderFindVisualFormat (display->xdisplay, 
                                    DefaultVisual (display->xdisplay,
                                                   screen->number));
  g_return_val_if_fail (format != NULL, None);
  
  picture = XRenderCreatePicture (display->xdisplay, pixmap, format, 
                                  CPRepeat, &pa);
  if ((picture != None) && (fill)) 
    {
      XRenderColor c;

      /* Background default to just plain ugly grey */
      c.red = 0x8080;
      c.green = 0x8080;
      c.blue = 0x8080;
      c.alpha = 0xffff;
      
      XRenderFillRectangle (display->xdisplay, PictOpSrc, picture,
                            &c, 0, 0, 1, 1);
      XFreePixmap (display->xdisplay, pixmap); 
    }

  return picture;
}
  
static Picture
create_root_buffer (MetaScreen *screen) 
{
  MetaDisplay *display = screen->display;
  MetaCompScreen *info = screen->compositor_data;
  Picture pict;
  XRenderPictFormat *format;
  Pixmap root_pixmap;
  Visual *visual;
  int depth, screen_width, screen_height, screen_number;

  screen_width = screen->rect.width;
  screen_height = screen->rect.height;
  screen_number = screen->number;
  visual = DefaultVisual (display->xdisplay, screen_number);
  depth = DefaultDepth (display->xdisplay, screen_number);

  format = XRenderFindVisualFormat (display->xdisplay, visual);
  g_return_val_if_fail (format != NULL, None);

  root_pixmap = XCreatePixmap (display->xdisplay,
                               info->output,
                               screen_width, screen_height, depth);
  g_return_val_if_fail (root_pixmap != None, None);

  pict = XRenderCreatePicture (display->xdisplay, root_pixmap, format, 0, NULL);
  XFreePixmap (display->xdisplay, root_pixmap);

  return pict;
}

static void
paint_root (MetaScreen *screen,
            Picture     root_buffer)
{
  MetaDisplay *display = screen->display;
  MetaCompScreen *info = screen->compositor_data;

  g_return_if_fail (root_buffer != None);

  if (info->root_tile == None) 
    {
      info->root_tile = root_tile (screen);
      g_return_if_fail (info->root_tile != None);
    }
  
  XRenderComposite (display->xdisplay, PictOpSrc,
                    info->root_tile, None, root_buffer,
                    0, 0, 0, 0, 0, 0,
                    screen->rect.width, screen->rect.height);
}

static gboolean
window_has_shadow (MetaCompWindow *cw)
{
  if (((MetaCompScreen *)cw->screen->compositor_data)->have_shadows == FALSE) 
    return FALSE;

  /* Always put a shadow around windows with a frame */
  if (cw->window) 
    {
      if (cw->window->frame) {
        meta_verbose ("Window has shadow because it has a frame\n");
        return TRUE;
      }
    }

  /* Don't put shadow around DND icon windows */
  if (cw->type == META_COMP_WINDOW_DND ||
      cw->type == META_COMP_WINDOW_DESKTOP) {
    meta_verbose ("Window has no shadow as it is DND or Desktop\n");
    return FALSE;
  }

  if (cw->mode != WINDOW_ARGB) {
    meta_verbose ("Window has shadow as it is not ARGB\n");
    return TRUE;
  }
  
  meta_verbose ("Window has no shadow as it fell through\n");
  return FALSE;
}

static XserverRegion
win_extents (MetaCompWindow *cw)
{
  MetaDisplay *display;
  MetaScreen *screen;
  MetaCompScreen *info;
  XRectangle r;

  screen = cw->screen;
  display = screen->display;
  info = screen->compositor_data;

  r.x = cw->attrs.x;
  r.y = cw->attrs.y;
  r.width = cw->attrs.width + cw->attrs.border_width * 2;
  r.height = cw->attrs.height + cw->attrs.border_width * 2;

  if (cw->needs_shadow)
    {
      XRectangle sr;

      cw->shadow_dx = SHADOW_OFFSET_X;
      cw->shadow_dy = SHADOW_OFFSET_Y;

      if (!cw->shadow) 
        {
          double opacity = SHADOW_OPACITY;
          if (cw->opacity != (guint) OPAQUE)
            opacity = opacity * ((double) cw->opacity) / ((double) OPAQUE);
          
          cw->shadow = shadow_picture (display, screen, opacity, cw->alpha_pict,
                                       cw->attrs.width + cw->attrs.border_width * 2,
                                       cw->attrs.height + cw->attrs.border_width * 2,
                                       &cw->shadow_width, &cw->shadow_height);
        }
      
      sr.x = cw->attrs.x + cw->shadow_dx;
      sr.y = cw->attrs.y + cw->shadow_dy;
      sr.width = cw->shadow_width;
      sr.height = cw->shadow_height;
      
      if (sr.x < r.x) 
        {
          r.width = (r.x + r.width) - sr.x;
          r.x = sr.x;
        }
      
      if (sr.y < r.y) 
        {
          r.height = (r.y + r.height) - sr.y;
          r.y = sr.y;
        }
      
      if (sr.x + sr.width > r.x + r.width)
        r.width = sr.x + sr.width - r.x;
      
      if (sr.y + sr.height > r.y + r.height) 
        r.height = sr.y + sr.height - r.y;
    }
  
  return XFixesCreateRegion (display->xdisplay, &r, 1);
}

static XserverRegion
border_size (MetaCompWindow *cw)
{
  MetaScreen *screen = cw->screen;
  MetaDisplay *display = screen->display;
  XserverRegion border;

  meta_error_trap_push (display);
  border = XFixesCreateRegionFromWindow (display->xdisplay, cw->id,
                                         WindowRegionBounding);
  meta_error_trap_pop (display, FALSE);

  g_return_val_if_fail (border != None, None);
  XFixesTranslateRegion (display->xdisplay, border,
                         cw->attrs.x + cw->attrs.border_width,
                         cw->attrs.y + cw->attrs.border_width);
  return border;
}

static XRenderPictFormat *
get_window_format (MetaCompWindow *cw)
{
  MetaScreen *screen = cw->screen;
  MetaDisplay *display = screen->display;
  XRenderPictFormat *format;

  format = XRenderFindVisualFormat (display->xdisplay, cw->attrs.visual);
  if (!format)
    format = XRenderFindVisualFormat (display->xdisplay,
                                      DefaultVisual (display->xdisplay,
                                                     screen->number));
  return format;
}

static Picture
get_window_picture (MetaCompWindow *cw)
{
  MetaScreen *screen = cw->screen;
  MetaDisplay *display = screen->display;
  XRenderPictureAttributes pa;
  XRenderPictFormat *format;
  Drawable draw;

  draw = cw->id;

  meta_error_trap_push (display);

#ifdef HAVE_NAME_WINDOW_PIXMAP
  if (cw->back_pixmap == None)
    cw->back_pixmap = XCompositeNameWindowPixmap (display->xdisplay, cw->id);

  if (cw->back_pixmap != None)
    draw = cw->back_pixmap;
#endif

  format = get_window_format (cw);
  if (format) 
    {
      Picture pict;

      pa.subwindow_mode = IncludeInferiors;

      pict = XRenderCreatePicture (display->xdisplay, draw, 
                                   format, CPSubwindowMode, &pa);
      meta_error_trap_pop (display, FALSE);

      return pict;
    }

  meta_error_trap_pop (display, FALSE);
  return None;
}

static void
paint_dock_shadows (MetaScreen   *screen,
                    Picture       root_buffer,
                    XserverRegion region)
{
  MetaDisplay *display = screen->display;
  Display *xdisplay = display->xdisplay;
  MetaCompScreen *info = screen->compositor_data;
  GSList *d;

  for (d = info->dock_windows; d; d = d->next) 
    {
      MetaCompWindow *cw = d->data;
      XserverRegion shadow_clip;

      if (cw->shadow)
        {
          shadow_clip = XFixesCreateRegion (xdisplay, NULL, 0);
          XFixesIntersectRegion (xdisplay, shadow_clip, 
                                 cw->border_clip, region);
          
          XFixesSetPictureClipRegion (xdisplay, root_buffer, 0, 0, shadow_clip);

          XRenderComposite (xdisplay, PictOpOver, info->black_picture,
                            cw->shadow, root_buffer,
                            0, 0, 0, 0,
                            cw->attrs.x + cw->shadow_dx,
                            cw->attrs.y + cw->shadow_dy,
                            cw->shadow_width, cw->shadow_height);
          XFixesDestroyRegion (xdisplay, shadow_clip);
        }
    }
}

static void
paint_windows (MetaScreen   *screen,
               GList        *windows,
               Picture       root_buffer,
               XserverRegion region)
{
  MetaDisplay *display = screen->display;
  MetaCompScreen *info = screen->compositor_data;
  Display *xdisplay = display->xdisplay;
  GList *index, *last;
  int screen_width, screen_height, screen_number;
  Window xroot;
  MetaCompWindow *cw;
  XserverRegion paint_region, desktop_region;

  screen_width = screen->rect.width;
  screen_height = screen->rect.height;
  screen_number = screen->number;
  xroot = screen->xroot;

  if (region == None) 
    {
      XRectangle r;
      r.x = 0;
      r.y = 0;
      r.width = screen_width;
      r.height = screen_height;
      paint_region = XFixesCreateRegion (xdisplay, &r, 1);
    } 
  else
    {
      paint_region = XFixesCreateRegion (xdisplay, NULL, 0);
      XFixesCopyRegion (xdisplay, paint_region, region);
    }

  desktop_region = None;

  /*
   * Painting from top to bottom, reducing the clipping area at 
   * each iteration. Only the opaque windows are painted 1st.
   */
  last = NULL;
  for (index = windows; index; index = index->next) 
    {
      /* Store the last window we dealt with */
      last = index;

      cw = (MetaCompWindow *) index->data;
      if (!cw->damaged) 
        {
          /* Not damaged */
          continue;
        }

#if 0
      if ((cw->attrs.x + cw->attrs.width < 1) ||
          (cw->attrs.y + cw->attrs.height < 1) ||
          (cw->attrs.x >= screen_width) || (cw->attrs.y >= screen_height)) 
        {
          /* Off screen */
          continue;
        }
#endif

      if (cw->picture == None) 
        cw->picture = get_window_picture (cw);

      /* If the clip region of the screen has been changed
         then we need to recreate the extents of the window */
      if (info->clip_changed) 
        {
          if (cw->border_size) 
            {
              XFixesDestroyRegion (xdisplay, cw->border_size);
              cw->border_size = None;
            }

#if 0          
          if (cw->extents) 
            {
              XFixesDestroyRegion (xdisplay, cw->extents);
              cw->extents = None;
            }
#endif
        }
      
      if (cw->border_size == None)
        cw->border_size = border_size (cw);
      
      if (cw->extents == None)
        cw->extents = win_extents (cw);
      
      if (cw->mode == WINDOW_SOLID) 
        {
          int x, y, wid, hei;
          
#ifdef HAVE_NAME_WINDOW_PIXMAP
          x = cw->attrs.x;
          y = cw->attrs.y;
          wid = cw->attrs.width + cw->attrs.border_width * 2;
          hei = cw->attrs.height + cw->attrs.border_width * 2;
#else
          x = cw->attrs.x + cw->attrs.border_width;
          y = cw->attrs.y + cw->attrs.border_width;
          wid = cw->attrs.width;
          hei = cw->attrs.height;
#endif
          
          XFixesSetPictureClipRegion (xdisplay, root_buffer, 
                                      0, 0, paint_region);
          XRenderComposite (xdisplay, PictOpSrc, cw->picture, 
                            None, root_buffer, 0, 0, 0, 0,
                            x, y, wid, hei);

          if (cw->type == META_COMP_WINDOW_DESKTOP) 
            {
              desktop_region = XFixesCreateRegion (xdisplay, 0, 0);
              XFixesCopyRegion (xdisplay, desktop_region, paint_region);
            }

          XFixesSubtractRegion (xdisplay, paint_region, 
                                paint_region, cw->border_size);
        }
      
      if (!cw->border_clip) 
        {
          cw->border_clip = XFixesCreateRegion (xdisplay, 0, 0);
          XFixesCopyRegion (xdisplay, cw->border_clip, paint_region);
        }
    }
  
  XFixesSetPictureClipRegion (xdisplay, root_buffer, 0, 0, paint_region);
  paint_root (screen, root_buffer);

  paint_dock_shadows (screen, root_buffer, desktop_region == None ?
                      paint_region : desktop_region);
  if (desktop_region != None)
    XFixesDestroyRegion (xdisplay, desktop_region);

  /* 
   * Painting from bottom to top, translucent windows and shadows are painted
   */
  for (index = last; index; index = index->prev) 
    { 
      cw = (MetaCompWindow *) index->data;
      
      if (cw->picture) 
        {
          if (cw->shadow && cw->type != META_COMP_WINDOW_DOCK) 
            {
              XserverRegion shadow_clip;

              shadow_clip = XFixesCreateRegion (xdisplay, NULL, 0);
              XFixesSubtractRegion (xdisplay, shadow_clip, cw->border_clip,
                                    cw->border_size);
              XFixesSetPictureClipRegion (xdisplay, root_buffer, 0, 0, 
                                          shadow_clip);
              
              XRenderComposite (xdisplay, PictOpOver, info->black_picture,
                                cw->shadow, root_buffer,
                                0, 0, 0, 0,
                                cw->attrs.x + cw->shadow_dx,
                                cw->attrs.y + cw->shadow_dy,
                                cw->shadow_width, cw->shadow_height);
              if (shadow_clip)
                XFixesDestroyRegion (xdisplay, shadow_clip);
            }

          if ((cw->opacity != (guint) OPAQUE) && !(cw->alpha_pict)) 
            {
              cw->alpha_pict = solid_picture (display, screen, FALSE,
                                              (double) cw->opacity / OPAQUE,
                                              0, 0, 0);
            }
          
          XFixesIntersectRegion (xdisplay, cw->border_clip, cw->border_clip, 
                                 cw->border_size);
          XFixesSetPictureClipRegion (xdisplay, root_buffer, 0, 0,
                                      cw->border_clip);
          if (cw->mode == WINDOW_ARGB) 
            {
              int x, y, wid, hei;
#ifdef HAVE_NAME_WINDOW_PIXMAP
              x = cw->attrs.x;
              y = cw->attrs.y;
              wid = cw->attrs.width + cw->attrs.border_width * 2;
              hei = cw->attrs.height + cw->attrs.border_width * 2;
#else
              x = cw->attrs.x + cw->attrs.border_width;
              y = cw->attrs.y + cw->attrs.border_width;
              wid = cw->attrs.width;
              hei = cw->attrs.height;
#endif
              
              XRenderComposite (xdisplay, PictOpOver, cw->picture, 
                                cw->alpha_pict, root_buffer, 0, 0, 0, 0,
                                x, y, wid, hei);
            } 
        }
      
      if (cw->border_clip) 
        {
          XFixesDestroyRegion (xdisplay, cw->border_clip);
          cw->border_clip = None;
        }
    }

  XFixesDestroyRegion (xdisplay, paint_region);
}

static void
paint_all (MetaScreen   *screen,
           XserverRegion region)
{
  MetaCompScreen *info = screen->compositor_data;
  Display *xdisplay = screen->display->xdisplay;
  int screen_width, screen_height;

  /* Set clipping to the given region */
  XFixesSetPictureClipRegion (xdisplay, info->root_picture, 0, 0, region);

  screen_width = screen->rect.width;
  screen_height = screen->rect.height;

  if (screen->display->compositor->show_redraw)
    {
      Picture overlay;

      dump_xserver_region ("paint_all", screen->display, region);

      /* Make a random colour overlay */
      overlay = solid_picture (screen->display, screen, TRUE, 1, /* 0.3, alpha */
                               ((double) (rand () % 100)) / 100.0,
                               ((double) (rand () % 100)) / 100.0,
                               ((double) (rand () % 100)) / 100.0);
      
      XRenderComposite (xdisplay, PictOpOver, overlay, None, info->root_picture,
                        0, 0, 0, 0, 0, 0, screen_width, screen_height);
      XRenderFreePicture (xdisplay, overlay);
      XFlush (xdisplay);
      usleep (100 * 1000);
    }
  
  if (info->root_buffer == None) 
    info->root_buffer = create_root_buffer (screen);
      
  paint_windows (screen, info->windows, info->root_buffer, region);

  XFixesSetPictureClipRegion (xdisplay, info->root_buffer, 0, 0, region);
  XRenderComposite (xdisplay, PictOpSrc, info->root_buffer, None,
                    info->root_picture, 0, 0, 0, 0, 0, 0, 
                    screen_width, screen_height);
}

static void
repair_screen (MetaScreen *screen)
{
  MetaCompScreen *info = screen->compositor_data;
  MetaDisplay *display = screen->display;

  if (info->all_damage != None) 
    {
      meta_error_trap_push (display);
      paint_all (screen, info->all_damage);
      XFixesDestroyRegion (display->xdisplay, info->all_damage);
      info->all_damage = None;
      info->clip_changed = FALSE;
      meta_error_trap_pop (display, FALSE);
    }
}

static void
repair_display (MetaDisplay *display)
{
  GSList *screens;
  
#ifdef USE_IDLE_REPAINT
  if (display->compositor->repaint_id > 0) 
    {
      g_source_remove (display->compositor->repaint_id);
      display->compositor->repaint_id = 0;
    }
#endif

  for (screens = display->screens; screens; screens = screens->next)
    repair_screen ((MetaScreen *) screens->data);
}

#ifdef USE_IDLE_REPAINT
static gboolean
compositor_idle_cb (gpointer data)
{
  MetaCompositor *compositor = (MetaCompositor *) data;

  compositor->repaint_id = 0;
  repair_display (compositor->display);

  return FALSE;
}

static void
add_repair (MetaDisplay *display)
{
  MetaCompositor *compositor = display->compositor;

  if (compositor->repaint_id > 0)
    return;

#if 0
  compositor->repaint_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                                            compositor_idle_cb, compositor,
                                            NULL);
#else
  /* Limit it to 50fps */
  compositor->repaint_id = g_timeout_add_full (G_PRIORITY_HIGH, 20,
                                               compositor_idle_cb, compositor,
                                               NULL);
#endif
}
#endif

static void
add_damage (MetaScreen     *screen,
            XserverRegion   damage)
{
  MetaDisplay *display = screen->display;
  MetaCompScreen *info = screen->compositor_data;

//  dump_xserver_region ("add_damage", display, damage);

  if (info->all_damage) 
    {
      XFixesUnionRegion (display->xdisplay, info->all_damage, 
                         info->all_damage, damage);
      XFixesDestroyRegion (display->xdisplay, damage);
    } 
  else
    info->all_damage = damage;

#ifdef USE_IDLE_REPAINT
  add_repair (display);
#endif
}

static void
damage_screen (MetaScreen *screen)
{
  MetaDisplay *display = screen->display;
  XserverRegion region;
  XRectangle r;

  r.x = 0;
  r.y = 0;
  r.width = screen->rect.width;
  r.height = screen->rect.height;

  region = XFixesCreateRegion (display->xdisplay, &r, 1);
  dump_xserver_region ("damage_screen", display, region);
  add_damage (screen, region);
}

static void
repair_win (MetaCompWindow *cw)
{
  MetaScreen *screen = cw->screen;
  MetaDisplay *display = screen->display;
  XserverRegion parts;

  meta_error_trap_push (display);
  if (!cw->damaged) 
    {
      parts = win_extents (cw);
      XDamageSubtract (display->xdisplay, cw->damage, None, None);
    } 
  else 
    {
      parts = XFixesCreateRegion (display->xdisplay, 0, 0);
      XDamageSubtract (display->xdisplay, cw->damage, None, parts);
      XFixesTranslateRegion (display->xdisplay, parts,
                             cw->attrs.x + cw->attrs.border_width,
                             cw->attrs.y + cw->attrs.border_width);
    }
  
  meta_error_trap_pop (display, FALSE);

  dump_xserver_region ("repair_win", display, parts);
  add_damage (screen, parts);
  cw->damaged = TRUE;
}

static void
free_win (MetaCompWindow *cw,
          gboolean        destroy)
{
  MetaDisplay *display = cw->screen->display;
  MetaCompScreen *info = cw->screen->compositor_data;

#ifdef HAVE_NAME_WINDOW_PIXMAP
  /* See comment in map_win */
  if (cw->back_pixmap && destroy) 
    {
      XFreePixmap (display->xdisplay, cw->back_pixmap);
      cw->back_pixmap = None;
    }

  if (cw->shaded_back_pixmap && destroy)
    {
      XFreePixmap (display->xdisplay, cw->shaded_back_pixmap);
      cw->shaded_back_pixmap = None;
    }
#endif

  if (cw->picture) 
    {
      XRenderFreePicture (display->xdisplay, cw->picture);
      cw->picture = None;
    }

  if (cw->shadow) 
    {
      XRenderFreePicture (display->xdisplay, cw->shadow);
      cw->shadow = None;
    }

  if (cw->alpha_pict) 
    {
      XRenderFreePicture (display->xdisplay, cw->alpha_pict);
      cw->alpha_pict = None;
    }

  if (cw->shadow_pict) 
    {
      XRenderFreePicture (display->xdisplay, cw->shadow_pict);
      cw->shadow_pict = None;
    }
  
  if (cw->border_size) 
    {
      XFixesDestroyRegion (display->xdisplay, cw->border_size);
      cw->border_size = None;
    }
  
  if (cw->border_clip) 
    {
      XFixesDestroyRegion (display->xdisplay, cw->border_clip);
      cw->border_clip = None;
    }

  if (cw->extents) 
    {
      XFixesDestroyRegion (display->xdisplay, cw->extents);
      cw->extents = None;
    }

  if (destroy) 
    { 
      if (cw->damage != None) {
        meta_error_trap_push (display);
        XDamageDestroy (display->xdisplay, cw->damage);
        meta_error_trap_pop (display, FALSE);

        cw->damage = None;
      }

      /* The window may not have been added to the list in this case,
         but we can check anyway */
      if (cw->type == META_COMP_WINDOW_DOCK)
        info->dock_windows = g_slist_remove (info->dock_windows, cw);

      g_free (cw);
    }
}
  
static void
map_win (MetaDisplay *display,
         MetaScreen  *screen,
         Window       id)
{
  MetaCompWindow *cw = find_window_for_screen (screen, id);

  if (cw == NULL)
    return;

#ifdef HAVE_NAME_WINDOW_PIXMAP
  /* The reason we deallocate this here and not in unmap
     is so that we will still have a valid pixmap for 
     whenever the window is unmapped */
  if (cw->back_pixmap) 
    {
      XFreePixmap (display->xdisplay, cw->back_pixmap);
      cw->back_pixmap = None;
    }

  if (cw->shaded_back_pixmap) 
    {
      XFreePixmap (display->xdisplay, cw->shaded_back_pixmap);
      cw->shaded_back_pixmap = None;
    }
#endif

  cw->attrs.map_state = IsViewable;
  cw->damaged = TRUE;
}

static void
unmap_win (MetaDisplay *display,
           MetaScreen  *screen,
           Window       id)
{
  MetaCompWindow *cw = find_window_for_screen (screen, id);
  MetaCompScreen *info = screen->compositor_data;

  if (cw == NULL) 
    {
      return;
    }

  cw->attrs.map_state = IsUnmapped;
  cw->damaged = FALSE;

  if (cw->extents != None) 
    {
      dump_xserver_region ("unmap_win", display, cw->extents);
      add_damage (screen, cw->extents);
      cw->extents = None;
    }

  free_win (cw, FALSE);
  info->clip_changed = TRUE;
}

static void
determine_mode (MetaDisplay    *display,
                MetaScreen     *screen,
                MetaCompWindow *cw)
{
  XRenderPictFormat *format;

  if (cw->alpha_pict) 
    {
      XRenderFreePicture (display->xdisplay, cw->alpha_pict);
      cw->alpha_pict = None;
    }

  if (cw->shadow_pict) 
    {
      XRenderFreePicture (display->xdisplay, cw->shadow_pict);
      cw->shadow_pict = None;
    }

  if (cw->attrs.class == InputOnly)
    format = NULL;
  else
    format = XRenderFindVisualFormat (display->xdisplay, cw->attrs.visual);
  
  if ((format && format->type == PictTypeDirect && format->direct.alphaMask)
      || cw->opacity != (guint) OPAQUE)
    cw->mode = WINDOW_ARGB;
  else
    cw->mode = WINDOW_SOLID;

  if (cw->extents) 
    {
      XserverRegion damage;
      damage = XFixesCreateRegion (display->xdisplay, NULL, 0);
      XFixesCopyRegion (display->xdisplay, damage, cw->extents);

      dump_xserver_region ("determine_mode", display, damage);
      add_damage (screen, damage);
    }
}

static gboolean
is_shaped (MetaDisplay *display,
           Window       xwindow)
{
  int xws, yws, xbs, ybs;
  unsigned wws, hws, wbs, hbs;
  int bounding_shaped, clip_shaped;

  if (META_DISPLAY_HAS_SHAPE (display))
    {
      XShapeQueryExtents (display->xdisplay, xwindow, &bounding_shaped,
                          &xws, &yws, &wws, &hws, &clip_shaped,
                          &xbs, &ybs, &wbs, &hbs);
      return (bounding_shaped != 0);
    }
  
  return FALSE;
}

static void
get_window_type (MetaDisplay    *display,
                 MetaCompWindow *cw)
{
  MetaCompositor *compositor = display->compositor;
  int n_atoms;
  Atom *atoms, type_atom;
  int i;

  type_atom = None;
  n_atoms = 0;
  atoms = NULL;
  
  meta_prop_get_atom_list (display, cw->id, 
                           display->atom_net_wm_window_type,
                           &atoms, &n_atoms);

  for (i = 0; i < n_atoms; i++) 
    {
      if (atoms[i] == compositor->atom_net_wm_window_type_dnd ||
          atoms[i] == display->atom_net_wm_window_type_desktop ||
          atoms[i] == display->atom_net_wm_window_type_dock ||
          atoms[i] == display->atom_net_wm_window_type_toolbar ||
          atoms[i] == display->atom_net_wm_window_type_menu ||
          atoms[i] == display->atom_net_wm_window_type_dialog ||
          atoms[i] == display->atom_net_wm_window_type_normal ||
          atoms[i] == display->atom_net_wm_window_type_utility ||
          atoms[i] == display->atom_net_wm_window_type_splash)
        {
          type_atom = atoms[i];
          break;
        }
    }

  meta_XFree (atoms);

  if (type_atom == compositor->atom_net_wm_window_type_dnd)
    cw->type = META_COMP_WINDOW_DND;
  else if (type_atom == display->atom_net_wm_window_type_desktop)
    cw->type = META_COMP_WINDOW_DESKTOP;
  else if (type_atom == display->atom_net_wm_window_type_dock)
    cw->type = META_COMP_WINDOW_DOCK;
  else
    cw->type = META_COMP_WINDOW_NORMAL;

/*   meta_verbose ("Window is %d\n", cw->type); */
}
  
/* Must be called with an error trap in place */
static void
add_win (MetaScreen *screen,
         MetaWindow *window,
         Window     xwindow)
{
  MetaDisplay *display = screen->display;
  MetaCompScreen *info = screen->compositor_data;
  MetaCompWindow *cw;
  gulong event_mask;

  if (info == NULL) 
    return;

  if (xwindow == info->output) 
    return;

  cw = g_new0 (MetaCompWindow, 1);
  cw->screen = screen;
  cw->window = window;
  cw->id = xwindow;

  if (!XGetWindowAttributes (display->xdisplay, xwindow, &cw->attrs)) 
    {
      g_warning ("Could not get attrs");
      g_free (cw);
      return;
    }
  get_window_type (display, cw);

  /* If Metacity has decided not to manage this window then the input events
     won't have been set on the window */
  event_mask = cw->attrs.your_event_mask | PropertyChangeMask;
  
  XSelectInput (display->xdisplay, xwindow, event_mask);


#ifdef HAVE_NAME_WINDOW_PIXMAP
  cw->back_pixmap = None;
  cw->shaded_back_pixmap = None;
#endif

  cw->damaged = FALSE;
  cw->shaped = is_shaped (display, xwindow);

  if (cw->attrs.class == InputOnly)
    cw->damage = None;
  else
    cw->damage = XDamageCreate (display->xdisplay, xwindow,
                                XDamageReportNonEmpty);

  cw->alpha_pict = None;
  cw->shadow_pict = None;
  cw->border_size = None;
  cw->extents = None;
  cw->shadow = None;
  cw->shadow_dx = 0;
  cw->shadow_dy = 0;
  cw->shadow_width = 0;
  cw->shadow_height = 0;
  cw->opacity = OPAQUE;
  
  cw->border_clip = None;

  determine_mode (display, screen, cw);
  cw->needs_shadow = window_has_shadow (cw);

  /* Only add the window to the list of docks if it needs a shadow */
  if (cw->type == META_COMP_WINDOW_DOCK && cw->needs_shadow) 
    {
      meta_verbose ("Appending %p to dock windows\n", cw);
      info->dock_windows = g_slist_append (info->dock_windows, cw);
    }

  /* Add this to the list at the top of the stack
     before it is mapped so that map_win can find it again */
  info->windows = g_list_prepend (info->windows, cw);
  g_hash_table_insert (info->windows_by_xid, (gpointer) xwindow, cw);

  if (cw->attrs.map_state == IsViewable)
    map_win (display, screen, xwindow);
}

static void
destroy_win (MetaDisplay *display,
             Window       xwindow,
             gboolean     gone)
{
  MetaScreen *screen;
  MetaCompScreen *info;
  MetaCompWindow *cw;

  cw = find_window_in_display (display, xwindow);
  
  if (cw == NULL)
    return;

  screen = cw->screen;
  
  if (cw->extents != None) 
    {
      dump_xserver_region ("destroy_win", display, cw->extents);
      add_damage (screen, cw->extents);
      cw->extents = None;
    }
  
  info = screen->compositor_data;
  info->windows = g_list_remove (info->windows, (gconstpointer) cw);
  g_hash_table_remove (info->windows_by_xid, (gpointer) xwindow);
  
  free_win (cw, TRUE);
}

static void
restack_win (MetaCompWindow *cw,
             Window          above)
{
  MetaScreen *screen;
  MetaCompScreen *info;
  Window previous_above;
  GList *sibling, *next;

  screen = cw->screen;
  info = screen->compositor_data;

  sibling = g_list_find (info->windows, (gconstpointer) cw);
  next = g_list_next (sibling);
  previous_above = None;

  if (next) 
    {
      MetaCompWindow *ncw = (MetaCompWindow *) next->data;
      previous_above = ncw->id;
    }

  /* If above is set to None, the window whose state was changed is on 
   * the bottom of the stack with respect to sibling.
   */
  if (above == None) 
    {
      /* Insert at bottom of window stack */
      info->windows = g_list_delete_link (info->windows, sibling);
      info->windows = g_list_append (info->windows, cw);
    } 
  else if (previous_above != above) 
    {
      GList *index;
      
      for (index = info->windows; index; index = index->next) {
        MetaCompWindow *cw2 = (MetaCompWindow *) index->data;
        if (cw2->id == above)
          break;
      }
      
      if (index != NULL) 
        {
          info->windows = g_list_delete_link (info->windows, sibling);
          info->windows = g_list_insert_before (info->windows, index, cw);
        }
    }
}

static void
resize_win (MetaCompWindow *cw,
            int             x,
            int             y,
            int             width,
            int             height,
            int             border_width,
            gboolean        override_redirect)
{
  MetaScreen *screen;
  MetaDisplay *display;
  MetaCompScreen *info;
  Display *xdisplay;
  XserverRegion damage;
  gboolean debug;

  screen = cw->screen;
  display = screen->display;
  xdisplay = display->xdisplay;
  info = screen->compositor_data;

  debug = ((MetaCompositor *) display->compositor)->debug;

  if (cw->extents)
    {
      damage = XFixesCreateRegion (xdisplay, NULL, 0);
      XFixesCopyRegion (xdisplay, damage, cw->extents);
    }
  else
    {
      damage = None;
      if (debug)
        fprintf (stderr, "no extents to damage !\n");
    }

  /*  { // Damage whole screen each time ! ;-)
    XRectangle r;

    r.x = 0;
    r.y = 0;
    r.width = screen->rect.width;
    r.height = screen->rect.height;
    fprintf (stderr, "Damage whole screen %d,%d (%d %d)\n",
             r.x, r.y, r.width, r.height);
    
    damage = XFixesCreateRegion (display->xdisplay, &r, 1);
    } */

  cw->attrs.x = x;
  cw->attrs.y = y;

  if (cw->attrs.width != width || cw->attrs.height != height) 
    {
#ifdef HAVE_NAME_WINDOW_PIXMAP
      if (cw->shaded_back_pixmap) 
        {
          XFreePixmap (xdisplay, cw->shaded_back_pixmap);
          cw->shaded_back_pixmap = None;
        }

      if (cw->back_pixmap) 
        {
          /* If the window is shaded, we store the old backing pixmap
             so we can return a proper image of the window */
          if (cw->window && cw->window->shaded)
            {
              cw->shaded_back_pixmap = cw->back_pixmap;
              cw->back_pixmap = None;
            }
          else
            {
              XFreePixmap (xdisplay, cw->back_pixmap);
              cw->back_pixmap = None;
            }
        }
#endif
      if (cw->picture) 
        {
          XRenderFreePicture (xdisplay, cw->picture);
          cw->picture = None;
        }
      
      if (cw->shadow) 
        {
          XRenderFreePicture (xdisplay, cw->shadow);
          cw->shadow = None;
        }
    }

  cw->attrs.width = width;
  cw->attrs.height = height;
  cw->attrs.border_width = border_width;
  cw->attrs.override_redirect = override_redirect;

  if (cw->extents)
    XFixesDestroyRegion (xdisplay, cw->extents);

  cw->extents = win_extents (cw);

  if (damage) 
    {
      if (debug)
        fprintf (stderr, "Inexplicable intersection with new extents!\n");      

      XFixesUnionRegion (xdisplay, damage, damage, cw->extents);      
    }
  else
    {
      damage = XFixesCreateRegion (xdisplay, NULL, 0);
      XFixesCopyRegion (xdisplay, damage, cw->extents);
    }

  dump_xserver_region ("resize_win", display, damage);
  add_damage (screen, damage);

  info->clip_changed = TRUE;
}

/* event processors must all be called with an error trap in place */
static void
process_circulate_notify (MetaCompositor  *compositor,
                          XCirculateEvent *event)
{
  MetaCompWindow *cw = find_window_in_display (compositor->display,
                                               event->window);
  MetaCompWindow *top;
  MetaCompScreen *info;
  MetaScreen *screen;
  GList *first;
  Window above;

  if (!cw) 
    return;

  screen = cw->screen;
  info = screen->compositor_data;
  first = info->windows;
  top = (MetaCompWindow *) first->data;

  if ((event->place == PlaceOnTop) && top)
    above = top->id;
  else
    above = None;
  restack_win (cw, above);

  info->clip_changed = TRUE;

#ifdef USE_IDLE_REPAINT
  add_repair (compositor->display);
#endif
}

static void
process_configure_notify (MetaCompositor  *compositor,
                          XConfigureEvent *event)
{
  MetaDisplay *display = compositor->display;
  MetaCompWindow *cw = find_window_in_display (display, event->window);

  if (cw) 
    {
#if 0
      int x = -1, y = -1, width = -1, height = -1;
      int ex = -1, ey = -1, ewidth = -1, eheight = -1;
      if (cw->window) {
        x = cw->window->rect.x;
        y = cw->window->rect.y;
        width = cw->window->rect.width;
        height = cw->window->rect.height;
      } 
      fprintf (stderr, "configure notify xy (%d %d) -> (%d %d), wh (%d %d) -> (%d %d)\n",
               x, y, event->x, event->y,
               width, height, event->width, event->height);
#endif

      if (compositor->debug)
        {
          fprintf (stderr, "configure notify %d %d %d\n", cw->damaged, 
                   cw->shaped, cw->needs_shadow);
          dump_xserver_region ("\textents", display, cw->extents);
          fprintf (stderr, "\txy (%d %d), wh (%d %d)\n",
                   event->x, event->y, event->width, event->height);
        }

      restack_win (cw, event->above);
      resize_win (cw, event->x, event->y, event->width, event->height,
                  event->border_width, event->override_redirect);
    }
  else
    { 
      MetaScreen *screen;
      MetaCompScreen *info;

      /* Might be the root window? */
      screen = meta_display_screen_for_root (display, event->window);
      if (screen == NULL)
        return;

      info = screen->compositor_data;
      if (info->root_buffer)
        {
          XRenderFreePicture (display->xdisplay, info->root_buffer);
          info->root_buffer = None;
        }

      damage_screen (screen);
    }
}

static void
process_property_notify (MetaCompositor *compositor,
                         XPropertyEvent *event)
{
  MetaDisplay *display = compositor->display;
  MetaScreen *screen;
  int p;
  Atom background_atoms[2];

  /* Check for the background property changing */
  background_atoms[0] = compositor->atom_x_root_pixmap;
  background_atoms[1] = compositor->atom_x_set_root;

  for (p = 0; p < 2; p++) 
    {
      if (event->atom == background_atoms[p])
        {
          screen = meta_display_screen_for_root (display, event->window);
          if (screen)
            {
              MetaCompScreen *info = screen->compositor_data;
              if (info->root_tile)
                {
                  XClearArea (display->xdisplay, screen->xroot,
                              0, 0, 0, 0, TRUE);
                  XRenderFreePicture (display->xdisplay, info->root_tile);
                  info->root_tile = None;
#ifdef USE_IDLE_REPAINT
                  add_repair (display);
#endif

                  return;
                }
            }
        }
    }

  /* Check for the opacity changing */
  if (event->atom == compositor->atom_net_wm_window_opacity) 
    {
      MetaCompWindow *cw = find_window_in_display (display, event->window);
      gulong value;

      if (!cw) 
        return;

      if (meta_prop_get_cardinal (display, event->window,
                                  compositor->atom_net_wm_window_opacity,
                                  &value) == FALSE)
        value = OPAQUE;

      cw->opacity = (guint)value;
      determine_mode (display, cw->screen, cw);
      cw->needs_shadow = window_has_shadow (cw);

      if (cw->shadow)
        {
          XRenderFreePicture (display->xdisplay, cw->shadow);
          cw->shadow = None;
        }

      if (cw->extents)
        XFixesDestroyRegion (display->xdisplay, cw->extents);
      cw->extents = win_extents (cw);

      cw->damaged = TRUE;
#ifdef USE_IDLE_REPAINT
      add_repair (display);
#endif

      return;
    }

  if (event->atom == display->atom_net_wm_window_type) {
    MetaCompWindow *cw = find_window_in_display (display, event->window);

    if (!cw)
      return;

    get_window_type (display, cw);
    cw->needs_shadow = window_has_shadow (cw);
    return;
  }
}

static void
expose_area (MetaScreen *screen,
             XRectangle *rects,
             int         nrects)
{
  MetaDisplay *display;
  XserverRegion region;

  display = screen->display;
  region = XFixesCreateRegion (display->xdisplay, rects, nrects);

  dump_xserver_region ("expose_area", display, region);
  add_damage (screen, region);
}

static void
process_expose (MetaCompositor *compositor,
                XExposeEvent   *event)
{
  MetaCompWindow *cw = find_window_in_display (compositor->display,
                                               event->window);
  MetaScreen *screen = NULL;
  XRectangle rect[1];
  int origin_x = 0, origin_y = 0;

  if (cw != NULL)
    {
      screen = cw->screen;
      origin_x = cw->attrs.x; /* + cw->attrs.border_width; ? */
      origin_y = cw->attrs.y; /* + cw->attrs.border_width; ? */
    }
  else
    {
      screen = meta_display_screen_for_root (compositor->display, 
                                             event->window);
      if (screen == NULL)
        return;
    }

  rect[0].x = event->x + origin_x;
  rect[0].y = event->y + origin_y;
  rect[0].width = event->width;
  rect[0].height = event->height;
  
  expose_area (screen, rect, 1);
}

static void
process_unmap (MetaCompositor *compositor,
               XUnmapEvent    *event)
{
  MetaCompWindow *cw;

  if (event->from_configure) 
    {
      /* Ignore unmap caused by parent's resize */
      return;
    }
  
  cw = find_window_in_display (compositor->display, event->window);
  if (cw)
    unmap_win (compositor->display, cw->screen, event->window);
}

static void
process_map (MetaCompositor *compositor,
             XMapEvent      *event)
{
  MetaCompWindow *cw = find_window_in_display (compositor->display, 
                                               event->window);

  if (cw)
    map_win (compositor->display, cw->screen, event->window);
}

static void
process_reparent (MetaCompositor *compositor,
                  XReparentEvent *event,
                  MetaWindow     *window)
{
  MetaScreen *screen;

  screen = meta_display_screen_for_root (compositor->display, event->parent);
  if (screen != NULL)
    add_win (screen, window, event->window);
  else
    destroy_win (compositor->display, event->window, FALSE); 
}

static void
process_create (MetaCompositor     *compositor,
                XCreateWindowEvent *event,
                MetaWindow         *window)
{
  MetaScreen *screen;
  /* We are only interested in top level windows, others will
     be caught by normal metacity functions */

  screen = meta_display_screen_for_root (compositor->display, event->parent);
  if (screen == NULL)
    return;
  
  if (!find_window_in_display (compositor->display, event->window))
    add_win (screen, window, event->window);
}

static void
process_destroy (MetaCompositor      *compositor,
                 XDestroyWindowEvent *event)
{
  destroy_win (compositor->display, event->window, FALSE);
}

static void
process_damage (MetaCompositor     *compositor,
                XDamageNotifyEvent *event)
{
  MetaCompWindow *cw = find_window_in_display (compositor->display,
                                               event->drawable);
  if (cw == NULL)
    return;

  repair_win (cw);

#ifdef USE_IDLE_REPAINT
  if (event->more == FALSE)
    add_repair (compositor->display);
#endif
}
  
static void
process_shape (MetaCompositor *compositor,
               XShapeEvent    *event)
{
  MetaCompWindow *cw = find_window_in_display (compositor->display,
                                               event->window);

  if (cw == NULL)
    return;

  if (event->kind == ShapeBounding) 
    {
      if (!event->shaped && cw->shaped)
        cw->shaped = FALSE;
      
      resize_win (cw, cw->attrs.x, cw->attrs.y,
                  event->width + event->x, event->height + event->y,
                  cw->attrs.border_width, cw->attrs.override_redirect);
      
      if (event->shaped && !cw->shaped)
        cw->shaped = TRUE;
    }
}

static int timeout_debug (MetaCompositor *compositor)
{
  compositor->show_redraw = (g_getenv ("METACITY_DEBUG_REDRAWS") != NULL);
  compositor->debug = (g_getenv ("METACITY_DEBUG_COMPOSITOR") != NULL);

  return FALSE;
}

MetaCompositor *
meta_compositor_new (MetaDisplay *display)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  char *atom_names[] = {
    "_XROOTPMAP_ID",
    "_XSETROOT_ID",
    "_NET_WM_WINDOW_OPACITY",
    "_NET_WM_WINDOW_TYPE_DND",
  };
  Atom atoms[G_N_ELEMENTS(atom_names)];
  MetaCompositor *compositor;

  compositor = g_new (MetaCompositor, 1);
  compositor->display = display;

  meta_verbose ("Creating %d atoms\n", (int) G_N_ELEMENTS (atom_names));
  XInternAtoms (display->xdisplay, atom_names, G_N_ELEMENTS (atom_names),
                False, atoms);
  compositor->atom_x_root_pixmap = atoms[0];
  compositor->atom_x_set_root = atoms[1];
  compositor->atom_net_wm_window_opacity = atoms[2];
  compositor->atom_net_wm_window_type_dnd = atoms[3];

#ifdef USE_IDLE_REPAINT
  meta_verbose ("Using idle repaint\n");
  compositor->repaint_id = 0;
#endif

  compositor->enabled = TRUE;
  g_timeout_add (2000, (GSourceFunc) timeout_debug, compositor);

  return compositor;
#else
  return NULL;
#endif
}

void
meta_compositor_add_window (MetaCompositor    *compositor,
                            MetaWindow        *window,
                            Window             xwindow,
                            XWindowAttributes *attrs)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaScreen *screen = meta_screen_for_x_screen (attrs->screen);

  meta_error_trap_push (compositor->display);
  add_win (screen, window, xwindow);
  meta_error_trap_pop (compositor->display, FALSE);
#endif
}

void
meta_compositor_remove_window (MetaCompositor *compositor,
                               Window          xwindow)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
#endif
}
#ifdef HAVE_COW
static void
show_overlay_window (MetaScreen *screen,
                     Window      cow)
{
  MetaDisplay *display = screen->display;
  XserverRegion region;

  region = XFixesCreateRegion (display->xdisplay, NULL, 0);

  XFixesSetWindowShapeRegion (display->xdisplay, cow, ShapeBounding, 0, 0, 0);
  XFixesSetWindowShapeRegion (display->xdisplay, cow, ShapeInput, 0, 0, region);

  XFixesDestroyRegion (display->xdisplay, region);

  damage_screen (screen);
}

static void
hide_overlay_window (MetaScreen *screen,
                     Window      cow)
{
  MetaDisplay *display = screen->display;
  XserverRegion region;

  region = XFixesCreateRegion (display->xdisplay, NULL, 0);
  XFixesSetWindowShapeRegion (display->xdisplay,
                              cow, ShapeBounding,
                              0, 0, region);
  XFixesDestroyRegion (display->xdisplay, region);
}

static Window
get_overlay_window (MetaScreen *screen)
{
  MetaDisplay *display = screen->display;
  Window cow;

  cow = XCompositeGetOverlayWindow (display->xdisplay, screen->xroot);
  XSelectInput (display->xdisplay, cow, ExposureMask);

  return cow;
}
#endif

void
meta_compositor_manage_screen (MetaCompositor *compositor,
                               MetaScreen     *screen)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaCompScreen *info;
  MetaDisplay *display = screen->display;
  XRenderPictureAttributes pa;
  XRenderPictFormat *visual_format;

  /* Check if the screen is already managed */
  if (screen->compositor_data)
    return;

  gdk_error_trap_push ();
  /* FIXME: Use correct composite mode */
  XCompositeRedirectSubwindows (display->xdisplay, screen->xroot,
                                CompositeRedirectManual);
  XSync (display->xdisplay, FALSE);

  if (gdk_error_trap_pop ())
    {
      g_warning ("Another compositing manager is running on screen %i",
                 screen->number);
      return;
    }

  info = g_new0 (MetaCompScreen, 1);
  info->screen = screen;
  
  screen->compositor_data = info;

  visual_format = XRenderFindVisualFormat (display->xdisplay,
                                           DefaultVisual (display->xdisplay,
                                                          screen->number));
  if (!visual_format) 
    {
      g_warning ("Cannot find visual format on screen %i", screen->number);
      return;
    }

#ifdef HAVE_COW
  info->output = get_overlay_window (screen);
#else
  info->output = screen->xroot;
#endif

  pa.subwindow_mode = IncludeInferiors;
  info->root_picture = XRenderCreatePicture (display->xdisplay, info->output,
                                             visual_format, 
                                             CPSubwindowMode, &pa);
  if (info->root_picture == None) 
    {
      g_warning ("Cannot create root picture on screen %i", screen->number);
      return;
    }
  
  info->root_buffer = None;
  info->black_picture = solid_picture (display, screen, TRUE, 1, 0, 0, 0);

  info->root_tile = None;
  info->all_damage = None;
  
  info->windows = NULL;
  info->windows_by_xid = g_hash_table_new (g_direct_hash, g_direct_equal);

  info->compositor_active = TRUE;
  info->overlays = 0;
  info->clip_changed = TRUE;

  info->have_shadows = (g_getenv("META_DEBUG_NO_SHADOW") == NULL);
  info->gaussian_map = make_gaussian_map (SHADOW_RADIUS);
  presum_gaussian (info);

  XClearArea (display->xdisplay, info->output, 0, 0, 0, 0, TRUE);

  meta_screen_set_cm_selection (screen);

#ifdef HAVE_COW
  /* Now we're up and running we can show the output if needed */
  show_overlay_window (screen, info->output);
#endif

#endif
}

void
meta_compositor_unmanage_screen (MetaCompositor *compositor,
                                 MetaScreen     *screen)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaDisplay *display = screen->display;
  MetaCompScreen *info;
  GList *index;

  /* This screen isn't managed */
  if (screen->compositor_data == NULL)
    return;

  info = screen->compositor_data;

  /* Destroy the windows */
  for (index = info->windows; index; index = index->next) 
    {
      MetaCompWindow *cw = (MetaCompWindow *) index->data;
      free_win (cw, TRUE);
    }
  g_list_free (info->windows);
  g_hash_table_destroy (info->windows_by_xid);

  if (info->root_picture)
    XRenderFreePicture (display->xdisplay, info->root_picture);

  if (info->black_picture)
    XRenderFreePicture (display->xdisplay, info->black_picture);

  g_free (info->gaussian_map);

  XCompositeUnredirectSubwindows (display->xdisplay, screen->xroot,
                                  CompositeRedirectManual);
  meta_screen_unset_cm_selection (screen);

  g_free (info);
  screen->compositor_data = NULL;
#endif
}

void
meta_compositor_set_updates (MetaCompositor *compositor,
                             MetaWindow     *window,
                             gboolean        updates)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  
#endif
}

void
meta_compositor_destroy (MetaCompositor *compositor)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  g_free (compositor);
#endif
}

void
meta_compositor_begin_move (MetaCompositor *compositor,
                            MetaWindow     *window,
                            MetaRectangle  *initial,
                            int             grab_x,
                            int             grab_y)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
#endif
}

void
meta_compositor_update_move (MetaCompositor *compositor,
                             MetaWindow     *window,
                             int             x,
                             int             y)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
#endif
}

void
meta_compositor_end_move (MetaCompositor *compositor,
                          MetaWindow     *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
#endif
}

void
meta_compositor_free_window (MetaCompositor *compositor,
                             MetaWindow     *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  destroy_win (compositor->display, window->xwindow, FALSE);
#endif
}
   
void
meta_compositor_process_event (MetaCompositor *compositor,
                               XEvent         *event,
                               MetaWindow     *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  /*
   * This trap is so that none of the compositor functions cause
   * X errors. This is really a hack, but I'm afraid I don't understand
   * enough about Metacity/X to know how else you are supposed to do it
   */
  meta_error_trap_push (compositor->display);
  switch (event->type) 
    {
    case CirculateNotify:
      process_circulate_notify (compositor, (XCirculateEvent *) event);
      break;
      
    case ConfigureNotify:
      process_configure_notify (compositor, (XConfigureEvent *) event);
      break;

    case PropertyNotify:
      process_property_notify (compositor, (XPropertyEvent *) event);
      break;

    case Expose:
      process_expose (compositor, (XExposeEvent *) event);
      break;
      
    case UnmapNotify:
      process_unmap (compositor, (XUnmapEvent *) event);
      break;
      
    case MapNotify:
      process_map (compositor, (XMapEvent *) event);
      break;
      
    case ReparentNotify:
      process_reparent (compositor, (XReparentEvent *) event, window);
      break;
      
    case CreateNotify:
      process_create (compositor, (XCreateWindowEvent *) event, window);
      break;
      
    case DestroyNotify:
      process_destroy (compositor, (XDestroyWindowEvent *) event);
      break;
      
    default:
      if (event->type == compositor->display->damage_event_base + XDamageNotify) 
        process_damage (compositor, (XDamageNotifyEvent *) event);
      else if (event->type == compositor->display->shape_event_base + ShapeNotify) 
        process_shape (compositor, (XShapeEvent *) event);
      else 
        {
          meta_error_trap_pop (compositor->display, FALSE);
          return;
        }
      break;
    }
  
  meta_error_trap_pop (compositor->display, FALSE);
#ifndef USE_IDLE_REPAINT
  repair_display (compositor->display);
#endif
  
  return;
#endif
}

Pixmap
meta_compositor_get_window_pixmap (MetaCompositor *compositor,
                                   MetaWindow     *window)
{
#ifdef HAVE_COMPOSITE_EXTENSIONS
  MetaCompWindow *cw = NULL;

  if (window->frame)
    {
      cw = find_window_for_screen (window->screen, window->frame->xwindow);
      if (cw == NULL)
        cw = find_window_for_screen (window->screen, window->xwindow);
    }

  if (cw == NULL)
    return None;

#ifdef HAVE_NAME_WINDOW_PIXMAP
  if (window->shaded)
    return cw->shaded_back_pixmap;
  else
    return cw->back_pixmap;
#else
  return None;
#endif
#endif
}
