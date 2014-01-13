/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * MetaTextureTower
 *
 * Mipmap emulation by creation of scaled down images
 *
 * Copyright (C) 2009 Red Hat, Inc.
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

#include <math.h>
#include <string.h>

#include "meta-texture-tower.h"
#include "meta-texture-rectangle.h"

#ifndef M_LOG2E
#define M_LOG2E 1.4426950408889634074
#endif

#define MAX_TEXTURE_LEVELS 12

/* If the texture format in memory doesn't match this, then Mesa
 * will do the conversion, so things will still work, but it might
 * be slow depending on how efficient Mesa is. These should be the
 * native formats unless the display is 16bpp. If conversions
 * here are a bottleneck, investigate whether we are converting when
 * storing window data *into* the texture before adding extra code
 * to handle multiple texture formats.
 */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define TEXTURE_FORMAT COGL_PIXEL_FORMAT_BGRA_8888_PRE
#else
#define TEXTURE_FORMAT COGL_PIXEL_FORMAT_ARGB_8888_PRE
#endif

typedef struct
{
  guint16 x1;
  guint16 y1;
  guint16 x2;
  guint16 y2;
} Box;

struct _MetaTextureTower
{
  int n_levels;
  CoglTexture *textures[MAX_TEXTURE_LEVELS];
  CoglOffscreen *fbos[MAX_TEXTURE_LEVELS];
  Box invalid[MAX_TEXTURE_LEVELS];
};

/**
 * meta_texture_tower_new:
 *
 * Creates a new texture tower. The base texture has to be set with
 * meta_texture_tower_set_base_texture() before use.
 *
 * Return value: the new texture tower. Free with meta_texture_tower_free()
 */
MetaTextureTower *
meta_texture_tower_new (void)
{
  MetaTextureTower *tower;

  tower = g_slice_new0 (MetaTextureTower);

  return tower;
}

/**
 * meta_texture_tower_free:
 * @tower: a #MetaTextureTower
 *
 * Frees a texture tower created with meta_texture_tower_new().
 */
void
meta_texture_tower_free (MetaTextureTower *tower)
{
  g_return_if_fail (tower != NULL);

  meta_texture_tower_set_base_texture (tower, NULL);

  g_slice_free (MetaTextureTower, tower);
}

/**
 * meta_texture_tower_set_base_texture:
 * @tower: a #MetaTextureTower
 * @texture: the new texture used as a base for scaled down versions
 *
 * Sets the base texture that is the scaled texture that the
 * scaled textures of the tower are derived from. The texture itself
 * will be used as level 0 of the tower and will be referenced until
 * unset or until the tower is freed.
 */
void
meta_texture_tower_set_base_texture (MetaTextureTower *tower,
                                     CoglTexture      *texture)
{
  int i;

  g_return_if_fail (tower != NULL);

  if (texture == tower->textures[0])
    return;

  if (tower->textures[0] != NULL)
    {
      for (i = 1; i < tower->n_levels; i++)
        {
          if (tower->textures[i] != NULL)
            {
              cogl_object_unref (tower->textures[i]);
              tower->textures[i] = NULL;
            }

          if (tower->fbos[i] != NULL)
            {
              cogl_object_unref (tower->fbos[i]);
              tower->fbos[i] = NULL;
            }
        }

      cogl_object_unref (tower->textures[0]);
    }

  tower->textures[0] = texture;

  if (tower->textures[0] != NULL)
    {
      int width, height;

      cogl_object_ref (tower->textures[0]);

      width = cogl_texture_get_width (tower->textures[0]);
      height = cogl_texture_get_height (tower->textures[0]);

      tower->n_levels = 1 + MAX ((int)(M_LOG2E * log (width)), (int)(M_LOG2E * log (height)));
      tower->n_levels = MIN(tower->n_levels, MAX_TEXTURE_LEVELS);

      meta_texture_tower_update_area (tower, 0, 0, width, height);
    }
  else
    {
      tower->n_levels = 0;
    }
}

