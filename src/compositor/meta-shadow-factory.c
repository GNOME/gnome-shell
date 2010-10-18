/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * MetaShadowFactory:
 *
 * Create and cache shadow textures for abritrary window shapes
 *
 * Copyright (C) 2010 Red Hat, Inc.
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
#include <math.h>
#include <string.h>

#include "meta-shadow-factory.h"
#include "region-utils.h"

/* This file implements blurring the shape of a window to produce a
 * shadow texture. The details are discussed below; a quick summary
 * of the optimizations we use:
 *
 * - If the window shape is along the lines of a rounded rectangle -
 *   a rectangular center portion with stuff at the corners - then
 *   the blur of this - the shadow - can also be represented as a
 *   9-sliced texture and the same texture can be used for different
 *   size.
 *
 * - We use the fact that a Gaussian blur is separable to do a
 *   2D blur as 1D blur of the rows followed by a 1D blur of the
 *   columns.
 *
 * - For better cache efficiency, we blur rows, transpose the image
 *   in blocks, blur rows again, and then transpose back.
 *
 * - We approximate the 1D gaussian blur as 3 successive box filters.
 */

typedef struct _MetaShadowCacheKey MetaShadowCacheKey;

struct _MetaShadowCacheKey
{
  MetaWindowShape *shape;
  int radius;
};

struct _MetaShadow
{
  int ref_count;

  MetaShadowFactory *factory;
  MetaShadowCacheKey key;
  CoglHandle texture;
  CoglHandle material;

  int spread;
  int border_top;
  int border_right;
  int border_bottom;
  int border_left;
};

struct _MetaShadowFactory
{
  /* MetaShadowCacheKey => MetaShadow; the shadows are not referenced
   * by the factory, they are simply removed from the table when freed */
  GHashTable *shadows;
};

static guint
meta_shadow_cache_key_hash (gconstpointer val)
{
  const MetaShadowCacheKey *key = val;

  return 59 * key->radius + 67 * meta_window_shape_hash (key->shape);
}

static gboolean
meta_shadow_cache_key_equal (gconstpointer a,
                             gconstpointer b)
{
  const MetaShadowCacheKey *key_a = a;
  const MetaShadowCacheKey *key_b = b;

  return (key_a->radius == key_b->radius &&
          meta_window_shape_equal (key_a->shape, key_b->shape));
}

MetaShadow *
meta_shadow_ref (MetaShadow *shadow)
{
  shadow->ref_count++;

  return shadow;
}

void
meta_shadow_unref (MetaShadow *shadow)
{
  shadow->ref_count--;
  if (shadow->ref_count == 0)
    {
      if (shadow->factory)
        {
          g_hash_table_remove (shadow->factory->shadows,
                               &shadow->key);
        }

      meta_window_shape_unref (shadow->key.shape);
      cogl_handle_unref (shadow->texture);
      cogl_handle_unref (shadow->material);

      g_slice_free (MetaShadow, shadow);
    }
}

/**
 * meta_shadow_paint:
 * @window_x: x position of the region to paint a shadow for
 * @window_y: y position of the region to paint a shadow for
 * @window_width: actual width of the region to paint a shadow for
 * @window_height: actual height of the region to paint a shadow for
 *
 * Paints the shadow at the given position, for the specified actual
 * size of the region. (Since a #MetaShadow can be shared between
 * different sizes with the same extracted #MetaWindowShape the
 * size needs to be passed in here.)
 */
