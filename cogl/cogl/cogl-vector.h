/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2010 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_VECTOR_H
#define __COGL_VECTOR_H

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-vector
 * @short_description: Functions for handling single precision float
 *                     vectors.
 *
 * This exposes a utility API that can be used for basic manipulation of 3
 * component float vectors.
 */

/**
 * cogl_vector3_init:
 * @vector: The 3 component vector you want to initialize
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
cogl_vector3_init (float *vector, float x, float y, float z);

/**
 * cogl_vector3_init_zero:
 * @vector: The 3 component vector you want to initialize
 *
 * Initializes a 3 component, single precision float vector with zero
 * for each component.
 *
 * Since: 1.4
 * Stability: Unstable
 */
void
cogl_vector3_init_zero (float *vector);

/**
 * cogl_vector3_equal:
 * @v1: The first 3 component vector you want to compare
 * @v2: The second 3 component vector you want to compare
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
CoglBool
cogl_vector3_equal (const void *v1, const void *v2);

/**
 * cogl_vector3_equal_with_epsilon:
 * @vector0: The first 3 component vector you want to compare
 * @vector1: The second 3 component vector you want to compare
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
CoglBool
cogl_vector3_equal_with_epsilon (const float *vector0,
                                 const float *vector1,
                                 float epsilon);

/**
 * cogl_vector3_copy:
 * @vector: The 3 component vector you want to copy
 *
 * Allocates a new 3 component float vector on the heap initializing
 * the components from the given @vector and returns a pointer to the
 * newly allocated vector. You should free the memory using
 * cogl_vector3_free()
 *
 * Returns: A newly allocated 3 component float vector
 *
 * Since: 1.4
 * Stability: Unstable
 */
float *
cogl_vector3_copy (const float *vector);

/**
 * cogl_vector3_free:
 * @vector: The 3 component you want to free
 *
 * Frees a 3 component vector that was previously allocated with
 * cogl_vector3_copy()
 *
 * Since: 1.4
 * Stability: Unstable
 */
void
cogl_vector3_free (float *vector);

/**
 * cogl_vector3_invert:
 * @vector: The 3 component vector you want to manipulate
 *
 * Inverts/negates all the components of the given @vector.
 *
 * Since: 1.4
 * Stability: Unstable
 */
void
cogl_vector3_invert (float *vector);

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
cogl_vector3_add (float *result,
                  const float *a,
                  const float *b);

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
cogl_vector3_subtract (float *result,
                       const float *a,
                       const float *b);

/**
 * cogl_vector3_multiply_scalar:
 * @vector: The 3 component vector you want to manipulate
 * @scalar: The scalar you want to multiply the vector components by
 *
 * Multiplies each of the @vector components by the given scalar.
 *
 * Since: 1.4
 * Stability: Unstable
 */
void
cogl_vector3_multiply_scalar (float *vector,
                              float scalar);

/**
 * cogl_vector3_divide_scalar:
 * @vector: The 3 component vector you want to manipulate
 * @scalar: The scalar you want to divide the vector components by
 *
 * Divides each of the @vector components by the given scalar.
 *
 * Since: 1.4
 * Stability: Unstable
 */
void
cogl_vector3_divide_scalar (float *vector,
                            float scalar);

/**
 * cogl_vector3_normalize:
 * @vector: The 3 component vector you want to manipulate
 *
 * Updates the vector so it is a "unit vector" such that the
 * @vector<!-- -->s magnitude or length is equal to 1.
 *
 * <note>It's safe to use this function with the [0, 0, 0] vector, it will not
 * try to divide components by 0 (its norm) and will leave the vector
 * untouched.</note>
 *
 * Since: 1.4
 * Stability: Unstable
 */
void
cogl_vector3_normalize (float *vector);

/**
 * cogl_vector3_magnitude:
 * @vector: The 3 component vector you want the magnitude for
 *
 * Calculates the scalar magnitude or length of @vector.
 *
 * Returns: The magnitude of @vector.
 *
 * Since: 1.4
 * Stability: Unstable
 */
float
cogl_vector3_magnitude (const float *vector);

/**
 * cogl_vector3_cross_product:
 * @result: Where you want the result written
 * @u: Your first 3 component vector
 * @v: Your second 3 component vector
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
cogl_vector3_cross_product (float *result,
                            const float *u,
                            const float *v);

/**
 * cogl_vector3_dot_product:
 * @a: Your first 3 component vector
 * @b: Your second 3 component vector
 *
 * Calculates the dot product of the two 3 component vectors. This
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
cogl_vector3_dot_product (const float *a, const float *b);

/**
 * cogl_vector3_distance:
 * @a: The first point
 * @b: The second point
 *
 * If you consider the two given vectors as (x,y,z) points instead
 * then this will compute the distance between those two points.
 *
 * Returns: The distance between two points given as 3 component
 *          vectors.
 *
 * Since: 1.4
 * Stability: Unstable
 */
float
cogl_vector3_distance (const float *a, const float *b);

COGL_END_DECLS

#endif /* __COGL_VECTOR_H */

