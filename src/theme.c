/* Metacity Theme Rendering */

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
#include "util.h"
#include "gradient.h"
#include <gtk/gtkwidget.h>
#include <string.h>

MetaFrameLayout*
meta_frame_layout_new  (void)
{
  MetaFrameLayout *layout;

  layout = g_new0 (MetaFrameLayout, 1);

  return layout;
}

void
meta_frame_layout_free (MetaFrameLayout *layout)
{
  g_return_if_fail (layout != NULL);
  
  g_free (layout);
}

void
meta_frame_layout_get_borders (const MetaFrameLayout *layout,
                               GtkWidget             *widget,
                               int                    text_height,
                               MetaFrameFlags         flags,
                               int                   *top_height,
                               int                   *bottom_height,
                               int                   *left_width,
                               int                   *right_width)
{
  int buttons_height, title_height, spacer_height;

  g_return_if_fail (top_height != NULL);
  g_return_if_fail (bottom_height != NULL);
  g_return_if_fail (left_width != NULL);
  g_return_if_fail (right_width != NULL);
  
  buttons_height = layout->button_height +
    layout->button_border.top + layout->button_border.bottom;
  title_height = text_height +
    layout->text_border.top + layout->text_border.bottom +
    layout->title_border.top + layout->title_border.bottom;
  spacer_height = layout->spacer_height;

  if (top_height)
    {
      *top_height = MAX (buttons_height, title_height);
      *top_height = MAX (*top_height, spacer_height);
    }

  if (left_width)
    *left_width = layout->left_width;
  if (right_width)
    *right_width = layout->right_width;

  if (bottom_height)
    {
      if (flags & META_FRAME_SHADED)
        *bottom_height = 0;
      else
        *bottom_height = layout->bottom_height;
    }
}

