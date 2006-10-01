/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity RGB color stuff */

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

#include "colors.h"


static void
visual_decompose_mask (gulong  mask,
                       gint   *shift,
                       gint   *prec)
{
  /* This code is from GTK+, (C) GTK+ Team */
  *shift = 0;
  *prec = 0;

  while (!(mask & 0x1))
    {
      (*shift)++;
      mask >>= 1;
    }

  while (mask & 0x1)
    {
      (*prec)++;
      mask >>= 1;
    }
}

void
meta_screen_init_visual_info (MetaScreen *screen)
{
  Visual *xvisual;
  int nxvisuals;
  XVisualInfo *visual_list;
  XVisualInfo visual_template;

  /* root window visual */
  xvisual = DefaultVisual (screen->display->xdisplay,
                           screen->number);

  visual_template.visualid = XVisualIDFromVisual (xvisual);
  visual_list = XGetVisualInfo (screen->display->xdisplay,
                                VisualIDMask, &visual_template, &nxvisuals);

  if (nxvisuals != 1)
    meta_warning ("Matched weird number of visuals %d\n", nxvisuals);

  screen->visual_info = *visual_list;

  meta_verbose ("Using visual class %d\n", screen->visual_info.class);
  
  XFree (visual_list);
}

gulong
meta_screen_get_x_pixel (MetaScreen       *screen,
                         const PangoColor *color)
{
  /* This code is derived from GTK+, (C) GTK+ Team */
  gulong pixel;

  if (screen->visual_info.class == TrueColor ||
      screen->visual_info.class == DirectColor)
    {
      int red_prec, red_shift, green_prec, green_shift, blue_prec, blue_shift;

      visual_decompose_mask (screen->visual_info.red_mask,
                             &red_shift, &red_prec);
      visual_decompose_mask (screen->visual_info.green_mask,
                             &green_shift, &green_prec);
      visual_decompose_mask (screen->visual_info.blue_mask,
                             &blue_shift, &blue_prec);
      
      pixel = (((color->red >> (16 - red_prec)) << red_shift) +
               ((color->green >> (16 - green_prec)) << green_shift) +
               ((color->blue >> (16 - blue_prec)) << blue_shift));
    }
  else
    {
#define INTENSITY(r, g, b) ((r) * 0.30 + (g) * 0.59 + (b) * 0.11)
      double r, g, b;
      
      r = color->red / (double) 0xffff;
      g = color->green / (double) 0xffff;
      b = color->blue / (double) 0xffff;    
      
      /* Now this is a low-bloat GdkRGB replacement! */
      if (INTENSITY (r, g, b) > 0.5)
        pixel = WhitePixel (screen->display->xdisplay, screen->number);
      else
        pixel = BlackPixel (screen->display->xdisplay, screen->number);  
#undef INTENSITY
    }

  return pixel;
}

void
meta_screen_set_ui_colors (MetaScreen         *screen,
                           const MetaUIColors *colors)
{
  screen->colors = *colors;
  meta_screen_queue_frame_redraws (screen);
}

/* Straight out of gtkstyle.c */
static PangoColor meta_default_normal_fg =      {      0,      0,      0 };
static PangoColor meta_default_active_fg =      {      0,      0,      0 };
static PangoColor meta_default_prelight_fg =    {      0,      0,      0 };
static PangoColor meta_default_selected_fg =    { 0xffff, 0xffff, 0xffff };
static PangoColor meta_default_insensitive_fg = { 0x7530, 0x7530, 0x7530 };

static PangoColor meta_default_normal_bg =      { 0xd6d6, 0xd6d6, 0xd6d6 };
static PangoColor meta_default_active_bg =      { 0xc350, 0xc350, 0xc350 };
static PangoColor meta_default_prelight_bg =    { 0xea60, 0xea60, 0xea60 };
static PangoColor meta_default_selected_bg =    {     0,      0, 0x9c40 };
static PangoColor meta_default_insensitive_bg = { 0xd6d6, 0xd6d6, 0xd6d6 };

static void
rgb_to_hls (gdouble *r,
            gdouble *g,
            gdouble *b)
{
  gdouble min;
  gdouble max;
  gdouble red;
  gdouble green;
  gdouble blue;
  gdouble h, l, s;
  gdouble delta;
  
  red = *r;
  green = *g;
  blue = *b;
  
  if (red > green)
    {
      if (red > blue)
        max = red;
      else
        max = blue;
      
      if (green < blue)
        min = green;
      else
        min = blue;
    }
  else
    {
      if (green > blue)
        max = green;
      else
        max = blue;
      
      if (red < blue)
        min = red;
      else
        min = blue;
    }
  
  l = (max + min) / 2;
  s = 0;
  h = 0;
  
  if (max != min)
    {
      if (l <= 0.5)
        s = (max - min) / (max + min);
      else
        s = (max - min) / (2 - max - min);
      
      delta = max -min;
      if (red == max)
        h = (green - blue) / delta;
      else if (green == max)
        h = 2 + (blue - red) / delta;
      else if (blue == max)
        h = 4 + (red - green) / delta;
      
      h *= 60;
      if (h < 0.0)
        h += 360;
    }
  
  *r = h;
  *g = l;
  *b = s;
}

