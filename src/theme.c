/* Metacity Default Theme Engine */

/* 
 * Copyright (C) 2001 Havoc Pennington
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

#include "theme.h"
#include "api.h"
#include "util.h"

typedef struct _DefaultFrameData     DefaultFrameData;
typedef struct _DefaultScreenData    DefaultScreenData;
typedef struct _DefaultFrameGeometry DefaultFrameGeometry;

struct _DefaultFrameGeometry
{
  /* We recalculate this every time, to save RAM */
  int left_width;
  int right_width;
  int top_height;
  int bottom_height;

  MetaRectangle close_rect;
  MetaRectangle max_rect;
  MetaRectangle min_rect;
  MetaRectangle spacer_rect;
  MetaRectangle menu_rect;
  MetaRectangle title_rect;  
};

struct _DefaultFrameData
{
  PangoLayout *layout;
  int layout_height;
};

struct _DefaultScreenData
{
  GC text_gc;
  GC fg_gc;
  GC light_gc;
  GC dark_gc;
  GC black_gc;
  GC selected_gc;
  GC selected_text_gc;
  GC active_gc;
  GC prelight_gc;
};

/* FIXME store this on the screen */
static DefaultScreenData *screen_data = NULL;

static gpointer
default_acquire_frame (MetaFrameInfo *info)
{
  DefaultFrameData *d;
  PangoFontDescription *desc;
  XGCValues vals;
  
  if (screen_data == NULL)
    {
      PangoColor color;
      
      screen_data = g_new (DefaultScreenData, 1);      

      vals.foreground = meta_get_x_pixel (info->screen,
                                          &info->colors->fg[META_STATE_NORMAL]);

      screen_data->text_gc = XCreateGC (info->display,
                                        RootWindowOfScreen (info->screen),
                                        GCForeground,
                                        &vals);
      screen_data->fg_gc = XCreateGC (info->display,
                                      RootWindowOfScreen (info->screen),
                                      GCForeground,
                                      &vals);

      vals.foreground = meta_get_x_pixel (info->screen,
                                          &info->colors->light[META_STATE_NORMAL]);
      screen_data->light_gc = XCreateGC (info->display,
                                         RootWindowOfScreen (info->screen),
                                         GCForeground,
                                         &vals);

      vals.foreground = meta_get_x_pixel (info->screen,
                                          &info->colors->dark[META_STATE_NORMAL]);
      screen_data->dark_gc = XCreateGC (info->display,
                                        RootWindowOfScreen (info->screen),
                                        GCForeground,
                                        &vals);

      vals.foreground = meta_get_x_pixel (info->screen,
                                          &info->colors->bg[META_STATE_SELECTED]);
      screen_data->selected_gc = XCreateGC (info->display,
                                            RootWindowOfScreen (info->screen),
                                            GCForeground,
                                            &vals);
      vals.foreground = meta_get_x_pixel (info->screen,
                                          &info->colors->fg[META_STATE_SELECTED]);
      screen_data->selected_text_gc = XCreateGC (info->display,
                                                 RootWindowOfScreen (info->screen),
                                                 GCForeground,
                                                 &vals);

      vals.foreground = meta_get_x_pixel (info->screen,
                                          &info->colors->bg[META_STATE_ACTIVE]);
      screen_data->active_gc = XCreateGC (info->display,
                                          RootWindowOfScreen (info->screen),
                                          GCForeground,
                                          &vals);

      vals.foreground = meta_get_x_pixel (info->screen,
                                          &info->colors->bg[META_STATE_PRELIGHT]);
      screen_data->prelight_gc = XCreateGC (info->display,
                                            RootWindowOfScreen (info->screen),
                                            GCForeground,
                                            &vals);

      
      color.red = color.green = color.blue = 0;
      vals.foreground = meta_get_x_pixel (info->screen,
                                          &color);
      screen_data->black_gc = XCreateGC (info->display,
                                         RootWindowOfScreen (info->screen),
                                         GCForeground,
                                         &vals);
    }
  
  d = g_new (DefaultFrameData, 1);

  desc = pango_font_description_from_string ("Sans 12");
  d->layout = pango_layout_new (meta_get_pango_context (info->screen,
                                                        desc));
  pango_font_description_free (desc);
  
  d->layout_height = 0;
  
  return d;
}

static void
default_release_frame (MetaFrameInfo *info,
                       gpointer       frame_data)
{
  DefaultFrameData *d;

  d = frame_data;

  if (d->layout)
    g_object_unref (G_OBJECT (d->layout));
  
  g_free (d);
}

