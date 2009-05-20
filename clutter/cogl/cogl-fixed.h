/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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
 */

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_FIXED_H__
#define __COGL_FIXED_H__

#include <cogl/cogl-types.h>

/**
 * SECTION:cogl-fixed
 * @short_description: Fixed Point API
 *
 * COGL has a fixed point API targeted at platforms without a floating
 * point unit, such as embedded devices. On such platforms this API should
 * be preferred to the floating point one as it does not trigger the slow
 * path of software emulation, relying on integer math for fixed-to-floating
 * and floating-to-fixed notations conversion.
 *
 * It is not recommened for use on platforms with a floating point unit
 * (e.g. desktop systems), nor for use in language bindings.
 *
 * Basic rules of Fixed Point arithmethic:
 * <itemizedlist>
 *   <listitem>
 *     <para>Two fixed point numbers can be directly added, subtracted and
 *     have their modulus taken.</para>
 *   </listitem>
 *   <listitem>
 *     <para>To add other numerical type to a fixed point number it has to
 *     be first converted to fixed point.</para>
 *   </listitem>
 *   <listitem>
 *     <para>A fixed point number can be directly multiplied or divided by
 *     an integer.</para>
 *   </listitem>
 *   <listitem>
 *     <para>Two fixed point numbers can only be multiplied and divided by
 *     the provided %COGL_FIXED_MUL and %COGL_FIXED_DIV macros.</para>
 *   </listitem>
 * </itemizedlist>
 *
 * The fixed point API is available since COGL 1.0.
 */

G_BEGIN_DECLS

/**
 * COGL_FIXED_BITS:
 *
 * Evaluates to the number of bits used by the #CoglFixed type.
 *
 * Since: 1.0
 */
#define COGL_FIXED_BITS         (32)

/**
 * COGL_FIXED_Q:
 *
 * Evaluates to the number of bits used for the non-integer part
 * of the #CoglFixed type.
 *
 * Since: 1.0
 */
#define COGL_FIXED_Q            (COGL_FIXED_BITS - 16)

/**
 * COGL_FIXED_1:
 *
 * The number 1 expressed as a #CoglFixed number.
 *
 * Since: 1.0
 */
#define COGL_FIXED_1            (1 << COGL_FIXED_Q)

/**
 * COGL_FIXED_0_5:
 *
 * The number 0.5 expressed as a #CoglFixed number.
 *
 * Since: 1.0
 */
#define COGL_FIXED_0_5          (32768)

/**
 * COGL_FIXED_EPSILON:
 *
 * A very small number expressed as a #CoglFixed number.
 *
 * Since: 1.0
 */
#define COGL_FIXED_EPSILON      (1)

/**
 * COGL_FIXED_MAX:
 *
 * The biggest number representable using #CoglFixed
 *
 * Since: 1.0
 */
#define COGL_FIXED_MAX          (0x7fffffff)

/**
 * COGL_FIXED_MIN:
 *
 * The smallest number representable using #CoglFixed
 *
 * Since: 1.0
 */
#define COGL_FIXED_MIN          (0x80000000)

/**
 * COGL_FIXED_PI:
 *
 * The number pi, expressed as a #CoglFixed number.
 *
 * Since: 1.0
 */
#define COGL_FIXED_PI           (0x0003243f)

/**
 * COGL_FIXED_2_PI:
 *
 * Two times pi, expressed as a #CoglFixed number.
 *
 * Since: 1.0
 */
#define COGL_FIXED_2_PI         (0x0006487f)

/**
 * COGL_FIXED_PI_2:
 *
 * Half pi, expressed as a #CoglFixed number.
 *
 * Since: 1.0
 */
#define COGL_FIXED_PI_2         (0x00019220)

/**
 * COGL_FIXED_PI_4:
 *
 * pi / 4, expressed as #CoglFixed number.
 *
 * Since: 1.0
 */
#define COGL_FIXED_PI_4         (0x0000c910)

/**
 * COGL_FIXED_360:
 *
 * Evaluates to the number 360 in fixed point notation.
 *
 * Since: 1.0
 */
#define COGL_FIXED_360          (COGL_FIXED_FROM_INT (360))

/**
 * COGL_FIXED_270:
 *
 * Evaluates to the number 270 in fixed point notation.
 *
 * Since: 1.0
 */
