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

#define GDK_COLOR_RGBA(color)                                   \
                         (0xff                         |        \
                         (((color).red / 256) << 24)   |        \
                         (((color).green / 256) << 16) |        \
                         (((color).blue / 256) << 8))

#define GDK_COLOR_RGB(color)                                    \
                         ((((color).red / 256) << 16)   |        \
                          (((color).green / 256) << 8)  |        \
                          (((color).blue / 256)))

static void
color_composite (const GdkColor *bg,
                 const GdkColor *fg,
                 double          alpha_d,
                 GdkColor       *color)
{
  guint16 alpha;
  
  *color = *bg;
  alpha = alpha_d * 0xffff;
  color->red = color->red + (((fg->red - color->red) * alpha + 0x8000) >> 16);
  color->green = color->green + (((fg->green - color->green) * alpha + 0x8000) >> 16);
  color->blue = color->blue + (((fg->blue - color->blue) * alpha + 0x8000) >> 16);
}

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
        GdkColor bg, fg;

        meta_color_spec_render (spec->data.blend.background, widget, &bg);
        meta_color_spec_render (spec->data.blend.foreground, widget, &fg);

        color_composite (&bg, &fg, spec->data.blend.alpha, color);
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

    case META_TEXTURE_COMPOSITE:
      size += sizeof (dummy.data.composite);
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

    case META_TEXTURE_COMPOSITE:
      if (spec->data.composite.background)
        meta_texture_spec_free (spec->data.composite.background);
      if (spec->data.composite.foreground)
        meta_texture_spec_free (spec->data.composite.foreground);
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


static void
render_pixbuf_aligned (GdkDrawable        *drawable,
                       const GdkRectangle *clip,
                       GdkPixbuf          *pixbuf,
                       double              xalign,
                       double              yalign,
                       int                 x,
                       int                 y,
                       int                 width,
                       int                 height)
{
  int pix_width;
  int pix_height;
  int rx, ry;
  
  pix_width = gdk_pixbuf_get_width (pixbuf);
  pix_height = gdk_pixbuf_get_height (pixbuf);
  
  rx = x + (width - pix_width) * xalign;
  ry = y + (height - pix_height) * yalign;
  
  render_pixbuf (drawable, clip, pixbuf, rx, ry);
}

static GdkPixbuf*
multiply_alpha (GdkPixbuf *pixbuf,
                guchar     alpha)
{
  GdkPixbuf *new_pixbuf;
  guchar *pixels;
  int rowstride;
  int height;
  int row;
  
  if (alpha == 255)
    return pixbuf;

  if (!gdk_pixbuf_get_has_alpha (pixbuf))
    {
      new_pixbuf = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);
      g_object_unref (G_OBJECT (pixbuf));
      pixbuf = new_pixbuf;
    }
  
  pixels = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  row = 0;
  while (row < height)
    {
      guchar *p;
      guchar *end;

      p = pixels + row * rowstride;
      end = p + rowstride;

      while (p != end)
        {
          p += 3; /* skip RGB */

          /* multiply the two alpha channels. not sure this is right.
           * but some end cases are that if the pixbuf contains 255,
           * then it should be modified to contain "alpha"; if the
           * pixbuf contains 0, it should remain 0.
           */
          *p = (*p * alpha) / 65025; /* (*p / 255) * (alpha / 255); */
          
          ++p; /* skip A */
        }
      
      ++row;
    }

  return pixbuf;
}