void
meta_frame_layout_calc_geometry (const MetaFrameLayout *layout,
                                 GtkWidget             *widget,
                                 int                    text_height,
                                 MetaFrameFlags         flags,
                                 int                    client_width,
                                 int                    client_height,
                                 MetaFrameGeometry     *fgeom)
{
  int x;
  int button_y;
  int title_right_edge;
  int width, height;

  meta_frame_layout_get_borders (layout, widget, text_height,
                                 flags,
                                 &fgeom->top_height,
                                 &fgeom->bottom_height,
                                 &fgeom->left_width,
                                 &fgeom->right_width);
  
  width = client_width + fgeom->left_width + fgeom->right_width;
  height = client_height + fgeom->top_height + fgeom->bottom_height;
  
  fgeom->width = width;
  fgeom->height = height;

  x = width - layout->right_inset;

  /* center buttons */
  button_y = (fgeom->top_height -
              (layout->button_height + layout->button_border.top + layout->button_border.bottom)) / 2 + layout->button_border.top;

  if ((flags & META_FRAME_ALLOWS_DELETE) &&
      x >= 0)
    {
      fgeom->close_rect.x = x - layout->button_border.right - layout->button_width;
      fgeom->close_rect.y = button_y;
      fgeom->close_rect.width = layout->button_width;
      fgeom->close_rect.height = layout->button_height;

      x = fgeom->close_rect.x - layout->button_border.left;
    }
  else
    {
      fgeom->close_rect.x = 0;
      fgeom->close_rect.y = 0;
      fgeom->close_rect.width = 0;
      fgeom->close_rect.height = 0;
    }

  if ((flags & META_FRAME_ALLOWS_MAXIMIZE) &&
      x >= 0)
    {
      fgeom->max_rect.x = x - layout->button_border.right - layout->button_width;
      fgeom->max_rect.y = button_y;
      fgeom->max_rect.width = layout->button_width;
      fgeom->max_rect.height = layout->button_height;

      x = fgeom->max_rect.x - layout->button_border.left;
    }
  else
    {
      fgeom->max_rect.x = 0;
      fgeom->max_rect.y = 0;
      fgeom->max_rect.width = 0;
      fgeom->max_rect.height = 0;
    }
  
  if ((flags & META_FRAME_ALLOWS_MINIMIZE) &&
      x >= 0)
    {
      fgeom->min_rect.x = x - layout->button_border.right - layout->button_width;
      fgeom->min_rect.y = button_y;
      fgeom->min_rect.width = layout->button_width;
      fgeom->min_rect.height = layout->button_height;

      x = fgeom->min_rect.x - layout->button_border.left;
    }
  else
    {
      fgeom->min_rect.x = 0;
      fgeom->min_rect.y = 0;
      fgeom->min_rect.width = 0;
      fgeom->min_rect.height = 0;
    }

  if ((fgeom->close_rect.width > 0 ||
       fgeom->max_rect.width > 0 ||
       fgeom->min_rect.width > 0) &&
      x >= 0)
    {
      fgeom->spacer_rect.x = x - layout->spacer_padding - layout->spacer_width;
      fgeom->spacer_rect.y = (fgeom->top_height - layout->spacer_height) / 2;
      fgeom->spacer_rect.width = layout->spacer_width;
      fgeom->spacer_rect.height = layout->spacer_height;

      x = fgeom->spacer_rect.x - layout->spacer_padding;
    }
  else
    {
      fgeom->spacer_rect.x = 0;
      fgeom->spacer_rect.y = 0;
      fgeom->spacer_rect.width = 0;
      fgeom->spacer_rect.height = 0;
    }

  title_right_edge = x - layout->title_border.right;
  
  /* Now x changes to be position from the left */
  x = layout->left_inset;
  
  if (flags & META_FRAME_ALLOWS_MENU)
    {
      fgeom->menu_rect.x = x + layout->button_border.left;
      fgeom->menu_rect.y = button_y;
      fgeom->menu_rect.width = layout->button_width;
      fgeom->menu_rect.height = layout->button_height;

      x = fgeom->menu_rect.x + fgeom->menu_rect.width + layout->button_border.right;
    }
  else
    {
      fgeom->menu_rect.x = 0;
      fgeom->menu_rect.y = 0;
      fgeom->menu_rect.width = 0;
      fgeom->menu_rect.height = 0;
    }

  /* If menu overlaps close button, then the menu wins since it
   * lets you perform any operation including close
   */
  if (fgeom->close_rect.width > 0 &&
      fgeom->close_rect.x < (fgeom->menu_rect.x + fgeom->menu_rect.height))
    {
      fgeom->close_rect.width = 0;
      fgeom->close_rect.height = 0;
    }

  /* Check for maximize overlap */
  if (fgeom->max_rect.width > 0 &&
      fgeom->max_rect.x < (fgeom->menu_rect.x + fgeom->menu_rect.height))
    {
      fgeom->max_rect.width = 0;
      fgeom->max_rect.height = 0;
    }
  
  /* Check for minimize overlap */
  if (fgeom->min_rect.width > 0 &&
      fgeom->min_rect.x < (fgeom->menu_rect.x + fgeom->menu_rect.height))
    {
      fgeom->min_rect.width = 0;
      fgeom->min_rect.height = 0;
    }

  /* Check for spacer overlap */
  if (fgeom->spacer_rect.width > 0 &&
      fgeom->spacer_rect.x < (fgeom->menu_rect.x + fgeom->menu_rect.height))
    {
      fgeom->spacer_rect.width = 0;
      fgeom->spacer_rect.height = 0;
    }
  
  /* We always fill as much vertical space as possible with title rect,
   * rather than centering it like the buttons and spacer
   */
  fgeom->title_rect.x = x + layout->title_border.left;
  fgeom->title_rect.y = layout->title_border.top;
  fgeom->title_rect.width = title_right_edge - fgeom->title_rect.x;
  fgeom->title_rect.height = fgeom->top_height - layout->title_border.top - layout->title_border.bottom;

  /* Nuke title if it won't fit */
  if (fgeom->title_rect.width < 0 ||
      fgeom->title_rect.height < 0)
    {
      fgeom->title_rect.width = 0;
      fgeom->title_rect.height = 0;
    }
}

MetaGradientSpec*
meta_gradient_spec_new (MetaGradientType type)
{
  MetaGradientSpec *spec;

  spec = g_new (MetaGradientSpec, 1);

  spec->type = type;
  spec->color_specs = NULL;
  
  return spec;
}

void
meta_gradient_spec_free (MetaGradientSpec *spec)
{
  g_return_if_fail (spec != NULL);
  
  g_slist_foreach (spec->color_specs, (GFunc) meta_color_spec_free, NULL);
  g_slist_free (spec->color_specs);
  g_free (spec);
}

