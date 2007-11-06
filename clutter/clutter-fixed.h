/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Tomas Frydrych <tf@openedhand.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _HAVE_CLUTTER_FIXED_H
#define _HAVE_CLUTTER_FIXED_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * ClutterFixed:
 *
 * Fixed point number (16.16)
 */
typedef gint32 ClutterFixed;

/**
 * ClutterAngle:
 *
 * Integer representation of an angle such that 1024 corresponds to
 * full circle (i.e., 2*Pi).
 */
typedef gint32 ClutterAngle;    /* angle such that 1024 == 2*PI */

#define CLUTTER_ANGLE_FROM_DEG(x)  (CLUTTER_FLOAT_TO_INT (((x) * 1024.0) / 360.0))
#define CLUTTER_ANGLE_FROM_DEGF(x) (CLUTTER_FLOAT_TO_INT (((float)(x) * 1024.0f) / 360.0f))
#define CLUTTER_ANGLE_FROM_DEGX(x) (CFX_INT((((x)/360)*1024) + CFX_HALF))
#define CLUTTER_ANGLE_TO_DEG(x)    (((x) * 360.0)/ 1024.0)
#define CLUTTER_ANGLE_TO_DEGF(x)   (((float)(x) * 360.0)/ 1024.0)
#define CLUTTER_ANGLE_TO_DEGX(x)   (CLUTTER_INT_TO_FIXED((x) * 45)/128)

/*
 * some commonly used constants
 */

/**
 * CFX_Q:
 *
 * Size in bits of decimal part of floating point value.
 */
#define CFX_Q      16		/* Decimal part size in bits */

/**
 * CFX_ONE:
 *
 * 1.0 represented as a fixed point value.
 */
#define CFX_ONE    (1 << CFX_Q)	/* 1 */

/**
 * CFX_HALF:
 *
 * 0.5 represented as a fixed point value.
 */
#define CFX_HALF   32768

/**
 * CFX_MAX:
 *
 * Maximum fixed point value.
 */
#define CFX_MAX    0x7fffffff

/**
 * CFX_MIN:
 *
 * Minimum fixed point value.
 */
#define CFX_MIN    0x80000000

/**
 * CFX_PI:
 *
 * Fixed point representation of Pi
 */
#define CFX_PI     0x0003243f
/**
 * CFX_2PI:
 *
 * Fixed point representation of Pi*2
 */
#define CFX_2PI    0x0006487f
/**
 * CFX_PI_2:
 *
 * Fixed point representation of Pi/2
 */
#define CFX_PI_2   0x00019220   /* pi/2 */
/**
 * CFX_PI_4:
 *
 * Fixed point representation of Pi/4
 */
#define CFX_PI_4   0x0000c910   /* pi/4 */
/**
 * CFX_360:
 *
 * Fixed point representation of the number 360
 */
#define CFX_360 CLUTTER_INT_TO_FIXED (360)
/**
 * CFX_240:
 *
 * Fixed point representation of the number 240
 */
#define CFX_240 CLUTTER_INT_TO_FIXED (240)
/**
 * CFX_180:
 *
 * Fixed point representation of the number 180
 */
#define CFX_180 CLUTTER_INT_TO_FIXED (180)
/**
 * CFX_120:
 *
 * Fixed point representation of the number 120
 */
#define CFX_120 CLUTTER_INT_TO_FIXED (120)
/**
 * CFX_60:
 *
 * Fixed point representation of the number 60
 */
#define CFX_60  CLUTTER_INT_TO_FIXED (60)
/**
 * CFX_255:
 *
 * Fixed point representation of the number 255
 */
#define CFX_255 CLUTTER_INT_TO_FIXED (255)

/**
 * CLUTTER_FIXED_TO_FLOAT:
 * @x: a fixed point value
 *
 * Convert a fixed point value to float.
 */
#define CLUTTER_FIXED_TO_FLOAT(x)       ((float) ((int)(x) / 65536.0))

/**
 * CLUTTER_FIXED_TO_DOUBLE:
 * @x: a fixed point value
 *
 * Convert a fixed point value to double.
 */
#define CLUTTER_FIXED_TO_DOUBLE(x)      ((double) ((int)(x) / 65536.0))

/**
 * CLUTTER_FLOAT_TO_FIXED:
 * @x: a floating point value
 *
 * Convert a float value to fixed.
 */
#define CLUTTER_FLOAT_TO_FIXED(x)       (_clutter_double_to_fixed ((x)))

/**
 * CLUTTER_FLOAT_TO_INT:
 * @x: a floating point value
 *
 * Convert a float value to int.
 */
#define CLUTTER_FLOAT_TO_INT(x)         (_clutter_double_to_int ((x)))

/**
 * CLUTTER_FLOAT_TO_UINT:
 * @x: a floating point value
 *
 * Convert a float value to unsigned int.
 */
#define CLUTTER_FLOAT_TO_UINT(x)         (_clutter_double_to_uint ((x)))

/**
 * CLUTTER_INT_TO_FIXED:
 * @x: an integer value
 *
 * Convert an integer value to fixed point.
 */
#define CLUTTER_INT_TO_FIXED(x)         ((x) << CFX_Q)

/**
 * CLUTTER_FIXED_TO_INT:
 * @x: a fixed point value
 *
 * Converts a fixed point value to integer (removing the decimal part).
 *
 * Since: 0.6
 */