/**
 * meta_texture_tower_update_area:
 * @tower: a #MetaTextureTower
 * @x: X coordinate of upper left of rectangle that changed
 * @y: Y coordinate of upper left of rectangle that changed
 * @width: width of rectangle that changed
 * @height: height rectangle that changed
 *
 * Mark a region of the base texture as having changed; the next
 * time a scaled down version of the base texture is retrieved,
 * the appropriate area of the scaled down texture will be updated.
 */
void
meta_texture_tower_update_area (MetaTextureTower *tower,
                                int               x,
                                int               y,
                                int               width,
                                int               height)
{
  int texture_width, texture_height;
  Box invalid;
  int i;

  g_return_if_fail (tower != NULL);

  if (tower->textures[0] == NULL)
    return;

  texture_width = cogl_texture_get_width (tower->textures[0]);
  texture_height = cogl_texture_get_height (tower->textures[0]);

  invalid.x1 = x;
  invalid.y1 = y;
  invalid.x2 = x + width;
  invalid.y2 = y + height;

  for (i = 1; i < tower->n_levels; i++)
    {
      texture_width = MAX (1, texture_width / 2);
      texture_height = MAX (1, texture_height / 2);

      invalid.x1 = invalid.x1 / 2;
      invalid.y1 = invalid.y1 / 2;
      invalid.x2 = MIN (texture_width, (invalid.x2 + 1) / 2);
      invalid.y2 = MIN (texture_height, (invalid.y2 + 1) / 2);

      if (tower->invalid[i].x1 == tower->invalid[i].x2 ||
          tower->invalid[i].y1 == tower->invalid[i].y2)
        {
          tower->invalid[i] = invalid;
        }
      else
        {
          tower->invalid[i].x1 = MIN (tower->invalid[i].x1, invalid.x1);
          tower->invalid[i].y1 = MIN (tower->invalid[i].y1, invalid.y1);
          tower->invalid[i].x2 = MAX (tower->invalid[i].x2, invalid.x2);
          tower->invalid[i].y2 = MAX (tower->invalid[i].y2, invalid.y2);
        }
    }
}

/* It generally looks worse if we scale up a window texture by even a
 * small amount than if we scale it down using bilinear filtering, so
 * we always pick the *larger* adjacent level. */
#define LOD_BIAS (-0.49)

/* This determines the appropriate level of detail to use when drawing the
 * texture, in a way that corresponds to what the GL specification does
 * when mip-mapping. This is probably fancier and slower than what we need,
 * but we do the computation only once each time we paint a window, and
 * its easier to just use the equations from the specification than to
 * come up with something simpler.
 *
 * If window is being painted at an angle from the viewer, then we have to
 * pick a point in the texture; we use the middle of the texture (which is
 * why the width/height are passed in.) This is not the normal case for
 * Meta.
 */