GdkPixbuf*
meta_gradient_spec_render (const MetaGradientSpec *spec,
                           GtkWidget              *widget,
                           int                     width,
                           int                     height)
{
  int n_colors;
  GdkColor *colors;
  GSList *tmp;
  int i;
  GdkPixbuf *pixbuf;
  
  n_colors = g_slist_length (spec->color_specs);

  if (n_colors == 0)
    return NULL;
  
  colors = g_new (GdkColor, n_colors);

  i = 0;
  tmp = spec->color_specs;
  while (tmp != NULL)
    {
      meta_color_spec_render (tmp->data, widget, &colors[i]);
      
      tmp = tmp->next;
      ++i;
    }
  
  pixbuf = meta_gradient_create_multi (width, height,
                                       colors, n_colors,
                                       spec->type);

  g_free (colors);

  return pixbuf;
}

MetaColorSpec*
meta_color_spec_new (MetaColorSpecType type)
{
  MetaColorSpec *spec;
  MetaColorSpec dummy;
  int size;

  size = G_STRUCT_OFFSET (MetaColorSpec, data);
  
  switch (type)
    {
    case META_COLOR_SPEC_BASIC:
      size += sizeof (dummy.data.basic);
      break;

    case META_COLOR_SPEC_GTK:
      size += sizeof (dummy.data.gtk);
      break;

    case META_COLOR_SPEC_BLEND:
      size += sizeof (dummy.data.blend);
      break;
    }
  
  spec = g_malloc0 (size);

  spec->type = type;  
  
  return spec;
}

void
meta_color_spec_free (MetaColorSpec *spec)
{
  g_return_if_fail (spec != NULL);
  
  switch (spec->type)
    {
    case META_COLOR_SPEC_BASIC:      
      break;

    case META_COLOR_SPEC_GTK:
      break;

    case META_COLOR_SPEC_BLEND:
      if (spec->data.blend.foreground)
        meta_color_spec_free (spec->data.blend.foreground);      
      if (spec->data.blend.background)
        meta_color_spec_free (spec->data.blend.background);
      break;
    }
  
  g_free (spec);
}

