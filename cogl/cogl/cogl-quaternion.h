/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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

#ifndef __COGL_QUATERNION_H__
#define __COGL_QUATERNION_H__

#include <cogl/cogl-types.h>
#include <cogl/cogl-vector.h>

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-quaternion
 * @short_description: Functions for initializing and manipulating
 * quaternions.
 *
 * Quaternions have become a standard form for representing 3D
 * rotations and have some nice properties when compared with other
 * representation such as (roll,pitch,yaw) Euler angles. They can be
 * used to interpolate between different rotations and they don't
 * suffer from a problem called
 * <ulink url="http://en.wikipedia.org/wiki/Gimbal_lock">"Gimbal lock"</ulink>
 * where two of the axis of rotation may become aligned and you loose a
 * degree of freedom.
 * .
 */
#include <cogl/cogl-vector.h>
#include <cogl/cogl-euler.h>

#ifdef COGL_HAS_GTYPE_SUPPORT
#include <glib-object.h>
#endif

/**
 * CoglQuaternion:
 * @w: based on the angle of rotation it is cos(𝜃/2)
 * @x: based on the angle of rotation and x component of the axis of
 *     rotation it is sin(𝜃/2)*axis.x
 * @y: based on the angle of rotation and y component of the axis of
 *     rotation it is sin(𝜃/2)*axis.y
 * @z: based on the angle of rotation and z component of the axis of
 *     rotation it is sin(𝜃/2)*axis.z
 *
 * A quaternion is comprised of a scalar component and a 3D vector
 * component. The scalar component is normally referred to as w and the
 * vector might either be referred to as v or a (for axis) or expanded
 * with the individual components: (x, y, z) A full quaternion would
 * then be written as <literal>[w (x, y, z)]</literal>.
 *
 * Quaternions can be considered to represent an axis and angle
 * pair although sadly these numbers are buried somewhat under some
 * maths...
 *
 * For the curious you can see here that a given axis (a) and angle (𝜃)
 * pair are represented in a quaternion as follows:
 * |[
 * [w=cos(𝜃/2) ( x=sin(𝜃/2)*a.x, y=sin(𝜃/2)*a.y, z=sin(𝜃/2)*a.x )]
 * ]|
 *
 * Unit Quaternions:
 * When using Quaternions to represent spatial orientations for 3D
 * graphics it's always assumed you have a unit quaternion. The
 * magnitude of a quaternion is defined as:
 * |[
 * sqrt (w² + x² + y² + z²)
 * ]|
 * and a unit quaternion satisfies this equation:
 * |[
 * w² + x² + y² + z² = 1
 * ]|
 *
 * Thankfully most of the time we don't actually have to worry about
 * the maths that goes on behind the scenes but if you are curious to
 * learn more here are some external references:
 *
 * <itemizedlist>
 * <listitem>
 * <ulink url="http://mathworld.wolfram.com/Quaternion.html"/>
 * </listitem>
 * <listitem>
 * <ulink url="http://www.gamedev.net/reference/articles/article1095.asp"/>
 * </listitem>
 * <listitem>
 * <ulink url="http://www.cprogramming.com/tutorial/3d/quaternions.html"/>
 * </listitem>
 * <listitem>
 * <ulink url="http://www.isner.com/tutorials/quatSpells/quaternion_spells_12.htm"/>
 * </listitem>
 * <listitem>
 * 3D Maths Primer for Graphics and Game Development ISBN-10: 1556229119
 * </listitem>
 * <listitem>
 * <ulink url="http://www.cs.caltech.edu/courses/cs171/quatut.pdf"/>
 * </listitem>
 * <listitem>
 * <ulink url="http://www.j3d.org/matrix_faq/matrfaq_latest.html#Q56"/>
 * </listitem>
 * </itemizedlist>
 *
 */
struct _CoglQuaternion
{
  /*< public >*/
  float w;

  float x;
  float y;
  float z;

  /*< private >*/
  float padding0;
  float padding1;
  float padding2;
  float padding3;
};
COGL_STRUCT_SIZE_ASSERT (CoglQuaternion, 32);

