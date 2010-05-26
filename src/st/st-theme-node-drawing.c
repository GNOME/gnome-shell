/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/* Drawing for StWidget.

   Copyright (C) 2009,2010 Red Hat, Inc.

   Contains code derived from:
   rectangle.c: Rounded rectangle.
   Copyright (C) 2008 litl, LLC.
   st-shadow-texture.c: a class for creating soft shadow texture
   Copyright (C) 2009 Florian Müllner <fmuellner@src.gnome.org>
   st-texture-frame.h: Expandible texture actor
   Copyright 2007 OpenedHand
   Copyright 2009 Intel Corporation.

   The St is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The St is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the St; see the file COPYING.LIB.
   If not, write to the Free Software Foundation, Inc., 59 Temple Place -
   Suite 330, Boston, MA 02111-1307, USA.
*/

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "st-shadow.h"
#include "st-theme-private.h"
#include "st-theme-context.h"
#include "st-texture-cache.h"
#include "st-theme-node-private.h"

/*****
 * Shadows
 *****/

static gdouble *
calculate_gaussian_kernel (gdouble   sigma,
                           guint     n_values)
{
  gdouble *ret, sum;
  gdouble exp_divisor;
  gint half, i;

  g_return_val_if_fail ((int) sigma > 0, NULL);

  half = n_values / 2;

  ret = g_malloc (n_values * sizeof (gdouble));
  sum = 0.0;

  exp_divisor = 2 * sigma * sigma;

  /* n_values of 1D Gauss function */
  for (i = 0; i < n_values; i++)
    {
      ret[i] = exp (-(i - half) * (i - half) / exp_divisor);
      sum += ret[i];
    }

  /* normalize */
  for (i = 0; i < n_values; i++)
    ret[i] /= sum;

  return ret;
}

static CoglHandle
create_shadow_material (StThemeNode  *node,
                        CoglHandle    src_texture)
{
  CoglHandle  material;
  CoglHandle  texture;
  StShadow   *shadow_spec;
  guchar     *pixels_in, *pixels_out;
  gint        width_in, height_in, rowstride_in;
  gint        width_out, height_out, rowstride_out;
  float       sigma;

  shadow_spec = st_theme_node_get_shadow (node);
  if (!shadow_spec)
    return COGL_INVALID_HANDLE;

  /* we use an approximation of the sigma - blur radius relationship used
     in Firefox for doing SVG blurs; see
     http://mxr.mozilla.org/mozilla-central/source/gfx/thebes/src/gfxBlur.cpp#280
  */
  sigma = shadow_spec->blur / 1.9;

  width_in  = cogl_texture_get_width  (src_texture);
  height_in = cogl_texture_get_height (src_texture);
  rowstride_in = (width_in + 3) & ~3;

  pixels_in  = g_malloc0 (rowstride_in * height_in);

  cogl_texture_get_data (src_texture, COGL_PIXEL_FORMAT_A_8,
                         rowstride_in, pixels_in);

  if ((guint) shadow_spec->blur == 0)
    {
      width_out  = width_in;
      height_out = height_in;
      rowstride_out = rowstride_in;
      pixels_out = g_memdup (pixels_in, rowstride_out * height_out);
    }
  else
    {
      gdouble *kernel;
      guchar  *line;
      gint     n_values, half;
      gint     x_in, y_in, x_out, y_out, i;

      n_values = (gint) 5 * sigma;
      half = n_values / 2;

      width_out  = width_in  + 2 * half;
      height_out = height_in + 2 * half;
      rowstride_out = (width_out + 3) & ~3;

      pixels_out = g_malloc0 (rowstride_out * height_out);
      line       = g_malloc0 (rowstride_out);

      kernel = calculate_gaussian_kernel (sigma, n_values);

      /* vertical blur */
      for (x_in = 0; x_in < width_in; x_in++)
        for (y_out = 0; y_out < height_out; y_out++)
          {
            guchar *pixel_in, *pixel_out;
            gint i0, i1;

            y_in = y_out - half;

            /* We read from the source at 'y = y_in + i - half'; clamp the
             * full i range [0, n_values) so that y is in [0, height_in).
             */
            i0 = MAX (half - y_in, 0);
            i1 = MIN (height_in + half - y_in, n_values);

            pixel_in  =  pixels_in + (y_in + i0 - half) * rowstride_in + x_in;
            pixel_out =  pixels_out + y_out * rowstride_out + (x_in + half);

            for (i = i0; i < i1; i++)
              {
                *pixel_out += *pixel_in * kernel[i];
                pixel_in += rowstride_in;
              }
          }

      /* horizontal blur */
      for (y_out = 0; y_out < height_out; y_out++)
        {
          memcpy (line, pixels_out + y_out * rowstride_out, rowstride_out);

          for (x_out = 0; x_out < width_out; x_out++)
            {
              gint i0, i1;
              guchar *pixel_out, *pixel_in;

              /* We read from the source at 'x = x_out + i - half'; clamp the
               * full i range [0, n_values) so that x is in [0, width_out).
               */
              i0 = MAX (half - x_out, 0);
              i1 = MIN (width_out + half - x_out, n_values);

              pixel_in  = line + x_out + i0 - half;
              pixel_out = pixels_out + rowstride_out * y_out + x_out;

              *pixel_out = 0;
              for (i = i0; i < i1; i++)
                {
                  *pixel_out += *pixel_in * kernel[i];
                  pixel_in++;
                }
            }
        }
      g_free (kernel);
      g_free (line);
    }

  texture = cogl_texture_new_from_data (width_out,
                                        height_out,
                                        COGL_TEXTURE_NONE,
                                        COGL_PIXEL_FORMAT_A_8,
                                        COGL_PIXEL_FORMAT_A_8,
                                        rowstride_out,
                                        pixels_out);

  g_free (pixels_in);
  g_free (pixels_out);

  material = cogl_material_new ();

  cogl_material_set_layer (material, 0, texture);

  /* We set up the material to blend the shadow texture with the combine
   * constant, but defer setting the latter until painting, so that we can
   * take the actor's overall opacity into account. */
  cogl_material_set_layer_combine (material, 0,
                                   "RGBA = MODULATE (CONSTANT, TEXTURE[A])",
                                   NULL);


  cogl_handle_unref (texture);

  return material;
}