void
meta_color_spec_render (MetaColorSpec *spec,
                        GtkWidget     *widget,
                        GdkColor      *color)
{
  g_return_if_fail (spec != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (widget->style != NULL);
  
  switch (spec->type)
    {
    case META_COLOR_SPEC_BASIC:
      *color = spec->data.basic.color;
      break;

    case META_COLOR_SPEC_GTK:
      switch (spec->data.gtk.component)
        {
        case GTK_RC_BG:
          *color = widget->style->bg[spec->data.gtk.state];
          break;
        case GTK_RC_FG:
          *color = widget->style->fg[spec->data.gtk.state];
          break;
        case GTK_RC_BASE:
          *color = widget->style->base[spec->data.gtk.state];
          break;
        case GTK_RC_TEXT:
          *color = widget->style->text[spec->data.gtk.state];
          break;
        }
      break;

    case META_COLOR_SPEC_BLEND:
      {
        GdkColor fg, bg;
        int alpha;
        
        meta_color_spec_render (spec->data.blend.foreground, widget, &fg);
        meta_color_spec_render (spec->data.blend.background, widget, &bg);

        *color = fg;
        alpha = spec->data.blend.alpha * 0xffff;
        color->red = color->red + (((bg.red - color->red) * alpha + 0x8000) >> 16);
        color->green = color->green + (((bg.green - color->green) * alpha + 0x8000) >> 16);
        color->blue = color->blue + (((bg.blue - color->blue) * alpha + 0x8000) >> 16);        
      }
      break;
    }
}


MetaTextureSpec*
meta_texture_spec_new (MetaTextureType type)
{
  MetaTextureSpec *spec;
  MetaTextureSpec dummy;
  int size;

  size = G_STRUCT_OFFSET (MetaTextureSpec, data);
  
  switch (type)
    {
    case META_TEXTURE_SOLID:
      size += sizeof (dummy.data.solid);
      break;

    case META_TEXTURE_GRADIENT:
      size += sizeof (dummy.data.gradient);
      break;

    case META_TEXTURE_IMAGE:
      size += sizeof (dummy.data.image);
      break;
    }
  
  spec = g_malloc0 (size);

  spec->type = type;  
  
  return spec;
}

void
meta_texture_spec_free (MetaTextureSpec *spec)
{
  g_return_if_fail (spec != NULL);
  
  switch (spec->type)
    {
    case META_TEXTURE_SOLID:
      if (spec->data.solid.color_spec)
        meta_color_spec_free (spec->data.solid.color_spec);
      break;

    case META_TEXTURE_GRADIENT:
      if (spec->data.gradient.gradient_spec)
        meta_gradient_spec_free (spec->data.gradient.gradient_spec);
      break;

    case META_TEXTURE_IMAGE:
      if (spec->data.image.pixbuf)
        g_object_unref (G_OBJECT (spec->data.image.pixbuf));
      break;
    }

  g_free (spec);
}

static void
render_pixbuf (GdkDrawable        *drawable,
               const GdkRectangle *clip,
               GdkPixbuf          *pixbuf,
               int                 x,
               int                 y)
{
  /* grumble, render_to_drawable_alpha does not accept a clip
   * mask, so we have to go through some BS
   */
  GdkRectangle pixbuf_rect;
  GdkRectangle draw_rect;
  
  pixbuf_rect.x = x;
  pixbuf_rect.y = y;
  pixbuf_rect.width = gdk_pixbuf_get_width (pixbuf);
  pixbuf_rect.height = gdk_pixbuf_get_height (pixbuf);

  if (clip)
    {
      if (!gdk_rectangle_intersect ((GdkRectangle*)clip,
                                    &pixbuf_rect, &draw_rect))
        return;
    }
  else
    {
      draw_rect = pixbuf_rect;
    }
  
  gdk_pixbuf_render_to_drawable_alpha (pixbuf,
                                       drawable,
                                       draw_rect.x - pixbuf_rect.x,
                                       draw_rect.y - pixbuf_rect.y,
                                       draw_rect.x, draw_rect.y,
                                       draw_rect.width,
                                       draw_rect.height,
                                       GDK_PIXBUF_ALPHA_FULL, /* ignored */
                                       128,                   /* ignored */
                                       GDK_RGB_DITHER_NORMAL,
                                       draw_rect.x - pixbuf_rect.x,
                                       draw_rect.y - pixbuf_rect.y);
}


void
meta_texture_spec_draw   (const MetaTextureSpec *spec,
                          GtkWidget             *widget,
                          GdkDrawable           *drawable,
                          const GdkRectangle    *clip,
                          MetaTextureDrawMode    mode,
                          int                    x,
                          int                    y,
                          int                    width,
                          int                    height)
{
  g_return_if_fail (spec != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_DRAWABLE (drawable));
  g_return_if_fail (widget->style != NULL);
  
  switch (spec->type)
    {
    case META_TEXTURE_SOLID:
      {
        GdkGC *gc;
        GdkGCValues values;

        g_return_if_fail (spec->data.solid.color_spec != NULL);
        
        meta_color_spec_render (spec->data.solid.color_spec,
                                widget,
                                &values.foreground);
        
        gdk_rgb_find_color (widget->style->colormap, &values.foreground);
        gc = gdk_gc_new_with_values (drawable, &values, GDK_GC_FOREGROUND);
                         
        gdk_draw_rectangle (drawable,
                            gc, TRUE, x, y, width, height);

        g_object_unref (G_OBJECT (gc));
      }
      break;

    case META_TEXTURE_GRADIENT:
      {
        GdkPixbuf *pixbuf;
        
        g_return_if_fail (spec->data.gradient.gradient_spec != NULL);

        pixbuf = meta_gradient_spec_render (spec->data.gradient.gradient_spec,
                                            widget, width, height);

        if (pixbuf == NULL)
          return;
        
        render_pixbuf (drawable, clip, pixbuf, x, y);
        
        g_object_unref (G_OBJECT (pixbuf));
      }      
      break;

    case META_TEXTURE_IMAGE:
      {
        GdkPixbuf *pixbuf;
        
        g_return_if_fail (spec->data.image.pixbuf != NULL);

        pixbuf = NULL;
        
        switch (mode)
          {
          case META_TEXTURE_DRAW_UNSCALED:
            pixbuf = spec->data.image.pixbuf;
            g_object_ref (G_OBJECT (pixbuf));
            break;
          case META_TEXTURE_DRAW_SCALED_VERTICALLY:
            pixbuf = spec->data.image.pixbuf;
            if (gdk_pixbuf_get_height (pixbuf) == height)
              {
                g_object_ref (G_OBJECT (pixbuf));
              }
            else
              {
                pixbuf = gdk_pixbuf_scale_simple (pixbuf,
                                                  gdk_pixbuf_get_width (pixbuf),
                                                  height,
                                                  GDK_INTERP_BILINEAR);
                if (pixbuf == NULL)
                  return;
              }
            break;
          case META_TEXTURE_DRAW_SCALED_HORIZONTALLY:
            pixbuf = spec->data.image.pixbuf;
            if (gdk_pixbuf_get_width (pixbuf) == width)
              {
                g_object_ref (G_OBJECT (pixbuf));
              }
            else
              {
                pixbuf = gdk_pixbuf_scale_simple (pixbuf,
                                                  width,
                                                  gdk_pixbuf_get_height (pixbuf),
                                                  GDK_INTERP_BILINEAR);
                if (pixbuf == NULL)
                  return;
              }
            break;
          case META_TEXTURE_DRAW_SCALED_BOTH:
            pixbuf = spec->data.image.pixbuf;
            if (gdk_pixbuf_get_width (pixbuf) == width &&
                gdk_pixbuf_get_height (pixbuf) == height)
              {
                g_object_ref (G_OBJECT (pixbuf));
              }
            else
              {
                pixbuf = gdk_pixbuf_scale_simple (pixbuf,
                                                  width, height,
                                                  GDK_INTERP_BILINEAR);
                if (pixbuf == NULL)
                  return;
              }
            break;
          }

        g_return_if_fail (pixbuf != NULL);
        
        render_pixbuf (drawable, clip, pixbuf, x, y);
        
        g_object_unref (G_OBJECT (pixbuf));
      }
      break;
    }  
}

MetaFrameStyle*
meta_frame_style_new (void)
{
  MetaFrameStyle *style;

  style = g_new0 (MetaFrameStyle, 1);

  style->refcount = 1;
  
  return style;
}

void
meta_frame_style_ref (MetaFrameStyle *style)
{
  g_return_if_fail (style != NULL);
  
  style->refcount += 1;
}

static void
free_button_textures (MetaTextureSpec *textures[META_BUTTON_TYPE_LAST][META_BUTTON_STATE_LAST])
{
  int i, j;
  
  i = 0;
  while (i < META_BUTTON_TYPE_LAST)
    {
      j = 0;
      while (j < META_BUTTON_STATE_LAST)
        {
          if (textures[i][j])
            meta_texture_spec_free (textures[i][j]);
          
          ++j;
        }
      
      ++i;
    }
}

void
meta_frame_style_unref (MetaFrameStyle *style)
{
  g_return_if_fail (style != NULL);
  g_return_if_fail (style->refcount > 0);
  
  style->refcount -= 1;

  if (style->refcount == 0)
    {
      int i;
      
      free_button_textures (style->button_icons);
      free_button_textures (style->button_backgrounds);

      i = 0;
      while (i < META_FRAME_PIECE_LAST)
        {
          if (style->pieces[i])
            meta_texture_spec_free (style->pieces[i]);
          
          ++i;
        }

      if (style->layout)
        meta_frame_layout_free (style->layout);
      
      g_free (style);
    }
}

MetaFrameStyleSet*
meta_frame_style_set_new (void)
{
  MetaFrameStyleSet *style_set;

  style_set = g_new0 (MetaFrameStyleSet, 1);

  return style_set;
}

void
meta_frame_style_set_free (MetaFrameStyleSet *style_set)
{
  int i, j;

  i = 0;
  while (i < META_WINDOW_TYPE_LAST)
    {
      j = 0;
      while (j < META_WINDOW_STATE_LAST)
        {
          if (style_set->styles[i][j])
            meta_frame_style_unref (style_set->styles[i][j]);
          
          ++j;
        }

      ++i;
    }

  g_free (style_set);
}