#ifdef COGL_HAS_GTYPE_SUPPORT
/**
 * cogl_quaternion_get_gtype:
 *
 * Returns: a #GType that can be used with the GLib type system.
 */
GType cogl_quaternion_get_gtype (void);
#endif

/**
 * cogl_quaternion_init:
 * @quaternion: An uninitialized #CoglQuaternion
 * @angle: The angle you want to rotate around the given axis
 * @x: The x component of your axis vector about which you want to
 * rotate.
 * @y: The y component of your axis vector about which you want to
 * rotate.
 * @z: The z component of your axis vector about which you want to
 * rotate.
 *
 * Initializes a quaternion that rotates @angle degrees around the
 * axis vector (@x, @y, @z). The axis vector does not need to be
 * normalized.
 *
 * Returns: A normalized, unit quaternion representing an orientation
 * rotated @angle degrees around the axis vector (@x, @y, @z)
 *
 * Since: 2.0
 */
void
cogl_quaternion_init (CoglQuaternion *quaternion,
                      float angle,
                      float x,
                      float y,
                      float z);

/**
 * cogl_quaternion_init_from_angle_vector:
 * @quaternion: An uninitialized #CoglQuaternion
 * @angle: The angle to rotate around @axis3f
 * @axis3f: your 3 component axis vector about which you want to rotate.
 *
 * Initializes a quaternion that rotates @angle degrees around the
 * given @axis vector. The axis vector does not need to be
 * normalized.
 *
 * Returns: A normalized, unit quaternion representing an orientation
 * rotated @angle degrees around the given @axis vector.
 *
 * Since: 2.0
 */
void
cogl_quaternion_init_from_angle_vector (CoglQuaternion *quaternion,
                                        float angle,
                                        const float *axis3f);

/**
 * cogl_quaternion_init_identity:
 * @quaternion: An uninitialized #CoglQuaternion
 *
 * Initializes the quaternion with the canonical quaternion identity
 * [1 (0, 0, 0)] which represents no rotation. Multiplying a
 * quaternion with this identity leaves the quaternion unchanged.
 *
 * You might also want to consider using
 * cogl_get_static_identity_quaternion().
 *
 * Since: 2.0
 */
void
cogl_quaternion_init_identity (CoglQuaternion *quaternion);

/**
 * cogl_quaternion_init_from_array:
 * @quaternion: A #CoglQuaternion
 * @array: An array of 4 floats w,(x,y,z)
 *
 * Initializes a [w (x, y,z)] quaternion directly from an array of 4
 * floats: [w,x,y,z].
 *
 * Since: 2.0
 */
void
cogl_quaternion_init_from_array (CoglQuaternion *quaternion,
                                 const float *array);

/**
 * cogl_quaternion_init_from_x_rotation:
 * @quaternion: An uninitialized #CoglQuaternion
 * @angle: The angle to rotate around the x axis
 *
 * XXX: check which direction this rotates
 *
 * Since: 2.0
 */
void
cogl_quaternion_init_from_x_rotation (CoglQuaternion *quaternion,
                                      float angle);

/**
 * cogl_quaternion_init_from_y_rotation:
 * @quaternion: An uninitialized #CoglQuaternion
 * @angle: The angle to rotate around the y axis
 *
 *
 * Since: 2.0
 */
void
cogl_quaternion_init_from_y_rotation (CoglQuaternion *quaternion,
                                      float angle);

/**
 * cogl_quaternion_init_from_z_rotation:
 * @quaternion: An uninitialized #CoglQuaternion
 * @angle: The angle to rotate around the z axis
 *
 *
 * Since: 2.0
 */
void
cogl_quaternion_init_from_z_rotation (CoglQuaternion *quaternion,
                                      float angle);

/**
 * cogl_quaternion_init_from_euler:
 * @quaternion: A #CoglQuaternion
 * @euler: A #CoglEuler with which to initialize the quaternion
 *
 * Since: 2.0
 */
void
cogl_quaternion_init_from_euler (CoglQuaternion *quaternion,
                                 const CoglEuler *euler);