/****
 * Rounded corners
 ****/

typedef struct {
  ClutterColor   color;
  ClutterColor   border_color_1;
  ClutterColor   border_color_2;
  guint          radius;
  guint          border_width_1;
  guint          border_width_2;
} StCornerSpec;

static CoglHandle
create_corner_texture (StCornerSpec *corner)
{
  CoglHandle texture;
  cairo_t *cr;
  cairo_surface_t *surface;
  guint rowstride;
  guint8 *data;
  guint size;
  guint max_border_width;

  max_border_width = MAX(corner->border_width_2, corner->border_width_1);
  size = 2 * MAX(max_border_width, corner->radius);
  rowstride = size * 4;
  data = g_new0 (guint8, size * rowstride);

  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 size, size,
                                                 rowstride);
  cr = cairo_create (surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_scale (cr, size, size);

  /* TODO support nonuniform border widths */

  if (corner->border_width_1 < corner->radius)
    {
      double internal_radius = 0.5 * (1.0 - (double) corner->border_width_1 / corner->radius);

      if (corner->border_width_1 != 0)
        {
          cairo_set_source_rgba (cr,
                                 corner->border_color_1.red / 255.,
                                 corner->border_color_1.green / 255.,
                                 corner->border_color_1.blue / 255.,
                                 corner->border_color_1.alpha / 255.);

          cairo_arc (cr, 0.5, 0.5, 0.5, 0, 2 * M_PI);
          cairo_fill (cr);
        }

      cairo_set_source_rgba (cr,
                             corner->color.red / 255.,
                             corner->color.green / 255.,
                             corner->color.blue / 255.,
                             corner->color.alpha / 255.);
      cairo_arc (cr, 0.5, 0.5, internal_radius, 0, 2 * M_PI);
      cairo_fill (cr);
    }
  else
    {
      double radius;

      radius = (gdouble)corner->radius / corner->border_width_1;

      cairo_set_source_rgba (cr,
                             corner->border_color_1.red / 255.,
                             corner->border_color_1.green / 255.,
                             corner->border_color_1.blue / 255.,
                             corner->border_color_1.alpha / 255.);

      cairo_arc (cr, radius, radius, radius, M_PI, 3 * M_PI / 2);
      cairo_line_to (cr, 1.0 - radius, 0.0);
      cairo_arc (cr, 1.0 - radius, radius, radius, 3 * M_PI / 2, 2 * M_PI);
      cairo_line_to (cr, 1.0, 1.0 - radius);
      cairo_arc (cr, 1.0 - radius, 1.0 - radius, radius, 0, M_PI / 2);
      cairo_line_to (cr, radius, 1.0);
      cairo_arc (cr, radius, 1.0 - radius, radius, M_PI / 2, M_PI);
      cairo_fill (cr);
    }
  cairo_destroy (cr);

  cairo_surface_destroy (surface);

  texture = cogl_texture_new_from_data (size, size,
                                        COGL_TEXTURE_NONE,
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                                        COGL_PIXEL_FORMAT_BGRA_8888_PRE,
#else
                                        COGL_PIXEL_FORMAT_ARGB_8888_PRE,
#endif
                                        COGL_PIXEL_FORMAT_ANY,
                                        rowstride,
                                        data);
  g_free (data);
  g_assert (texture != COGL_INVALID_HANDLE);

  return texture;
}

