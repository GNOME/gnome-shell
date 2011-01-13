/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * st-theme-node-drawing.c: Code to draw themed elements
 *
 * Copyright 2009, 2010 Red Hat, Inc.
 * Copyright 2009, 2010 Florian Müllner
 * Copyright 2010 Intel Corporation.
 *
 * Contains code derived from:
 *   rectangle.c: Rounded rectangle.
 *     Copyright 2008 litl, LLC.
 *   st-texture-frame.h: Expandible texture actor
 *     Copyright 2007 OpenedHand
 *     Copyright 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <math.h>

#include "st-shadow.h"
#include "st-private.h"
#include "st-theme-private.h"
#include "st-theme-context.h"
#include "st-texture-cache.h"
#include "st-theme-node-private.h"

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

static void
elliptical_arc (cairo_t *cr,
                double   x_center,
                double   y_center,
                double   x_radius,
                double   y_radius,
                double   angle1,
                double   angle2)
{
  cairo_save (cr);
  cairo_translate (cr, x_center, y_center);
  cairo_scale (cr, x_radius, y_radius);
  cairo_arc (cr, 0, 0, 1.0, angle1, angle2);
  cairo_restore (cr);
}

static CoglHandle
create_corner_material (StCornerSpec *corner)
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

  if (max_border_width <= corner->radius)
    {
      double x_radius, y_radius;

      if (max_border_width != 0)
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

      x_radius = 0.5 * (1.0 - (double) corner->border_width_2 / corner->radius);
      y_radius = 0.5 * (1.0 - (double) corner->border_width_1 / corner->radius);

      /* TOPRIGHT */
      elliptical_arc (cr,
                      0.5, 0.5,
                      x_radius, y_radius,
                      3 * M_PI / 2, 2 * M_PI);

      /* BOTTOMRIGHT */
      elliptical_arc (cr,
                      0.5, 0.5,
                      x_radius, y_radius,
                      0, M_PI / 2);

      /* TOPLEFT */
      elliptical_arc (cr,
                      0.5, 0.5,
                      x_radius, y_radius,
                      M_PI, 3 * M_PI / 2);

      /* BOTTOMLEFT */
      elliptical_arc (cr,
                      0.5, 0.5,
                      x_radius, y_radius,
                      M_PI / 2, M_PI);

      cairo_fill (cr);
    }
  else
    {
      double radius;

      radius = (gdouble)corner->radius / max_border_width;

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

  return create_corner_material (data->corner);
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

static void
st_theme_node_get_corner_border_widths (StThemeNode *node,
                                        StCorner     corner_id,
                                        guint       *border_width_1,
                                        guint       *border_width_2)
{
  switch (corner_id)
    {
      case ST_CORNER_TOPLEFT:
        if (border_width_1)
            *border_width_1 = node->border_width[ST_SIDE_TOP];
        if (border_width_2)
            *border_width_2 = node->border_width[ST_SIDE_LEFT];
        break;
      case ST_CORNER_TOPRIGHT:
        if (border_width_1)
            *border_width_1 = node->border_width[ST_SIDE_TOP];
        if (border_width_2)
            *border_width_2 = node->border_width[ST_SIDE_RIGHT];
        break;
      case ST_CORNER_BOTTOMRIGHT:
        if (border_width_1)
            *border_width_1 = node->border_width[ST_SIDE_BOTTOM];
        if (border_width_2)
            *border_width_2 = node->border_width[ST_SIDE_RIGHT];
        break;
      case ST_CORNER_BOTTOMLEFT:
        if (border_width_1)
            *border_width_1 = node->border_width[ST_SIDE_BOTTOM];
        if (border_width_2)
            *border_width_2 = node->border_width[ST_SIDE_LEFT];
        break;
    }
}

static CoglHandle
st_theme_node_lookup_corner (StThemeNode    *node,
                             StCorner        corner_id)
{
  CoglHandle texture, material;
  char *key;
  StTextureCache *cache;
  StCornerSpec corner;
  LoadCornerData data;

  if (node->border_radius[corner_id] == 0)
    return COGL_INVALID_HANDLE;

  cache = st_texture_cache_get_default ();

  corner.radius = node->border_radius[corner_id];
  corner.color = node->background_color;
  st_theme_node_get_corner_border_widths (node, corner_id,
                                          &corner.border_width_1,
                                          &corner.border_width_2);

  switch (corner_id)
    {
      case ST_CORNER_TOPLEFT:
        over (&node->border_color[ST_SIDE_TOP], &corner.color, &corner.border_color_1);
        over (&node->border_color[ST_SIDE_LEFT], &corner.color, &corner.border_color_2);
        break;
      case ST_CORNER_TOPRIGHT:
        over (&node->border_color[ST_SIDE_TOP], &corner.color, &corner.border_color_1);
        over (&node->border_color[ST_SIDE_RIGHT], &corner.color, &corner.border_color_2);
        break;
      case ST_CORNER_BOTTOMRIGHT:
        over (&node->border_color[ST_SIDE_BOTTOM], &corner.color, &corner.border_color_1);
        over (&node->border_color[ST_SIDE_RIGHT], &corner.color, &corner.border_color_2);
        break;
      case ST_CORNER_BOTTOMLEFT:
        over (&node->border_color[ST_SIDE_BOTTOM], &corner.color, &corner.border_color_1);
        over (&node->border_color[ST_SIDE_LEFT], &corner.color, &corner.border_color_2);
        break;
    }

  if (corner.color.alpha == 0 &&
      corner.border_color_1.alpha == 0 &&
      corner.border_color_2.alpha == 0)
    return COGL_INVALID_HANDLE;

  key = corner_to_string (&corner);

  data.node = node;
  data.corner = &corner;
  texture = st_texture_cache_load (cache, key, ST_TEXTURE_CACHE_POLICY_NONE, load_corner, &data, NULL);
  material = _st_create_texture_material (texture);
  cogl_handle_unref (texture);

  g_free (key);

  return material;
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

  /* scale the background into the allocated bounds, when not being absolutely positioned */
  if ((w > result->x2 || h > result->y2) && !self->background_position_set)
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
      /* honor the specified position if any */
      if (self->background_position_set)
        {
          result->x1 = self->background_position_x;
          result->y1 = self->background_position_y;
        }
      else
        {
          /* center the background on the widget */
          result->x1 = (int)(((allocation->x2 - allocation->x1) / 2) - (w / 2));
          result->y1 = (int)(((allocation->y2 - allocation->y1) / 2) - (h / 2));
        }
      result->x2 = result->x1 + w;
      result->y2 = result->y1 + h;
    }
}