#define CLUTTER_FIXED_TO_INT(x)         ((x) >> CFX_Q)

#ifndef CLUTTER_DISABLE_DEPRECATED

/**
 * CLUTTER_FIXED_INT:
 * @x: a fixed point value
 *
 * Convert a fixed point value to integer (removing decimal part).
 *
 * Deprecated:0.6: Use %CLUTTER_FIXED_TO_INT instead
 */
#define CLUTTER_FIXED_INT(x)            CLUTTER_FIXED_TO_INT((x))

#endif /* !CLUTTER_DISABLE_DEPRECATED */

/**
 * CLUTTER_FIXED_FRACTION:
 * @x: a fixed point value
 *
 * Retrieves the fractionary part of a fixed point value
 */
#define CLUTTER_FIXED_FRACTION(x)       ((x) & ((1 << CFX_Q) - 1))

/**
 * CLUTTER_FIXED_FLOOR:
 * @x: a fixed point value
 *
 * Round down a fixed point value to an integer.
 */
#define CLUTTER_FIXED_FLOOR(x)          (((x) >= 0) ? ((x) >> CFX_Q) \
                                                    : ~((~(x)) >> CFX_Q))
/**
 * CLUTTER_FIXED_CEIL:
 * @x: a fixed point value
 *
 * Round up a fixed point value to an integer.
 */
#define CLUTTER_FIXED_CEIL(x)           (CLUTTER_FIXED_FLOOR (x + 0xffff))

/**
 * CLUTTER_FIXED_MUL:
 * @x: a fixed point value
 * @y: a fixed point value
 *
 * Multiply two fixed point values
 */
#define CLUTTER_FIXED_MUL(x,y) ((x) >> 8) * ((y) >> 8)

/**
 * CLUTTER_FIXED_DIV:
 * @x: a fixed point value
 * @y: a fixed point value
 *
 * Divide two fixed point values
 */
#define CLUTTER_FIXED_DIV(x,y) ((((x) << 8)/(y)) << 8)

/* Some handy fixed point short aliases to avoid exessively long lines */
/* FIXME: Remove from public API */
#define CFX_INT         CLUTTER_FIXED_INT
#define CFX_MUL         CLUTTER_FIXED_MUL
#define CFX_DIV         CLUTTER_FIXED_DIV
#define CFX_QMUL(x,y)   clutter_qmulx (x,y)

/* Fixed point math routines */
extern inline
ClutterFixed clutter_qmulx (ClutterFixed op1, ClutterFixed op2);

ClutterFixed clutter_sinx (ClutterFixed angle);
ClutterFixed clutter_sini (ClutterAngle angle);

ClutterFixed clutter_tani (ClutterAngle angle);

/* convenience macros for the cos functions */

/**
 * clutter_cosx:
 * @angle: a #ClutterFixed angle in radians
 *
 * Fixed point cosine function
 *
 * Return value: #ClutterFixed cosine value.
 *
 * Note: Implemneted as a macro.
 *
 * Since: 0.2
 */
#define clutter_cosx(angle) (clutter_sinx((angle) + CFX_PI_2))

/**
 * clutter_cosi:
 * @angle: a #ClutterAngle angle
 *
 * Very fast fixed point implementation of cosine function.
 *
 * ClutterAngle is an integer such that 1024 represents
 * full circle.
 *
 * Return value: #ClutterFixed cosine value.
 *
 * Note: Implemneted as a macro.
 *
 * Since: 0.2
 */
#define clutter_cosi(angle) (clutter_sini ((angle) + 256))

/**
 * CLUTTER_SQRTI_ARG_MAX
 *
 * Maximum argument that can be passed to #clutter_sqrti function.
 *
 * Since: 0.6
 */
#ifndef __SSE2__
#define CLUTTER_SQRTI_ARG_MAX 0x3fffff
#else
#define CLUTTER_SQRTI_ARG_MAX INT_MAX
#endif

/**
 * CLUTTER_SQRTI_ARG_5_PERCENT
 *
 * Maximum argument that can be passed to #clutter_sqrti for which the
 * resulting error is < 5%
 *
 * Since: 0.6
 */
#ifndef __SSE2__
#define CLUTTER_SQRTI_ARG_5_PERCENT  131
#else
#define CLUTTER_SQRTI_ARG_5_PERCENT INT_MAX
#endif

/**
 * CLUTTER_SQRTI_ARG_10_PERCENT
 *
 * Maximum argument that can be passed to #clutter_sqrti for which the
 * resulting error is < 10%
 *
 * Since: 0.6
 */
#ifndef __SSE2__
#define CLUTTER_SQRTI_ARG_10_PERCENT 5590
#else
#define CLUTTER_SQRTI_ARG_10_PERCENT INT_MAX
#endif

ClutterFixed clutter_sqrtx (ClutterFixed x);
gint         clutter_sqrti (gint         x);

ClutterFixed clutter_log2x (guint x);
guint        clutter_pow2x (ClutterFixed x);
guint        clutter_powx  (guint x, ClutterFixed y);

/* <private> */
extern inline
ClutterFixed _clutter_double_to_fixed (double value);

extern inline
gint _clutter_double_to_int (double value);

extern inline
guint _clutter_double_to_uint (double value);

G_END_DECLS

#endif /* _HAVE_CLUTTER_FIXED_H */
