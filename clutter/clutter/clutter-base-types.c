/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:clutter-geometric-types
 * @Title: Base geometric types
 * @Short_Description: Common geometric data types used by Clutter
 *
 * Clutter defines a set of geometric data structures that are commonly used
 * across the whole API.
 */

#ifdef HAVE_CONFIG_H
#include "clutter-build-config.h"
#endif

#include "clutter-types.h"
#include "clutter-private.h"

#include <math.h>

#define FLOAT_EPSILON   (1e-15)



/*
 * ClutterGeometry
 */

static ClutterGeometry*
clutter_geometry_copy (const ClutterGeometry *geometry)
{
  return g_slice_dup (ClutterGeometry, geometry);
}

static void
clutter_geometry_free (ClutterGeometry *geometry)
{
  if (G_LIKELY (geometry != NULL))
    g_slice_free (ClutterGeometry, geometry);
}

/**
 * clutter_geometry_union:
 * @geometry_a: a #ClutterGeometry
 * @geometry_b: another #ClutterGeometry
 * @result: (out): location to store the result
 *
 * Find the union of two rectangles represented as #ClutterGeometry.
 *
 * Since: 1.4
 *
 * Deprecated: 1.16: Use #ClutterRect and clutter_rect_union()
 */
void
clutter_geometry_union (const ClutterGeometry *geometry_a,
                        const ClutterGeometry *geometry_b,
                        ClutterGeometry       *result)
{
  /* We don't try to handle rectangles that can't be represented
   * as a signed integer box */
  gint x_1 = MIN (geometry_a->x, geometry_b->x);
  gint y_1 = MIN (geometry_a->y, geometry_b->y);
  gint x_2 = MAX (geometry_a->x + (gint)geometry_a->width,
                  geometry_b->x + (gint)geometry_b->width);
  gint y_2 = MAX (geometry_a->y + (gint)geometry_a->height,
                  geometry_b->y + (gint)geometry_b->height);
  result->x = x_1;
  result->y = y_1;
  result->width = x_2 - x_1;
  result->height = y_2 - y_1;
}

/**
 * clutter_geometry_intersects:
 * @geometry0: The first geometry to test
 * @geometry1: The second geometry to test
 *
 * Determines if @geometry0 and geometry1 intersect returning %TRUE if
 * they do else %FALSE.
 *
 * Return value: %TRUE of @geometry0 and geometry1 intersect else
 * %FALSE.
 *
 * Since: 1.4
 *
 * Deprecated: 1.16: Use #ClutterRect and clutter_rect_intersection()
 */
gboolean
clutter_geometry_intersects (const ClutterGeometry *geometry0,
                             const ClutterGeometry *geometry1)
{
  if (geometry1->x >= (geometry0->x + (gint)geometry0->width) ||
      geometry1->y >= (geometry0->y + (gint)geometry0->height) ||
      (geometry1->x + (gint)geometry1->width) <= geometry0->x ||
      (geometry1->y + (gint)geometry1->height) <= geometry0->y)
    return FALSE;
  else
    return TRUE;
}

static gboolean
clutter_geometry_progress (const GValue *a,
                           const GValue *b,
                           gdouble       progress,
                           GValue       *retval)
{
  const ClutterGeometry *a_geom = g_value_get_boxed (a);
  const ClutterGeometry *b_geom = g_value_get_boxed (b);
  ClutterGeometry res = { 0, };
  gint a_width = a_geom->width;
  gint b_width = b_geom->width;
  gint a_height = a_geom->height;
  gint b_height = b_geom->height;

  res.x = a_geom->x + (b_geom->x - a_geom->x) * progress;
  res.y = a_geom->y + (b_geom->y - a_geom->y) * progress;

  res.width = a_width + (b_width - a_width) * progress;
  res.height = a_height + (b_height - a_height) * progress;

  g_value_set_boxed (retval, &res);

  return TRUE;
}

G_DEFINE_BOXED_TYPE_WITH_CODE (ClutterGeometry, clutter_geometry,
                               clutter_geometry_copy,
                               clutter_geometry_free,
                               CLUTTER_REGISTER_INTERVAL_PROGRESS (clutter_geometry_progress));



/*
 * ClutterVertices
 */

/**
 * clutter_vertex_new:
 * @x: X coordinate
 * @y: Y coordinate
 * @z: Z coordinate
 *
 * Creates a new #ClutterVertex for the point in 3D space
 * identified by the 3 coordinates @x, @y, @z.
 *
 * This function is the logical equivalent of:
 *
 * |[
 *   clutter_vertex_init (clutter_vertex_alloc (), x, y, z);
 * ]|
 *
 * Return value: (transfer full): the newly allocated #ClutterVertex.
 *   Use clutter_vertex_free() to free the resources
 *
 * Since: 1.0
 */
ClutterVertex *
clutter_vertex_new (gfloat x,
                    gfloat y,
                    gfloat z)
{
  return clutter_vertex_init (clutter_vertex_alloc (), x, y, z);
}

/**
 * clutter_vertex_alloc: (constructor)
 *
 * Allocates a new, empty #ClutterVertex.
 *
 * Return value: (transfer full): the newly allocated #ClutterVertex.
 *   Use clutter_vertex_free() to free its resources
 *
 * Since: 1.12
 */