#define ABOVE_TITLE_PAD 4
#define BELOW_TITLE_PAD 3
#define RIGHT_TITLE_PAD 4
#define LEFT_TITLE_PAD 3
#define VERTICAL_TEXT_PAD 2
#define HORIZONTAL_TEXT_PAD 2
#define LEFT_WIDTH 6
#define RIGHT_WIDTH 6
#define BOTTOM_HEIGHT 7
#define SPACER_SPACING 3
#define SPACER_WIDTH 2
#define SPACER_HEIGHT 10
#define BUTTON_WIDTH 14
#define BUTTON_HEIGHT 14
#define BUTTON_PAD 1
#define INNER_BUTTON_PAD 3
static void
calc_geometry (MetaFrameInfo *info,
               DefaultFrameData *d,
               DefaultFrameGeometry *fgeom)
{
  int x;
  int button_y;
  int title_right_edge;
  gboolean shaded;

  shaded = (info->flags & META_FRAME_SHADED) != 0;
  
  fgeom->top_height = MAX (d->layout_height + VERTICAL_TEXT_PAD * 2 + ABOVE_TITLE_PAD + BELOW_TITLE_PAD,
                           BUTTON_HEIGHT + BUTTON_PAD * 2);

  fgeom->left_width = LEFT_WIDTH;
  fgeom->right_width = RIGHT_WIDTH;

  if (shaded)
    fgeom->bottom_height = 0;
  else
    fgeom->bottom_height = BOTTOM_HEIGHT;

  x = info->width - fgeom->right_width;
  button_y = (fgeom->top_height - BUTTON_HEIGHT) / 2;

  if (info->flags & META_FRAME_ALLOWS_DELETE)
    {
      fgeom->close_rect.x = x - BUTTON_PAD - BUTTON_WIDTH;
      fgeom->close_rect.y = button_y;
      fgeom->close_rect.width = BUTTON_WIDTH;
      fgeom->close_rect.height = BUTTON_HEIGHT;

      x = fgeom->close_rect.x;
    }
  else
    {
      fgeom->close_rect.x = 0;
      fgeom->close_rect.y = 0;
      fgeom->close_rect.width = 0;
      fgeom->close_rect.height = 0;
    }

  if (info->flags & META_FRAME_ALLOWS_MAXIMIZE)
    {
      fgeom->max_rect.x = x - BUTTON_PAD - BUTTON_WIDTH;
      fgeom->max_rect.y = button_y;
      fgeom->max_rect.width = BUTTON_WIDTH;
      fgeom->max_rect.height = BUTTON_HEIGHT;

      x = fgeom->max_rect.x;
    }
  else
    {
      fgeom->max_rect.x = 0;
      fgeom->max_rect.y = 0;
      fgeom->max_rect.width = 0;
      fgeom->max_rect.height = 0;
    }
  
  if (info->flags & META_FRAME_ALLOWS_ICONIFY)
    {
      fgeom->min_rect.x = x - BUTTON_PAD - BUTTON_WIDTH;
      fgeom->min_rect.y = button_y;
      fgeom->min_rect.width = BUTTON_WIDTH;
      fgeom->min_rect.height = BUTTON_HEIGHT;

      x = fgeom->min_rect.x;
    }
  else
    {
      fgeom->min_rect.x = 0;
      fgeom->min_rect.y = 0;
      fgeom->min_rect.width = 0;
      fgeom->min_rect.height = 0;
    }

  if (fgeom->close_rect.width > 0 ||
      fgeom->max_rect.width > 0 ||
      fgeom->min_rect.width > 0)
    {
      fgeom->spacer_rect.x = x - SPACER_SPACING - SPACER_WIDTH;
      fgeom->spacer_rect.y = (fgeom->top_height - SPACER_HEIGHT) / 2;
      fgeom->spacer_rect.width = SPACER_WIDTH;
      fgeom->spacer_rect.height = SPACER_HEIGHT;

      x = fgeom->spacer_rect.x;
    }
  else
    {
      fgeom->spacer_rect.x = 0;
      fgeom->spacer_rect.y = 0;
      fgeom->spacer_rect.width = 0;
      fgeom->spacer_rect.height = 0;
    }

  title_right_edge = x - RIGHT_TITLE_PAD;

  /* Now x changes to be position from the left */
  x = fgeom->left_width;
  
  if (info->flags & META_FRAME_ALLOWS_MENU)
    {
      fgeom->menu_rect.x = x + BUTTON_PAD;
      fgeom->menu_rect.y = button_y;
      fgeom->menu_rect.width = BUTTON_WIDTH;
      fgeom->menu_rect.height = BUTTON_HEIGHT;

      x = fgeom->menu_rect.x + fgeom->menu_rect.width;
    }
  else
    {
      fgeom->menu_rect.x = 0;
      fgeom->menu_rect.y = 0;
      fgeom->menu_rect.width = 0;
      fgeom->menu_rect.height = 0;
    }

  fgeom->title_rect.x = x + LEFT_TITLE_PAD;
  fgeom->title_rect.y = ABOVE_TITLE_PAD;
  fgeom->title_rect.width = title_right_edge - fgeom->title_rect.x;
  fgeom->title_rect.height = fgeom->top_height - ABOVE_TITLE_PAD - BELOW_TITLE_PAD;

  if (fgeom->title_rect.width < 0)
    fgeom->title_rect.width = 0;  
}