#define COGL_FIXED_270          (COGL_FIXED_FROM_INT (270))

/**
 * COGL_FIXED_255:
 *
 * Evaluates to the number 255 in fixed point notation.
 *
 * Since: 1.0
 */
#define COGL_FIXED_255          (COGL_FIXED_FROM_INT (255))

/**
 * COGL_FIXED_240:
 *
 * Evaluates to the number 240 in fixed point notation.
 *
 * Since: 1.0
 */
#define COGL_FIXED_240          (COGL_FIXED_FROM_INT (240))

/**
 * COGL_FIXED_180:
 *
 * Evaluates to the number 180 in fixed point notation.
 *
 * Since: 1.0
 */
#define COGL_FIXED_180          (COGL_FIXED_FROM_INT (180))

/**
 * COGL_FIXED_120:
 *
 * Evaluates to the number 120 in fixed point notation.
 *
 * Since: 1.0
 */
#define COGL_FIXED_120          (COGL_FIXED_FROM_INT (120))

/**
 * COGL_FIXED_90:
 *
 * Evaluates to the number 90 in fixed point notation.
 *
 * Since: 1.0
 */
#define COGL_FIXED_90           (COGL_FIXED_FROM_INT (90))

/**
 * COGL_FIXED_60:
 *
 * Evaluates to the number 60 in fixed point notation.
 *
 * Since: 1.0
 */
#define COGL_FIXED_60           (COGL_FIXED_FROM_INT (60))

/**
 * COGL_FIXED_45:
 *
 * Evaluates to the number 45 in fixed point notation.
 *
 * Since: 1.0
 */
#define COGL_FIXED_45           (COGL_FIXED_FROM_INT (45))

/**
 * COGL_FIXED_30:
 *
 * Evaluates to the number 30 in fixed point notation.
 *
 * Since: 1.0
 */
#define COGL_FIXED_30           (COGL_FIXED_FROM_INT (30))

/**
 * COGL_RADIANS_TO_DEGREES:
 *
 * Evaluates to 180 / pi in fixed point notation.
 *
 * Since: 1.0
 */
#define COGL_RADIANS_TO_DEGREES (0x394bb8)

/*
 * conversion macros
 */

/**
 * COGL_FIXED_FROM_FLOAT:
 * @x: a floating point number
 *
 * Converts @x from a floating point to a fixed point notation.
 *
 * Since: 1.0
 */
#define COGL_FIXED_FROM_FLOAT(x)        ((float) cogl_double_to_fixed (x))

/**
 * COGL_FIXED_TO_FLOAT:
 * @x: a #CoglFixed number
 *
 * Converts @x from a fixed point to a floating point notation, in
 * single precision.
 *
 * Since: 1.0
 */
#define COGL_FIXED_TO_FLOAT(x)          ((float) ((int)(x) / 65536.0))

/**
 * COGL_FIXED_FROM_DOUBLE:
 * @x: a floating point number
 *
 * Converts @x from a double precision, floating point to a fixed
 * point notation.
 *
 * Since: 1.0
 */
#define COGL_FIXED_FROM_DOUBLE(x)       (cogl_double_to_fixed (x))

/**
 * COGL_FIXED_TO_FLOAT:
 * @x: a #CoglFixed number
 *
 * Converts @x from a fixed point to a floating point notation, in
 * double precision.
 *
 * Since: 1.0
 */
#define COGL_FIXED_TO_DOUBLE(x)         ((double) ((int)(x) / 65536.0))

/**
 * COGL_FIXED_FROM_INT:
 * @x: an integer number
 *
 * Converts @x from an integer to a fixed point notation.
 *
 * Since: 1.0
 */
#define COGL_FIXED_FROM_INT(x)          ((x) << COGL_FIXED_Q)

/**
 * COGL_FIXED_TO_INT:
 * @x: a #CoglFixed number
 *
 * Converts @x from a fixed point notation to an integer, dropping
 * the fractional part without rounding.
 *
 * Since: 1.0
 */
#define COGL_FIXED_TO_INT(x)            ((x) >> COGL_FIXED_Q)

/**
 * COGL_FLOAT_TO_INT:
 * @x: a floatint point number
 *
 * Converts @x from a floating point notation to a signed integer.
 *
 * Since: 1.0
 */
#define COGL_FLOAT_TO_INT(x)            (cogl_double_to_int ((x)))

