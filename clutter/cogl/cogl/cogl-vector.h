/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_VECTOR_H
#define __COGL_VECTOR_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * SECTION:cogl-vector
 * @short_description: Functions for handling single precision float
 *                     vectors.
 *
 * This exposes a utility API that can be used for basic manipulation of 3
 * component float vectors.
 */

/* All of the cogl-vector API is currently experimental so we
 * suffix the actual symbols with _EXP so if somone is monitoring for
 * ABI changes it will hopefully be clearer to them what's going on if
 * any of the symbols dissapear at a later date.
 */
#define cogl_vector3_init cogl_vector3_init_EXP
#define cogl_vector3_init_zero cogl_vector3_init_zero_EXP
#define cogl_vector3_equal cogl_vector3_equal_EXP
#define cogl_vector3_equal_with_epsilon cogl_vector3_equal_with_epsilon_EXP
#define cogl_vector3_copy cogl_vector3_copy_EXP
#define cogl_vector3_free cogl_vector3_free_EXP
#define cogl_vector3_invert cogl_vector3_invert_EXP
#define cogl_vector3_add cogl_vector3_add_EXP
#define cogl_vector3_subtract cogl_vector3_subtract_EXP
#define cogl_vector3_multiply_scalar cogl_vector3_multiply_scalar_EXP
#define cogl_vector3_divide_scalar cogl_vector3_divide_scalar_EXP
#define cogl_vector3_normalize cogl_vector3_normalize_EXP
#define cogl_vector3_magnitude cogl_vector3_magnitude_EXP
#define cogl_vector3_cross_product cogl_vector3_cross_product_EXP
#define cogl_vector3_dot_product cogl_vector3_dot_product_EXP
#define cogl_vector3_distance cogl_vector3_distance_EXP

typedef struct
{
  /* FIXME: add sse alignment constraint? */
  float x;
  float y;
  float z;
} CoglVector3;

#if 0
typedef struct
{
  /* FIXME: add sse alignment constraint? */
  float x;
  float y;
  float z;
  float w;
} CoglVector4;
#endif

/**
 * cogl_vector3_init:
 * @vector: The CoglVector3 you want to initialize
 * @x: The x component
 * @y: The y component
 * @z: The z component
 *
 * Initializes a 3 component, single precision float vector which can
 * then be manipulated with the cogl_vector convenience APIs. Vectors
 * can also be used in places where a "point" is often desired.
 *
 * Since: 1.4
 * Stability: Unstable
 */
void
cogl_vector3_init (CoglVector3 *vector, float x, float y, float z);

/**
 * cogl_vector3_init_zero:
 * @vector: The CoglVector3 you want to initialize
 *
 * Initializes a 3 component, single precision float vector with zero
 * for each component.
 *
 * Since: 1.4
 * Stability: Unstable
 */
void
cogl_vector3_init_zero (CoglVector3 *vector);

/**
 * cogl_vector3_equal:
 * @v1: The first CoglVector3 you want to compare
 * @v2: The second CoglVector3 you want to compare
 *
 * Compares the components of two vectors and returns TRUE if they are
 * the same.
 *
 * The comparison of the components is done with the '==' operator
 * such that -0 is considered equal to 0, but otherwise there is no
 * fuzziness such as an epsilon to consider vectors that are
 * essentially identical except for some minor precision error
 * differences due to the way they have been manipulated.
 *
 * Returns: TRUE if the vectors are equal else FALSE.
 *
 * Since: 1.4
 * Stability: Unstable
 */
gboolean
cogl_vector3_equal (gconstpointer v1, gconstpointer v2);

/**
 * cogl_vector3_equal_with_epsilon:
 * @vector0: The first CoglVector3 you want to compare
 * @vector1: The second CoglVector3 you want to compare
 * @epsilon: The allowable difference between components to still be
 *           considered equal
 *
 * Compares the components of two vectors using the given epsilon and
 * returns TRUE if they are the same, using an internal epsilon for
 * comparing the floats.
 *
 * Each component is compared against the epsilon value in this way:
 * |[
 *   if (fabsf (vector0->x - vector1->x) < epsilon)
 * ]|
 *
 * Returns: TRUE if the vectors are equal else FALSE.
 *
 * Since: 1.4
 * Stability: Unstable
 */
gboolean
cogl_vector3_equal_with_epsilon (const CoglVector3 *vector0,
                                 const CoglVector3 *vector1,
                                 float epsilon);

/**
 * cogl_vector3_copy:
 * @vector: The CoglVector3 you want to copy
 *
 * Allocates a new #CoglVector3 structure on the heap initializing the
 * components from the given @vector and returns a pointer to the newly
 * allocated vector. You should free the memory using
 * cogl_vector3_free()
 *
 * Returns: A newly allocated #CoglVector3.
 *
 * Since: 1.4
 * Stability: Unstable
 */
CoglVector3 *
cogl_vector3_copy (const CoglVector3 *vector);

/**
 * cogl_vector3_free:
 * @vector: The CoglVector3 you want to free
 *
 * Frees a #CoglVector3 that was previously allocated with
 * cogl_vector_copy()
 *
 * Since: 1.4
 * Stability: Unstable
 */
void
cogl_vector3_free (CoglVector3 *vector);

/**
 * cogl_vector3_invert:
 * @vector: The CoglVector3 you want to manipulate
 *
 * Inverts/negates all the components of the given @vector.
 *
 * Since: 1.4
 * Stability: Unstable
 */