/**
 * cogl_quaternion_init_from_quaternion:
 * @quaternion: A #CoglQuaternion
 * @src: A #CoglQuaternion with which to initialize @quaternion
 *
 * Since: 2.0
 */
void
cogl_quaternion_init_from_quaternion (CoglQuaternion *quaternion,
                                      CoglQuaternion *src);

/**
 * cogl_quaternion_init_from_matrix:
 * @quaternion: A Cogl Quaternion
 * @matrix: A rotation matrix with which to initialize the quaternion
 *
 * Initializes a quaternion from a rotation matrix.
 *
 * Since: 1.10
 * Stability: unstable
 */
void
cogl_quaternion_init_from_matrix (CoglQuaternion *quaternion,
                                  const CoglMatrix *matrix);

/**
 * cogl_quaternion_equal:
 * @v1: A #CoglQuaternion
 * @v2: A #CoglQuaternion
 *
 * Compares that all the components of quaternions @a and @b are
 * equal.
 *
 * An epsilon value is not used to compare the float components, but
 * the == operator is at least used so that 0 and -0 are considered
 * equal.
 *
 * Returns: %TRUE if the quaternions are equal else %FALSE.
 *
 * Since: 2.0
 */
CoglBool
cogl_quaternion_equal (const void *v1, const void *v2);

/**
 * cogl_quaternion_copy:
 * @src: A #CoglQuaternion
 *
 * Allocates a new #CoglQuaternion on the stack and initializes it with
 * the same values as @src.
 *
 * Returns: A newly allocated #CoglQuaternion which should be freed
 * using cogl_quaternion_free()
 *
 * Since: 2.0
 */
CoglQuaternion *
cogl_quaternion_copy (const CoglQuaternion *src);

/**
 * cogl_quaternion_free:
 * @quaternion: A #CoglQuaternion
 *
 * Frees a #CoglQuaternion that was previously allocated via
 * cogl_quaternion_copy().
 *
 * Since: 2.0
 */
void
cogl_quaternion_free (CoglQuaternion *quaternion);

/**
 * cogl_quaternion_get_rotation_angle:
 * @quaternion: A #CoglQuaternion
 *
 *
 * Since: 2.0
 */
float
cogl_quaternion_get_rotation_angle (const CoglQuaternion *quaternion);

/**
 * cogl_quaternion_get_rotation_axis:
 * @quaternion: A #CoglQuaternion
 * @vector3: (out): an allocated 3-float array
 *
 * Since: 2.0
 */
void
cogl_quaternion_get_rotation_axis (const CoglQuaternion *quaternion,
                                   float *vector3);

/**
 * cogl_quaternion_normalize:
 * @quaternion: A #CoglQuaternion
 *
 *
 * Since: 2.0
 */
void
cogl_quaternion_normalize (CoglQuaternion *quaternion);

/**
 * cogl_quaternion_dot_product:
 * @a: A #CoglQuaternion
 * @b: A #CoglQuaternion
 *
 * Since: 2.0
 */
float
cogl_quaternion_dot_product (const CoglQuaternion *a,
                             const CoglQuaternion *b);

/**
 * cogl_quaternion_invert:
 * @quaternion: A #CoglQuaternion
 *
 *
 * Since: 2.0
 */
void
cogl_quaternion_invert (CoglQuaternion *quaternion);

/**
 * cogl_quaternion_multiply:
 * @result: The destination #CoglQuaternion
 * @left: The second #CoglQuaternion rotation to apply
 * @right: The first #CoglQuaternion rotation to apply
 *
 * This combines the rotations of two quaternions into @result. The
 * operation is not commutative so the order is important because AxB
 * != BxA. Cogl follows the standard convention for quaternions here
 * so the rotations are applied @right to @left. This is similar to the
 * combining of matrices.
 *
 * <note>It is possible to multiply the @a quaternion in-place, so
 * @result can be equal to @a but can't be equal to @b.</note>
 *
 * Since: 2.0
 */
void
cogl_quaternion_multiply (CoglQuaternion *result,
                          const CoglQuaternion *left,
                          const CoglQuaternion *right);

/**
 * cogl_quaternion_pow:
 * @quaternion: A #CoglQuaternion
 * @exponent: the exponent
 *
 *
 * Since: 2.0
 */