static void
default_fill_frame_geometry (MetaFrameInfo *info,
                             MetaFrameGeometry *geom,
                             gpointer       frame_data)
{
  DefaultFrameData *d;
  PangoRectangle rect;
  DefaultFrameGeometry fgeom;
  
  d = frame_data;
      
  if (info->title)
    pango_layout_set_text (d->layout, info->title, -1);
  else
    pango_layout_set_text (d->layout, " ", -1);

  pango_layout_get_pixel_extents (d->layout, NULL, &rect);

  d->layout_height = rect.height;

  calc_geometry (info, d, &fgeom);
  geom->top_height = fgeom.top_height;
  geom->left_width = fgeom.left_width;
  geom->right_width = fgeom.right_width;
  geom->bottom_height = fgeom.bottom_height;

  geom->background_pixel = meta_get_x_pixel (info->screen,
                                             &info->colors->bg[META_STATE_NORMAL]);
}

static void
draw_vline (MetaFrameInfo *info,
            Drawable       drawable,
            GC             light_gc,
            GC             dark_gc,
            int            y1,
            int            y2,
            int            x)
{
  int thickness_light;
  int thickness_dark;
  int i;

  /* From GTK+ */
  
  thickness_light = 1;
  thickness_dark = 1;  

  for (i = 0; i < thickness_dark; i++)
    {
      XDrawLine (info->display, drawable, light_gc,
                 x + i, y2 - i - 1, x + i, y2);
      XDrawLine (info->display, drawable, dark_gc,
                 x + i, y1, x + i, y2 - i - 1);
    }
  
  x += thickness_dark;
  for (i = 0; i < thickness_light; i++)
    {
      XDrawLine (info->display, drawable, dark_gc,
                 x + i, y1, x + i, y1 + thickness_light - i);
      XDrawLine (info->display, drawable, light_gc,
                 x + i, y1 + thickness_light - i, x + i, y2);
    }
}

static void
draw_varrow (MetaFrameInfo *info,
             Drawable       drawable,
             GC             gc,
             gboolean       down,
	     gint           x,
	     gint           y,
	     gint           width,
	     gint           height)
{
  gint steps, extra;
  gint y_start, y_increment;
  gint i;

  /* From GTK+ */
  
  width = width + width % 2 - 1;	/* Force odd */
  
  steps = 1 + width / 2;

  extra = height - steps;

  if (down)
    {
      y_start = y;
      y_increment = 1;
    }
  else
    {
      y_start = y + height - 1;
      y_increment = -1;
    }

  for (i = 0; i < extra; i++)
    {
      XDrawLine (info->display, drawable,
                 gc,
                 x,              y_start + i * y_increment,
                 x + width - 1,  y_start + i * y_increment);
    }
  for (; i < height; i++)
    {
      XDrawLine (info->display, drawable,
                 gc,
                 x + (i - extra),              y_start + i * y_increment,
                 x + width - (i - extra) - 1,  y_start + i * y_increment);
    }
}

static void
set_clip (Display *display, GC gc, MetaRectangle *rect)
{
  if (rect)
    {
      XRectangle xrect;

      xrect.x = rect->x;
      xrect.y = rect->y;
      xrect.width = rect->width;
      xrect.height = rect->height;

      XSetClipRectangles (display, gc, 0, 0,
                          &xrect, 1, YXBanded);
    }
  else
    {
      XSetClipMask (display, gc, None);
    }
}