void
cogl_vector3_invert (CoglVector3 *vector);

/**
 * cogl_vector3_add:
 * @result: Where you want the result written
 * @a: The first vector operand
 * @b: The second vector operand
 *
 * Adds each of the corresponding components in vectors @a and @b
 * storing the results in @result.
 *
 * Since: 1.4
 * Stability: Unstable
 */
void
cogl_vector3_add (CoglVector3 *result,
                  const CoglVector3 *a,
                  const CoglVector3 *b);

/**
 * cogl_vector3_subtract:
 * @result: Where you want the result written
 * @a: The first vector operand
 * @b: The second vector operand
 *
 * Subtracts each of the corresponding components in vector @b from
 * @a storing the results in @result.
 *
 * Since: 1.4
 * Stability: Unstable
 */
void
cogl_vector3_subtract (CoglVector3 *result,
                       const CoglVector3 *a,
                       const CoglVector3 *b);

/**
 * cogl_vector3_multiply_scalar:
 * @vector: The CoglVector3 you want to manipulate
 * @scalar: The scalar you want to multiply the vector components by
 *
 * Multiplies each of the @vector components by the given scalar.
 *
 * Since: 1.4
 * Stability: Unstable
 */
void
cogl_vector3_multiply_scalar (CoglVector3 *vector,
                              float scalar);

/**
 * cogl_vector3_divide_scalar:
 * @vector: The CoglVector3 you want to manipulate
 * @scalar: The scalar you want to divide the vector components by
 *
 * Divides each of the @vector components by the given scalar.
 *
 * Since: 1.4
 * Stability: Unstable
 */
void
cogl_vector3_divide_scalar (CoglVector3 *vector,
                            float scalar);

/**
 * cogl_vector3_normalize:
 * @vector: The CoglVector3 you want to manipulate
 *
 * Updates the vector so it is a "unit vector" such that the
 * @vector<!-- -->s magnitude or length is equal to 1.
 *
 * Since: 1.4
 * Stability: Unstable
 */
void
cogl_vector3_normalize (CoglVector3 *vector);

/**
 * cogl_vector3_magnitude:
 * @vector: The CoglVector3 you want the magnitude for
 *
 * Calculates the scalar magnitude or length of @vector.
 *
 * Returns: The magnitude of @vector.
 *
 * Since: 1.4
 * Stability: Unstable
 */
float
cogl_vector3_magnitude (const CoglVector3 *vector);

/**
 * cogl_vector3_cross_product:
 * @result: Where you want the result written
 * @u: Your first CoglVector3
 * @v: Your second CoglVector3
 *
 * Calculates the cross product between the two vectors @u and @v.
 *
 * The cross product is a vector perpendicular to both @u and @v. This
 * can be useful for calculating the normal of a polygon by creating
 * two vectors in its plane using the polygons vertices and taking
 * their cross product.
 *
 * If the two vectors are parallel then the cross product is 0.
 *
 * You can use a right hand rule to determine which direction the
 * perpendicular vector will point: If you place the two vectors tail,
 * to tail and imagine grabbing the perpendicular line that extends
 * through the common tail with your right hand such that you fingers
 * rotate in the direction from @u to @v then the resulting vector
 * points along your extended thumb.
 *
 * Returns: The cross product between two vectors @u and @v.
 *
 * Since: 1.4
 * Stability: Unstable
 */
void
cogl_vector3_cross_product (CoglVector3 *result,
                            const CoglVector3 *u,
                            const CoglVector3 *v);

/**
 * cogl_vector3_dot_product:
 * @a: Your first CoglVector3
 * @b: Your second CoglVector3
 *
 * Calculates the dot product of the two #CoglVector3<!-- -->s. This
 * can be used to determine the magnitude of one vector projected onto
 * another. (for example a surface normal)
 *
 * For example if you have a polygon with a given normal vector and
 * some other point for which you want to calculate its distance from
 * the polygon, you can create a vector between one of the polygon
 * vertices and that point and use the dot product to calculate the
 * magnitude for that vector but projected onto the normal of the
 * polygon. This way you don't just get the distance from the point to
 * the edge of the polygon you get the distance from the point to the
 * nearest part of the polygon.
 *
 * <note>If you don't use a unit length normal in the above example
 * then you would then also have to divide the result by the magnitude
 * of the normal</note>
 *
 * The dot product is calculated as:
 * |[
 *  (a->x * b->x + a->y * b->y + a->z * b->z)
 * ]|
 *
 * For reference, the dot product can also be calculated from the
 * angle between two vectors as:
 * |[
 *  |a||b|cos𝜃
 * ]|
 *
 * Returns: The dot product of two vectors.
 *
 * Since: 1.4
 * Stability: Unstable
 */
float
cogl_vector3_dot_product (const CoglVector3 *a, const CoglVector3 *b);

/**
 * cogl_vector3_distance:
 * @a: The first point
 * @b: The second point
 *
 * If you consider the two given vectors as (x,y,z) points instead
 * then this will compute the distance between those two points.
 *
 * Returns: The distance between two points given as @CoglVector3<!-- -->s
 *
 * Since: 1.4
 * Stability: Unstable
 */
float
cogl_vector3_distance (const CoglVector3 *a, const CoglVector3 *b);

G_END_DECLS

#endif /* __COGL_VECTOR_H */