static char *
corner_to_string (StCornerSpec *corner)
{
  return g_strdup_printf ("st-theme-node-corner:%02x%02x%02x%02x,%02x%02x%02x%02x,%02x%02x%02x%02x,%u,%u,%u",
                          corner->color.red, corner->color.blue, corner->color.green, corner->color.alpha,
                          corner->border_color_1.red, corner->border_color_1.green, corner->border_color_1.blue, corner->border_color_1.alpha,
                          corner->border_color_2.red, corner->border_color_2.green, corner->border_color_2.blue, corner->border_color_2.alpha,
                          corner->radius,
                          corner->border_width_1,
                          corner->border_width_2);
}

typedef struct {
  StThemeNode *node;
  StCornerSpec *corner;
} LoadCornerData;

static CoglHandle
load_corner (StTextureCache  *cache,
             const char      *key,
             void            *datap,
             GError         **error)
{
  LoadCornerData *data = datap;

  return create_corner_texture (data->corner);
}

/* To match the CSS specification, we want the border to look like it was
 * drawn over the background. But actually drawing the border over the
 * background will produce slightly bad antialiasing at the edges, so
 * compute the effective border color instead.
 */
#define NORM(x) (t = (x) + 127, (t + (t >> 8)) >> 8)
#define MULT(c,a) NORM(c*a)

static void
premultiply (ClutterColor *color)
{
  guint t;
  color->red = MULT (color->red, color->alpha);
  color->green = MULT (color->green, color->alpha);
  color->blue = MULT (color->blue, color->alpha);
}

static void
unpremultiply (ClutterColor *color)
{
  if (color->alpha != 0)
    {
      color->red = (color->red * 255 + 127) / color->alpha;
      color->green = (color->green * 255 + 127) / color->alpha;
      color->blue = (color->blue * 255 + 127) / color->alpha;
    }
}

static void
over (const ClutterColor *source,
      const ClutterColor *destination,
      ClutterColor       *result)
{
  guint t;
  ClutterColor src = *source;
  ClutterColor dst = *destination;

  premultiply (&src);
  premultiply (&dst);

  result->alpha = src.alpha + NORM ((255 - src.alpha) * dst.alpha);
  result->red   = src.red +   NORM ((255 - src.alpha) * dst.red);
  result->green = src.green + NORM ((255 - src.alpha) * dst.green);
  result->blue  = src.blue +  NORM ((255 - src.alpha) * dst.blue);

  unpremultiply (result);
}

static CoglHandle
st_theme_node_lookup_corner (StThemeNode    *node,
                             StCorner        corner_id)
{
  CoglHandle texture;
  char *key;
  StTextureCache *cache;
  StCornerSpec corner;
  LoadCornerData data;

  if (node->border_radius[corner_id] == 0)
    return COGL_INVALID_HANDLE;

  cache = st_texture_cache_get_default ();

  corner.radius = node->border_radius[corner_id];
  corner.color = node->background_color;

  switch (corner_id)
    {
      case ST_CORNER_TOPLEFT:
        corner.border_width_1 = node->border_width[ST_SIDE_TOP];
        corner.border_width_2 = node->border_width[ST_SIDE_LEFT];
        over (&node->border_color[ST_SIDE_TOP], &corner.color, &corner.border_color_1);
        over (&node->border_color[ST_SIDE_LEFT], &corner.color, &corner.border_color_2);
        break;
      case ST_CORNER_TOPRIGHT:
        corner.border_width_1 = node->border_width[ST_SIDE_TOP];
        corner.border_width_2 = node->border_width[ST_SIDE_RIGHT];
        over (&node->border_color[ST_SIDE_TOP], &corner.color, &corner.border_color_1);
        over (&node->border_color[ST_SIDE_RIGHT], &corner.color, &corner.border_color_2);
        break;
      case ST_CORNER_BOTTOMRIGHT:
        corner.border_width_1 = node->border_width[ST_SIDE_BOTTOM];
        corner.border_width_2 = node->border_width[ST_SIDE_RIGHT];
        over (&node->border_color[ST_SIDE_BOTTOM], &corner.color, &corner.border_color_1);
        over (&node->border_color[ST_SIDE_RIGHT], &corner.color, &corner.border_color_2);
        break;
      case ST_CORNER_BOTTOMLEFT:
        corner.border_width_1 = node->border_width[ST_SIDE_BOTTOM];
        corner.border_width_2 = node->border_width[ST_SIDE_LEFT];
        over (&node->border_color[ST_SIDE_BOTTOM], &corner.color, &corner.border_color_1);
        over (&node->border_color[ST_SIDE_LEFT], &corner.color, &corner.border_color_2);
        break;
    }

  key = corner_to_string (&corner);

  data.node = node;
  data.corner = &corner;
  texture = st_texture_cache_load (cache, key, ST_TEXTURE_CACHE_POLICY_NONE, load_corner, &data, NULL);

  g_free (key);

  return texture;
}