ClutterVertex *
clutter_vertex_alloc (void)
{
  return g_slice_new0 (ClutterVertex);
}

/**
 * clutter_vertex_init:
 * @vertex: a #ClutterVertex
 * @x: X coordinate
 * @y: Y coordinate
 * @z: Z coordinate
 *
 * Initializes @vertex with the given coordinates.
 *
 * Return value: (transfer none): the initialized #ClutterVertex
 *
 * Since: 1.10
 */
ClutterVertex *
clutter_vertex_init (ClutterVertex *vertex,
                     gfloat         x,
                     gfloat         y,
                     gfloat         z)
{
  g_return_val_if_fail (vertex != NULL, NULL);

  vertex->x = x;
  vertex->y = y;
  vertex->z = z;

  return vertex;
}

/**
 * clutter_vertex_copy:
 * @vertex: a #ClutterVertex
 *
 * Copies @vertex
 *
 * Return value: (transfer full): a newly allocated copy of #ClutterVertex.
 *   Use clutter_vertex_free() to free the allocated resources
 *
 * Since: 1.0
 */
ClutterVertex *
clutter_vertex_copy (const ClutterVertex *vertex)
{
  if (G_LIKELY (vertex != NULL))
    return g_slice_dup (ClutterVertex, vertex);

  return NULL;
}

/**
 * clutter_vertex_free:
 * @vertex: a #ClutterVertex
 *
 * Frees a #ClutterVertex allocated using clutter_vertex_alloc() or
 * clutter_vertex_copy().
 *
 * Since: 1.0
 */
void
clutter_vertex_free (ClutterVertex *vertex)
{
  if (G_UNLIKELY (vertex != NULL))
    g_slice_free (ClutterVertex, vertex);
}

/**
 * clutter_vertex_equal:
 * @vertex_a: a #ClutterVertex
 * @vertex_b: a #ClutterVertex
 *
 * Compares @vertex_a and @vertex_b for equality
 *
 * Return value: %TRUE if the passed #ClutterVertex are equal
 *
 * Since: 1.0
 */
gboolean
clutter_vertex_equal (const ClutterVertex *vertex_a,
                      const ClutterVertex *vertex_b)
{
  g_return_val_if_fail (vertex_a != NULL && vertex_b != NULL, FALSE);

  if (vertex_a == vertex_b)
    return TRUE;

  return fabsf (vertex_a->x - vertex_b->x) < FLOAT_EPSILON &&
         fabsf (vertex_a->y - vertex_b->y) < FLOAT_EPSILON &&
         fabsf (vertex_a->z - vertex_b->z) < FLOAT_EPSILON;
}

static void
clutter_vertex_interpolate (const ClutterVertex *a,
                            const ClutterVertex *b,
                            double               progress,
                            ClutterVertex       *res)
{
  res->x = a->x + (b->x - a->x) * progress;
  res->y = a->y + (b->y - a->y) * progress;
  res->z = a->z + (b->z - a->z) * progress;
}

static gboolean
clutter_vertex_progress (const GValue *a,
                         const GValue *b,
                         gdouble       progress,
                         GValue       *retval)
{
  const ClutterVertex *av = g_value_get_boxed (a);
  const ClutterVertex *bv = g_value_get_boxed (b);
  ClutterVertex res;

  clutter_vertex_interpolate (av, bv, progress, &res);

  g_value_set_boxed (retval, &res);

  return TRUE;
}

G_DEFINE_BOXED_TYPE_WITH_CODE (ClutterVertex, clutter_vertex,
                               clutter_vertex_copy,
                               clutter_vertex_free,
                               CLUTTER_REGISTER_INTERVAL_PROGRESS (clutter_vertex_progress));



/*
 * ClutterMargin
 */

/**
 * clutter_margin_new:
 *
 * Creates a new #ClutterMargin.
 *
 * Return value: (transfer full): a newly allocated #ClutterMargin. Use
 *   clutter_margin_free() to free the resources associated with it when
 *   done.
 *
 * Since: 1.10
 */
ClutterMargin *
clutter_margin_new (void)
{
  return g_slice_new0 (ClutterMargin);
}

/**
 * clutter_margin_copy:
 * @margin_: a #ClutterMargin
 *
 * Creates a new #ClutterMargin and copies the contents of @margin_ into
 * the newly created structure.
 *
 * Return value: (transfer full): a copy of the #ClutterMargin.
 *
 * Since: 1.10
 */
ClutterMargin *
clutter_margin_copy (const ClutterMargin *margin_)
{
  if (G_LIKELY (margin_ != NULL))
    return g_slice_dup (ClutterMargin, margin_);

  return NULL;
}

/**
 * clutter_margin_free:
 * @margin_: a #ClutterMargin
 *
 * Frees the resources allocated by clutter_margin_new() and
 * clutter_margin_copy().
 *
 * Since: 1.10
 */
void
clutter_margin_free (ClutterMargin *margin_)
{
  if (G_LIKELY (margin_ != NULL))
    g_slice_free (ClutterMargin, margin_);
}