/* Use of this function marks code which doesn't support
 * non-uniform colors.
 */
static void
get_arbitrary_border_color (StThemeNode   *node,
                            ClutterColor  *color)
{
  if (color)
    st_theme_node_get_border_color (node, ST_SIDE_TOP, color);
}

static CoglHandle
st_theme_node_render_gradient (StThemeNode *node)
{
  CoglHandle texture;
  int radius[4], i;
  cairo_t *cr;
  cairo_surface_t *surface;
  cairo_pattern_t *pattern;
  ClutterColor border_color;
  int border_width[4];
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

  /* TODO - support non-uniform border colors */
  get_arbitrary_border_color (node, &border_color);

  for (i = 0; i < 4; i++)
    {
      border_width[i] = st_theme_node_get_border_width (node, i);

      radius[i] = st_theme_node_get_border_radius (node, i);
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

  /* Create a path for the background's outline first */
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
  if (radius[ST_CORNER_BOTTOMRIGHT] > 0)
    cairo_arc (cr,
               node->alloc_width - radius[ST_CORNER_BOTTOMRIGHT],
               node->alloc_height - radius[ST_CORNER_BOTTOMRIGHT],
               radius[ST_CORNER_BOTTOMRIGHT], 0, M_PI / 2);
  cairo_line_to (cr, radius[ST_CORNER_BOTTOMLEFT], node->alloc_height);
  if (radius[ST_CORNER_BOTTOMLEFT] > 0)
    cairo_arc (cr,
               radius[ST_CORNER_BOTTOMLEFT],
               node->alloc_height - radius[ST_CORNER_BOTTOMLEFT],
               radius[ST_CORNER_BOTTOMLEFT], M_PI / 2, M_PI);
  cairo_close_path (cr);


  /* If we have a border, we fill the outline with the border
   * color and create the inline shape for the background gradient;
   * otherwise the outline shape is filled with the background
   * gradient directly
   */
  if (border_width[ST_SIDE_TOP] > 0 ||
      border_width[ST_SIDE_RIGHT] > 0 ||
      border_width[ST_SIDE_BOTTOM] > 0 ||
      border_width[ST_SIDE_LEFT] > 0)
    {
      cairo_set_source_rgba (cr,
                             border_color.red / 255.,
                             border_color.green / 255.,
                             border_color.blue / 255.,
                             border_color.alpha / 255.);
      cairo_fill (cr);

      if (radius[ST_CORNER_TOPLEFT] > MAX(border_width[ST_SIDE_TOP],
                                          border_width[ST_SIDE_LEFT]))
        elliptical_arc (cr,
                        radius[ST_CORNER_TOPLEFT],
                        radius[ST_CORNER_TOPLEFT],
                        radius[ST_CORNER_TOPLEFT] - border_width[ST_SIDE_LEFT],
                        radius[ST_CORNER_TOPLEFT] - border_width[ST_SIDE_TOP],
                        M_PI, 3 * M_PI / 2);
      else
        cairo_move_to (cr,
                       border_width[ST_SIDE_LEFT],
                       border_width[ST_SIDE_TOP]);

      cairo_line_to (cr,
                     node->alloc_width - MAX(radius[ST_CORNER_TOPRIGHT], border_width[ST_SIDE_RIGHT]),
                     border_width[ST_SIDE_TOP]);

      if (radius[ST_CORNER_TOPRIGHT] > MAX(border_width[ST_SIDE_TOP],
                                           border_width[ST_SIDE_RIGHT]))
        elliptical_arc (cr,
                        node->alloc_width - radius[ST_CORNER_TOPRIGHT],
                        radius[ST_CORNER_TOPRIGHT],
                        radius[ST_CORNER_TOPRIGHT] - border_width[ST_SIDE_RIGHT],
                        radius[ST_CORNER_TOPRIGHT] - border_width[ST_SIDE_TOP],
                        3 * M_PI / 2, 2 * M_PI);
      else
        cairo_line_to (cr,
                       node->alloc_width - border_width[ST_SIDE_RIGHT],
                       border_width[ST_SIDE_TOP]);

      cairo_line_to (cr,
                     node->alloc_width - border_width[ST_SIDE_RIGHT],
                     node->alloc_height - MAX(radius[ST_CORNER_BOTTOMRIGHT], border_width[ST_SIDE_BOTTOM]));

      if (radius[ST_CORNER_BOTTOMRIGHT] > MAX(border_width[ST_SIDE_BOTTOM],
                                              border_width[ST_SIDE_RIGHT]))
        elliptical_arc (cr,
                        node->alloc_width - radius[ST_CORNER_BOTTOMRIGHT],
                        node->alloc_height - radius[ST_CORNER_BOTTOMRIGHT],
                        radius[ST_CORNER_BOTTOMRIGHT] - border_width[ST_SIDE_RIGHT],
                        radius[ST_CORNER_BOTTOMRIGHT] - border_width[ST_SIDE_BOTTOM],
                        0, M_PI / 2);
      else
        cairo_line_to (cr,
                       node->alloc_width - border_width[ST_SIDE_RIGHT],
                       node->alloc_height - border_width[ST_SIDE_BOTTOM]);

      cairo_line_to (cr,
                     MAX(radius[ST_CORNER_BOTTOMLEFT], border_width[ST_SIDE_LEFT]),
                     node->alloc_height - border_width[ST_SIDE_BOTTOM]);

      if (radius[ST_CORNER_BOTTOMLEFT] > MAX(border_width[ST_SIDE_BOTTOM],
                                             border_width[ST_SIDE_LEFT]))
        elliptical_arc (cr,
                        radius[ST_CORNER_BOTTOMLEFT],
                        node->alloc_height - radius[ST_CORNER_BOTTOMLEFT],
                        radius[ST_CORNER_BOTTOMLEFT] - border_width[ST_SIDE_LEFT],
                        radius[ST_CORNER_BOTTOMLEFT] - border_width[ST_SIDE_BOTTOM],
                        M_PI / 2, M_PI);
      else
        cairo_line_to (cr,
                       border_width[ST_SIDE_LEFT],
                       node->alloc_height - border_width[ST_SIDE_BOTTOM]);

      cairo_close_path (cr);
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
  int corner_id;

  if (node->background_texture != COGL_INVALID_HANDLE)
    cogl_handle_unref (node->background_texture);
  if (node->background_material != COGL_INVALID_HANDLE)
    cogl_handle_unref (node->background_material);
  if (node->background_shadow_material != COGL_INVALID_HANDLE)
    cogl_handle_unref (node->background_shadow_material);
  if (node->border_texture != COGL_INVALID_HANDLE)
    cogl_handle_unref (node->border_texture);
  if (node->border_material != COGL_INVALID_HANDLE)
    cogl_handle_unref (node->border_material);
  if (node->box_shadow_material != COGL_INVALID_HANDLE)
    cogl_handle_unref (node->box_shadow_material);

  for (corner_id = 0; corner_id < 4; corner_id++)
    if (node->corner_material[corner_id] != COGL_INVALID_HANDLE)
      cogl_handle_unref (node->corner_material[corner_id]);

  _st_theme_node_init_drawing_state (node);
}

void
_st_theme_node_init_drawing_state (StThemeNode *node)
{
  int corner_id;

  node->background_texture = COGL_INVALID_HANDLE;
  node->background_material = COGL_INVALID_HANDLE;
  node->background_shadow_material = COGL_INVALID_HANDLE;
  node->box_shadow_material = COGL_INVALID_HANDLE;
  node->border_texture = COGL_INVALID_HANDLE;
  node->border_material = COGL_INVALID_HANDLE;

  for (corner_id = 0; corner_id < 4; corner_id++)
    node->corner_material[corner_id] = COGL_INVALID_HANDLE;
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
  StShadow *box_shadow_spec;
  StShadow *background_image_shadow_spec;
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

  box_shadow_spec = st_theme_node_get_box_shadow (node);

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

  if (node->border_texture)
    node->border_material = _st_create_texture_material (node->border_texture);
  else
    node->border_material = COGL_INVALID_HANDLE;

  if (box_shadow_spec)
    {
      if (node->border_texture != COGL_INVALID_HANDLE)
        node->box_shadow_material = _st_create_shadow_material (box_shadow_spec,
                                                                node->border_texture);
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

              node->box_shadow_material = _st_create_shadow_material (box_shadow_spec,
                                                                      buffer);
            }
          cogl_handle_unref (buffer);
        }
    }

  background_image = st_theme_node_get_background_image (node);
  background_image_shadow_spec = st_theme_node_get_background_image_shadow (node);

  if (background_image != NULL)
    {

      node->background_texture = st_texture_cache_load_file_to_cogl_texture (texture_cache, background_image);
      node->background_material = _st_create_texture_material (node->background_texture);

      if (background_image_shadow_spec)
        {
          node->background_shadow_material = _st_create_shadow_material (background_image_shadow_spec,
                                                                         node->background_texture);
        }
    }

  node->corner_material[ST_CORNER_TOPLEFT] =
    st_theme_node_lookup_corner (node, ST_CORNER_TOPLEFT);
  node->corner_material[ST_CORNER_TOPRIGHT] =
    st_theme_node_lookup_corner (node, ST_CORNER_TOPRIGHT);
  node->corner_material[ST_CORNER_BOTTOMRIGHT] =
    st_theme_node_lookup_corner (node, ST_CORNER_BOTTOMRIGHT);
  node->corner_material[ST_CORNER_BOTTOMLEFT] =
    st_theme_node_lookup_corner (node, ST_CORNER_BOTTOMLEFT);
}

