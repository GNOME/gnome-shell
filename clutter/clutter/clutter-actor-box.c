#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "clutter-types.h"
#include "clutter-interval.h"
#include "clutter-private.h"

/**
 * clutter_actor_box_new:
 * @x_1: X coordinate of the top left point
 * @y_1: Y coordinate of the top left point
 * @x_2: X coordinate of the bottom right point
 * @y_2: Y coordinate of the bottom right point
 *
 * Allocates a new #ClutterActorBox using the passed coordinates
 * for the top left and bottom right points.
 *
 * This function is the logical equivalent of:
 *
 * |[
 *   clutter_actor_box_init (clutter_actor_box_alloc (),
 *                           x_1, y_1,
 *                           x_2, y_2);
 * ]|
 *
 * Return value: (transfer full): the newly allocated #ClutterActorBox.
 *   Use clutter_actor_box_free() to free the resources
 *
 * Since: 1.0
 */
ClutterActorBox *
clutter_actor_box_new (gfloat x_1,
                       gfloat y_1,
                       gfloat x_2,
                       gfloat y_2)
{
  return clutter_actor_box_init (clutter_actor_box_alloc (),
                                 x_1, y_1,
                                 x_2, y_2);
}

/**
 * clutter_actor_box_alloc:
 *
 * Allocates a new #ClutterActorBox.
 *
 * Return value: (transfer full): the newly allocated #ClutterActorBox.
 *   Use clutter_actor_box_free() to free its resources
 *
 * Since: 1.12
 */
ClutterActorBox *
clutter_actor_box_alloc (void)
{
  return g_slice_new0 (ClutterActorBox);
}

/**
 * clutter_actor_box_init:
 * @box: a #ClutterActorBox
 * @x_1: X coordinate of the top left point
 * @y_1: Y coordinate of the top left point
 * @x_2: X coordinate of the bottom right point
 * @y_2: Y coordinate of the bottom right point
 *
 * Initializes @box with the given coordinates.
 *
 * Return value: (transfer none): the initialized #ClutterActorBox
 *
 * Since: 1.10
 */
ClutterActorBox *
clutter_actor_box_init (ClutterActorBox *box,
                        gfloat           x_1,
                        gfloat           y_1,
                        gfloat           x_2,
                        gfloat           y_2)
{
  g_return_val_if_fail (box != NULL, NULL);

  box->x1 = x_1;
  box->y1 = y_1;
  box->x2 = x_2;
  box->y2 = y_2;

  return box;
}

/**
 * clutter_actor_box_init_rect:
 * @box: a #ClutterActorBox
 * @x: X coordinate of the origin
 * @y: Y coordinate of the origin
 * @width: width of the box
 * @height: height of the box
 *
 * Initializes @box with the given origin and size.
 *
 * Since: 1.10
 */
void
clutter_actor_box_init_rect (ClutterActorBox *box,
                             gfloat           x,
                             gfloat           y,
                             gfloat           width,
                             gfloat           height)
{
  g_return_if_fail (box != NULL);

  box->x1 = x;
  box->y1 = y;
  box->x2 = box->x1 + width;
  box->y2 = box->y1 + height;
}

/**
 * clutter_actor_box_copy:
 * @box: a #ClutterActorBox
 *
 * Copies @box
 *
 * Return value: a newly allocated copy of #ClutterActorBox. Use
 *   clutter_actor_box_free() to free the allocated resources
 *
 * Since: 1.0
 */
ClutterActorBox *
clutter_actor_box_copy (const ClutterActorBox *box)
{
  if (G_LIKELY (box != NULL))
    return g_slice_dup (ClutterActorBox, box);

  return NULL;
}

/**
 * clutter_actor_box_free:
 * @box: a #ClutterActorBox
 *
 * Frees a #ClutterActorBox allocated using clutter_actor_box_new()
 * or clutter_actor_box_copy()
 *
 * Since: 1.0
 */
void
clutter_actor_box_free (ClutterActorBox *box)
{
  if (G_LIKELY (box != NULL))
    g_slice_free (ClutterActorBox, box);
}

/**
 * clutter_actor_box_equal:
 * @box_a: a #ClutterActorBox
 * @box_b: a #ClutterActorBox
 *
 * Checks @box_a and @box_b for equality
 *
 * Return value: %TRUE if the passed #ClutterActorBox are equal
 *
 * Since: 1.0
 */
gboolean
clutter_actor_box_equal (const ClutterActorBox *box_a,
                         const ClutterActorBox *box_b)
{
  g_return_val_if_fail (box_a != NULL && box_b != NULL, FALSE);

  if (box_a == box_b)
    return TRUE;

  return box_a->x1 == box_b->x1 && box_a->y1 == box_b->y1 &&
         box_a->x2 == box_b->x2 && box_a->y2 == box_b->y2;
}