G_DEFINE_BOXED_TYPE (ClutterMargin, clutter_margin,
                     clutter_margin_copy,
                     clutter_margin_free)



/*
 * ClutterPoint
 */

static const ClutterPoint _clutter_point_zero = CLUTTER_POINT_INIT_ZERO;

/**
 * clutter_point_zero:
 *
 * A point centered at (0, 0).
 *
 * The returned value can be used as a guard.
 *
 * Return value: a point centered in (0, 0); the returned #ClutterPoint
 *   is owned by Clutter and it should not be modified or freed.
 *
 * Since: 1.12
 */
const ClutterPoint *
clutter_point_zero (void)
{
  return &_clutter_point_zero;
}

/**
 * clutter_point_alloc: (constructor)
 *
 * Allocates a new #ClutterPoint.
 *
 * Return value: (transfer full): the newly allocated #ClutterPoint.
 *   Use clutter_point_free() to free its resources.
 *
 * Since: 1.12
 */
ClutterPoint *
clutter_point_alloc (void)
{
  return g_slice_new0 (ClutterPoint);
}

/**
 * clutter_point_init:
 * @point: a #ClutterPoint
 * @x: the X coordinate of the point
 * @y: the Y coordinate of the point
 *
 * Initializes @point with the given coordinates.
 *
 * Return value: (transfer none): the initialized #ClutterPoint
 *
 * Since: 1.12
 */
ClutterPoint *
clutter_point_init (ClutterPoint *point,
                    float         x,
                    float         y)
{
  g_return_val_if_fail (point != NULL, NULL);

  point->x = x;
  point->y = y;

  return point;
}

/**
 * clutter_point_copy:
 * @point: a #ClutterPoint
 *
 * Creates a new #ClutterPoint with the same coordinates of @point.
 *
 * Return value: (transfer full): a newly allocated #ClutterPoint.
 *   Use clutter_point_free() to free its resources.
 *
 * Since: 1.12
 */
ClutterPoint *
clutter_point_copy (const ClutterPoint *point)
{
  return g_slice_dup (ClutterPoint, point);
}

/**
 * clutter_point_free:
 * @point: a #ClutterPoint
 *
 * Frees the resources allocated for @point.
 *
 * Since: 1.12
 */
void
clutter_point_free (ClutterPoint *point)
{
  if (point != NULL && point != &_clutter_point_zero)
    g_slice_free (ClutterPoint, point);
}

/**
 * clutter_point_equals:
 * @a: the first #ClutterPoint to compare
 * @b: the second #ClutterPoint to compare
 *
 * Compares two #ClutterPoint for equality.
 *
 * Return value: %TRUE if the #ClutterPoints are equal
 *
 * Since: 1.12
 */
gboolean
clutter_point_equals (const ClutterPoint *a,
                      const ClutterPoint *b)
{
  if (a == b)
    return TRUE;

  if (a == NULL || b == NULL)
    return FALSE;

  return fabsf (a->x - b->x) < FLOAT_EPSILON &&
         fabsf (a->y - b->y) < FLOAT_EPSILON;
}

/**
 * clutter_point_distance:
 * @a: a #ClutterPoint
 * @b: a #ClutterPoint
 * @x_distance: (out) (allow-none): return location for the horizontal
 *   distance between the points
 * @y_distance: (out) (allow-none): return location for the vertical
 *   distance between the points
 *
 * Computes the distance between two #ClutterPoint.
 *
 * Return value: the distance between the points.
 *
 * Since: 1.12
 */
float
clutter_point_distance (const ClutterPoint *a,
                        const ClutterPoint *b,
                        float              *x_distance,
                        float              *y_distance)
{
  float x_d, y_d;

  g_return_val_if_fail (a != NULL, 0.f);
  g_return_val_if_fail (b != NULL, 0.f);

  if (clutter_point_equals (a, b))
    return 0.f;

  x_d = (a->x - b->x);
  y_d = (a->y - b->y);

  if (x_distance != NULL)
    *x_distance = fabsf (x_d);

  if (y_distance != NULL)
    *y_distance = fabsf (y_d);

  return sqrt ((x_d * x_d) + (y_d * y_d));
}

static gboolean
clutter_point_progress (const GValue *a,
                        const GValue *b,
                        gdouble       progress,
                        GValue       *retval)
{
  const ClutterPoint *ap = g_value_get_boxed (a);
  const ClutterPoint *bp = g_value_get_boxed (b);
  ClutterPoint res = CLUTTER_POINT_INIT (0, 0);

  res.x = ap->x + (bp->x - ap->x) * progress;
  res.y = ap->y + (bp->y - ap->y) * progress;

  g_value_set_boxed (retval, &res);

  return TRUE;
}

G_DEFINE_BOXED_TYPE_WITH_CODE (ClutterPoint, clutter_point,
                               clutter_point_copy,
                               clutter_point_free,
                               CLUTTER_REGISTER_INTERVAL_PROGRESS (clutter_point_progress))



/*
 * ClutterSize
 */

/**
 * clutter_size_alloc: (constructor)
 *
 * Allocates a new #ClutterSize.
 *
 * Return value: (transfer full): the newly allocated #ClutterSize.
 *   Use clutter_size_free() to free its resources.
 *
 * Since: 1.12
 */