static void
hls_to_rgb (gdouble *h,
            gdouble *l,
            gdouble *s)
{
  gdouble hue;
  gdouble lightness;
  gdouble saturation;
  gdouble m1, m2;
  gdouble r, g, b;
  
  lightness = *l;
  saturation = *s;
  
  if (lightness <= 0.5)
    m2 = lightness * (1 + saturation);
  else
    m2 = lightness + saturation - lightness * saturation;
  m1 = 2 * lightness - m2;
  
  if (saturation == 0)
    {
      *h = lightness;
      *l = lightness;
      *s = lightness;
    }
  else
    {
      hue = *h + 120;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;
      
      if (hue < 60)
        r = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        r = m2;
      else if (hue < 240)
        r = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        r = m1;
      
      hue = *h;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;
      
      if (hue < 60)
        g = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        g = m2;
      else if (hue < 240)
        g = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        g = m1;
      
      hue = *h - 120;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;
      
      if (hue < 60)
        b = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        b = m2;
      else if (hue < 240)
        b = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        b = m1;
      
      *h = r;
      *l = g;
      *s = b;
    }
}

static void
style_shade (PangoColor *a,
             PangoColor *b,
             gdouble     k)
{
  gdouble red;
  gdouble green;
  gdouble blue;
  
  red = (gdouble) a->red / 65535.0;
  green = (gdouble) a->green / 65535.0;
  blue = (gdouble) a->blue / 65535.0;
  
  rgb_to_hls (&red, &green, &blue);
  
  green *= k;
  if (green > 1.0)
    green = 1.0;
  else if (green < 0.0)
    green = 0.0;
  
  blue *= k;
  if (blue > 1.0)
    blue = 1.0;
  else if (blue < 0.0)
    blue = 0.0;
  
  hls_to_rgb (&red, &green, &blue);
  
  b->red = red * 65535.0;
  b->green = green * 65535.0;
  b->blue = blue * 65535.0;
}

#define LIGHTNESS_MULT  1.3
#define DARKNESS_MULT   0.7
void
meta_screen_init_ui_colors (MetaScreen *screen)
{
  int i;
  MetaUIColors *colors;

  colors = &screen->colors;
  
  colors->fg[META_STATE_NORMAL] = meta_default_normal_fg;
  colors->fg[META_STATE_ACTIVE] = meta_default_active_fg;
  colors->fg[META_STATE_PRELIGHT] = meta_default_prelight_fg;
  colors->fg[META_STATE_SELECTED] = meta_default_selected_fg;
  colors->fg[META_STATE_INSENSITIVE] = meta_default_insensitive_fg;
  
  colors->bg[META_STATE_NORMAL] = meta_default_normal_bg;
  colors->bg[META_STATE_ACTIVE] = meta_default_active_bg;
  colors->bg[META_STATE_PRELIGHT] = meta_default_prelight_bg;
  colors->bg[META_STATE_SELECTED] = meta_default_selected_bg;
  colors->bg[META_STATE_INSENSITIVE] = meta_default_insensitive_bg;
  
  for (i = 0; i < 4; i++)
    {
      colors->text[i] = colors->fg[i];
      colors->base[i].red = G_MAXUSHORT;
      colors->base[i].green = G_MAXUSHORT;
      colors->base[i].blue = G_MAXUSHORT;
    }

  colors->base[META_STATE_SELECTED] = meta_default_selected_bg;
  colors->base[META_STATE_INSENSITIVE] = meta_default_prelight_bg;
  colors->text[META_STATE_INSENSITIVE] = meta_default_insensitive_fg;

  for (i = 0; i < 5; i++)
    {
      style_shade (&colors->bg[i], &colors->light[i], LIGHTNESS_MULT);
      style_shade (&colors->bg[i], &colors->dark[i], DARKNESS_MULT);
      
      colors->mid[i].red = (colors->light[i].red + colors->dark[i].red) / 2;
      colors->mid[i].green = (colors->light[i].green + colors->dark[i].green) / 2;
      colors->mid[i].blue = (colors->light[i].blue + colors->dark[i].blue) / 2;

      colors->text_aa[i].red = (colors->text[i].red + colors->base[i].red) / 2;
      colors->text_aa[i].green = (colors->text[i].green + colors->base[i].green) / 2;
      colors->text_aa[i].blue = (colors->text[i].blue + colors->base[i].blue) / 2;
    }
}