/**
 * COGL_FLOAT_TO_UINT:
 * @x: a floatint point number
 *
 * Converts @x from a floating point notation to an unsigned integer.
 *
 * Since: 1.0
 */
#define COGL_FLOAT_TO_UINT(x)           (cogl_double_to_uint ((x)))

/*
 * fixed point math functions
 */

/**
 * COGL_FIXED_FRACTION:
 * @x: a #CoglFixed number
 *
 * Retrieves the fractionary part of @x.
 *
 * Since: 1.0
 */
#define COGL_FIXED_FRACTION(x)          ((x) & ((1 << COGL_FIXED_Q) - 1))

/**
 * COGL_FIXED_FLOOR:
 * @x: a #CoglFixed number
 *
 * Rounds down a fixed point number to the previous integer.
 *
 * Since: 1.0
 */
#define COGL_FIXED_FLOOR(x)             (((x) >= 0) ? ((x) >> COGL_FIXED_Q) \
                                                    : ~((~(x)) >> COGL_FIXED_Q))

/**
 * COGL_FIXED_CEIL:
 * @x: a #CoglFixed number
 *
 * Rounds up a fixed point number to the next integer.
 *
 * Since: 1.0
 */
#define COGL_FIXED_CEIL(x)              (COGL_FIXED_FLOOR ((x) + 0xffff))

/**
 * COGL_FIXED_MUL:
 * @a: a #CoglFixed number
 * @b: a #CoglFixed number
 *
 * Computes (a * b).
 *
 * Since: 1.0
 */
#define COGL_FIXED_MUL(a,b)             (cogl_fixed_mul ((a), (b)))

/**
 * COGL_FIXED_DIV:
 * @a: a #CoglFixed number
 * @b: a #CoglFixed number
 *
 * Computes (a / b).
 *
 * Since: 1.0
 */
#define COGL_FIXED_DIV(a,b)             (cogl_fixed_div ((a), (b)))

/**
 * COGL_FIXED_MUL_DIV:
 * @a: a #CoglFixed number
 * @b: a #CoglFixed number
 * @c: a #CoglFixed number
 *
 * Computes ((a * b) / c). It is logically equivalent to:
 *
 * |[
 *   res = COGL_FIXED_DIV (COGL_FIXED_MUL (a, b), c);
 * ]|
 *
 * But it is shorter to type.
 *
 * Since: 1.0
 */
#define COGL_FIXED_MUL_DIV(a,b,c)       (cogl_fixed_mul_div ((a), (b), (c)))

/**
 * COGL_FIXED_FAST_MUL:
 * @a: a #CoglFixed number
 * @b: a #CoglFixed number
 *
 * Fast version of %COGL_FIXED_MUL, implemented as a macro.
 *
 * <note>This macro might lose precision. If the precision of the result
 * is important use %COGL_FIXED_MUL instead.</note>
 *
 * Since: 1.0
 */
#define COGL_FIXED_FAST_MUL(a,b)        ((a) >> 8) * ((b) >> 8)

/**
 * COGL_FIXED_FAST_DIV:
 * @a: a #CoglFixed number
 * @b: a #CoglFixed number
 *
 * Fast version of %COGL_FIXED_DIV, implemented as a macro.
 *
 * <note>This macro might lose precision. If the precision of the result
 * is important use %COGL_FIXED_DIV instead.</note>
 *
 * Since: 1.0
 */
#define COGL_FIXED_FAST_DIV(a,b)        ((((a) << 8) / (b)) << 8)

/**
 * cogl_fixed_sin:
 * @angle: a #CoglFixed number
 *
 * Computes the sine of @angle.
 *
 * Return value: the sine of the passed angle, in fixed point notation
 *
 * Since: 1.0
 */
CoglFixed cogl_fixed_sin    (CoglFixed angle);

/**
 * cogl_fixed_tan:
 * @angle: a #CoglFixed number
 *
 * Computes the tangent of @angle.
 *
 * Return value: the tangent of the passed angle, in fixed point notation
 *
 * Since: 1.0
 */
CoglFixed cogl_fixed_tan    (CoglFixed angle);

/**
 * cogl_fixed_cos:
 * @angle: a #CoglFixed number
 *
 * Computes the cosine of @angle.
 *
 * Return value: the cosine of the passed angle, in fixed point notation
 *
 * Since: 1.0
 */