/**
 * clutter_actor_box_get_x:
 * @box: a #ClutterActorBox
 *
 * Retrieves the X coordinate of the origin of @box
 *
 * Return value: the X coordinate of the origin
 *
 * Since: 1.0
 */
gfloat
clutter_actor_box_get_x (const ClutterActorBox *box)
{
  g_return_val_if_fail (box != NULL, 0.);

  return box->x1;
}

/**
 * clutter_actor_box_get_y:
 * @box: a #ClutterActorBox
 *
 * Retrieves the Y coordinate of the origin of @box
 *
 * Return value: the Y coordinate of the origin
 *
 * Since: 1.0
 */
gfloat
clutter_actor_box_get_y (const ClutterActorBox *box)
{
  g_return_val_if_fail (box != NULL, 0.);

  return box->y1;
}

/**
 * clutter_actor_box_get_width:
 * @box: a #ClutterActorBox
 *
 * Retrieves the width of the @box
 *
 * Return value: the width of the box
 *
 * Since: 1.0
 */
gfloat
clutter_actor_box_get_width (const ClutterActorBox *box)
{
  g_return_val_if_fail (box != NULL, 0.);

  return box->x2 - box->x1;
}

/**
 * clutter_actor_box_get_height:
 * @box: a #ClutterActorBox
 *
 * Retrieves the height of the @box
 *
 * Return value: the height of the box
 *
 * Since: 1.0
 */
gfloat
clutter_actor_box_get_height (const ClutterActorBox *box)
{
  g_return_val_if_fail (box != NULL, 0.);

  return box->y2 - box->y1;
}

/**
 * clutter_actor_box_get_origin:
 * @box: a #ClutterActorBox
 * @x: (out) (allow-none): return location for the X coordinate, or %NULL
 * @y: (out) (allow-none): return location for the Y coordinate, or %NULL
 *
 * Retrieves the origin of @box
 *
 * Since: 1.0
 */
void
clutter_actor_box_get_origin (const ClutterActorBox *box,
                              gfloat                *x,
                              gfloat                *y)
{
  g_return_if_fail (box != NULL);

  if (x)
    *x = box->x1;

  if (y)
    *y = box->y1;
}

/**
 * clutter_actor_box_get_size:
 * @box: a #ClutterActorBox
 * @width: (out) (allow-none): return location for the width, or %NULL
 * @height: (out) (allow-none): return location for the height, or %NULL
 *
 * Retrieves the size of @box
 *
 * Since: 1.0
 */
void
clutter_actor_box_get_size (const ClutterActorBox *box,
                            gfloat                *width,
                            gfloat                *height)
{
  g_return_if_fail (box != NULL);

  if (width)
    *width = box->x2 - box->x1;

  if (height)
    *height = box->y2 - box->y1;
}

/**
 * clutter_actor_box_get_area:
 * @box: a #ClutterActorBox
 *
 * Retrieves the area of @box
 *
 * Return value: the area of a #ClutterActorBox, in pixels
 *
 * Since: 1.0
 */
gfloat
clutter_actor_box_get_area (const ClutterActorBox *box)
{
  g_return_val_if_fail (box != NULL, 0.);

  return (box->x2 - box->x1) * (box->y2 - box->y1);
}

/**
 * clutter_actor_box_contains:
 * @box: a #ClutterActorBox
 * @x: X coordinate of the point
 * @y: Y coordinate of the point
 *
 * Checks whether a point with @x, @y coordinates is contained
 * withing @box
 *
 * Return value: %TRUE if the point is contained by the #ClutterActorBox
 *
 * Since: 1.0
 */
gboolean
clutter_actor_box_contains (const ClutterActorBox *box,
                            gfloat                 x,
                            gfloat                 y)
{
  g_return_val_if_fail (box != NULL, FALSE);

  return (x > box->x1 && x < box->x2) &&
         (y > box->y1 && y < box->y2);
}

/**
 * clutter_actor_box_from_vertices:
 * @box: a #ClutterActorBox
 * @verts: (array fixed-size=4): array of four #ClutterVertex
 *
 * Calculates the bounding box represented by the four vertices; for details
 * of the vertex array see clutter_actor_get_abs_allocation_vertices().
 *
 * Since: 1.0
 */
void
clutter_actor_box_from_vertices (ClutterActorBox     *box,
                                 const ClutterVertex  verts[])
{
  gfloat x_1, x_2, y_1, y_2;

  g_return_if_fail (box != NULL);
  g_return_if_fail (verts != NULL);

  /* 4-way min/max */
  x_1 = verts[0].x;
  y_1 = verts[0].y;

  if (verts[1].x < x_1)
    x_1 = verts[1].x;

  if (verts[2].x < x_1)
    x_1 = verts[2].x;

  if (verts[3].x < x_1)
    x_1 = verts[3].x;

  if (verts[1].y < y_1)
    y_1 = verts[1].y;

  if (verts[2].y < y_1)
    y_1 = verts[2].y;

  if (verts[3].y < y_1)
    y_1 = verts[3].y;

  x_2 = verts[0].x;
  y_2 = verts[0].y;

  if (verts[1].x > x_2)
    x_2 = verts[1].x;

  if (verts[2].x > x_2)
    x_2 = verts[2].x;

  if (verts[3].x > x_2)
    x_2 = verts[3].x;

  if (verts[1].y > y_2)
    y_2 = verts[1].y;

  if (verts[2].y > y_2)
    y_2 = verts[2].y;

  if (verts[3].y > y_2)
    y_2 = verts[3].y;

  box->x1 = x_1;
  box->x2 = x_2;
  box->y1 = y_1;
  box->y2 = y_2;
}