static void
get_background_position (StThemeNode             *self,
                         const ClutterActorBox   *allocation,
                         ClutterActorBox         *result)
{
  gfloat w, h;

  result->x1 = result->y1 = 0;
  result->x2 = allocation->x2 - allocation->x1;
  result->y2 = allocation->y2 - allocation->y1;

  w = cogl_texture_get_width (self->background_texture);
  h = cogl_texture_get_height (self->background_texture);

  /* scale the background into the allocated bounds */
  if (w > result->x2 || h > result->y2)
    {
      gint new_h, new_w, offset;
      gint box_w, box_h;

      box_w = (int) result->x2;
      box_h = (int) result->y2;

      /* scale to fit */
      new_h = (int)((h / w) * ((gfloat) box_w));
      new_w = (int)((w / h) * ((gfloat) box_h));

      if (new_h > box_h)
        {
          /* center for new width */
          offset = ((box_w) - new_w) * 0.5;
          result->x1 = offset;
          result->x2 = offset + new_w;

          result->y2 = box_h;
        }
      else
        {
          /* center for new height */
          offset = ((box_h) - new_h) * 0.5;
          result->y1 = offset;
          result->y2 = offset + new_h;

          result->x2 = box_w;
        }
    }
  else
    {
      /* center the background on the widget */
      result->x1 = (int)(((allocation->x2 - allocation->x1) / 2) - (w / 2));
      result->y1 = (int)(((allocation->y2 - allocation->y1) / 2) - (h / 2));
      result->x2 = result->x1 + w;
      result->y2 = result->y1 + h;
    }
}

/* Use of this function marks code which doesn't support
 * non-uniform widths and/or colors.
 */
static gboolean
get_arbitrary_border (StThemeNode   *node,
                      int           *width,
                      ClutterColor  *color)
{
  int w;

  w = st_theme_node_get_border_width (node, ST_SIDE_TOP);
  if (w > 0)
    {
      if (width)
        *width = w;
      if (color)
        st_theme_node_get_border_color (node, ST_SIDE_TOP, color);
      return TRUE;
    }

  if (width)
    *width = 0;
  return FALSE;
}