static GdkPixbuf*
meta_texture_spec_render (const MetaTextureSpec *spec,
                          GtkWidget             *widget,
                          MetaTextureDrawMode    mode,
                          guchar                 alpha,
                          int                    width,
                          int                    height)
{
  GdkPixbuf *pixbuf;

  pixbuf = NULL;

  g_return_val_if_fail (spec != NULL, NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (widget->style != NULL, NULL);
  
  switch (spec->type)
    {
    case META_TEXTURE_SOLID:
      {
        GdkColor color;
        
        g_return_val_if_fail (spec->data.solid.color_spec != NULL,
                              NULL);

        meta_color_spec_render (spec->data.solid.color_spec,
                                widget, &color);

        if (alpha == 255)
          {
            pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                     FALSE,
                                     8, width, height);
            gdk_pixbuf_fill (pixbuf, GDK_COLOR_RGBA (color));
          }
        else
          {
            guint32 rgba;
            
            pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                     TRUE,
                                     8, width, height);
            rgba = GDK_COLOR_RGBA (color);
            rgba &= ~0xff;
            rgba |= alpha;
            gdk_pixbuf_fill (pixbuf, rgba);
          }
      }
      break;

    case META_TEXTURE_GRADIENT:
      {
        GdkPixbuf *pixbuf;
        
        g_return_val_if_fail (spec->data.gradient.gradient_spec != NULL,
                              NULL);

        pixbuf = meta_gradient_spec_render (spec->data.gradient.gradient_spec,
                                            widget, width, height);

        pixbuf = multiply_alpha (pixbuf, alpha);
      }
      break;

    case META_TEXTURE_IMAGE:
      {
        GdkPixbuf *pixbuf;
        
        g_return_val_if_fail (spec->data.image.pixbuf != NULL,
                              NULL);

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
              }
            break;
          }

        pixbuf = multiply_alpha (pixbuf, alpha);
      }
      break;

    case META_TEXTURE_COMPOSITE:
      break;
    }

  return pixbuf;
}

static void
draw_color_rectangle (GtkWidget   *widget,
                      GdkDrawable *drawable,
                      GdkColor    *color,
                      const GdkRectangle *clip,
                      int          x,
                      int          y,
                      int          width,
                      int          height)
{
  GdkGC *gc;
  GdkGCValues values;  

  values.foreground = *color;
  gdk_rgb_find_color (widget->style->colormap, &values.foreground);  
  
  gc = gdk_gc_new_with_values (drawable, &values, GDK_GC_FOREGROUND);

  if (clip)
    gdk_gc_set_clip_rectangle (gc,
                               (GdkRectangle*) clip); /* const cast */
  
  gdk_draw_rectangle (drawable,
                      gc, TRUE, x, y, width, height);
  
  g_object_unref (G_OBJECT (gc));
}

static void
draw_bg_solid_composite (const MetaTextureSpec *bg,
                         const MetaTextureSpec *fg,
                         double                 alpha,
                         GtkWidget             *widget,
                         GdkDrawable           *drawable,
                         const GdkRectangle    *clip,
                         MetaTextureDrawMode    mode,
                         double                 xalign,
                         double                 yalign,
                         int                    x,
                         int                    y,
                         int                    width,
                         int                    height)
{
  GdkColor bg_color;
  
  g_assert (bg->type == META_TEXTURE_SOLID);
  g_assert (fg->type != META_TEXTURE_COMPOSITE);

  meta_color_spec_render (bg->data.solid.color_spec,
                          widget,
                          &bg_color);  
  
  switch (fg->type)
    {
    case META_TEXTURE_SOLID:
      {
        GdkColor fg_color;

        meta_color_spec_render (fg->data.solid.color_spec,
                                widget,
                                &fg_color);

        color_composite (&bg_color, &fg_color,
                         alpha, &fg_color);

        draw_color_rectangle (widget, drawable, &fg_color, clip,
                              x, y, width, height);
      }
      break;

    case META_TEXTURE_GRADIENT:
      /* FIXME I think we could just composite all the colors in
       * the gradient prior to generating the gradient?
       */
      /* FALL THRU */
    case META_TEXTURE_IMAGE:
      {
        GdkPixbuf *pixbuf;
        GdkPixbuf *composited;
        
        pixbuf = meta_texture_spec_render (fg, widget, mode, 255,
                                           width, height);

        if (pixbuf == NULL)
          return;
        
        composited = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                     gdk_pixbuf_get_has_alpha (pixbuf), 8,
                                     gdk_pixbuf_get_width (pixbuf),
                                     gdk_pixbuf_get_height (pixbuf));

        if (composited == NULL)
          {
            g_object_unref (G_OBJECT (pixbuf));
            return;
          }
        
        gdk_pixbuf_composite_color (pixbuf,
                                    composited,
                                    0, 0,
                                    gdk_pixbuf_get_width (pixbuf),
                                    gdk_pixbuf_get_height (pixbuf),
                                    0.0, 0.0, /* offsets */
                                    1.0, 1.0, /* scale */
                                    GDK_INTERP_BILINEAR,
                                    255 * alpha,
                                    0, 0,     /* check offsets */
                                    0,        /* check size */
                                    GDK_COLOR_RGB (bg_color),
                                    GDK_COLOR_RGB (bg_color));

        /* Need to draw background since pixbuf is not
         * necessarily covering the whole thing
         */
        draw_color_rectangle (widget, drawable, &bg_color, clip,
                              x, y, width, height);
        
        render_pixbuf_aligned (drawable, clip, composited,
                               xalign, yalign,
                               x, y, width, height);
        
        g_object_unref (G_OBJECT (pixbuf));
        g_object_unref (G_OBJECT (composited));
      }
      break;

    case META_TEXTURE_COMPOSITE:
      g_assert_not_reached ();
      break;
    }
}

