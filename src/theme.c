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

typedef struct _DefaultFrameData DefaultFrameData;
typedef struct _DefaultScreenData DefaultScreenData;

struct _DefaultFrameData
{
  PangoLayout *layout;
  int title_height;
};

struct _DefaultScreenData
{
  GC text_gc;
  GC fg_gc;
  GC light_gc;
  GC dark_gc;
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
      screen_data = g_new (DefaultScreenData, 1);      

      vals.foreground = meta_get_x_pixel (info->screen,
                                          &info->colors->fg[META_STATE_NORMAL]);
      /* FIXME memory-inefficient, could use the same one for all frames
       * w/ the same root window
       */
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
    }
  
  d = g_new (DefaultFrameData, 1);

  desc = pango_font_description_from_string ("Sans 16");
  d->layout = pango_layout_new (meta_get_pango_context (info->screen,
                                                        desc,
                                                        info->frame));
  pango_font_description_free (desc);
  
  d->title_height = 0;
  
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

#define VERTICAL_TEXT_PAD 3
#define LEFT_WIDTH 15
#define RIGHT_WIDTH 15
#define BOTTOM_HEIGHT 20
#define SPACER_SPACING 3
static void
default_fill_frame_geometry (MetaFrameInfo *info,
                             MetaFrameGeometry *geom,
                             gpointer       frame_data)
{
  DefaultFrameData *d;
  PangoRectangle rect;
  
  d = frame_data;
      
  if (info->title)
    pango_layout_set_text (d->layout, info->title, -1);
  else
    pango_layout_set_text (d->layout, " ", -1);

  pango_layout_get_pixel_extents (d->layout, NULL, &rect);

  d->title_height = rect.height + VERTICAL_TEXT_PAD * 2;
  geom->top_height = d->title_height;
  
  geom->left_width = LEFT_WIDTH;
  geom->right_width = RIGHT_WIDTH;
  geom->bottom_height = BOTTOM_HEIGHT;
  
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
default_expose_frame (MetaFrameInfo *info,
                      int x, int y,
                      int width, int height,
                      gpointer frame_data)
{
  DefaultFrameData *d;
  int close_size;
  XGCValues vals;
  
  d = frame_data;
  
  pango_x_render_layout (info->display,
                         info->frame,
                         screen_data->text_gc,
                         d->layout,
                         LEFT_WIDTH,
                         VERTICAL_TEXT_PAD);

  close_size = d->title_height;

  vals.line_width = 2;
  XChangeGC (info->display,
             screen_data->fg_gc,
             GCLineWidth,
             &vals);
  
  XDrawLine (info->display,
             info->frame,
             screen_data->fg_gc,
             info->width - RIGHT_WIDTH - close_size,
             VERTICAL_TEXT_PAD,
             info->width - RIGHT_WIDTH,
             d->title_height - VERTICAL_TEXT_PAD);

  XDrawLine (info->display,
             info->frame,
             screen_data->fg_gc,
             info->width - RIGHT_WIDTH,
             VERTICAL_TEXT_PAD,
             info->width - RIGHT_WIDTH - close_size,
             d->title_height - VERTICAL_TEXT_PAD);

  vals.line_width = 0;
  XChangeGC (info->display,
             screen_data->fg_gc,
             GCLineWidth,
             &vals);
  
  draw_vline (info, info->frame,
              screen_data->light_gc,
              screen_data->dark_gc,
              VERTICAL_TEXT_PAD,
              d->title_height - VERTICAL_TEXT_PAD,
              info->width - RIGHT_WIDTH - close_size - SPACER_SPACING);
}

#define RESIZE_EXTENDS 10
static MetaFrameControl
default_get_control (MetaFrameInfo *info,
                     int x, int y,
                     gpointer frame_data)
{
  DefaultFrameData *d;
  int close_size;
  
  d = frame_data;

  close_size = d->title_height;
  if (y < d->title_height &&
      x > info->width - RIGHT_WIDTH - close_size)
    return META_FRAME_CONTROL_DELETE;
  
  if (y < d->title_height)
    return META_FRAME_CONTROL_TITLE;
  
  if (y > (info->height - BOTTOM_HEIGHT - RESIZE_EXTENDS) &&
      x > (info->width - RIGHT_WIDTH - RESIZE_EXTENDS))
    return META_FRAME_CONTROL_RESIZE_SE;
  
  return META_FRAME_CONTROL_NONE;
}

/* FIXME add this to engine vtable */
static void
default_release_screen (Screen *screen)
{
  if (screen_data)
    {
      XFreeGC (DisplayOfScreen (screen), screen_data->text_gc);
      XFreeGC (DisplayOfScreen (screen), screen_data->fg_gc);
    }
}

MetaThemeEngine meta_default_engine = {
  NULL,
  default_acquire_frame,
  default_release_frame,
  default_fill_frame_geometry,
  default_expose_frame,
  default_get_control
};