static CoglHandle
st_theme_node_render_gradient (StThemeNode *node)
{
  CoglHandle texture;
  int radius[4], i;
  cairo_t *cr;
  cairo_surface_t *surface;
  cairo_pattern_t *pattern;
  gboolean round_border = FALSE;
  ClutterColor border_color;
  int border_width;
  guint rowstride;
  guchar *data;

  rowstride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, node->alloc_width);
  data = g_new0 (guchar, node->alloc_height * rowstride);
  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 node->alloc_width,
                                                 node->alloc_height,
                                                 rowstride);
  cr = cairo_create (surface);

  /* TODO - support non-uniform border colors and widths */
  get_arbitrary_border (node, &border_width, &border_color);

  for (i = 0; i < 4; i++)
    {
      radius[i] = st_theme_node_get_border_radius (node, i);
      if (radius[i] > 0)
        round_border = TRUE;
    }

  if (node->background_gradient_type == ST_GRADIENT_VERTICAL)
    pattern = cairo_pattern_create_linear (0, 0, 0, node->alloc_height);
  else if (node->background_gradient_type == ST_GRADIENT_HORIZONTAL)
    pattern = cairo_pattern_create_linear (0, 0, node->alloc_width, 0);
  else
    {
      gdouble cx, cy;

      cx = node->alloc_width / 2.;
      cy = node->alloc_height / 2.;
      pattern = cairo_pattern_create_radial (cx, cy, 0, cx, cy, MIN (cx, cy));
    }

  cairo_pattern_add_color_stop_rgba (pattern, 0,
                                     node->background_color.red / 255.,
                                     node->background_color.green / 255.,
                                     node->background_color.blue / 255.,
                                     node->background_color.alpha / 255.);
  cairo_pattern_add_color_stop_rgba (pattern, 1,
                                     node->background_gradient_end.red / 255.,
                                     node->background_gradient_end.green / 255.,
                                     node->background_gradient_end.blue / 255.,
                                     node->background_gradient_end.alpha / 255.);

  if (round_border)
    {
      if (radius[ST_CORNER_TOPLEFT] > 0)
        cairo_arc (cr,
                   radius[ST_CORNER_TOPLEFT],
                   radius[ST_CORNER_TOPLEFT],
                   radius[ST_CORNER_TOPLEFT], M_PI, 3 * M_PI / 2);
      else
        cairo_move_to (cr, 0, 0);
      cairo_line_to (cr, node->alloc_width - radius[ST_CORNER_TOPRIGHT], 0);
      if (radius[ST_CORNER_TOPRIGHT] > 0)
        cairo_arc (cr,
                   node->alloc_width - radius[ST_CORNER_TOPRIGHT],
                   radius[ST_CORNER_TOPRIGHT],
                   radius[ST_CORNER_TOPRIGHT], 3 * M_PI / 2, 2 * M_PI);
      cairo_line_to (cr, node->alloc_width, node->alloc_height - radius[ST_CORNER_BOTTOMRIGHT]);
      if (radius[ST_CORNER_BOTTOMRIGHT])
        cairo_arc (cr,
                   node->alloc_width - radius[ST_CORNER_BOTTOMRIGHT],
                   node->alloc_height - radius[ST_CORNER_BOTTOMRIGHT],
                   radius[ST_CORNER_BOTTOMRIGHT], 0, M_PI / 2);
      cairo_line_to (cr, radius[ST_CORNER_BOTTOMLEFT], node->alloc_height);
      if (radius[ST_CORNER_BOTTOMLEFT])
        cairo_arc (cr,
                   radius[ST_CORNER_BOTTOMLEFT],
                   node->alloc_height - radius[ST_CORNER_BOTTOMLEFT],
                   radius[ST_CORNER_BOTTOMLEFT], M_PI / 2, M_PI);
      cairo_close_path (cr);
    }
  else
    cairo_rectangle (cr, 0, 0, node->alloc_width, node->alloc_height);

  if (border_width > 0)
    {
      cairo_path_t *path;

      path = cairo_copy_path (cr);
      cairo_set_source_rgba (cr,
                             border_color.red / 255.,
                             border_color.green / 255.,
                             border_color.blue / 255.,
                             border_color.alpha / 255.);
      cairo_fill (cr);

      cairo_translate (cr, border_width, border_width);
      cairo_scale (cr,
                   (gdouble)(node->alloc_width - 2 * border_width) / node->alloc_width,
                   (gdouble)(node->alloc_height - 2 * border_width) / node->alloc_height);
      cairo_append_path (cr, path);
      cairo_path_destroy (path);
    }

  cairo_set_source (cr, pattern);
  cairo_fill (cr);

  cairo_pattern_destroy (pattern);

  texture = cogl_texture_new_from_data (node->alloc_width, node->alloc_height,
                                        COGL_TEXTURE_NONE,
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                                        COGL_PIXEL_FORMAT_BGRA_8888_PRE,
#elif G_BYTE_ORDER == G_BIG_ENDIAN
                                        COGL_PIXEL_FORMAT_ARGB_8888_PRE,
#else
                                        COGL_PIXEL_FORMAT_ANY,
#error unknown endianness type
#endif
                                        COGL_PIXEL_FORMAT_ANY,
                                        rowstride,
                                        data);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);
  g_free (data);

  return texture;
}

void
_st_theme_node_free_drawing_state (StThemeNode  *node)
{
  if (node->background_texture != COGL_INVALID_HANDLE)
    cogl_handle_unref (node->background_texture);
  if (node->background_shadow_material != COGL_INVALID_HANDLE)
    cogl_handle_unref (node->background_shadow_material);
  if (node->border_texture != COGL_INVALID_HANDLE)
    cogl_handle_unref (node->border_texture);
  if (node->border_shadow_material != COGL_INVALID_HANDLE)
    cogl_handle_unref (node->border_shadow_material);

  _st_theme_node_init_drawing_state (node);
}

void
_st_theme_node_init_drawing_state (StThemeNode *node)
{
  node->background_texture = COGL_INVALID_HANDLE;
  node->background_shadow_material = COGL_INVALID_HANDLE;
  node->border_shadow_material = COGL_INVALID_HANDLE;
  node->border_texture = COGL_INVALID_HANDLE;
}

static void st_theme_node_paint_borders (StThemeNode           *node,
                                         const ClutterActorBox *box,
                                         guint8                 paint_opacity);