ClutterSize *
clutter_size_alloc (void)
{
  return g_slice_new0 (ClutterSize);
}

/**
 * clutter_size_init:
 * @size: a #ClutterSize
 * @width: the width
 * @height: the height
 *
 * Initializes a #ClutterSize with the given dimensions.
 *
 * Return value: (transfer none): the initialized #ClutterSize
 *
 * Since: 1.12
 */
ClutterSize *
clutter_size_init (ClutterSize *size,
                   float        width,
                   float        height)
{
  g_return_val_if_fail (size != NULL, NULL);

  size->width = width;
  size->height = height;

  return size;
}

/**
 * clutter_size_copy:
 * @size: a #ClutterSize
 *
 * Creates a new #ClutterSize and duplicates @size.
 *
 * Return value: (transfer full): the newly allocated #ClutterSize.
 *   Use clutter_size_free() to free its resources.
 *
 * Since: 1.12
 */
ClutterSize *
clutter_size_copy (const ClutterSize *size)
{
  return g_slice_dup (ClutterSize, size);
}

/**
 * clutter_size_free:
 * @size: a #ClutterSize
 *
 * Frees the resources allocated for @size.
 *
 * Since: 1.12
 */
void
clutter_size_free (ClutterSize *size)
{
  if (size != NULL)
    g_slice_free (ClutterSize, size);
}

/**
 * clutter_size_equals:
 * @a: a #ClutterSize to compare
 * @b: a #ClutterSize to compare
 *
 * Compares two #ClutterSize for equality.
 *
 * Return value: %TRUE if the two #ClutterSize are equal
 *
 * Since: 1.12
 */
gboolean
clutter_size_equals (const ClutterSize *a,
                     const ClutterSize *b)
{
  if (a == b)
    return TRUE;

  if (a == NULL || b == NULL)
    return FALSE;

  return fabsf (a->width - b->width) < FLOAT_EPSILON &&
         fabsf (a->height - b->height) < FLOAT_EPSILON;
}

static gboolean
clutter_size_progress (const GValue *a,
                       const GValue *b,
                       gdouble       progress,
                       GValue       *retval)
{
  const ClutterSize *as = g_value_get_boxed (a);
  const ClutterSize *bs = g_value_get_boxed (b);
  ClutterSize res = CLUTTER_SIZE_INIT (0, 0);

  res.width = as->width + (bs->width - as->width) * progress;
  res.height = as->height + (bs->height - as->height) * progress;

  g_value_set_boxed (retval, &res);

  return TRUE;
}

G_DEFINE_BOXED_TYPE_WITH_CODE (ClutterSize, clutter_size,
                               clutter_size_copy,
                               clutter_size_free,
                               CLUTTER_REGISTER_INTERVAL_PROGRESS (clutter_size_progress))



/*
 * ClutterRect
 */

static const ClutterRect _clutter_rect_zero = CLUTTER_RECT_INIT_ZERO;

static gboolean clutter_rect_progress (const GValue *a,
                                       const GValue *b,
                                       gdouble       progress,
                                       GValue       *res);

G_DEFINE_BOXED_TYPE_WITH_CODE (ClutterRect, clutter_rect,
                               clutter_rect_copy,
                               clutter_rect_free,
                               CLUTTER_REGISTER_INTERVAL_PROGRESS (clutter_rect_progress))

static inline void
clutter_rect_normalize_internal (ClutterRect *rect)
{
  if (rect->size.width >= 0.f && rect->size.height >= 0.f)
    return;

  if (rect->size.width < 0.f)
    {
      float size = fabsf (rect->size.width);

      rect->origin.x -= size;
      rect->size.width = size;
    }

  if (rect->size.height < 0.f)
    {
      float size = fabsf (rect->size.height);

      rect->origin.y -= size;
      rect->size.height = size;
    }
}

/**
 * clutter_rect_zero:
 *
 * A #ClutterRect with #ClutterRect.origin set at (0, 0) and a size
 * of 0.
 *
 * The returned value can be used as a guard.
 *
 * Return value: a rectangle with origin in (0, 0) and a size of 0.
 *   The returned #ClutterRect is owned by Clutter and it should not
 *   be modified or freed.
 *
 * Since: 1.12
 */
const ClutterRect *
clutter_rect_zero (void)
{
  return &_clutter_rect_zero;
}

/**
 * clutter_rect_alloc: (constructor)
 *
 * Creates a new, empty #ClutterRect.
 *
 * You can use clutter_rect_init() to initialize the returned rectangle,
 * for instance:
 *
 * |[
 *   rect = clutter_rect_init (clutter_rect_alloc (), x, y, width, height);
 * ]|
 *
 * Return value: (transfer full): the newly allocated #ClutterRect.
 *   Use clutter_rect_free() to free its resources
 *
 * Since: 1.12
 */
ClutterRect *
clutter_rect_alloc (void)
{
  return g_slice_new0 (ClutterRect);
}