static void
draw_bg_gradient_composite (const MetaTextureSpec *bg,
                            const MetaTextureSpec *fg,
                            double                 alpha,
                            GtkWidget             *widget,
                            GdkDrawable           *drawable,
                            const GdkRectangle    *clip,
                            MetaTextureDrawMode    mode,
                            double                 xalign,
                            double                 yalign,
                            int                    x,
                            int                    y,
                            int                    width,
                            int                    height)
{
  g_assert (bg->type == META_TEXTURE_GRADIENT);
  g_assert (fg->type != META_TEXTURE_COMPOSITE);
  
  switch (fg->type)
    {
    case META_TEXTURE_SOLID:
    case META_TEXTURE_GRADIENT:
    case META_TEXTURE_IMAGE:
      {
        GdkPixbuf *bg_pixbuf;
        GdkPixbuf *fg_pixbuf;
        GdkPixbuf *composited;
        int fg_width, fg_height;
        
        bg_pixbuf = meta_texture_spec_render (bg, widget, mode, 255,
                                              width, height);

        if (bg_pixbuf == NULL)
          return;

        fg_pixbuf = meta_texture_spec_render (fg, widget, mode, 255,
                                              width, height);

        if (fg_pixbuf == NULL)
          {
            g_object_unref (G_OBJECT (bg_pixbuf));            
            return;
          }

        /* gradients always fill the entire target area */
        g_assert (gdk_pixbuf_get_width (bg_pixbuf) == width);
        g_assert (gdk_pixbuf_get_height (bg_pixbuf) == height);
        
        composited = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                     gdk_pixbuf_get_has_alpha (bg_pixbuf), 8,
                                     gdk_pixbuf_get_width (bg_pixbuf),
                                     gdk_pixbuf_get_height (bg_pixbuf));

        if (composited == NULL)
          {
            g_object_unref (G_OBJECT (bg_pixbuf));
            g_object_unref (G_OBJECT (fg_pixbuf));
            return;
          }

        fg_width = gdk_pixbuf_get_width (fg_pixbuf);
        fg_height = gdk_pixbuf_get_height (fg_pixbuf);

        /* If we wanted to be all cool we could deal with the
         * offsets and try to composite only in the clip rectangle,
         * but I just don't care enough to figure it out.
         */
        
        gdk_pixbuf_composite (fg_pixbuf,
                              composited,
                              x + (width - fg_width) * xalign,
                              y + (height - fg_height) * yalign,
                              gdk_pixbuf_get_width (fg_pixbuf),
                              gdk_pixbuf_get_height (fg_pixbuf),
                              0.0, 0.0, /* offsets */
                              1.0, 1.0, /* scale */
                              GDK_INTERP_BILINEAR,
                              255 * alpha);
        
        render_pixbuf (drawable, clip, composited, x, y);
        
        g_object_unref (G_OBJECT (bg_pixbuf));
        g_object_unref (G_OBJECT (fg_pixbuf));
        g_object_unref (G_OBJECT (composited));
      }
      break;

    case META_TEXTURE_COMPOSITE:
      g_assert_not_reached ();
      break;
    }
}