static void
st_theme_node_render_resources (StThemeNode   *node,
                                float          width,
                                float          height)
{
  StTextureCache *texture_cache;
  StBorderImage *border_image;
  StShadow *shadow_spec;
  const char *background_image;

  texture_cache = st_texture_cache_get_default ();

  /* FIXME - need to separate this into things that need to be recomputed on
   * geometry change versus things that can be cached regardless, such as
   * a background image.
   */
  _st_theme_node_free_drawing_state (node);

  node->alloc_width = width;
  node->alloc_height = height;

  _st_theme_node_ensure_background (node);
  _st_theme_node_ensure_geometry (node);

  shadow_spec = st_theme_node_get_shadow (node);

  /* Load referenced images from disk and draw anything we need with cairo now */

  border_image = st_theme_node_get_border_image (node);
  if (border_image)
    {
      const char *filename;

      filename = st_border_image_get_filename (border_image);

      node->border_texture = st_texture_cache_load_file_to_cogl_texture (texture_cache, filename);
    }
  else if (node->background_gradient_type != ST_GRADIENT_NONE)
    {
      node->border_texture = st_theme_node_render_gradient (node);
    }

  if (shadow_spec)
    {
      if (node->border_texture != COGL_INVALID_HANDLE)
        node->border_shadow_material = create_shadow_material (node, node->border_texture);
      else if (node->background_color.alpha > 0 ||
               node->border_width[ST_SIDE_TOP] > 0 ||
               node->border_width[ST_SIDE_LEFT] > 0 ||
               node->border_width[ST_SIDE_RIGHT] > 0 ||
               node->border_width[ST_SIDE_BOTTOM] > 0)
        {
          CoglHandle buffer, offscreen;

          buffer = cogl_texture_new_with_size (width,
                                               height,
                                               COGL_TEXTURE_NO_SLICING,
                                               COGL_PIXEL_FORMAT_ANY);
          offscreen = cogl_offscreen_new_to_texture (buffer);

          if (offscreen != COGL_INVALID_HANDLE)
            {
              ClutterActorBox box = { 0, 0, width, height };

              cogl_push_framebuffer (offscreen);
              cogl_ortho (0, width, height, 0, 0, 1.0);
              st_theme_node_paint_borders (node, &box, 0xFF);
              cogl_pop_framebuffer ();
              cogl_handle_unref (offscreen);

              node->border_shadow_material = create_shadow_material (node,
                                                                     buffer);
            }
          cogl_handle_unref (buffer);
        }
    }

  background_image = st_theme_node_get_background_image (node);
  if (background_image != NULL)
    {

      node->background_texture = st_texture_cache_load_file_to_cogl_texture (texture_cache, background_image);

      if (shadow_spec)
        {
          node->background_shadow_material = create_shadow_material (node, node->background_texture);
        }
    }

  node->corner_texture = st_theme_node_lookup_corner (node, ST_CORNER_TOPLEFT);
}

static void
paint_texture_with_opacity (CoglHandle       texture,
                            ClutterActorBox *box,
                            guint8           paint_opacity)
{
  if (paint_opacity == 255)
    {
      /* Minor: optimization use the default material if we can */
      cogl_set_source_texture (texture);
      cogl_rectangle (box->x1, box->y1, box->x2, box->y2);
      return;
    }

  CoglHandle material;

  material = cogl_material_new ();
  cogl_material_set_layer (material, 0, texture);
  cogl_material_set_color4ub (material,
                              paint_opacity, paint_opacity, paint_opacity, paint_opacity);

  cogl_set_source (material);
  cogl_rectangle (box->x1, box->y1, box->x2, box->y2);

  cogl_handle_unref (material);
}

static void
paint_shadow_with_opacity (CoglHandle       shadow_material,
                           StShadow        *shadow_spec,
                           ClutterActorBox *box,
                           guint8           paint_opacity)
{
  ClutterActorBox shadow_box;
  CoglColor       color;

  st_shadow_get_box (shadow_spec, box, &shadow_box);

  cogl_color_set_from_4ub (&color,
                           shadow_spec->color.red   * paint_opacity / 255,
                           shadow_spec->color.green * paint_opacity / 255,
                           shadow_spec->color.blue  * paint_opacity / 255,
                           shadow_spec->color.alpha * paint_opacity / 255);
  cogl_color_premultiply (&color);

  cogl_material_set_layer_combine_constant (shadow_material, 0, &color);

  cogl_set_source (shadow_material);
  cogl_rectangle_with_texture_coords (shadow_box.x1, shadow_box.y1,
                                      shadow_box.x2, shadow_box.y2,
                                      0, 0, 1, 1);
}

static void
st_theme_node_paint_borders (StThemeNode           *node,
                             const ClutterActorBox *box,
                             guint8                 paint_opacity)