static void
paint_material_with_opacity (CoglHandle       material,
                             ClutterActorBox *box,
                             guint8           paint_opacity)
{
  cogl_material_set_color4ub (material,
                              paint_opacity, paint_opacity, paint_opacity, paint_opacity);

  cogl_set_source (material);
  cogl_rectangle (box->x1, box->y1, box->x2, box->y2);
}

static void
st_theme_node_paint_borders (StThemeNode           *node,
                             const ClutterActorBox *box,
                             guint8                 paint_opacity)

{
  float width, height;
  int border_width[4];
  int max_border_radius = 0;
  int max_width_radius[4];
  int corner_id, side_id;
  ClutterColor border_color;
  guint8 alpha;

  width = box->x2 - box->x1;
  height = box->y2 - box->y1;

  /* TODO - support non-uniform border colors */
  get_arbitrary_border_color (node, &border_color);

  for (side_id = 0; side_id < 4; side_id++)
    border_width[side_id] = st_theme_node_get_border_width(node, side_id);

  for (corner_id = 0; corner_id < 4; corner_id++)
    {
      guint border_width_1, border_width_2;

      st_theme_node_get_corner_border_widths (node, corner_id,
                                              &border_width_1, &border_width_2);

      if (node->border_radius[corner_id] > max_border_radius)
        max_border_radius = node->border_radius[corner_id];
      max_width_radius[corner_id] = MAX(MAX(border_width_1, border_width_2),
                                        node->border_radius[corner_id]);
    }

  /* borders */
  if (border_width[ST_SIDE_TOP] > 0 ||
      border_width[ST_SIDE_RIGHT] > 0 ||
      border_width[ST_SIDE_BOTTOM] > 0 ||
      border_width[ST_SIDE_LEFT] > 0)
    {
      ClutterColor effective_border;
      gboolean skip_corner_1, skip_corner_2;
      float x1, y1, x2, y2;

      over (&border_color, &node->background_color, &effective_border);
      alpha = paint_opacity * effective_border.alpha / 255;

      if (alpha > 0)
        {
          cogl_set_source_color4ub (effective_border.red,
                                    effective_border.green,
                                    effective_border.blue,
                                    alpha);

          /* NORTH */
          skip_corner_1 = node->border_radius[ST_CORNER_TOPLEFT] > 0;
          skip_corner_2 = node->border_radius[ST_CORNER_TOPRIGHT] > 0;

          x1 = skip_corner_1 ? max_width_radius[ST_CORNER_TOPLEFT] : 0;
          y1 = 0;
          x2 = skip_corner_2 ? width - max_width_radius[ST_CORNER_TOPRIGHT] : width;
          y2 = border_width[ST_SIDE_TOP];
          cogl_rectangle (x1, y1, x2, y2);

          /* EAST */
          skip_corner_1 = node->border_radius[ST_CORNER_TOPRIGHT] > 0;
          skip_corner_2 = node->border_radius[ST_CORNER_BOTTOMRIGHT] > 0;

          x1 = width - border_width[ST_SIDE_RIGHT];
          y1 = skip_corner_1 ? max_width_radius[ST_CORNER_TOPRIGHT]
                             : border_width[ST_SIDE_TOP];
          x2 = width;
          y2 = skip_corner_2 ? height - max_width_radius[ST_CORNER_BOTTOMRIGHT]
                             : height - border_width[ST_SIDE_BOTTOM];
          cogl_rectangle (x1, y1, x2, y2);

          /* SOUTH */
          skip_corner_1 = node->border_radius[ST_CORNER_BOTTOMLEFT] > 0;
          skip_corner_2 = node->border_radius[ST_CORNER_BOTTOMRIGHT] > 0;

          x1 = skip_corner_1 ? max_width_radius[ST_CORNER_BOTTOMLEFT] : 0;
          y1 = height - border_width[ST_SIDE_BOTTOM];
          x2 = skip_corner_2 ? width - max_width_radius[ST_CORNER_BOTTOMRIGHT]
                             : width;
          y2 = height;
          cogl_rectangle (x1, y1, x2, y2);

          /* WEST */
          skip_corner_1 = node->border_radius[ST_CORNER_TOPLEFT] > 0;
          skip_corner_2 = node->border_radius[ST_CORNER_BOTTOMLEFT] > 0;

          x1 = 0;
          y1 = skip_corner_1 ? max_width_radius[ST_CORNER_TOPLEFT]
                             : border_width[ST_SIDE_TOP];
          x2 = border_width[ST_SIDE_LEFT];
          y2 = skip_corner_2 ? height - max_width_radius[ST_CORNER_BOTTOMLEFT]
                             : height - border_width[ST_SIDE_BOTTOM];
          cogl_rectangle (x1, y1, x2, y2);
        }
    }

  /* corners */
  if (max_border_radius > 0 && paint_opacity > 0)
    {
      for (corner_id = 0; corner_id < 4; corner_id++)
        {
          if (node->corner_material[corner_id] == COGL_INVALID_HANDLE)
            continue;

          cogl_material_set_color4ub (node->corner_material[corner_id],
                                      paint_opacity, paint_opacity,
                                      paint_opacity, paint_opacity);
          cogl_set_source (node->corner_material[corner_id]);

          switch (corner_id)
            {
              case ST_CORNER_TOPLEFT:
                cogl_rectangle_with_texture_coords (0, 0,
                                                    max_width_radius[ST_CORNER_TOPLEFT], max_width_radius[ST_CORNER_TOPLEFT],
                                                    0, 0, 0.5, 0.5);
                break;
              case ST_CORNER_TOPRIGHT:
                cogl_rectangle_with_texture_coords (width - max_width_radius[ST_CORNER_TOPRIGHT], 0,
                                                    width, max_width_radius[ST_CORNER_TOPRIGHT],
                                                    0.5, 0, 1, 0.5);
                break;
              case ST_CORNER_BOTTOMRIGHT:
                cogl_rectangle_with_texture_coords (width - max_width_radius[ST_CORNER_BOTTOMRIGHT], height - max_width_radius[ST_CORNER_BOTTOMRIGHT],
                                                    width, height,
                                                    0.5, 0.5, 1, 1);
                break;
              case ST_CORNER_BOTTOMLEFT:
                cogl_rectangle_with_texture_coords (0, height - max_width_radius[ST_CORNER_BOTTOMLEFT],
                                                    max_width_radius[ST_CORNER_BOTTOMLEFT], height,
                                                    0, 0.5, 0.5, 1);
                break;
            }
        }
    }

  /* background color */
  alpha = paint_opacity * node->background_color.alpha / 255;
  if (alpha > 0)
    {
      cogl_set_source_color4ub (node->background_color.red,
                                node->background_color.green,
                                node->background_color.blue,
                                alpha);

      /* We add padding to each corner, so that all corners end up as if they
       * had a border-radius of max_border_radius, which allows us to treat
       * corners as uniform further on.
       */
      for (corner_id = 0; corner_id < 4; corner_id++)
        {
          float verts[8];
          int n_rects;

          /* corner texture does not need padding */
          if (max_border_radius == node->border_radius[corner_id])
            continue;

          n_rects = node->border_radius[corner_id] == 0 ? 1 : 2;

          switch (corner_id)
            {
              case ST_CORNER_TOPLEFT:
                verts[0] = border_width[ST_SIDE_LEFT];
                verts[1] = MAX(node->border_radius[corner_id],
                               border_width[ST_SIDE_TOP]);
                verts[2] = max_border_radius;
                verts[3] = max_border_radius;
                if (n_rects == 2)
                  {
                    verts[4] = MAX(node->border_radius[corner_id],
                                   border_width[ST_SIDE_LEFT]);
                    verts[5] = border_width[ST_SIDE_TOP];
                    verts[6] = max_border_radius;
                    verts[7] = MAX(node->border_radius[corner_id],
                                   border_width[ST_SIDE_TOP]);
                  }
                break;
              case ST_CORNER_TOPRIGHT:
                verts[0] = width - max_border_radius;
                verts[1] = MAX(node->border_radius[corner_id],
                               border_width[ST_SIDE_TOP]);
                verts[2] = width - border_width[ST_SIDE_RIGHT];
                verts[3] = max_border_radius;
                if (n_rects == 2)
                  {
                    verts[4] = width - max_border_radius;
                    verts[5] = border_width[ST_SIDE_TOP];
                    verts[6] = width - MAX(node->border_radius[corner_id],
                                           border_width[ST_SIDE_RIGHT]);
                    verts[7] = MAX(node->border_radius[corner_id],
                                   border_width[ST_SIDE_TOP]);
                  }
                break;
              case ST_CORNER_BOTTOMRIGHT:
                verts[0] = width - max_border_radius;
                verts[1] = height - max_border_radius;
                verts[2] = width - border_width[ST_SIDE_RIGHT];
                verts[3] = height - MAX(node->border_radius[corner_id],
                                        border_width[ST_SIDE_BOTTOM]);
                if (n_rects == 2)
                  {
                    verts[4] = width - max_border_radius;
                    verts[5] = height - MAX(node->border_radius[corner_id],
                                            border_width[ST_SIDE_BOTTOM]);
                    verts[6] = width - MAX(node->border_radius[corner_id],
                                           border_width[ST_SIDE_RIGHT]);
                    verts[7] = height - border_width[ST_SIDE_BOTTOM];
                  }
                break;
              case ST_CORNER_BOTTOMLEFT:
                verts[0] = border_width[ST_SIDE_LEFT];
                verts[1] = height - max_border_radius;
                verts[2] = max_border_radius;
                verts[3] = height - MAX(node->border_radius[corner_id],
                                        border_width[ST_SIDE_BOTTOM]);
                if (n_rects == 2)
                  {
                    verts[4] = MAX(node->border_radius[corner_id],
                                   border_width[ST_SIDE_LEFT]);
                    verts[5] = height - MAX(node->border_radius[corner_id],
                                            border_width[ST_SIDE_BOTTOM]);
                    verts[6] = max_border_radius;
                    verts[7] = height - border_width[ST_SIDE_BOTTOM];
                  }
                break;
            }
          cogl_rectangles (verts, n_rects);
        }

      /* Once we've drawn the borders and corners, if the corners are bigger
       * then the border width, the remaining area is shaped like
       *
       *  ########
       * ##########
       * ##########
       *  ########
       *
       * We draw it in at most 3 pieces - first the top and bottom if
       * necessary, then the main rectangle
       */
      if (max_border_radius > border_width[ST_SIDE_TOP])
        cogl_rectangle (MAX(max_border_radius, border_width[ST_SIDE_LEFT]),
                        border_width[ST_SIDE_TOP],
                        width - MAX(max_border_radius, border_width[ST_SIDE_RIGHT]),
                        max_border_radius);
      if (max_border_radius > border_width[ST_SIDE_BOTTOM])
        cogl_rectangle (MAX(max_border_radius, border_width[ST_SIDE_LEFT]),
                        height - max_border_radius,
                        width - MAX(max_border_radius, border_width[ST_SIDE_RIGHT]),
                        height - border_width[ST_SIDE_BOTTOM]);

      cogl_rectangle (border_width[ST_SIDE_LEFT],
                      MAX(border_width[ST_SIDE_TOP], max_border_radius),
                      width - border_width[ST_SIDE_RIGHT],
                      height - MAX(border_width[ST_SIDE_BOTTOM], max_border_radius));
    }
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

  material = node->border_material;
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

  if (node->box_shadow_material)
    _st_paint_shadow_with_opacity (node->box_shadow,
                                   node->box_shadow_material,
                                   &allocation,
                                   paint_opacity);

  if (node->border_material != COGL_INVALID_HANDLE)
    {
      /* Gradients and border images are mutually exclusive at this time */
      if (node->background_gradient_type != ST_GRADIENT_NONE)
        paint_material_with_opacity (node->border_material, &allocation, paint_opacity);
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
        _st_paint_shadow_with_opacity (node->background_image_shadow,
                                       node->background_shadow_material,
                                       &background_box,
                                       paint_opacity);

      paint_material_with_opacity (node->background_material, &background_box, paint_opacity);
    }
}