CoglFixed cogl_fixed_cos    (CoglFixed angle);

/**
 * cogl_fixed_atani:
 * @a: a #CoglFixed number
 *
 * Computes the arc tangent of @a.
 *
 * Return value: the arc tangent of the passed value, in fixed point notation
 *
 * Since: 1.0
 */
CoglFixed cogl_fixed_atani  (CoglFixed a);

/**
 * cogl_fixed_atan2:
 * @a: the numerator as a #CoglFixed number
 * @b: the denominator as a #CoglFixed number
 *
 * Computes the arc tangent of @a / @b but uses the sign of both
 * arguments to return the angle in right quadrant.
 *
 * Return value: the arc tangent of the passed fraction, in fixed point
 *   notation
 *
 * Since: 1.0
 */
CoglFixed cogl_fixed_atan2 (CoglFixed a,
                            CoglFixed b);

/*< public >*/

/* Fixed point math routines */
G_INLINE_FUNC CoglFixed cogl_fixed_mul     (CoglFixed a,
                                            CoglFixed b);
G_INLINE_FUNC CoglFixed cogl_fixed_div     (CoglFixed a,
                                            CoglFixed b);
G_INLINE_FUNC CoglFixed cogl_fixed_mul_div (CoglFixed a,
                                            CoglFixed b,
                                            CoglFixed c);

/**
 * COGL_SQRTI_ARG_MAX:
 *
 * Maximum argument that can be passed to cogl_sqrti() function.
 *
 * Since: 1.0
 */
#ifndef __SSE2__
#define COGL_SQRTI_ARG_MAX 0x3fffff
#else
#define COGL_SQRTI_ARG_MAX INT_MAX
#endif

/**
 * COGL_SQRTI_ARG_5_PERCENT:
 *
 * Maximum argument that can be passed to cogl_sqrti() for which the
 * resulting error is < 5%
 *
 * Since: 1.0
 */
#ifndef __SSE2__
#define COGL_SQRTI_ARG_5_PERCENT 210
#else
#define COGL_SQRTI_ARG_5_PERCENT INT_MAX
#endif

/**
 * COGL_SQRTI_ARG_10_PERCENT:
 *
 * Maximum argument that can be passed to cogl_sqrti() for which the
 * resulting error is < 10%
 *
 * Since: 1.0
 */
#ifndef __SSE2__
#define COGL_SQRTI_ARG_10_PERCENT 5590
#else
#define COGL_SQRTI_ARG_10_PERCENT INT_MAX
#endif

/**
 * cogl_fixed_sqrt:
 * @x: a #CoglFixed number
 *
 * Computes the square root of @x.
 *
 * Return value: the square root of the passed value, in floating point
 *   notation
 *
 * Since: 1.0
 */
CoglFixed cogl_fixed_sqrt (CoglFixed x);

/**
 * cogl_fixed_log2:
 * @x: value to calculate base 2 logarithm from
 *
 * Calculates base 2 logarithm.
 *
 * This function is some 2.5 times faster on x86, and over 12 times faster on
 * fpu-less arm, than using libc log().
 *
 * Return value: base 2 logarithm.
 *
 * Since: 1.0
 */
CoglFixed cogl_fixed_log2 (guint     x);

/**
 * cogl_fixed_pow2:
 * @x: a #CoglFixed number
 *
 * Calculates 2 to the @x power.
 *
 * This function is around 11 times faster on x86, and around 22 times faster
 * on fpu-less arm than libc pow(2, x).
 *
 * Return value: the power of 2 to the passed value
 *
 * Since: 1.0
 */
guint     cogl_fixed_pow2 (CoglFixed x);

/**
 * cogl_fixed_pow:
 * @x: base
 * @y: #CoglFixed exponent
 *
 * Calculates @x to the @y power.
 *
 * Return value: the power of @x to the @y
 *
 * Since: 1.0
 */
guint     cogl_fixed_pow  (guint     x,
                           CoglFixed y);

/**
 * cogl_sqrti:
 * @x: integer value
 *
 * Very fast fixed point implementation of square root for integers.
 *
 * This function is at least 6x faster than clib sqrt() on x86, and (this is
 * not a typo!) about 500x faster on ARM without FPU. It's error is less than
 * 5% for arguments smaller than %COGL_SQRTI_ARG_5_PERCENT and less than 10%
 * for narguments smaller than %COGL_SQRTI_ARG_10_PERCENT. The maximum
 * argument that can be passed to this function is %COGL_SQRTI_ARG_MAX.
 *
 * Return value: integer square root.
 *
 * Since: 1.0
 */