{
  float width, height;
  int border_width;
  int border_radius;
  int max_width_radius;
  ClutterColor border_color;
  CoglHandle material;

  width = box->x2 - box->x1;
  height = box->y2 - box->y1;

  get_arbitrary_border (node, &border_width, &border_color);
  border_radius = node->border_radius[ST_CORNER_TOPLEFT];

  max_width_radius = MAX(border_width, border_radius);

  /* borders */
  if (border_width > 0)
    {
      ClutterColor effective_border;

      over (&border_color, &node->background_color, &effective_border);

      cogl_set_source_color4ub (effective_border.red,
                                effective_border.green,
                                effective_border.blue,
                                paint_opacity * effective_border.alpha / 255);

      if (border_radius > 0) /* skip corners */
        {
          /* NORTH */
          cogl_rectangle (max_width_radius, 0,
                          width - max_width_radius, border_width);

          /* EAST */
          cogl_rectangle (width - border_width, max_width_radius,
                          width, height - max_width_radius);

          /* SOUTH */
          cogl_rectangle (max_width_radius, height - border_width,
                          width - max_width_radius, height);

          /* WEST */
          cogl_rectangle (0, max_width_radius,
                          border_width, height - max_width_radius);
        }
      else /* include corners */
        {
          /* NORTH */
          cogl_rectangle (0, 0,
                          width, border_width);

          /* EAST */
          cogl_rectangle (width - border_width, border_width,
                          width, height - border_width);

          /* SOUTH */
          cogl_rectangle (0, height - border_width,
                          width, height);

          /* WEST */
          cogl_rectangle (0, border_width,
                          border_width, height - border_width);
        }
    }

  /* corners */
  if (node->corner_texture != COGL_INVALID_HANDLE)
    {
      material = cogl_material_new ();
      cogl_material_set_layer (material, 0, node->corner_texture);
      cogl_material_set_color4ub (material,
                                  paint_opacity, paint_opacity, paint_opacity, paint_opacity);

      cogl_set_source (material);

      cogl_rectangle_with_texture_coords (0, 0, max_width_radius, max_width_radius, 0, 0, 0.5, 0.5);
      cogl_rectangle_with_texture_coords (width - max_width_radius, 0, width, max_width_radius, 0.5, 0, 1, 0.5);
      cogl_rectangle_with_texture_coords (width - max_width_radius, height - max_width_radius, width, height, 0.5, 0.5, 1, 1);
      cogl_rectangle_with_texture_coords (0, height - max_width_radius, max_width_radius, height, 0, 0.5, 0.5, 1);

      cogl_handle_unref (material);
    }

  /* background color */
  cogl_set_source_color4ub (node->background_color.red,
                            node->background_color.green,
                            node->background_color.blue,
                            paint_opacity * node->background_color.alpha / 255);

  if (border_radius > border_width)
    {
      /* Once we've drawn the borders and corners, if the corners are bigger
       * the the border width, the remaining area is shaped like
       *
       *  ########
       * ##########
       * ##########
       *  ########
       *
       * We draw it in 3 pieces - first the top and bottom, then the main
       * rectangle
       */
      cogl_rectangle (border_radius, border_width,
                      width - border_radius, border_radius);
      cogl_rectangle (border_radius, height - border_radius,
                      width - border_radius, height - border_width);
    }

  cogl_rectangle (border_width, max_width_radius,
                  width - border_width, height - max_width_radius);
}

static void
st_theme_node_paint_sliced_border_image (StThemeNode           *node,
                                         const ClutterActorBox *box,
                                         guint8                 paint_opacity)
{
  gfloat ex, ey;
  gfloat tx1, ty1, tx2, ty2;
  gint border_left, border_right, border_top, border_bottom;
  float img_width, img_height;
  StBorderImage *border_image;
  CoglHandle material;

  border_image = st_theme_node_get_border_image (node);
  g_assert (border_image != NULL);

  st_border_image_get_borders (border_image,
                               &border_left, &border_right, &border_top, &border_bottom);

  img_width = cogl_texture_get_width (node->border_texture);
  img_height = cogl_texture_get_height (node->border_texture);

  tx1 = border_left / img_width;
  tx2 = (img_width - border_right) / img_width;
  ty1 = border_top / img_height;
  ty2 = (img_height - border_bottom) / img_height;

  ex = node->alloc_width - border_right;
  if (ex < 0)
    ex = border_right;           /* FIXME ? */

  ey = node->alloc_height - border_bottom;
  if (ey < 0)
    ey = border_bottom;          /* FIXME ? */

  material = cogl_material_new ();
  cogl_material_set_layer (material, 0, node->border_texture);
  cogl_material_set_color4ub (material,
                              paint_opacity, paint_opacity, paint_opacity, paint_opacity);

  cogl_set_source (material);

  {
    GLfloat rectangles[] =
    {
      /* top left corner */
      0, 0, border_left, border_top,
      0.0, 0.0,
      tx1, ty1,

      /* top middle */
      border_left, 0, ex, border_top,
      tx1, 0.0,
      tx2, ty1,

      /* top right */
      ex, 0, node->alloc_width, border_top,
      tx2, 0.0,
      1.0, ty1,

      /* mid left */
      0, border_top, border_left, ey,
      0.0, ty1,
      tx1, ty2,

      /* center */
      border_left, border_top, ex, ey,
      tx1, ty1,
      tx2, ty2,

      /* mid right */
      ex, border_top, node->alloc_width, ey,
      tx2, ty1,
      1.0, ty2,

      /* bottom left */
      0, ey, border_left, node->alloc_height,
      0.0, ty2,
      tx1, 1.0,

      /* bottom center */
      border_left, ey, ex, node->alloc_height,
      tx1, ty2,
      tx2, 1.0,

      /* bottom right */
      ex, ey, node->alloc_width, node->alloc_height,
      tx2, ty2,
      1.0, 1.0
    };

    cogl_rectangles_with_texture_coords (rectangles, 9);
  }

  cogl_handle_unref (material);
}