/**
 * st_theme_node_copy_cached_paint_state:
 * @node: a #StThemeNode
 * @other: a different #StThemeNode
 *
 * Copy cached painting state from @other to @node. This function can be used to
 * optimize redrawing cached background images when the style on an element changess
 * in a way that doesn't affect background drawing. This function must only be called
 * if st_theme_node_paint_equal (node, other) returns %TRUE.
 */
void
st_theme_node_copy_cached_paint_state (StThemeNode *node,
                                       StThemeNode *other)
{
  int corner_id;

  g_return_if_fail (ST_IS_THEME_NODE (node));
  g_return_if_fail (ST_IS_THEME_NODE (other));

  /* Check omitted for speed: */
  /* g_return_if_fail (st_theme_node_paint_equal (node, other)); */

  _st_theme_node_free_drawing_state (node);

  node->alloc_width = other->alloc_width;
  node->alloc_height = other->alloc_height;

  if (other->background_shadow_material)
    node->background_shadow_material = cogl_handle_ref (other->background_shadow_material);
  if (other->box_shadow_material)
    node->box_shadow_material = cogl_handle_ref (other->box_shadow_material);
  if (other->background_texture)
    node->background_texture = cogl_handle_ref (other->background_texture);
  if (other->background_material)
    node->background_material = cogl_handle_ref (other->background_material);
  if (other->border_texture)
    node->border_texture = cogl_handle_ref (other->border_texture);
  if (other->border_material)
    node->border_material = cogl_handle_ref (other->border_material);
  for (corner_id = 0; corner_id < 4; corner_id++)
    if (other->corner_material[corner_id])
      node->corner_material[corner_id] = cogl_handle_ref (other->corner_material[corner_id]);
}