/**
 * clutter_rect_init:
 * @rect: a #ClutterRect
 * @x: X coordinate of the origin
 * @y: Y coordinate of the origin
 * @width: width of the rectangle
 * @height: height of the rectangle
 *
 * Initializes a #ClutterRect with the given origin and size.
 *
 * Return value: (transfer none): the updated rectangle
 *
 * Since: 1.12
 */
ClutterRect *
clutter_rect_init (ClutterRect *rect,
                   float        x,
                   float        y,
                   float        width,
                   float        height)
{
  g_return_val_if_fail (rect != NULL, NULL);

  rect->origin.x = x;
  rect->origin.y = y;

  rect->size.width = width;
  rect->size.height = height;

  return rect;
}

/**
 * clutter_rect_copy:
 * @rect: a #ClutterRect
 *
 * Copies @rect into a new #ClutterRect instance.
 *
 * Return value: (transfer full): the newly allocate copy of @rect.
 *   Use clutter_rect_free() to free the associated resources
 *
 * Since: 1.12
 */
ClutterRect *
clutter_rect_copy (const ClutterRect *rect)
{
  if (rect != NULL)
    {
      ClutterRect *res;

      res = g_slice_dup (ClutterRect, rect);
      clutter_rect_normalize_internal (res);

      return res;
    }

  return NULL;
}

/**
 * clutter_rect_free:
 * @rect: a #ClutterRect
 *
 * Frees the resources allocated by @rect.
 *
 * Since: 1.12
 */
void
clutter_rect_free (ClutterRect *rect)
{
  if (rect != NULL && rect != &_clutter_rect_zero)
    g_slice_free (ClutterRect, rect);
}

/**
 * clutter_rect_equals:
 * @a: a #ClutterRect
 * @b: a #ClutterRect
 *
 * Checks whether @a and @b are equals.
 *
 * This function will normalize both @a and @b before comparing
 * their origin and size.
 *
 * Return value: %TRUE if the rectangles match in origin and size.
 *
 * Since: 1.12
 */
gboolean
clutter_rect_equals (ClutterRect *a,
                     ClutterRect *b)
{
  if (a == b)
    return TRUE;

  if (a == NULL || b == NULL)
    return FALSE;

  clutter_rect_normalize_internal (a);
  clutter_rect_normalize_internal (b);

  return clutter_point_equals (&a->origin, &b->origin) &&
         clutter_size_equals (&a->size, &b->size);
}

/**
 * clutter_rect_normalize:
 * @rect: a #ClutterRect
 *
 * Normalizes a #ClutterRect.
 *
 * A #ClutterRect is defined by the area covered by its size; this means
 * that a #ClutterRect with #ClutterRect.origin in [ 0, 0 ] and a
 * #ClutterRect.size of [ 10, 10 ] is equivalent to a #ClutterRect with
 * #ClutterRect.origin in [ 10, 10 ] and a #ClutterRect.size of [ -10, -10 ].
 *
 * This function is useful to ensure that a rectangle has positive width
 * and height; it will modify the passed @rect and normalize its size.
 *
 * Since: 1.12
 */
ClutterRect *
clutter_rect_normalize (ClutterRect *rect)
{
  g_return_val_if_fail (rect != NULL, NULL);

  clutter_rect_normalize_internal (rect);

  return rect;
}

/**
 * clutter_rect_get_center:
 * @rect: a #ClutterRect
 * @center: (out caller-allocates): a #ClutterPoint
 *
 * Retrieves the center of @rect, after normalizing the rectangle,
 * and updates @center with the correct coordinates.
 *
 * Since: 1.12
 */
void
clutter_rect_get_center (ClutterRect  *rect,
                         ClutterPoint *center)
{
  g_return_if_fail (rect != NULL);
  g_return_if_fail (center != NULL);

  clutter_rect_normalize_internal (rect);

  center->x = rect->origin.x + (rect->size.width / 2.0f);
  center->y = rect->origin.y + (rect->size.height / 2.0f);
}

/**
 * clutter_rect_contains_point:
 * @rect: a #ClutterRect
 * @point: the point to check
 *
 * Checks whether @point is contained by @rect, after normalizing the
 * rectangle.
 *
 * Return value: %TRUE if the @point is contained by @rect.
 *
 * Since: 1.12
 */
gboolean
clutter_rect_contains_point (ClutterRect  *rect,
                             ClutterPoint *point)
{
  g_return_val_if_fail (rect != NULL, FALSE);
  g_return_val_if_fail (point != NULL, FALSE);

  clutter_rect_normalize_internal (rect);

  return (point->x >= rect->origin.x) &&
         (point->y >= rect->origin.y) &&
         (point->x <= (rect->origin.x + rect->size.width)) &&
         (point->y <= (rect->origin.y + rect->size.height));
}

/**
 * clutter_rect_contains_rect:
 * @a: a #ClutterRect
 * @b: a #ClutterRect
 *
 * Checks whether @a contains @b.
 *
 * The first rectangle contains the second if the union of the
 * two #ClutterRect is equal to the first rectangle.
 *
 * Return value: %TRUE if the first rectangle contains the second.
 *
 * Since: 1.12
 */