void
cogl_quaternion_pow (CoglQuaternion *quaternion, float exponent);

/**
 * cogl_quaternion_slerp:
 * @result: The destination #CoglQuaternion
 * @a: The first #CoglQuaternion
 * @b: The second #CoglQuaternion
 * @t: The factor in the range [0,1] used to interpolate between
 * quaternion @a and @b.
 *
 * Performs a spherical linear interpolation between two quaternions.
 *
 * Noteable properties:
 * <itemizedlist>
 * <listitem>
 * commutative: No
 * </listitem>
 * <listitem>
 * constant velocity: Yes
 * </listitem>
 * <listitem>
 * torque minimal (travels along the surface of the 4-sphere): Yes
 * </listitem>
 * <listitem>
 * more expensive than cogl_quaternion_nlerp()
 * </listitem>
 * </itemizedlist>
 */
void
cogl_quaternion_slerp (CoglQuaternion *result,
                       const CoglQuaternion *a,
                       const CoglQuaternion *b,
                       float t);

/**
 * cogl_quaternion_nlerp:
 * @result: The destination #CoglQuaternion
 * @a: The first #CoglQuaternion
 * @b: The second #CoglQuaternion
 * @t: The factor in the range [0,1] used to interpolate between
 * quaterion @a and @b.
 *
 * Performs a normalized linear interpolation between two quaternions.
 * That is it does a linear interpolation of the quaternion components
 * and then normalizes the result. This will follow the shortest arc
 * between the two orientations (just like the slerp() function) but
 * will not progress at a constant speed. Unlike slerp() nlerp is
 * commutative which is useful if you are blending animations
 * together. (I.e. nlerp (tmp, a, b) followed by nlerp (result, tmp,
 * d) is the same as nlerp (tmp, a, d) followed by nlerp (result, tmp,
 * b)). Finally nlerp is cheaper than slerp so it can be a good choice
 * if you don't need the constant speed property of the slerp() function.
 *
 * Notable properties:
 * <itemizedlist>
 * <listitem>
 * commutative: Yes
 * </listitem>
 * <listitem>
 * constant velocity: No
 * </listitem>
 * <listitem>
 * torque minimal (travels along the surface of the 4-sphere): Yes
 * </listitem>
 * <listitem>
 * faster than cogl_quaternion_slerp()
 * </listitem>
 * </itemizedlist>
 */
void
cogl_quaternion_nlerp (CoglQuaternion *result,
                       const CoglQuaternion *a,
                       const CoglQuaternion *b,
                       float t);
/**
 * cogl_quaternion_squad:
 * @result: The destination #CoglQuaternion
 * @prev: A #CoglQuaternion used before @a
 * @a: The first #CoglQuaternion
 * @b: The second #CoglQuaternion
 * @next: A #CoglQuaternion that will be used after @b
 * @t: The factor in the range [0,1] used to interpolate between
 * quaternion @a and @b.
 *
 *
 * Since: 2.0
 */
void
cogl_quaternion_squad (CoglQuaternion *result,
                       const CoglQuaternion *prev,
                       const CoglQuaternion *a,
                       const CoglQuaternion *b,
                       const CoglQuaternion *next,
                       float t);

/**
 * cogl_get_static_identity_quaternion:
 *
 * Returns a pointer to a singleton quaternion constant describing the
 * canonical identity [1 (0, 0, 0)] which represents no rotation.
 *
 * If you multiply a quaternion with the identity quaternion you will
 * get back the same value as the original quaternion.
 *
 * Returns: A pointer to an identity quaternion
 *
 * Since: 2.0
 */
const CoglQuaternion *
cogl_get_static_identity_quaternion (void);

/**
 * cogl_get_static_zero_quaternion:
 *
 * Returns: a pointer to a singleton quaternion constant describing a
 *          rotation of 180 degrees around a degenerate axis:
 *          [0 (0, 0, 0)]
 *
 * Since: 2.0
 */
const CoglQuaternion *
cogl_get_static_zero_quaternion (void);

COGL_END_DECLS

#endif /* __COGL_QUATERNION_H__ */