static int
get_paint_level (int width, int height)
{
  CoglMatrix projection, modelview, pm;
  float v[4];
  double viewport_width, viewport_height;
  double u0, v0;
  double xc, yc, wc;
  double dxdu_, dxdv_, dydu_, dydv_;
  double det_, det_sq;
  double rho_sq;
  double lambda;

  /* See
   * http://www.opengl.org/registry/doc/glspec32.core.20090803.pdf
   * Section 3.8.9, p. 1.6.2. Here we have
   *
   *  u(x,y) = x_o;
   *  v(x,y) = y_o;
   *
   * Since we are mapping 1:1 from object coordinates into pixel
   * texture coordinates, the clip coordinates are:
   *
   *  (x_c)                               (x_o)        (u)
   *  (y_c) = (M_projection)(M_modelview) (y_o) = (PM) (v)
   *  (z_c)                               (z_o)        (0)
   *  (w_c)                               (w_o)        (1)
   */

  cogl_get_projection_matrix (&projection);
  cogl_get_modelview_matrix (&modelview);

  cogl_matrix_multiply (&pm, &projection, &modelview);

  cogl_get_viewport (v);
  viewport_width = v[2];
  viewport_height = v[3];

  u0 = width / 2.;
  v0 = height / 2.;

  xc = pm.xx * u0 + pm.xy * v0 + pm.xw;
  yc = pm.yx * u0 + pm.yy * v0 + pm.yw;
  wc = pm.wx * u0 + pm.wy * v0 + pm.ww;

  /* We'll simplify the equations below for a bit of micro-optimization.
   * The commented out code is the unsimplified version.

  // Partial derivates of window coordinates:
  //
  //  x_w = 0.5 * viewport_width * x_c / w_c + viewport_center_x
  //  y_w = 0.5 * viewport_height * y_c / w_c + viewport_center_y
  //
  // with respect to u, v, using
  // d(a/b)/dx = da/dx * (1/b) - a * db/dx / (b^2)

  dxdu = 0.5 * viewport_width * (pm.xx - pm.wx * (xc/wc)) / wc;
  dxdv = 0.5 * viewport_width * (pm.xy - pm.wy * (xc/wc)) / wc;
  dydu = 0.5 * viewport_height * (pm.yx - pm.wx * (yc/wc)) / wc;
  dydv = 0.5 * viewport_height * (pm.yy - pm.wy * (yc/wc)) / wc;

  // Compute the inverse partials as the matrix inverse
  det = dxdu * dydv - dxdv * dydu;

  dudx =   dydv / det;
  dudy = - dxdv / det;
  dvdx = - dydu / det;
  dvdy =   dvdu / det;

  // Scale factor; maximum of the distance in texels for a change of 1 pixel
  // in the X direction or 1 pixel in the Y direction
  rho = MAX (sqrt (dudx * dudx + dvdx * dvdx), sqrt(dudy * dudy + dvdy * dvdy));

  // Level of detail
  lambda = log2 (rho) + LOD_BIAS;
  */

  /* dxdu * wc, etc */
  dxdu_ = 0.5 * viewport_width * (pm.xx - pm.wx * (xc/wc));
  dxdv_ = 0.5 * viewport_width * (pm.xy - pm.wy * (xc/wc));
  dydu_ = 0.5 * viewport_height * (pm.yx - pm.wx * (yc/wc));
  dydv_ = 0.5 * viewport_height * (pm.yy - pm.wy * (yc/wc));

  /* det * wc^2 */
  det_ = dxdu_ * dydv_ - dxdv_ * dydu_;
  det_sq = det_ * det_;
  if (det_sq == 0.0)
    return -1;

  /* (rho * det * wc)^2 */
  rho_sq = MAX (dydv_ * dydv_ + dydu_ * dydu_, dxdv_ * dxdv_ + dxdu_ * dxdu_);
  lambda = 0.5 * M_LOG2E * log (rho_sq * wc * wc / det_sq) + LOD_BIAS;

#if 0
  g_print ("%g %g %g\n", 0.5 * viewport_width * pm.xx / pm.ww, 0.5 * viewport_height * pm.yy / pm.ww, lambda);
#endif

  if (lambda <= 0.)
    return 0;
  else
    return (int)(0.5 + lambda);
}

static gboolean
is_power_of_two (int x)
{
  return (x & (x - 1)) == 0;
}

static void
texture_tower_create_texture (MetaTextureTower *tower,
                              int               level,
                              int               width,
                              int               height)
{
  if ((!is_power_of_two (width) || !is_power_of_two (height)) &&
      meta_texture_rectangle_check (tower->textures[level - 1]))
    {
      ClutterBackend *backend = clutter_get_default_backend ();
      CoglContext *context = clutter_backend_get_cogl_context (backend);

      tower->textures[level] = cogl_texture_rectangle_new_with_size (context, width, height);
    }
  else
    {
      tower->textures[level] = cogl_texture_new_with_size (width, height,
                                                           COGL_TEXTURE_NO_AUTO_MIPMAP,
                                                           TEXTURE_FORMAT);
    }

  tower->invalid[level].x1 = 0;
  tower->invalid[level].y1 = 0;
  tower->invalid[level].x2 = width;
  tower->invalid[level].y2 = height;
}