gboolean
clutter_rect_contains_rect (ClutterRect *a,
                            ClutterRect *b)
{
  ClutterRect res;

  g_return_val_if_fail (a != NULL, FALSE);
  g_return_val_if_fail (b != NULL, FALSE);

  clutter_rect_union (a, b, &res);

  return clutter_rect_equals (a, &res);
}

/**
 * clutter_rect_union:
 * @a: a #ClutterRect
 * @b: a #ClutterRect
 * @res: (out caller-allocates): a #ClutterRect
 *
 * Computes the smallest possible rectangle capable of fully containing
 * both @a and @b, and places it into @res.
 *
 * This function will normalize both @a and @b prior to computing their
 * union.
 *
 * Since: 1.12
 */
void
clutter_rect_union (ClutterRect *a,
                    ClutterRect *b,
                    ClutterRect *res)
{
  g_return_if_fail (a != NULL);
  g_return_if_fail (b != NULL);
  g_return_if_fail (res != NULL);

  clutter_rect_normalize_internal (a);
  clutter_rect_normalize_internal (b);

  res->origin.x = MIN (a->origin.x, b->origin.x);
  res->origin.y = MIN (a->origin.y, b->origin.y);

  res->size.width = MAX (a->size.width, b->size.width);
  res->size.height = MAX (a->size.height, b->size.height);
}

/**
 * clutter_rect_intersection:
 * @a: a #ClutterRect
 * @b: a #ClutterRect
 * @res: (out caller-allocates) (allow-none): a #ClutterRect, or %NULL
 *
 * Computes the intersection of @a and @b, and places it in @res, if @res
 * is not %NULL.
 *
 * This function will normalize both @a and @b prior to computing their
 * intersection.
 *
 * This function can be used to simply check if the intersection of @a and @b
 * is not empty, by using %NULL for @res.
 *
 * Return value: %TRUE if the intersection of @a and @b is not empty
 *
 * Since: 1.12
 */
gboolean
clutter_rect_intersection (ClutterRect *a,
                           ClutterRect *b,
                           ClutterRect *res)
{
  float x_1, y_1, x_2, y_2;

  g_return_val_if_fail (a != NULL, FALSE);
  g_return_val_if_fail (b != NULL, FALSE);

  clutter_rect_normalize_internal (a);
  clutter_rect_normalize_internal (b);

  x_1 = MAX (a->origin.x, b->origin.x);
  y_1 = MAX (a->origin.y, b->origin.y);
  x_2 = MIN (a->origin.x + a->size.width, b->origin.x + b->size.width);
  y_2 = MIN (a->origin.y + a->size.height, b->origin.y + b->size.height);

  if (x_1 >= x_2 || y_1 >= y_2)
    {
      if (res != NULL)
        clutter_rect_init (res, 0.f, 0.f, 0.f, 0.f);

      return FALSE;
    }

  if (res != NULL)
    clutter_rect_init (res, x_1, y_1, x_2 - x_1, y_2 - y_1);

  return TRUE;
}

/**
 * clutter_rect_offset:
 * @rect: a #ClutterRect
 * @d_x: the horizontal offset value
 * @d_y: the vertical offset value
 *
 * Offsets the origin of @rect by the given values, after normalizing
 * the rectangle.
 *
 * Since: 1.12
 */
void
clutter_rect_offset (ClutterRect *rect,
                     float        d_x,
                     float        d_y)
{
  g_return_if_fail (rect != NULL);

  clutter_rect_normalize_internal (rect);

  rect->origin.x += d_x;
  rect->origin.y += d_y;
}

/**
 * clutter_rect_inset:
 * @rect: a #ClutterRect
 * @d_x: an horizontal value; a positive @d_x will create an inset rectangle,
 *   and a negative value will create a larger rectangle
 * @d_y: a vertical value; a positive @d_x will create an inset rectangle,
 *   and a negative value will create a larger rectangle
 *
 * Normalizes the @rect and offsets its origin by the @d_x and @d_y values;
 * the size is adjusted by (2 * @d_x, 2 * @d_y).
 *
 * If @d_x and @d_y are positive the size of the rectangle is decreased; if
 * the values are negative, the size of the rectangle is increased.
 *
 * If the resulting rectangle has a negative width or height, the size is
 * set to 0.
 *
 * Since: 1.12
 */
void
clutter_rect_inset (ClutterRect *rect,
                    float        d_x,
                    float        d_y)
{
  g_return_if_fail (rect != NULL);

  clutter_rect_normalize_internal (rect);

  rect->origin.x += d_x;
  rect->origin.y += d_y;

  if (d_x >= 0.f)
    rect->size.width -= (d_x * 2.f);
  else
    rect->size.width += (d_x * -2.f);

  if (d_y >= 0.f)
    rect->size.height -= (d_y * 2.f);
  else
    rect->size.height += (d_y * -2.f);

  if (rect->size.width < 0.f)
    rect->size.width = 0.f;

  if (rect->size.height < 0.f)
    rect->size.height = 0.f;
}

/**
 * clutter_rect_clamp_to_pixel:
 * @rect: a #ClutterRect
 *
 * Rounds the origin of @rect downwards to the nearest integer, and rounds
 * the size of @rect upwards to the nearest integer, so that @rect is
 * updated to the smallest rectangle capable of fully containing the
 * original, fractional rectangle.
 *
 * Since: 1.12
 */