static MetaRectangle*
control_rect (MetaFrameControl control,
              DefaultFrameGeometry *fgeom)
{
  MetaRectangle *rect;
  
  rect = NULL;
  switch (control)
    {
    case META_FRAME_CONTROL_TITLE:
      rect = &fgeom->title_rect;
      break;
    case META_FRAME_CONTROL_DELETE:
      rect = &fgeom->close_rect;
      break;
    case META_FRAME_CONTROL_MENU:
      rect = &fgeom->menu_rect;
      break;
    case META_FRAME_CONTROL_ICONIFY:
      rect = &fgeom->min_rect;
      break;
    case META_FRAME_CONTROL_MAXIMIZE:
      rect = &fgeom->max_rect;
      break;
    case META_FRAME_CONTROL_RESIZE_SE:
      break;
    case META_FRAME_CONTROL_RESIZE_S:
      break;
    case META_FRAME_CONTROL_RESIZE_SW:
      break;
    case META_FRAME_CONTROL_RESIZE_N:
      break;
    case META_FRAME_CONTROL_RESIZE_NE:
      break;
    case META_FRAME_CONTROL_RESIZE_NW:
      break;
    case META_FRAME_CONTROL_RESIZE_W:
      break;
    case META_FRAME_CONTROL_RESIZE_E:
      break;
    case META_FRAME_CONTROL_NONE:
      break;
    }

  return rect;
}

static void
draw_current_control_bg (MetaFrameInfo *info,
                         DefaultFrameGeometry *fgeom)
{
  int xoff, yoff;
  MetaRectangle *rect;
  
  xoff = info->xoffset;
  yoff = info->yoffset;

  rect = control_rect (info->current_control, fgeom);

  if (rect == NULL)
    return;

  if (info->current_control == META_FRAME_CONTROL_TITLE)
    return;
  
 switch (info->current_control_state)
    {
      /* FIXME turn this off after testing */
    case META_STATE_PRELIGHT:
      XFillRectangle (info->display,
                      info->drawable,
                      screen_data->prelight_gc,
                      xoff + rect->x,
                      yoff + rect->y,
                      rect->width, rect->height);
      break;

    case META_STATE_ACTIVE:
      XFillRectangle (info->display,
                      info->drawable,
                      screen_data->active_gc,
                      xoff + rect->x,
                      yoff + rect->y,
                      rect->width, rect->height);
      break;

    default:
      break;
    }
}