static gboolean
texture_tower_revalidate_fbo (MetaTextureTower *tower,
                              int               level)
{
  CoglTexture *source_texture = tower->textures[level - 1];
  int source_texture_width = cogl_texture_get_width (source_texture);
  int source_texture_height = cogl_texture_get_height (source_texture);
  CoglTexture *dest_texture = tower->textures[level];
  int dest_texture_width = cogl_texture_get_width (dest_texture);
  int dest_texture_height = cogl_texture_get_height (dest_texture);
  Box *invalid = &tower->invalid[level];
  CoglMatrix modelview;

  if (tower->fbos[level] == NULL)
    tower->fbos[level] = cogl_offscreen_new_to_texture (dest_texture);

  if (tower->fbos[level] == NULL)
    return FALSE;

  cogl_push_framebuffer (COGL_FRAMEBUFFER (tower->fbos[level]));

  cogl_ortho (0, dest_texture_width, dest_texture_height, 0, -1., 1.);

  cogl_matrix_init_identity (&modelview);
  cogl_set_modelview_matrix (&modelview);

  cogl_set_source_texture (tower->textures[level - 1]);
  cogl_rectangle_with_texture_coords (invalid->x1, invalid->y1,
                                      invalid->x2, invalid->y2,
                                      (2. * invalid->x1) / source_texture_width,
                                      (2. * invalid->y1) / source_texture_height,
                                      (2. * invalid->x2) / source_texture_width,
                                      (2. * invalid->y2) / source_texture_height);

  cogl_pop_framebuffer ();

  return TRUE;
}

static void
fill_copy (guchar       *buf,
           const guchar *source,
           int           width)
{
  memcpy (buf, source, width * 4);
}

static void
fill_scale_down (guchar       *buf,
                 const guchar *source,
                 int           width)
{
  while (width > 1)
    {
      buf[0] = (source[0] + source[4]) / 2;
      buf[1] = (source[1] + source[5]) / 2;
      buf[2] = (source[2] + source[6]) / 2;
      buf[3] = (source[3] + source[7]) / 2;

      buf += 4;
      source += 8;
      width -= 2;
    }

  if (width > 0)
    {
      buf[0] = source[0] / 2;
      buf[1] = source[1] / 2;
      buf[2] = source[2] / 2;
      buf[3] = source[3] / 2;
    }
}