void
clutter_rect_clamp_to_pixel (ClutterRect *rect)
{
  g_return_if_fail (rect != NULL);

  clutter_rect_normalize_internal (rect);

  rect->origin.x = floorf (rect->origin.x);
  rect->origin.y = floorf (rect->origin.y);

  rect->size.width = ceilf (rect->size.width);
  rect->size.height = ceilf (rect->size.height);
}

/**
 * clutter_rect_get_x:
 * @rect: a #ClutterRect
 *
 * Retrieves the X coordinate of the origin of @rect.
 *
 * Return value: the X coordinate of the origin of the rectangle
 *
 * Since: 1.12
 */
float
clutter_rect_get_x (ClutterRect *rect)
{
  g_return_val_if_fail (rect != NULL, 0.f);

  clutter_rect_normalize_internal (rect);

  return rect->origin.x;
}

/**
 * clutter_rect_get_y:
 * @rect: a #ClutterRect
 *
 * Retrieves the Y coordinate of the origin of @rect.
 *
 * Return value: the Y coordinate of the origin of the rectangle
 *
 * Since: 1.12
 */
float
clutter_rect_get_y (ClutterRect *rect)
{
  g_return_val_if_fail (rect != NULL, 0.f);

  clutter_rect_normalize_internal (rect);

  return rect->origin.y;
}

/**
 * clutter_rect_get_width:
 * @rect: a #ClutterRect
 *
 * Retrieves the width of @rect.
 *
 * Return value: the width of the rectangle
 *
 * Since: 1.12
 */
float
clutter_rect_get_width (ClutterRect *rect)
{
  g_return_val_if_fail (rect != NULL, 0.f);

  clutter_rect_normalize_internal (rect);

  return rect->size.width;
}

/**
 * clutter_rect_get_height:
 * @rect: a #ClutterRect
 *
 * Retrieves the height of @rect.
 *
 * Return value: the height of the rectangle
 *
 * Since: 1.12
 */
float
clutter_rect_get_height (ClutterRect *rect)
{
  g_return_val_if_fail (rect != NULL, 0.f);

  clutter_rect_normalize_internal (rect);

  return rect->size.height;
}

static gboolean
clutter_rect_progress (const GValue *a,
                       const GValue *b,
                       gdouble       progress,
                       GValue       *retval)
{
  const ClutterRect *rect_a = g_value_get_boxed (a);
  const ClutterRect *rect_b = g_value_get_boxed (b);
  ClutterRect res = CLUTTER_RECT_INIT_ZERO;

#define INTERPOLATE(r_a,r_b,member,field,factor)     ((r_a)->member.field + (((r_b)->member.field - ((r_a)->member.field)) * (factor)))

  res.origin.x = INTERPOLATE (rect_a, rect_b, origin, x, progress);
  res.origin.y = INTERPOLATE (rect_a, rect_b, origin, y, progress);

  res.size.width = INTERPOLATE (rect_a, rect_b, size, width, progress);
  res.size.height = INTERPOLATE (rect_a, rect_b, size, height, progress);

#undef INTERPOLATE

  g_value_set_boxed (retval, &res);

  return TRUE;
}

/**
 * ClutterMatrix:
 *
 * A type representing a 4x4 matrix.
 *
 * It is identicaly to #CoglMatrix.
 *
 * Since: 1.12
 */

static gpointer
clutter_matrix_copy (gpointer data)
{
  return cogl_matrix_copy (data);
}