static void
default_expose_frame (MetaFrameInfo *info,
                      int x, int y,
                      int width, int height,
                      gpointer frame_data)
{
  DefaultFrameData *d;
  XGCValues vals;
  int xoff, yoff;
  DefaultFrameGeometry fgeom;
  
  d = frame_data;

  calc_geometry (info, d, &fgeom);
  
  xoff = info->xoffset;
  yoff = info->yoffset;

  /* Black line around outside to give definition */
  XDrawRectangle (info->display,
                  info->drawable,
                  screen_data->black_gc,
                  xoff, yoff,
                  info->width - 1,
                  info->height - 1);

  /* Light GC on top/left edges */
  XDrawLine (info->display,
             info->drawable,
             screen_data->light_gc,
             xoff + 1, yoff + 1,
             xoff + 1, yoff + info->height - 2);
  XDrawLine (info->display,
             info->drawable,
             screen_data->light_gc,
             xoff + 1, yoff + 1,
             xoff + info->width - 2, yoff + 1);

  /* Dark GC on bottom/right edges */
  XDrawLine (info->display,
             info->drawable,
             screen_data->dark_gc,
             xoff + info->width - 2, yoff + 1,
             xoff + info->width - 2, yoff + info->height - 2);
  XDrawLine (info->display,
             info->drawable,
             screen_data->dark_gc,
             xoff + 1, yoff + info->height - 2,
             xoff + info->width - 2, yoff + info->height - 2);  

  if (info->flags & META_FRAME_HAS_FOCUS)
    {
      /* Black line around inside while we have focus */
      XDrawRectangle (info->display,
                      info->drawable,
                      screen_data->black_gc,
                      xoff + fgeom.left_width - 1,
                      yoff + fgeom.top_height - 1,
                      info->width - fgeom.right_width - fgeom.left_width + 1,
                      info->height - fgeom.bottom_height - fgeom.top_height + 1);
    }

  draw_current_control_bg (info, &fgeom);
  
  if (fgeom.title_rect.width > 0 && fgeom.title_rect.height > 0)
    {
      int layout_y;
      MetaRectangle clip;
      GC layout_gc;
      
      /* center vertically */
      layout_y = fgeom.title_rect.y +
        (fgeom.title_rect.height - d->layout_height) / 2;

      clip = fgeom.title_rect;
      clip.x += xoff;
      clip.y += yoff;
      clip.width -= HORIZONTAL_TEXT_PAD;

      layout_gc = screen_data->text_gc;
      
      if (info->flags & META_FRAME_HAS_FOCUS)
        {
          layout_gc = screen_data->selected_text_gc;

          /* Draw blue background */
          XFillRectangle (info->display,
                          info->drawable,
                          screen_data->selected_gc,
                          xoff + fgeom.title_rect.x,
                          yoff + fgeom.title_rect.y,
                          fgeom.title_rect.width,
                          fgeom.title_rect.height);
        }

      set_clip (info->display, layout_gc, &clip);
            
      pango_x_render_layout (info->display,
                             info->drawable,
                             layout_gc,
                             d->layout,
                             xoff + fgeom.title_rect.x + HORIZONTAL_TEXT_PAD,
                             yoff + layout_y);
      set_clip (info->display, screen_data->text_gc, NULL);
    }

  if (fgeom.close_rect.width > 0 && fgeom.close_rect.height > 0)
    {
      XDrawLine (info->display,
                 info->drawable,
                 screen_data->fg_gc,
                 xoff + fgeom.close_rect.x + INNER_BUTTON_PAD,
                 yoff + fgeom.close_rect.y + INNER_BUTTON_PAD,
                 xoff + fgeom.close_rect.x + fgeom.close_rect.width - INNER_BUTTON_PAD,
                 yoff + fgeom.close_rect.y + fgeom.close_rect.height - INNER_BUTTON_PAD);

      
      XDrawLine (info->display,
                 info->drawable,
                 screen_data->fg_gc,
                 xoff + fgeom.close_rect.x + INNER_BUTTON_PAD,
                 yoff + fgeom.close_rect.y + fgeom.close_rect.height - INNER_BUTTON_PAD,
                 xoff + fgeom.close_rect.x + fgeom.close_rect.width - INNER_BUTTON_PAD,
                 yoff + fgeom.close_rect.y + INNER_BUTTON_PAD);
    }

  if (fgeom.max_rect.width > 0 && fgeom.max_rect.height > 0)
    {      
      XDrawRectangle (info->display,
                      info->drawable,
                      screen_data->fg_gc,
                      xoff + fgeom.max_rect.x + INNER_BUTTON_PAD,
                      yoff + fgeom.max_rect.y + INNER_BUTTON_PAD,
                      fgeom.max_rect.width - INNER_BUTTON_PAD * 2,
                      fgeom.max_rect.height - INNER_BUTTON_PAD * 2);

      vals.line_width = 3;
      XChangeGC (info->display,
                 screen_data->fg_gc,
                 GCLineWidth,
                 &vals);
      
      XDrawLine (info->display,
                 info->drawable,
                 screen_data->fg_gc,
                 xoff + fgeom.max_rect.x + INNER_BUTTON_PAD,
                 yoff + fgeom.max_rect.y + INNER_BUTTON_PAD + vals.line_width / 2,
                 xoff + fgeom.max_rect.x + fgeom.max_rect.width - INNER_BUTTON_PAD,
                 yoff + fgeom.max_rect.y + INNER_BUTTON_PAD + vals.line_width / 2);
      
      vals.line_width = 0;
      XChangeGC (info->display,
                 screen_data->fg_gc,
                 GCLineWidth,
                 &vals);      
    }

  if (fgeom.min_rect.width > 0 && fgeom.min_rect.height > 0)
    {
      vals.line_width = 3;
      XChangeGC (info->display,
                 screen_data->fg_gc,
                 GCLineWidth,
                 &vals);
      
      XDrawLine (info->display,
                 info->drawable,
                 screen_data->fg_gc,
                 xoff + fgeom.min_rect.x + INNER_BUTTON_PAD,
                 yoff + fgeom.min_rect.y + fgeom.min_rect.height - INNER_BUTTON_PAD - vals.line_width / 2,
                 xoff + fgeom.min_rect.x + fgeom.min_rect.width - INNER_BUTTON_PAD,
                 yoff + fgeom.min_rect.y + fgeom.min_rect.height - INNER_BUTTON_PAD - vals.line_width / 2);

      vals.line_width = 0;
      XChangeGC (info->display,
                 screen_data->fg_gc,
                 GCLineWidth,
                 &vals);      
    }
  
  if (fgeom.spacer_rect.width > 0 && fgeom.spacer_rect.height > 0)
    {
      draw_vline (info, info->drawable,
                  screen_data->light_gc,
                  screen_data->dark_gc,
                  yoff + fgeom.spacer_rect.y,
                  yoff + fgeom.spacer_rect.y + fgeom.spacer_rect.height,
                  xoff + fgeom.spacer_rect.x);
    }

  if (fgeom.menu_rect.width > 0 && fgeom.menu_rect.height > 0)
    {
      int x, y;
      x = fgeom.menu_rect.x;
      y = fgeom.menu_rect.y;
      x += (fgeom.menu_rect.width - 7) / 2;
      y += (fgeom.menu_rect.height - 5) / 2;
      
      draw_varrow (info, info->drawable, screen_data->fg_gc, TRUE,
                   xoff + x, yoff + y, 7, 5);
    }
}