static void
draw_bg_image_composite (const MetaTextureSpec *bg,
                         const MetaTextureSpec *fg,
                         double                 alpha,
                         GtkWidget             *widget,
                         GdkDrawable           *drawable,
                         const GdkRectangle    *clip,
                         MetaTextureDrawMode    mode,
                         double                 xalign,
                         double                 yalign,
                         int                    x,
                         int                    y,
                         int                    width,
                         int                    height)
{
  g_assert (bg->type == META_TEXTURE_IMAGE);
  g_assert (fg->type != META_TEXTURE_COMPOSITE);

  /* This one is tricky since the image doesn't cover the entire x,y
   * width, height rectangle, so we need to handle the fact that there
   * may be existing stuff in the uncovered portions of the drawable
   * that we need to composite over the top of.
   *
   * i.e. the "bg" we are compositing onto is equivalent to the image
   * composited over the top of whatever is already in the drawable.
   *
   * To implement this we just draw the background to drawable, then
   * render the foreground to a pixbuf, multiply its alpha channel by
   * the composite alpha, then composite the foreground onto the
   * drawable.
   */
  
  switch (fg->type)
    {
    case META_TEXTURE_SOLID:
    case META_TEXTURE_GRADIENT:
    case META_TEXTURE_IMAGE:
      {
        GdkPixbuf *bg_pixbuf, *fg_pixbuf;
        
        bg_pixbuf = meta_texture_spec_render (bg, widget, mode, 255,
                                              width, height);

        if (bg_pixbuf == NULL)
          return;

        /* fg_pixbuf has its alpha multiplied, note */
        fg_pixbuf = meta_texture_spec_render (fg, widget, mode,
                                              255 * alpha,
                                              width, height);

        if (fg_pixbuf == NULL)
          {
            g_object_unref (G_OBJECT (bg_pixbuf));            
            return;
          }

        render_pixbuf_aligned (drawable, clip, bg_pixbuf,
                               xalign, yalign,
                               x, y, width, height);

        render_pixbuf_aligned (drawable, clip, fg_pixbuf,
                               xalign, yalign,
                               x, y, width, height);
        
        g_object_unref (G_OBJECT (bg_pixbuf));
        g_object_unref (G_OBJECT (fg_pixbuf));
      }
      break;

    case META_TEXTURE_COMPOSITE:
      g_assert_not_reached ();
      break;
    }
}

void
meta_texture_spec_draw   (const MetaTextureSpec *spec,
                          GtkWidget             *widget,
                          GdkDrawable           *drawable,
                          const GdkRectangle    *clip,
                          MetaTextureDrawMode    mode,
                          double                 xalign,
                          double                 yalign,
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
        GdkColor color;

        g_return_if_fail (spec->data.solid.color_spec != NULL);
        
        meta_color_spec_render (spec->data.solid.color_spec,
                                widget, &color);

        draw_color_rectangle (widget, drawable, &color, clip,
                              x, y, width, height);
      }
      break;

    case META_TEXTURE_GRADIENT:
    case META_TEXTURE_IMAGE:
      {
        GdkPixbuf *pixbuf;
        
        g_return_if_fail (spec->data.gradient.gradient_spec != NULL);

        pixbuf = meta_texture_spec_render (spec, widget, mode, 255,
                                           width, height);

        if (pixbuf == NULL)
          return;
        
        render_pixbuf_aligned (drawable, clip, pixbuf,
                               xalign, yalign,
                               x, y, width, height);
        
        g_object_unref (G_OBJECT (pixbuf));
      }
      break;

    case META_TEXTURE_COMPOSITE:
      {
        MetaTextureSpec *fg;
        MetaTextureSpec *bg;
        
        /* We could just render both things to a pixbuf then squish them
         * but we are instead going to try to be all optimized for
         * certain cases.
         */

        fg = spec->data.composite.foreground;
        bg = spec->data.composite.background;

        g_return_if_fail (fg != NULL);
        g_return_if_fail (bg != NULL);
        g_return_if_fail (fg->type != META_TEXTURE_COMPOSITE);
        g_return_if_fail (bg->type != META_TEXTURE_COMPOSITE);
        
        switch (bg->type)
          {
          case META_TEXTURE_SOLID:
            draw_bg_solid_composite (bg, fg, spec->data.composite.alpha,
                                     widget, drawable, clip, mode,
                                     xalign, yalign,
                                     x, y, width, height);
            break;

          case META_TEXTURE_GRADIENT:
            draw_bg_gradient_composite (bg, fg, spec->data.composite.alpha,
                                        widget, drawable, clip, mode,
                                        xalign, yalign,
                                        x, y, width, height);
            break;
            
          case META_TEXTURE_IMAGE:
            draw_bg_image_composite (bg, fg, spec->data.composite.alpha,
                                     widget, drawable, clip, mode,
                                     xalign, yalign,
                                     x, y, width, height);
            break;

          case META_TEXTURE_COMPOSITE:
            g_assert_not_reached ();
            break;
          }
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