/**
 * clutter_actor_box_interpolate:
 * @initial: the initial #ClutterActorBox
 * @final: the final #ClutterActorBox
 * @progress: the interpolation progress
 * @result: (out): return location for the interpolation
 *
 * Interpolates between @initial and @final #ClutterActorBox<!-- -->es
 * using @progress
 *
 * Since: 1.2
 */
void
clutter_actor_box_interpolate (const ClutterActorBox *initial,
                               const ClutterActorBox *final,
                               gdouble                progress,
                               ClutterActorBox       *result)
{
  g_return_if_fail (initial != NULL);
  g_return_if_fail (final != NULL);
  g_return_if_fail (result != NULL);

  result->x1 = initial->x1 + (final->x1 - initial->x1) * progress;
  result->y1 = initial->y1 + (final->y1 - initial->y1) * progress;
  result->x2 = initial->x2 + (final->x2 - initial->x2) * progress;
  result->y2 = initial->y2 + (final->y2 - initial->y2) * progress;
}

/**
 * clutter_actor_box_clamp_to_pixel:
 * @box: (inout): the #ClutterActorBox to clamp
 *
 * Clamps the components of @box to the nearest integer
 *
 * Since: 1.2
 */
void
clutter_actor_box_clamp_to_pixel (ClutterActorBox *box)
{
  g_return_if_fail (box != NULL);

  box->x1 = floorf (box->x1);
  box->y1 = floorf (box->y1);
  box->x2 = ceilf (box->x2);
  box->y2 = ceilf (box->y2);
}

/**
 * clutter_actor_box_union:
 * @a: (in): the first #ClutterActorBox
 * @b: (in): the second #ClutterActorBox
 * @result: (out): the #ClutterActorBox representing a union
 *   of @a and @b
 *
 * Unions the two boxes @a and @b and stores the result in @result.
 *
 * Since: 1.4
 */
void
clutter_actor_box_union (const ClutterActorBox *a,
                         const ClutterActorBox *b,
                         ClutterActorBox       *result)
{
  g_return_if_fail (a != NULL);
  g_return_if_fail (b != NULL);
  g_return_if_fail (result != NULL);

  result->x1 = MIN (a->x1, b->x1);
  result->y1 = MIN (a->y1, b->y1);

  result->x2 = MAX (a->x2, b->x2);
  result->y2 = MAX (a->y2, b->y2);
}

static gboolean
clutter_actor_box_progress (const GValue *a,
                            const GValue *b,
                            gdouble       factor,
                            GValue       *retval)
{
  ClutterActorBox res = { 0, };

  clutter_actor_box_interpolate (g_value_get_boxed (a),
                                 g_value_get_boxed (b),
                                 factor,
                                 &res);

  g_value_set_boxed (retval, &res);

  return TRUE;
}

/**
 * clutter_actor_box_set_origin:
 * @box: a #ClutterActorBox
 * @x: the X coordinate of the new origin
 * @y: the Y coordinate of the new origin
 *
 * Changes the origin of @box, maintaining the size of the #ClutterActorBox.
 *
 * Since: 1.6
 */
void
clutter_actor_box_set_origin (ClutterActorBox *box,
                              gfloat           x,
                              gfloat           y)
{
  gfloat width, height;

  g_return_if_fail (box != NULL);

  width = box->x2 - box->x1;
  height = box->y2 - box->y1;

  clutter_actor_box_init_rect (box, x, y, width, height);
}

/**
 * clutter_actor_box_set_size:
 * @box: a #ClutterActorBox
 * @width: the new width
 * @height: the new height
 *
 * Sets the size of @box, maintaining the origin of the #ClutterActorBox.
 *
 * Since: 1.6
 */
void
clutter_actor_box_set_size (ClutterActorBox *box,
                            gfloat           width,
                            gfloat           height)
{
  g_return_if_fail (box != NULL);

  box->x2 = box->x1 + width;
  box->y2 = box->y1 + height;
}

G_DEFINE_BOXED_TYPE_WITH_CODE (ClutterActorBox, clutter_actor_box,
                               clutter_actor_box_copy,
                               clutter_actor_box_free,
                               CLUTTER_REGISTER_INTERVAL_PROGRESS (clutter_actor_box_progress));