void
meta_shadow_paint (MetaShadow *shadow,
                   int         window_x,
                   int         window_y,
                   int         window_width,
                   int         window_height,
                   guint8      opacity)
{
  float texture_width = cogl_texture_get_width (shadow->texture);
  float texture_height = cogl_texture_get_height (shadow->texture);
  int i, j;

  cogl_material_set_color4ub (shadow->material,
                              opacity, opacity, opacity, opacity);

  cogl_set_source (shadow->material);

  if (window_width + 2 * shadow->spread == shadow->border_left &&
      window_height + 2 * shadow->spread == shadow->border_top)
    {
      /* The non-scaled case - paint with a single rectangle */
      cogl_rectangle_with_texture_coords (window_x - shadow->spread,
                                          window_y - shadow->spread,
                                          window_x + window_width + shadow->spread,
                                          window_y + window_height + shadow->spread,
                                          0.0, 0.0, 1.0, 1.0);
    }
  else
    {
      float src_x[4];
      float src_y[4];
      float dest_x[4];
      float dest_y[4];

      src_x[0] = 0.0;
      src_x[1] = shadow->border_left / texture_width;
      src_x[2] = (texture_width - shadow->border_right) / texture_width;
      src_x[3] = 1.0;

      src_y[0] = 0.0;
      src_y[1] = shadow->border_top / texture_height;
      src_y[2] = (texture_height - shadow->border_bottom) / texture_height;
      src_y[3] = 1.0;

      dest_x[0] = window_x - shadow->spread;
      dest_x[1] = window_x - shadow->spread + shadow->border_left;
      dest_x[2] = window_x + window_width + shadow->spread - shadow->border_right;
      dest_x[3] = window_x + window_width + shadow->spread;

      dest_y[0] = window_y - shadow->spread;
      dest_y[1] = window_y - shadow->spread + shadow->border_top;
      dest_y[2] = window_y + window_height + shadow->spread - shadow->border_bottom;
      dest_y[3] = window_y + window_height + shadow->spread;

      for (j = 0; j < 3; j++)
        for (i = 0; i < 3; i++)
          cogl_rectangle_with_texture_coords (dest_x[i], dest_y[j],
                                              dest_x[i + 1], dest_y[j + 1],
                                              src_x[i], src_y[j],
                                              src_x[i + 1], src_y[j + 1]);
    }
}

/**
 * meta_shadow_get_bounds:
 * @shadow: a #MetaShadow
 * @window_x: x position of the region to paint a shadow for
 * @window_y: y position of the region to paint a shadow for
 * @window_width: actual width of the region to paint a shadow for
 * @window_height: actual height of the region to paint a shadow for
 *
 * Computes the bounds of the pixels that will be affected by
 * meta_shadow_paints()
 */
void
meta_shadow_get_bounds  (MetaShadow            *shadow,
                         int                    window_x,
                         int                    window_y,
                         int                    window_width,
                         int                    window_height,
                         cairo_rectangle_int_t *bounds)
{
  bounds->x = window_x - shadow->spread;
  bounds->y = window_x - shadow->spread;
  bounds->width = window_width + 2 * shadow->spread;
  bounds->height = window_width + 2 * shadow->spread;
}

MetaShadowFactory *
meta_shadow_factory_new (void)
{
  MetaShadowFactory *factory;

  factory = g_slice_new0 (MetaShadowFactory);

  factory->shadows = g_hash_table_new (meta_shadow_cache_key_hash,
                                       meta_shadow_cache_key_equal);

  return factory;
}

void
meta_shadow_factory_free (MetaShadowFactory *factory)
{
  GHashTableIter iter;
  gpointer key, value;

  /* Detach from the shadows in the table so we won't try to
   * remove them when they freed. */
  g_hash_table_iter_init (&iter, factory->shadows);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      MetaShadow *shadow = key;
      shadow->factory = NULL;
    }

  g_hash_table_destroy (factory->shadows);

  g_slice_free (MetaShadowFactory, factory);
}

MetaShadowFactory *
meta_shadow_factory_get_default (void)
{
  static MetaShadowFactory *factory;

  if (factory == NULL)
    factory = meta_shadow_factory_new ();

  return factory;
}

/* We emulate a 1D Gaussian blur by using 3 consecutive box blurs;
 * this produces a result that's within 3% of the original and can be
 * implemented much faster for large filter sizes because of the
 * efficiency of implementation of a box blur. Idea and formula
 * for choosing the box blur size come from:
 *
 * http://www.w3.org/TR/SVG/filters.html#feGaussianBlurElement
 *
 * The 2D blur is then done by blurring the rows, flipping the
 * image and blurring the columns. (This is possible because the
 * Gaussian kernel is separable - it's the product of a horizontal
 * blur and a vertical blur.)
 */
static int
get_box_filter_size (int radius)
{
  return (int)(0.5 + radius * (0.75 * sqrt(2*M_PI)));
}