static void
texture_tower_revalidate_client (MetaTextureTower *tower,
                                 int               level)
{
  CoglTexture *source_texture = tower->textures[level - 1];
  int source_texture_width = cogl_texture_get_width (source_texture);
  int source_texture_height = cogl_texture_get_height (source_texture);
  guint source_rowstride;
  guchar *source_data;
  CoglTexture *dest_texture = tower->textures[level];
  int dest_texture_width = cogl_texture_get_width (dest_texture);
  int dest_texture_height = cogl_texture_get_height (dest_texture);
  int dest_x = tower->invalid[level].x1;
  int dest_y = tower->invalid[level].y1;
  int dest_width = tower->invalid[level].x2 - tower->invalid[level].x1;
  int dest_height = tower->invalid[level].y2 - tower->invalid[level].y1;
  guchar *dest_data;
  guchar *source_tmp1 = NULL, *source_tmp2 = NULL;
  int i, j;

  source_rowstride = source_texture_width * 4;

  source_data = g_malloc (source_texture_height * source_rowstride);
  cogl_texture_get_data (source_texture, TEXTURE_FORMAT, source_rowstride,
                         source_data);

  dest_data = g_malloc (dest_height * dest_width * 4);

  if (dest_texture_height < source_texture_height)
    {
      source_tmp1 = g_malloc (dest_width * 4);
      source_tmp2 = g_malloc (dest_width * 4);
    }

  for (i = 0; i < dest_height; i++)
    {
      guchar *dest_row = dest_data + i * dest_width * 4;
      if (dest_texture_height < source_texture_height)
        {
          guchar *source1, *source2;
          guchar *dest;

          if (dest_texture_width < source_texture_width)
            {
              fill_scale_down (source_tmp1,
                               source_data + ((i + dest_y) * 2) * source_rowstride + dest_x * 2 * 4,
                               dest_width * 2);
              fill_scale_down (source_tmp2,
                               source_data + ((i + dest_y) * 2 + 1) * source_rowstride + dest_x * 2 * 4,
                               dest_width * 2);
            }
          else
            {
              fill_copy (source_tmp1,
                         source_data + ((i + dest_y) * 2) * source_rowstride + dest_x * 4,
                         dest_width);
              fill_copy (source_tmp2,
                         source_data + ((i + dest_y) * 2 + 1) * source_rowstride + dest_x * 4,
                         dest_width);
            }

          source1 = source_tmp1;
          source2 = source_tmp2;

          dest = dest_row;
          for (j = 0; j < dest_width * 4; j++)
            *(dest++) = (*(source1++) + *(source2++)) / 2;
        }
      else
        {
          if (dest_texture_width < source_texture_width)
            fill_scale_down (dest_row,
                             source_data + (i + dest_y) * source_rowstride + dest_x * 2 * 4,
                             dest_width * 2);
          else
            fill_copy (dest_row,
                       source_data + (i + dest_y) * source_rowstride,
                       dest_width);
        }
    }

  cogl_texture_set_region (dest_texture,
                           0, 0,
                           dest_x, dest_y,
                           dest_width, dest_height,
                           dest_width, dest_height,
                           TEXTURE_FORMAT,
                           4 * dest_width,
                           dest_data);

  if (dest_texture_height < source_texture_height)
    {
      g_free (source_tmp1);
      g_free (source_tmp2);
    }

  g_free (source_data);
  g_free (dest_data);
}

static void
texture_tower_revalidate (MetaTextureTower *tower,
                          int               level)
{
  if (!texture_tower_revalidate_fbo (tower, level))
    texture_tower_revalidate_client (tower, level);
}

/**
 * meta_texture_tower_get_paint_texture:
 * @tower: a #MetaTextureTower
 *
 * Gets the texture from the tower that best matches the current
 * rendering scale. (On the assumption here the texture is going to
 * be rendered with vertex coordinates that correspond to its
 * size in pixels, so a 200x200 texture will be rendered on the
 * rectangle (0, 0, 200, 200).
 *
 * Return value: the COGL texture handle to use for painting, or
 *  %NULL if no base texture has yet been set.
 */
CoglTexture *
meta_texture_tower_get_paint_texture (MetaTextureTower *tower)
{
  int texture_width, texture_height;
  int level;

  g_return_val_if_fail (tower != NULL, NULL);

  if (tower->textures[0] == NULL)
    return NULL;

  texture_width = cogl_texture_get_width (tower->textures[0]);
  texture_height = cogl_texture_get_height (tower->textures[0]);

  level = get_paint_level(texture_width, texture_height);
  if (level < 0) /* singular paint matrix, scaled to nothing */
    return NULL;
  level = MIN (level, tower->n_levels - 1);

  if (tower->textures[level] == NULL ||
      (tower->invalid[level].x2 != tower->invalid[level].x1 &&
       tower->invalid[level].y2 != tower->invalid[level].y1))
    {
      int i;

      for (i = 1; i <= level; i++)
       {
         /* Use "floor" convention here to be consistent with the NPOT texture extension */
         texture_width = MAX (1, texture_width / 2);
         texture_height = MAX (1, texture_height / 2);

         if (tower->textures[i] == NULL)
           texture_tower_create_texture (tower, i, texture_width, texture_height);
       }

      for (i = 1; i <= level; i++)
       {
         if (tower->invalid[level].x2 != tower->invalid[level].x1 &&
             tower->invalid[level].y2 != tower->invalid[level].y1)
           texture_tower_revalidate (tower, i);
       }
   }

  return tower->textures[level];
}