static gboolean
clutter_matrix_progress (const GValue *a,
                         const GValue *b,
                         gdouble       progress,
                         GValue       *retval)
{
  const ClutterMatrix *matrix1 = g_value_get_boxed (a);
  const ClutterMatrix *matrix2 = g_value_get_boxed (b);
  ClutterVertex scale1 = CLUTTER_VERTEX_INIT (1.f, 1.f, 1.f);
  float shear1[3] = { 0.f, 0.f, 0.f };
  ClutterVertex rotate1 = CLUTTER_VERTEX_INIT_ZERO;
  ClutterVertex translate1 = CLUTTER_VERTEX_INIT_ZERO;
  ClutterVertex4 perspective1 = { 0.f, 0.f, 0.f, 0.f };
  ClutterVertex scale2 = CLUTTER_VERTEX_INIT (1.f, 1.f, 1.f);
  float shear2[3] = { 0.f, 0.f, 0.f };
  ClutterVertex rotate2 = CLUTTER_VERTEX_INIT_ZERO;
  ClutterVertex translate2 = CLUTTER_VERTEX_INIT_ZERO;
  ClutterVertex4 perspective2 = { 0.f, 0.f, 0.f, 0.f };
  ClutterVertex scale_res = CLUTTER_VERTEX_INIT (1.f, 1.f, 1.f);
  float shear_res = 0.f;
  ClutterVertex rotate_res = CLUTTER_VERTEX_INIT_ZERO;
  ClutterVertex translate_res = CLUTTER_VERTEX_INIT_ZERO;
  ClutterVertex4 perspective_res = { 0.f, 0.f, 0.f, 0.f };
  ClutterMatrix res;

  clutter_matrix_init_identity (&res);

  _clutter_util_matrix_decompose (matrix1,
                                  &scale1, shear1, &rotate1, &translate1,
                                  &perspective1);
  _clutter_util_matrix_decompose (matrix2,
                                  &scale2, shear2, &rotate2, &translate2,
                                  &perspective2);

  /* perspective */
  _clutter_util_vertex4_interpolate (&perspective1, &perspective2, progress, &perspective_res);
  res.wx = perspective_res.x;
  res.wy = perspective_res.y;
  res.wz = perspective_res.z;
  res.ww = perspective_res.w;

  /* translation */
  clutter_vertex_interpolate (&translate1, &translate2, progress, &translate_res);
  cogl_matrix_translate (&res, translate_res.x, translate_res.y, translate_res.z);

  /* rotation */
  clutter_vertex_interpolate (&rotate1, &rotate2, progress, &rotate_res);
  cogl_matrix_rotate (&res, rotate_res.x, 1.0f, 0.0f, 0.0f);
  cogl_matrix_rotate (&res, rotate_res.y, 0.0f, 1.0f, 0.0f);
  cogl_matrix_rotate (&res, rotate_res.z, 0.0f, 0.0f, 1.0f);

  /* skew */
  shear_res = shear1[2] + (shear2[2] - shear1[2]) * progress; /* YZ */
  if (shear_res != 0.f)
    _clutter_util_matrix_skew_yz (&res, shear_res);

  shear_res = shear1[1] + (shear2[1] - shear1[1]) * progress; /* XZ */
  if (shear_res != 0.f)
    _clutter_util_matrix_skew_xz (&res, shear_res);

  shear_res = shear1[0] + (shear2[0] - shear1[0]) * progress; /* XY */
  if (shear_res != 0.f)
    _clutter_util_matrix_skew_xy (&res, shear_res);

  /* scale */
  clutter_vertex_interpolate (&scale1, &scale2, progress, &scale_res);
  cogl_matrix_scale (&res, scale_res.x, scale_res.y, scale_res.z);

  g_value_set_boxed (retval, &res);

  return TRUE;
}

G_DEFINE_BOXED_TYPE_WITH_CODE (ClutterMatrix, clutter_matrix,
                               clutter_matrix_copy,
                               clutter_matrix_free,
                               CLUTTER_REGISTER_INTERVAL_PROGRESS (clutter_matrix_progress))

/**
 * clutter_matrix_alloc:
 *
 * Allocates enough memory to hold a #ClutterMatrix.
 *
 * Return value: (transfer full): the newly allocated #ClutterMatrix
 *
 * Since: 1.12
 */
ClutterMatrix *
clutter_matrix_alloc (void)
{
  return g_new0 (ClutterMatrix, 1);
}

/**
 * clutter_matrix_free:
 * @matrix: (allow-none): a #ClutterMatrix
 *
 * Frees the memory allocated by clutter_matrix_alloc().
 *
 * Since: 1.12
 */
void
clutter_matrix_free (ClutterMatrix *matrix)
{
  cogl_matrix_free (matrix);
}

/**
 * clutter_matrix_init_identity:
 * @matrix: a #ClutterMatrix
 *
 * Initializes @matrix with the identity matrix, i.e.:
 *
 * |[
 *   .xx = 1.0, .xy = 0.0, .xz = 0.0, .xw = 0.0
 *   .yx = 0.0, .yy = 1.0, .yz = 0.0, .yw = 0.0
 *   .zx = 0.0, .zy = 0.0, .zz = 1.0, .zw = 0.0
 *   .wx = 0.0, .wy = 0.0, .wz = 0.0, .ww = 1.0
 * ]|
 *
 * Return value: (transfer none): the initialized #ClutterMatrix
 *
 * Since: 1.12
 */
ClutterMatrix *
clutter_matrix_init_identity (ClutterMatrix *matrix)
{
  cogl_matrix_init_identity (matrix);

  return matrix;
}

/**
 * clutter_matrix_init_from_array:
 * @matrix: a #ClutterMatrix
 * @values: (array fixed-size=16): a C array of 16 floating point values,
 *   representing a 4x4 matrix, with column-major order
 *
 * Initializes @matrix with the contents of a C array of floating point
 * values.
 *
 * Return value: (transfer none): the initialzed #ClutterMatrix
 *
 * Since: 1.12
 */
ClutterMatrix *
clutter_matrix_init_from_array (ClutterMatrix *matrix,
                                const float    values[16])
{
  cogl_matrix_init_from_array (matrix, values);

  return matrix;
}

/**
 * clutter_matrix_init_from_matrix:
 * @a: the #ClutterMatrix to initialize
 * @b: the #ClutterMatrix to copy
 *
 * Initializes the #ClutterMatrix @a with the contents of the
 * #ClutterMatrix @b.
 *
 * Return value: (transfer none): the initialized #ClutterMatrix
 *
 * Since: 1.12
 */
ClutterMatrix *
clutter_matrix_init_from_matrix (ClutterMatrix       *a,
                                 const ClutterMatrix *b)
{
  return memcpy (a, b, sizeof (ClutterMatrix));
}