#define POINT_IN_RECT(xcoord, ycoord, rect) \
 ((xcoord) >= (rect).x &&                   \
  (xcoord) <  ((rect).x + (rect).width) &&  \
  (ycoord) >= (rect).y &&                   \
  (ycoord) <  ((rect).y + (rect).height))

#define RESIZE_EXTENDS 10
static MetaFrameControl
default_get_control (MetaFrameInfo *info,
                     int x, int y,
                     gpointer frame_data)
{
  DefaultFrameData *d;
  DefaultFrameGeometry fgeom;
  
  d = frame_data;

  calc_geometry (info, d, &fgeom);
  
  if (POINT_IN_RECT (x, y, fgeom.close_rect))
    return META_FRAME_CONTROL_DELETE;

  if (POINT_IN_RECT (x, y, fgeom.min_rect))
    return META_FRAME_CONTROL_ICONIFY;

  if (POINT_IN_RECT (x, y, fgeom.max_rect))
    return META_FRAME_CONTROL_MAXIMIZE;

  if (POINT_IN_RECT (x, y, fgeom.menu_rect))
    return META_FRAME_CONTROL_MENU;
  
  if (POINT_IN_RECT (x, y, fgeom.title_rect))
    return META_FRAME_CONTROL_TITLE;
  
  if (y > (info->height - fgeom.bottom_height - RESIZE_EXTENDS) &&
      x > (info->width - fgeom.right_width - RESIZE_EXTENDS))
    return META_FRAME_CONTROL_RESIZE_SE;
  
  return META_FRAME_CONTROL_NONE;
}

static void
default_get_control_rect (MetaFrameInfo *info,
                          MetaFrameControl control,
                          int *x, int *y,
                          int *width, int *height,
                          gpointer frame_data)
{
  MetaRectangle *rect;
  DefaultFrameData *d;
  DefaultFrameGeometry fgeom;
  
  d = frame_data;

  calc_geometry (info, d, &fgeom);
  
  rect = control_rect (control, &fgeom);

  if (rect)
    {
      *x = rect->x;
      *y = rect->y;
      *width = rect->width;
      *height = rect->height;
    }
  else
    {
      *x = *y = *width = *height = 0;
    }
}

/* FIXME add this to engine vtable */
static void
default_release_screen (Screen *screen)
{
  if (screen_data)
    {
      XFreeGC (DisplayOfScreen (screen), screen_data->text_gc);
      XFreeGC (DisplayOfScreen (screen), screen_data->fg_gc);
      XFreeGC (DisplayOfScreen (screen), screen_data->selected_gc);
      XFreeGC (DisplayOfScreen (screen), screen_data->selected_text_gc);
      XFreeGC (DisplayOfScreen (screen), screen_data->black_gc);
      XFreeGC (DisplayOfScreen (screen), screen_data->light_gc);
      XFreeGC (DisplayOfScreen (screen), screen_data->dark_gc);
      XFreeGC (DisplayOfScreen (screen), screen_data->active_gc);
      XFreeGC (DisplayOfScreen (screen), screen_data->prelight_gc);
    }
}

MetaThemeEngine meta_default_engine = {
  NULL,
  default_acquire_frame,
  default_release_frame,
  default_fill_frame_geometry,
  default_expose_frame,
  default_get_control,
  default_get_control_rect
};