/* The "spread" of the filter is the number of pixels from an original
 * pixel that it's blurred image extends. (A no-op blur that doesn't
 * blur would have a spread of 0.) See comment in blur_rows() for why the
 * odd and even cases are different
 */
static int
get_shadow_spread (int radius)
{
  int d = get_box_filter_size (radius);

  if (d % 2 == 1)
    return 3 * (d / 2);
  else
    return 3 * (d / 2) - 1;
}

/* This applies a single box blur pass to a horizontal range of pixels;
 * since the box blur has the same weight for all pixels, we can
 * implement an efficient sliding window algorithm where we add
 * in pixels coming into the window from the right and remove
 * them when they leave the windw to the left.
 *
 * d is the filter width; for even d shift indicates how the blurred
 * result is aligned with the original - does ' x ' go to ' yy' (shift=1)
 * or 'yy ' (shift=-1)
 */
static void
blur_xspan (guchar *row,
            guchar *tmp_buffer,
            int     row_width,
            int     x0,
            int     x1,
            int     d,
            int     shift)
{
  int offset;
  int sum = 0;
  int i;

  if (d % 2 == 1)
    offset = d / 2;
  else
    offset = (d - shift) / 2;

  /* All the conditionals in here look slow, but the branches will
   * be well predicted and there are enough different possibilities
   * that trying to write this as a series of unconditional loops
   * is hard and not an obvious win. The main slow down here seems
   * to be the integer division for pixel; one possible optimization
   * would be to accumulate into two 16-bit integer buffers and
   * only divide down after all three passes. (SSE parallel implementation
   * of the divide step is possible.)
   */
  for (i = x0 - d + offset; i < x1 + offset; i++)
    {
      if (i >= 0 && i < row_width)
	sum += row[i];

      if (i >= x0 + offset)
	{
	  if (i >= d)
	    sum -= row[i - d];

	  tmp_buffer[i - offset] = (sum + d / 2) / d;
	}
    }

  memcpy(row + x0, tmp_buffer + x0, x1 - x0);
}

static void
blur_rows (cairo_region_t   *convolve_region,
           int               x_offset,
           int               y_offset,
	   guchar           *buffer,
	   int               buffer_width,
	   int               buffer_height,
           int               d)
{
  int i, j;
  int n_rectangles;
  guchar *tmp_buffer;

  tmp_buffer = g_malloc (buffer_width);

  n_rectangles = cairo_region_num_rectangles (convolve_region);
  for (i = 0; i < n_rectangles; i++)
    {
      cairo_rectangle_int_t rect;

      cairo_region_get_rectangle (convolve_region, i, &rect);

      for (j = y_offset + rect.y; j < y_offset + rect.y + rect.height; j++)
	{
	  guchar *row = buffer + j * buffer_width;
	  int x0 = x_offset + rect.x;
	  int x1 = x0 + rect.width;

          /* We want to produce a symmetric blur that spreads a pixel
           * equally far to the left and right. If d is odd that happens
           * naturally, but for d even, we approximate by using a blur
           * on either side and then a centered blur of size d + 1.
           * (techique also from the SVG specification)
           */
	  if (d % 2 == 1)
	    {
	      blur_xspan (row, tmp_buffer, buffer_width, x0, x1, d, 0);
	      blur_xspan (row, tmp_buffer, buffer_width, x0, x1, d, 0);
	      blur_xspan (row, tmp_buffer, buffer_width, x0, x1, d, 0);
	    }
	  else
	    {
	      blur_xspan (row, tmp_buffer, buffer_width, x0, x1, d, 1);
	      blur_xspan (row, tmp_buffer, buffer_width, x0, x1, d, -1);
	      blur_xspan (row, tmp_buffer, buffer_width, x0, x1, d + 1, 0);
	    }
	}
    }

  g_free (tmp_buffer);
}

/* Swaps width and height. Either swaps in-place and returns the original
 * buffer or allocates a new buffer, frees the original buffer and returns
 * the new buffer.
 */
