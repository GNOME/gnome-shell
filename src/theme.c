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

struct _DefaultFrameData
{
  PangoLayout *layout;
  GC text_gc;
};

static gpointer
default_acquire_frame (MetaFrameInfo *info)
{
  DefaultFrameData *d;
  PangoFontDescription *desc;
  XGCValues vals;
  PangoColor color;
  
  d = g_new (DefaultFrameData, 1);

  desc = pango_font_description_from_string ("Sans 16");
  d->layout = pango_layout_new (meta_get_pango_context (info->screen,
                                                        desc,
                                                        info->frame));

  color.red = color.green = color.blue = 0xffff;
  vals.foreground = meta_get_x_pixel (info->screen, &color);
  d->text_gc = XCreateGC (info->display,
                          RootWindowOfScreen (info->screen),
                          GCForeground,
                          &vals);
  
  return d;
}

void
default_release_frame (MetaFrameInfo *info,
                       gpointer       frame_data)
{
  DefaultFrameData *d;

  d = frame_data;

  if (d->layout)
    g_object_unref (G_OBJECT (d->layout));

  XFreeGC (info->display, d->text_gc);
  
  g_free (d);
}

#define VERTICAL_TEXT_PAD 3
#define LEFT_WIDTH 2
#define RIGHT_WIDTH 2
#define BOTTOM_HEIGHT 2
void
default_fill_frame_geometry (MetaFrameInfo *info,
                             MetaFrameGeometry *geom,
                             gpointer       frame_data)
{
  DefaultFrameData *d;
  PangoRectangle rect;
  PangoColor color;
  
  d = frame_data;
      
  if (info->title)
    pango_layout_set_text (d->layout, info->title, -1);
  else
    pango_layout_set_text (d->layout, " ", -1);
  
  pango_layout_get_pixel_extents (d->layout, NULL, &rect);

  geom->top_height = rect.height + VERTICAL_TEXT_PAD * 2;
  
  geom->left_width = LEFT_WIDTH;
  geom->right_width = RIGHT_WIDTH;
  geom->bottom_height = BOTTOM_HEIGHT;

  color.red = color.blue = color.green = 0;
  
  geom->background_pixel = meta_get_x_pixel (info->screen, &color);
}

void
default_expose_frame (MetaFrameInfo *info,
                      int x, int y,
                      int width, int height,
                      gpointer frame_data)
{
  DefaultFrameData *d;
  
  d = frame_data;
  
  pango_x_render_layout (info->display,
                         info->frame,
                         d->text_gc,
                         d->layout,
                         LEFT_WIDTH,
                         VERTICAL_TEXT_PAD);
}

MetaFrameControl
default_get_control (MetaFrameInfo *info,
                     int x, int y,
                     gpointer frame_data)
{


  return META_FRAME_CONTROL_NONE;
}

MetaThemeEngine meta_default_engine = {
  NULL,
  default_acquire_frame,
  default_release_frame,
  default_fill_frame_geometry,
  default_expose_frame,
  default_get_control
};