gint      cogl_sqrti      (gint      x);

/**
 * COGL_ANGLE_FROM_DEG:
 * @x: an angle in degrees in floating point notation
 *
 * Converts an angle in degrees into a #CoglAngle.
 *
 * Since: 1.0
 */
#define COGL_ANGLE_FROM_DEG(x)  (COGL_FLOAT_TO_INT (((float)(x) * 1024.0f) / 360.0f))

/**
 * COGL_ANGLE_TO_DEG:
 * @x: a #CoglAngle
 *
 * Converts a #CoglAngle into an angle in degrees, using floatint point
 * notation.
 *
 * Since: 1.0
 */
#define COGL_ANGLE_TO_DEG(x)    (((float)(x) * 360.0) / 1024.0)

/**
 * COGL_ANGLE_FROM_DEGX:
 * @x: an angle in degrees in fixed point notation
 *
 * Converts an angle in degrees into a #CoglAngle.
 *
 * Since: 1.0
 */
#define COGL_ANGLE_FROM_DEGX(x) (COGL_FIXED_TO_INT ((((x) / 360) * 1024) + COGL_FIXED_0_5))

/**
 * COGL_ANGLE_TO_DEGX:
 * @x: a #CoglAngle
 *
 * Converts a #CoglAngle into an angle in degrees, using fixed point notation
 *
 * Since: 1.0
 */
#define COGL_ANGLE_TO_DEGX(x)   (COGL_FIXED_FROM_INT ((x) * 45) / 128)

/**
 * cogl_angle_sin:
 * @angle: an angle expressed using #CoglAngle
 *
 * Computes the sine of @angle
 *
 * Return value: the sine of the passed angle
 *
 * Since: 1.0
 */
CoglFixed cogl_angle_sin (CoglAngle angle);

/**
 * cogl_angle_tan:
 * @angle: an angle expressed using #CoglAngle
 *
 * Computes the tangent of @angle
 *
 * Return value: the tangent of the passed angle
 *
 * Since: 1.0
 */
CoglFixed cogl_angle_tan (CoglAngle angle);

/**
 * cogl_angle_cos:
 * @angle: an angle expressed using #CoglAngle
 *
 * Computes the cosine of @angle
 *
 * Return value: the cosine of the passed angle
 *
 * Since: 1.0
 */
CoglFixed cogl_angle_cos (CoglAngle angle);

/*< private >*/

#if defined (G_CAN_INLINE)
G_INLINE_FUNC CoglFixed
cogl_fixed_mul (CoglFixed a,
                CoglFixed b)
{
# ifdef __arm__
    int res_low, res_hi;

    __asm__ ("smull %0, %1, %2, %3     \n"
	     "mov   %0, %0,     lsr %4 \n"
	     "add   %1, %0, %1, lsl %5 \n"
	     : "=r"(res_hi), "=r"(res_low)\
	     : "r"(a), "r"(b), "i"(COGL_FIXED_Q), "i"(32 - COGL_FIXED_Q));

    return (CoglFixed) res_low;
# else
    long long r = (long long) a * (long long) b;

    return (unsigned int)(r >> COGL_FIXED_Q);
# endif
}
#endif

#if defined (G_CAN_INLINE)
G_INLINE_FUNC CoglFixed
cogl_fixed_div (CoglFixed a,
                CoglFixed b)
{
  return (CoglFixed) ((((gint64) a) << COGL_FIXED_Q) / b);
}
#endif

#if defined(G_CAN_INLINE)
G_INLINE_FUNC CoglFixed
cogl_fixed_mul_div (CoglFixed a,
                    CoglFixed b,
                    CoglFixed c)
{
  CoglFixed ab = cogl_fixed_mul (a, b);
  CoglFixed quo = cogl_fixed_div (ab, c);

  return quo;
}
#endif

extern CoglFixed cogl_double_to_fixed (double value);
extern gint      cogl_double_to_int   (double value);
extern guint     cogl_double_to_unit  (double value);

G_END_DECLS

#endif /* __COGL_FIXED_H__ */