static guchar *
flip_buffer (guchar *buffer,
	     int     width,
             int     height)
{
  /* Working in blocks increases cache efficiency, compared to reading
   * or writing an entire column at once */
#define BLOCK_SIZE 16

  if (width == height)
    {
      int i0, j0;

      for (j0 = 0; j0 < height; j0 += BLOCK_SIZE)
	for (i0 = 0; i0 <= j0; i0 += BLOCK_SIZE)
	  {
	    int max_j = MIN(j0 + BLOCK_SIZE, height);
	    int max_i = MIN(i0 + BLOCK_SIZE, width);
	    int i, j;

	    if (i0 == j0)
	      {
		for (j = j0; j < max_j; j++)
		  for (i = i0; i < j; i++)
		    {
		      guchar tmp = buffer[j * width + i];
		      buffer[j * width + i] = buffer[i * width + j];
		      buffer[i * width + j] = tmp;
		    }
	      }
	    else
	      {
		for (j = j0; j < max_j; j++)
		  for (i = i0; i < max_i; i++)
		    {
		      guchar tmp = buffer[j * width + i];
		      buffer[j * width + i] = buffer[i * width + j];
		      buffer[i * width + j] = tmp;
		    }
	      }
	  }

      return buffer;
    }
  else
    {
      guchar *new_buffer = g_malloc (height * width);
      int i0, j0;

      for (i0 = 0; i0 < width; i0 += BLOCK_SIZE)
        for (j0 = 0; j0 < height; j0 += BLOCK_SIZE)
	  {
	    int max_j = MIN(j0 + BLOCK_SIZE, height);
	    int max_i = MIN(i0 + BLOCK_SIZE, width);
	    int i, j;

            for (i = i0; i < max_i; i++)
              for (j = j0; j < max_j; j++)
		new_buffer[i * height + j] = buffer[j * width + i];
	  }

      g_free (buffer);

      return new_buffer;
    }
#undef BLOCK_SIZE
}

static CoglHandle
make_shadow (cairo_region_t *region,
             int             radius)
{
  int d = get_box_filter_size (radius);
  int spread = get_shadow_spread (radius);
  CoglHandle result;
  cairo_rectangle_int_t extents;
  cairo_region_t *row_convolve_region;
  cairo_region_t *column_convolve_region;
  guchar *buffer;
  int buffer_width;
  int buffer_height;
  int n_rectangles, j, k;

  cairo_region_get_extents (region, &extents);

  buffer_width = extents.width +  2 * spread;
  buffer_height = extents.height +  2 * spread;

  /* Round up so we have aligned rows/columns */
  buffer_width = (buffer_width + 3) & ~3;
  buffer_height = (buffer_height + 3) & ~3;

  /* Square buffer allows in-place swaps, which are roughly 70% faster, but we
   * don't want to over-allocate too much memory.
   */
  if (buffer_height < buffer_width && buffer_height > (3 * buffer_width) / 4)
    buffer_height = buffer_width;
  if (buffer_width < buffer_height && buffer_width > (3 * buffer_height) / 4)
    buffer_width = buffer_height;

  buffer = g_malloc0 (buffer_width * buffer_height);

  /* Blurring with multiple box-blur passes is fast, but (especially for
   * large shadow sizes) we can improve efficiency by restricting the blur
   * to the region that actually needs to be blurred.
   */
  row_convolve_region = meta_make_border_region (region, spread, 0, FALSE);
  column_convolve_region = meta_make_border_region (region, spread, spread, TRUE);

  /* Step 1: unblurred image */
  n_rectangles = cairo_region_num_rectangles (region);
  for (k = 0; k < n_rectangles; k++)
    {
      cairo_rectangle_int_t rect;

      cairo_region_get_rectangle (region, k, &rect);
      for (j = spread + rect.y; j < spread + rect.y + rect.height; j++)
	memset (buffer + buffer_width * j + spread + rect.x, 255, rect.width);
    }

  /* Step 2: blur rows */
  blur_rows (row_convolve_region, spread, spread, buffer, buffer_width, buffer_height, d);

  /* Step 2: swap rows and columns */
  buffer = flip_buffer (buffer, buffer_width, buffer_height);

  /* Step 3: blur rows (really columns) */
  blur_rows (column_convolve_region, spread, spread, buffer, buffer_height, buffer_width, d);

  /* Step 3: swap rows and columns */
  buffer = flip_buffer (buffer, buffer_height, buffer_width);

  result = cogl_texture_new_from_data (extents.width +  2 * spread,
				       extents.height +  2 * spread,
				       COGL_TEXTURE_NONE,
				       COGL_PIXEL_FORMAT_A_8,
				       COGL_PIXEL_FORMAT_ANY,
				       buffer_width,
				       buffer);

  cairo_region_destroy (row_convolve_region);
  cairo_region_destroy (column_convolve_region);
  g_free (buffer);

  return result;
}