static void
st_theme_node_paint_outline (StThemeNode           *node,
                             const ClutterActorBox *box,
                             guint8                 paint_opacity)

{
  float width, height;
  int outline_width;
  ClutterColor outline_color, effective_outline;

  width = box->x2 - box->x1;
  height = box->y2 - box->y1;

  outline_width = st_theme_node_get_outline_width (node);
  if (outline_width == 0)
    return;

  st_theme_node_get_outline_color (node, &outline_color);
  over (&outline_color, &node->background_color, &effective_outline);

  cogl_set_source_color4ub (effective_outline.red,
                            effective_outline.green,
                            effective_outline.blue,
                            paint_opacity * effective_outline.alpha / 255);

  /* The outline is drawn just outside the border, which means just
   * outside the allocation box. This means that in some situations
   * involving clip_to_allocation or the screen edges, you won't be
   * able to see the outline. In practice, it works well enough.
   */

  /* NORTH */
  cogl_rectangle (-outline_width, -outline_width,
                  width + outline_width, 0);

  /* EAST */
  cogl_rectangle (width, 0,
                  width + outline_width, height);

  /* SOUTH */
  cogl_rectangle (-outline_width, height,
                  width + outline_width, height + outline_width);

  /* WEST */
  cogl_rectangle (-outline_width, 0,
                  0, height);
}

void
st_theme_node_paint (StThemeNode           *node,
                     const ClutterActorBox *box,
                     guint8                 paint_opacity)
{
  float width, height;
  ClutterActorBox allocation;

  /* Some things take an ActorBox, some things just width/height */
  width = box->x2 - box->x1;
  height = box->y2 - box->y1;
  allocation.x1 = allocation.y1 = 0;
  allocation.x2 = width;
  allocation.y2 = height;

  if (node->alloc_width != width || node->alloc_height != height)
    st_theme_node_render_resources (node, width, height);

  /* Rough notes about the relationship of borders and backgrounds in CSS3;
   * see http://www.w3.org/TR/css3-background/ for more accurate details.
   *
   * - Things are drawn in 4 layers, from the bottom:
   *     Background color
   *     Background image
   *     Border color or border image
   *     Content
   * - The background color, gradient and image extend to and are clipped by
   *   the edge of the border area, so will be rounded if the border is
   *   rounded. (CSS3 background-clip property modifies this)
   * - The border image replaces what would normally be drawn by the border
   * - The border image is not clipped by a rounded border-radius
   * - The border radius rounds the background even if the border is
   *   zero width or a border image is being used.
   *
   * Deviations from the above as implemented here:
   *  - Nonuniform border widths combined with a non-zero border radius result
   *    in the border radius being ignored
   *  - The combination of border image and a non-zero border radius is
   *    not supported; the background color will be drawn with square
   *    corners.
   *  - The combination of border image and a background gradient is not
   *    supported; the background will be drawn as a solid color
   *  - The background image is drawn above the border color or image,
   *    not below it.
   *  - We don't clip the background image to the (rounded) border area.
   *
   * The first three allow us to always draw with no more than a single
   * border_image and a single background image above it.
   */

  if (node->border_shadow_material)
    paint_shadow_with_opacity (node->border_shadow_material,
                               node->shadow,
                               &allocation,
                               paint_opacity);

  if (node->border_texture != COGL_INVALID_HANDLE)
    {
      /* Gradients and border images are mutually exclusive at this time */
      if (node->background_gradient_type != ST_GRADIENT_NONE)
        paint_texture_with_opacity (node->border_texture, &allocation, paint_opacity);
      else
        st_theme_node_paint_sliced_border_image (node, &allocation, paint_opacity);
    }
  else
    st_theme_node_paint_borders (node, box, paint_opacity);

  st_theme_node_paint_outline (node, box, paint_opacity);

  if (node->background_texture != COGL_INVALID_HANDLE)
    {
      ClutterActorBox background_box;

      get_background_position (node, &allocation, &background_box);

      /* CSS based drop shadows
       *
       * Drop shadows in ST are modelled after the CSS3 box-shadow property;
       * see http://www.css3.info/preview/box-shadow/ for a detailed description.
       *
       * While the syntax of the property is mostly identical - we do not support
       * multiple shadows and allow for a more liberal placement of the color
       * parameter - its interpretation defers significantly in that the shadow's
       * shape is not determined by the bounding box, but by the CSS background
       * image (we could exend this in the future to take other CSS properties
       * like boder and background color into account).
       */
      if (node->background_shadow_material != COGL_INVALID_HANDLE)
        paint_shadow_with_opacity (node->background_shadow_material,
                                   node->shadow,
                                   &background_box,
                                   paint_opacity);

      paint_texture_with_opacity (node->background_texture, &background_box, paint_opacity);
    }
}