/**
 * meta_shadow_factory_get_shadow:
 * @factory: a #MetaShadowFactory
 * @shape: the size-invariant shape of the window's region
 * @width: the actual width of the window's region
 * @width: the actual height of the window's region
 * @radius: the radius (gaussian standard deviation) of the shadow
 *
 * Gets the appropriate shadow object for drawing shadows for the
 * specified window shape. The region that we are shadowing is specified
 * as a combination of a size-invariant extracted shape and the size.
 * In some cases, the same shadow object can be shared between sizes;
 * in other cases a different shadow object is used for each size.
 *
 * Return value: (transfer full): a newly referenced #MetaShadow; unref with
 *  meta_shadow_unref()
 */
MetaShadow *
meta_shadow_factory_get_shadow (MetaShadowFactory  *factory,
                                MetaWindowShape    *shape,
                                int                 width,
                                int                 height,
                                int                 radius)
{
  MetaShadowCacheKey key;
  MetaShadow *shadow;
  cairo_region_t *region;
  int spread;
  int border_top, border_right, border_bottom, border_left;
  gboolean cacheable;

  /* Using a single shadow texture for different window sizes only works
   * when there is a central scaled area that is greater than twice
   * the spread of the gaussian blur we are applying to get to the
   * shadow image.
   *                         *********          ***********
   *  /----------\         *###########*      *#############*
   *  |          |   =>   **#*********#** => **#***********#**
   *  |          |        **#**     **#**    **#**       **#**
   *  |          |        **#*********#**    **#***********#**
   *  \----------/         *###########*      *#############*
   *                         **********         ************
   *   Original                Blur            Stretched Blur
   *
   * For smaller sizes, we create a separate shadow image for each size;
   * since we assume that there will be little reuse, we don't try to
   * cache such images but just recreate them. (Since the current cache
   * policy is to only keep around referenced shadows, there wouldn't
   * be any harm in caching them, it would just make the book-keeping
   * a bit tricker.)
   */
  spread = get_shadow_spread (radius);
  meta_window_shape_get_borders (shape,
                                 &border_top,
                                 &border_right,
                                 &border_bottom,
                                 &border_left);

  cacheable = (border_top + 2 * spread + border_bottom <= height &&
               border_left + 2 * spread + border_right <= width);

  if (cacheable)
    {
      key.shape = shape;
      key.radius = radius;

      shadow = g_hash_table_lookup (factory->shadows, &key);
      if (shadow)
        return meta_shadow_ref (shadow);
    }

  shadow = g_slice_new0 (MetaShadow);

  shadow->ref_count = 1;
  shadow->factory = factory;
  shadow->key.shape = meta_window_shape_ref (shape);
  shadow->key.radius = radius;

  shadow->spread = spread;

  if (cacheable)
    {
      shadow->border_top = border_top + 2 * spread;
      shadow->border_right += border_right + 2 * spread;
      shadow->border_bottom += border_bottom + 2 * spread;
      shadow->border_left += border_left + 2 * spread;

      region = meta_window_shape_to_region (shape, 2 * spread, 2 * spread);
    }
  else
    {
      /* In the non-scaled case, we put the entire shadow into the
       * upper-left-hand corner of the 9-slice */
      shadow->border_top = height + 2 * spread;
      shadow->border_right = 0;
      shadow->border_bottom = 0;
      shadow->border_left = width + 2 * spread;

      region = meta_window_shape_to_region (shape,
                                              width - border_left - border_right,
                                              height - border_top - border_bottom);
    }

  shadow->texture = make_shadow (region, radius);
  shadow->material = cogl_material_new ();
  cogl_material_set_layer (shadow->material, 0, shadow->texture);
  cairo_region_destroy (region);

  if (cacheable)
    g_hash_table_insert (factory->shadows, &shadow->key, shadow);

  return shadow;
}
