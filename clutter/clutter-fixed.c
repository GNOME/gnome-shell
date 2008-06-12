/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Tomas Frydrych  <tf@openedhand.com>
 *
 * Copyright (C) 2006, 2007 OpenedHand
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <gobject/gvaluecollector.h>

#include "clutter-fixed.h"
#include "clutter-private.h"

/**
 * SECTION:clutter-fixed
 * @short_description: Fixed Point API
 *
 * Clutter has a fixed point API targeted at platforms without a
 * floating point unit, such as embedded devices. On such platforms
 * this API should be preferred to the floating point one as it does
 * not trigger the slow path of software emulation, relying on integer
 * math for fixed-to-floating and floating-to-fixed conversion.
 *
 * It is no recommened for use on platforms with a floating point unit
 * (eg desktop systems) nor for use in bindings.
 *
 * Basic rules of Fixed Point arithmethic:
 *
 * <itemizedlist>
 *   <listitem>
 *     <para>Two fixed point numbers can be directly added and
 *     subtracted.</para>
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
 *     <para>Two fixed point numbers can only be multiplied and divided by the
 *     provided #CLUTTER_FIXED_MUL and #CLUTTER_FIXED_DIV macros.</para>
 *   </listitem>
 * </itemizedlist>
 */

/* pre-computed sin table for 1st quadrant
 *
 * Currently contains 257 entries.
 *
 * The current error (compared to system sin) is about
 * 0.5% for values near the start of the table where the
 * curve is steep, but improving rapidly. If this precission
 * is not enough, we can increase the size of the table
 */
static ClutterFixed sin_tbl [] =
{
 0x00000000L, 0x00000192L, 0x00000324L, 0x000004B6L,
 0x00000648L, 0x000007DAL, 0x0000096CL, 0x00000AFEL,
 0x00000C90L, 0x00000E21L, 0x00000FB3L, 0x00001144L,
 0x000012D5L, 0x00001466L, 0x000015F7L, 0x00001787L,
 0x00001918L, 0x00001AA8L, 0x00001C38L, 0x00001DC7L,
 0x00001F56L, 0x000020E5L, 0x00002274L, 0x00002402L,
 0x00002590L, 0x0000271EL, 0x000028ABL, 0x00002A38L,
 0x00002BC4L, 0x00002D50L, 0x00002EDCL, 0x00003067L,
 0x000031F1L, 0x0000337CL, 0x00003505L, 0x0000368EL,
 0x00003817L, 0x0000399FL, 0x00003B27L, 0x00003CAEL,
 0x00003E34L, 0x00003FBAL, 0x0000413FL, 0x000042C3L,
 0x00004447L, 0x000045CBL, 0x0000474DL, 0x000048CFL,
 0x00004A50L, 0x00004BD1L, 0x00004D50L, 0x00004ECFL,
 0x0000504DL, 0x000051CBL, 0x00005348L, 0x000054C3L,
 0x0000563EL, 0x000057B9L, 0x00005932L, 0x00005AAAL,
 0x00005C22L, 0x00005D99L, 0x00005F0FL, 0x00006084L,
 0x000061F8L, 0x0000636BL, 0x000064DDL, 0x0000664EL,
 0x000067BEL, 0x0000692DL, 0x00006A9BL, 0x00006C08L,
 0x00006D74L, 0x00006EDFL, 0x00007049L, 0x000071B2L,
 0x0000731AL, 0x00007480L, 0x000075E6L, 0x0000774AL,
 0x000078ADL, 0x00007A10L, 0x00007B70L, 0x00007CD0L,
 0x00007E2FL, 0x00007F8CL, 0x000080E8L, 0x00008243L,
 0x0000839CL, 0x000084F5L, 0x0000864CL, 0x000087A1L,
 0x000088F6L, 0x00008A49L, 0x00008B9AL, 0x00008CEBL,
 0x00008E3AL, 0x00008F88L, 0x000090D4L, 0x0000921FL,
 0x00009368L, 0x000094B0L, 0x000095F7L, 0x0000973CL,
 0x00009880L, 0x000099C2L, 0x00009B03L, 0x00009C42L,
 0x00009D80L, 0x00009EBCL, 0x00009FF7L, 0x0000A130L,
 0x0000A268L, 0x0000A39EL, 0x0000A4D2L, 0x0000A605L,
 0x0000A736L, 0x0000A866L, 0x0000A994L, 0x0000AAC1L,
 0x0000ABEBL, 0x0000AD14L, 0x0000AE3CL, 0x0000AF62L,
 0x0000B086L, 0x0000B1A8L, 0x0000B2C9L, 0x0000B3E8L,
 0x0000B505L, 0x0000B620L, 0x0000B73AL, 0x0000B852L,
 0x0000B968L, 0x0000BA7DL, 0x0000BB8FL, 0x0000BCA0L,
 0x0000BDAFL, 0x0000BEBCL, 0x0000BFC7L, 0x0000C0D1L,
 0x0000C1D8L, 0x0000C2DEL, 0x0000C3E2L, 0x0000C4E4L,
 0x0000C5E4L, 0x0000C6E2L, 0x0000C7DEL, 0x0000C8D9L,
 0x0000C9D1L, 0x0000CAC7L, 0x0000CBBCL, 0x0000CCAEL,
 0x0000CD9FL, 0x0000CE8EL, 0x0000CF7AL, 0x0000D065L,
 0x0000D14DL, 0x0000D234L, 0x0000D318L, 0x0000D3FBL,
 0x0000D4DBL, 0x0000D5BAL, 0x0000D696L, 0x0000D770L,
 0x0000D848L, 0x0000D91EL, 0x0000D9F2L, 0x0000DAC4L,
 0x0000DB94L, 0x0000DC62L, 0x0000DD2DL, 0x0000DDF7L,
 0x0000DEBEL, 0x0000DF83L, 0x0000E046L, 0x0000E107L,
 0x0000E1C6L, 0x0000E282L, 0x0000E33CL, 0x0000E3F4L,
 0x0000E4AAL, 0x0000E55EL, 0x0000E610L, 0x0000E6BFL,
 0x0000E76CL, 0x0000E817L, 0x0000E8BFL, 0x0000E966L,
 0x0000EA0AL, 0x0000EAABL, 0x0000EB4BL, 0x0000EBE8L,
 0x0000EC83L, 0x0000ED1CL, 0x0000EDB3L, 0x0000EE47L,
 0x0000EED9L, 0x0000EF68L, 0x0000EFF5L, 0x0000F080L,
 0x0000F109L, 0x0000F18FL, 0x0000F213L, 0x0000F295L,
 0x0000F314L, 0x0000F391L, 0x0000F40CL, 0x0000F484L,
 0x0000F4FAL, 0x0000F56EL, 0x0000F5DFL, 0x0000F64EL,
 0x0000F6BAL, 0x0000F724L, 0x0000F78CL, 0x0000F7F1L,
 0x0000F854L, 0x0000F8B4L, 0x0000F913L, 0x0000F96EL,
 0x0000F9C8L, 0x0000FA1FL, 0x0000FA73L, 0x0000FAC5L,
 0x0000FB15L, 0x0000FB62L, 0x0000FBADL, 0x0000FBF5L,
 0x0000FC3BL, 0x0000FC7FL, 0x0000FCC0L, 0x0000FCFEL,
 0x0000FD3BL, 0x0000FD74L, 0x0000FDACL, 0x0000FDE1L,
 0x0000FE13L, 0x0000FE43L, 0x0000FE71L, 0x0000FE9CL,
 0x0000FEC4L, 0x0000FEEBL, 0x0000FF0EL, 0x0000FF30L,
 0x0000FF4EL, 0x0000FF6BL, 0x0000FF85L, 0x0000FF9CL,
 0x0000FFB1L, 0x0000FFC4L, 0x0000FFD4L, 0x0000FFE1L,
 0x0000FFECL, 0x0000FFF5L, 0x0000FFFBL, 0x0000FFFFL,
 0x00010000L,
};

/* the difference of the angle for two adjacent values in the table
 * expressed as ClutterFixed number
 */
#define CFX_SIN_STEP 0x00000192

/* <private> */
const double _magic = 68719476736.0 * 1.5;

/* Where in the 64 bits of double is the mantisa */
#if (__FLOAT_WORD_ORDER == 1234)
#define _CFX_MAN			0
#elif (__FLOAT_WORD_ORDER == 4321)
#define _CFX_MAN			1
#else
#define CFX_NO_FAST_CONVERSIONS
#endif

/*
 * clutter_double_to_fixed :
 * @value: value to be converted
 *
 * A fast conversion from double precision floating to fixed point
 *
 * Return value: Fixed point representation of the value
 *
 * Since: 0.2
 */
ClutterFixed
clutter_double_to_fixed (double val)
{
#ifdef CFX_NO_FAST_CONVERSIONS
    return (ClutterFixed)(val * (double)CFX_ONE);
#else
    union
    {
	double d;
	unsigned int i[2];
    } dbl;

    dbl.d = val;
    dbl.d = dbl.d + _magic;
    return dbl.i[_CFX_MAN];
#endif
}

/*
 * clutter_double_to_int :
 * @value: value to be converted
 *
 * A fast conversion from doulbe precision floatint point  to int;
 * used this instead of casting double/float to int.
 *
 * Return value: Integer part of the double
 *
 * Since: 0.2
 */
gint
clutter_double_to_int (double val)
{
#ifdef CFX_NO_FAST_CONVERSIONS
    return (gint)(val);
#else
    union
    {
	double d;
	unsigned int i[2];
    } dbl;

    dbl.d = val;
    dbl.d = dbl.d + _magic;
    return ((int)dbl.i[_CFX_MAN]) >> 16;
#endif
}

guint
clutter_double_to_uint (double val)
{
#ifdef CFX_NO_FAST_CONVERSIONS
    return (guint)(val);
#else
    union
    {
	double d;
	unsigned int i[2];
    } dbl;

    dbl.d = val;
    dbl.d = dbl.d + _magic;
    return (dbl.i[_CFX_MAN]) >> 16;
#endif
}

#undef _CFX_MAN

/**
 * clutter_sinx:
 * @angle: a #ClutterFixed angle in radians
 *
 * Fixed point implementation of sine function
 *
 * Return value: #ClutterFixed sine value.
 *
 * Since: 0.2
 */
ClutterFixed
clutter_sinx (ClutterFixed angle)
{
    int sign = 1, indx1, indx2;
    ClutterFixed low, high, d1, d2;

    /* convert negative angle to positive + sign */
    if ((int)angle < 0)
    {
	sign  = 1 + ~sign;
	angle = 1 + ~angle;
    }

    /* reduce to <0, 2*pi) */
    if (angle >= CFX_2PI)
    {
	ClutterFixed f = CLUTTER_FIXED_DIV (angle, CFX_2PI);
	angle = angle - f;
    }

    /* reduce to first quadrant and sign */
    if (angle > CFX_PI)
    {
	sign = 1 + ~sign;
	if (angle > CFX_PI + CFX_PI_2)
	{
	    /* fourth qudrant */
	    angle = CFX_2PI - angle;
	}
	else
	{
	    /* third quadrant */
	    angle -= CFX_PI;
	}
    }
    else
    {
	if (angle > CFX_PI_2)
	{
	    /* second quadrant */
	    angle = CFX_PI - angle;
	}
    }

    /* Calculate indices of the two nearest values in our table
     * and return weighted average
     *
     * Handle the end of the table gracefully
     */
    indx1 = CLUTTER_FIXED_DIV (angle, CFX_SIN_STEP);
    indx1 = CLUTTER_FIXED_TO_INT (indx1);

    if (indx1 == sizeof (sin_tbl)/sizeof (ClutterFixed) - 1)
    {
	indx2 = indx1;
	indx1 = indx2 - 1;
    }
    else
    {
	indx2 = indx1 + 1;
    }

    low  = sin_tbl[indx1];
    high = sin_tbl[indx2];

    d1 = angle - indx1 * CFX_SIN_STEP;
    d2 = indx2 * CFX_SIN_STEP - angle;

    angle = ((low * d2 + high * d1) / (CFX_SIN_STEP));

    if (sign < 0)
	angle = (1 + ~angle);

    return angle;
}

/**
 * clutter_sini:
 * @angle: a #ClutterAngle
 *
 * Very fast fixed point implementation of sine function.
 *
 * ClutterAngle is an integer such that 1024 represents
 * full circle.
 *
 * Return value: #ClutterFixed sine value.
 *
 * Since: 0.2
 */
ClutterFixed
clutter_sini (ClutterAngle angle)
{
    int sign = 1;
    ClutterFixed result;

    /* reduce negative angle to positive + sign */
    if (angle < 0)
    {
	sign  = 1 + ~sign;
	angle = 1 + ~angle;
    }

    /* reduce to <0, 2*pi) */
    angle &= 0x3ff;

    /* reduce to first quadrant and sign */
    if (angle > 512)
    {
	sign = 1 + ~sign;
	if (angle > 768)
	{
	    /* fourth qudrant */
	    angle = 1024 - angle;
	}
	else
	{
	    /* third quadrant */
	    angle -= 512;
	}
    }
    else
    {
	if (angle > 256)
	{
	    /* second quadrant */
	    angle = 512 - angle;
	}
    }

    result = sin_tbl[angle];

    if (sign < 0)
	result = (1 + ~result);

    return result;
}

/* pre-computed tan table for 1st quadrant
 *
 * Currently contains 257 entries.
 *
 */
static ClutterFixed tan_tbl [] =
{
  0x00000000L, 0x00000192L, 0x00000324L, 0x000004b7L,
  0x00000649L, 0x000007dbL, 0x0000096eL, 0x00000b01L,
  0x00000c94L, 0x00000e27L, 0x00000fbaL, 0x0000114eL,
  0x000012e2L, 0x00001477L, 0x0000160cL, 0x000017a1L,
  0x00001937L, 0x00001acdL, 0x00001c64L, 0x00001dfbL,
  0x00001f93L, 0x0000212cL, 0x000022c5L, 0x0000245fL,
  0x000025f9L, 0x00002795L, 0x00002931L, 0x00002aceL,
  0x00002c6cL, 0x00002e0aL, 0x00002faaL, 0x0000314aL,
  0x000032ecL, 0x0000348eL, 0x00003632L, 0x000037d7L,
  0x0000397dL, 0x00003b24L, 0x00003cccL, 0x00003e75L,
  0x00004020L, 0x000041ccL, 0x00004379L, 0x00004528L,
  0x000046d8L, 0x0000488aL, 0x00004a3dL, 0x00004bf2L,
  0x00004da8L, 0x00004f60L, 0x0000511aL, 0x000052d5L,
  0x00005492L, 0x00005651L, 0x00005812L, 0x000059d5L,
  0x00005b99L, 0x00005d60L, 0x00005f28L, 0x000060f3L,
  0x000062c0L, 0x0000648fL, 0x00006660L, 0x00006834L,
  0x00006a0aL, 0x00006be2L, 0x00006dbdL, 0x00006f9aL,
  0x0000717aL, 0x0000735dL, 0x00007542L, 0x0000772aL,
  0x00007914L, 0x00007b02L, 0x00007cf2L, 0x00007ee6L,
  0x000080dcL, 0x000082d6L, 0x000084d2L, 0x000086d2L,
  0x000088d6L, 0x00008adcL, 0x00008ce7L, 0x00008ef4L,
  0x00009106L, 0x0000931bL, 0x00009534L, 0x00009750L,
  0x00009971L, 0x00009b95L, 0x00009dbeL, 0x00009febL,
  0x0000a21cL, 0x0000a452L, 0x0000a68cL, 0x0000a8caL,
  0x0000ab0eL, 0x0000ad56L, 0x0000afa3L, 0x0000b1f5L,
  0x0000b44cL, 0x0000b6a8L, 0x0000b909L, 0x0000bb70L,
  0x0000bdddL, 0x0000c04fL, 0x0000c2c7L, 0x0000c545L,
  0x0000c7c9L, 0x0000ca53L, 0x0000cce3L, 0x0000cf7aL,
  0x0000d218L, 0x0000d4bcL, 0x0000d768L, 0x0000da1aL,
  0x0000dcd4L, 0x0000df95L, 0x0000e25eL, 0x0000e52eL,
  0x0000e806L, 0x0000eae7L, 0x0000edd0L, 0x0000f0c1L,
  0x0000f3bbL, 0x0000f6bfL, 0x0000f9cbL, 0x0000fce1L,
  0x00010000L, 0x00010329L, 0x0001065dL, 0x0001099aL,
  0x00010ce3L, 0x00011036L, 0x00011394L, 0x000116feL,
  0x00011a74L, 0x00011df6L, 0x00012184L, 0x0001251fL,
  0x000128c6L, 0x00012c7cL, 0x0001303fL, 0x00013410L,
  0x000137f0L, 0x00013bdfL, 0x00013fddL, 0x000143ebL,
  0x00014809L, 0x00014c37L, 0x00015077L, 0x000154c9L,
  0x0001592dL, 0x00015da4L, 0x0001622eL, 0x000166ccL,
  0x00016b7eL, 0x00017045L, 0x00017523L, 0x00017a17L,
  0x00017f22L, 0x00018444L, 0x00018980L, 0x00018ed5L,
  0x00019445L, 0x000199cfL, 0x00019f76L, 0x0001a53aL,
  0x0001ab1cL, 0x0001b11dL, 0x0001b73fL, 0x0001bd82L,
  0x0001c3e7L, 0x0001ca71L, 0x0001d11fL, 0x0001d7f4L,
  0x0001def1L, 0x0001e618L, 0x0001ed6aL, 0x0001f4e8L,
  0x0001fc96L, 0x00020473L, 0x00020c84L, 0x000214c9L,
  0x00021d44L, 0x000225f9L, 0x00022ee9L, 0x00023818L,
  0x00024187L, 0x00024b3aL, 0x00025534L, 0x00025f78L,
  0x00026a0aL, 0x000274edL, 0x00028026L, 0x00028bb8L,
  0x000297a8L, 0x0002a3fbL, 0x0002b0b5L, 0x0002bdddL,
  0x0002cb79L, 0x0002d98eL, 0x0002e823L, 0x0002f740L,
  0x000306ecL, 0x00031730L, 0x00032816L, 0x000339a6L,
  0x00034bebL, 0x00035ef2L, 0x000372c6L, 0x00038776L,
  0x00039d11L, 0x0003b3a6L, 0x0003cb48L, 0x0003e40aL,
  0x0003fe02L, 0x00041949L, 0x000435f7L, 0x0004542bL,
  0x00047405L, 0x000495a9L, 0x0004b940L, 0x0004def6L,
  0x00050700L, 0x00053196L, 0x00055ef9L, 0x00058f75L,
  0x0005c35dL, 0x0005fb14L, 0x00063709L, 0x000677c0L,
  0x0006bdd0L, 0x000709ecL, 0x00075ce6L, 0x0007b7bbL,
  0x00081b98L, 0x000889e9L, 0x0009046eL, 0x00098d4dL,
  0x000a2736L, 0x000ad593L, 0x000b9cc6L, 0x000c828aL,
  0x000d8e82L, 0x000ecb1bL, 0x001046eaL, 0x00121703L,
  0x00145b00L, 0x0017448dL, 0x001b2672L, 0x002095afL,
  0x0028bc49L, 0x0036519aL, 0x00517bb6L, 0x00a2f8fdL,
  0x46d3eab2L,
};

/**
 * clutter_tani:
 * @angle: a #ClutterAngle
 *
 * Very fast fixed point implementation of tan function.
 *
 * ClutterAngle is an integer such that 1024 represents
 * full circle.
 *
 * Return value: #ClutterFixed sine value.
 *
 * Since: 0.3
 */
ClutterFixed
clutter_tani (ClutterAngle angle)
{
    int sign = 1;
    ClutterFixed result;

    /* reduce negative angle to positive + sign */
    if (angle < 0)
    {
	sign  = 1 + ~sign;
	angle = 1 + ~angle;
    }

    /* reduce to <0,  pi) */
    angle &= 0x1ff;

    /* reduce to first quadrant and sign */
    if (angle > 256)
    {
	sign = 1 + ~sign;
	angle = 512 - angle;
    }

    result = tan_tbl[angle];

    if (sign < 0)
	result = (1 + ~result);

    return result;
}

/* 257-value table of atan. atan_tbl[0] is atan(0.0) and atan_tbl[256]
   is atan(1). The angles are radians in ClutterFixed
   truncated to 16-bit (they're all less than one) */
static guint16 atan_tbl[] = 
  {
    0x0000, 0x00FF, 0x01FF, 0x02FF, 0x03FF, 0x04FF, 0x05FF, 0x06FF,
    0x07FF, 0x08FF, 0x09FE, 0x0AFE, 0x0BFD, 0x0CFD, 0x0DFC, 0x0EFB,
    0x0FFA, 0x10F9, 0x11F8, 0x12F7, 0x13F5, 0x14F3, 0x15F2, 0x16F0,
    0x17EE, 0x18EB, 0x19E9, 0x1AE6, 0x1BE3, 0x1CE0, 0x1DDD, 0x1ED9,
    0x1FD5, 0x20D1, 0x21CD, 0x22C8, 0x23C3, 0x24BE, 0x25B9, 0x26B3,
    0x27AD, 0x28A7, 0x29A1, 0x2A9A, 0x2B93, 0x2C8B, 0x2D83, 0x2E7B,
    0x2F72, 0x306A, 0x3160, 0x3257, 0x334D, 0x3442, 0x3538, 0x362D,
    0x3721, 0x3815, 0x3909, 0x39FC, 0x3AEF, 0x3BE2, 0x3CD4, 0x3DC5,
    0x3EB6, 0x3FA7, 0x4097, 0x4187, 0x4277, 0x4365, 0x4454, 0x4542,
    0x462F, 0x471C, 0x4809, 0x48F5, 0x49E0, 0x4ACB, 0x4BB6, 0x4CA0,
    0x4D89, 0x4E72, 0x4F5B, 0x5043, 0x512A, 0x5211, 0x52F7, 0x53DD,
    0x54C2, 0x55A7, 0x568B, 0x576F, 0x5852, 0x5934, 0x5A16, 0x5AF7,
    0x5BD8, 0x5CB8, 0x5D98, 0x5E77, 0x5F55, 0x6033, 0x6110, 0x61ED,
    0x62C9, 0x63A4, 0x647F, 0x6559, 0x6633, 0x670C, 0x67E4, 0x68BC,
    0x6993, 0x6A6A, 0x6B40, 0x6C15, 0x6CEA, 0x6DBE, 0x6E91, 0x6F64,
    0x7036, 0x7108, 0x71D9, 0x72A9, 0x7379, 0x7448, 0x7516, 0x75E4,
    0x76B1, 0x777E, 0x7849, 0x7915, 0x79DF, 0x7AA9, 0x7B72, 0x7C3B,
    0x7D03, 0x7DCA, 0x7E91, 0x7F57, 0x801C, 0x80E1, 0x81A5, 0x8269,
    0x832B, 0x83EE, 0x84AF, 0x8570, 0x8630, 0x86F0, 0x87AF, 0x886D,
    0x892A, 0x89E7, 0x8AA4, 0x8B5F, 0x8C1A, 0x8CD5, 0x8D8E, 0x8E47,
    0x8F00, 0x8FB8, 0x906F, 0x9125, 0x91DB, 0x9290, 0x9345, 0x93F9,
    0x94AC, 0x955F, 0x9611, 0x96C2, 0x9773, 0x9823, 0x98D2, 0x9981,
    0x9A2F, 0x9ADD, 0x9B89, 0x9C36, 0x9CE1, 0x9D8C, 0x9E37, 0x9EE0,
    0x9F89, 0xA032, 0xA0DA, 0xA181, 0xA228, 0xA2CE, 0xA373, 0xA418,
    0xA4BC, 0xA560, 0xA602, 0xA6A5, 0xA746, 0xA7E8, 0xA888, 0xA928,
    0xA9C7, 0xAA66, 0xAB04, 0xABA1, 0xAC3E, 0xACDB, 0xAD76, 0xAE11,
    0xAEAC, 0xAF46, 0xAFDF, 0xB078, 0xB110, 0xB1A7, 0xB23E, 0xB2D5,
    0xB36B, 0xB400, 0xB495, 0xB529, 0xB5BC, 0xB64F, 0xB6E2, 0xB773,
    0xB805, 0xB895, 0xB926, 0xB9B5, 0xBA44, 0xBAD3, 0xBB61, 0xBBEE,
    0xBC7B, 0xBD07, 0xBD93, 0xBE1E, 0xBEA9, 0xBF33, 0xBFBC, 0xC046,
    0xC0CE, 0xC156, 0xC1DD, 0xC264, 0xC2EB, 0xC371, 0xC3F6, 0xC47B,
    0xC4FF, 0xC583, 0xC606, 0xC689, 0xC70B, 0xC78D, 0xC80E, 0xC88F,
    0xC90F
  };

/**
 * clutter_atani:
 * @x: The tangent to calculate the angle for
 *
 * Fast fixed-point version of the arctangent function.
 *
 * Return value: The angle in radians represented as a #ClutterFixed
 * for which the tangent is @x.
 */
ClutterFixed
clutter_atani (ClutterFixed x)
{
  gboolean negative = FALSE;
  ClutterFixed angle;

  if (x < 0)
    {
      negative = TRUE;
      x = -x;
    }

  if (x > CFX_ONE)
    /* if x > 1 then atan(x) = pi/2 - atan(1/x) */
    angle = CFX_PI / 2 - atan_tbl[CFX_QDIV (CFX_ONE, x) >> 8];
  else
    angle = atan_tbl[x >> 8];

  return negative ? -angle : angle;
}

/**
 * clutter_atan2i:
 * @y: Numerator of tangent
 * @x: Denominator of tangent
 *
 * Calculates the arctangent of @y / @x but uses the sign of both
 * arguments to return the angle in right quadrant.
 *
 * Return value: The arctangent of @y / @x
 */
ClutterFixed
clutter_atan2i (ClutterFixed y, ClutterFixed x)
{
  ClutterFixed angle;

  if (x == 0)
    angle = y >= 0 ? CFX_PI_2 : -CFX_PI_2;
  else
    {
      angle = clutter_atani (CFX_QDIV (y, x));

      if (x < 0)
	angle += y >= 0 ? CFX_PI : -CFX_PI;
    }

  return angle;
}

ClutterFixed sqrt_tbl [] =
{
 0x00000000L, 0x00010000L, 0x00016A0AL, 0x0001BB68L,
 0x00020000L, 0x00023C6FL, 0x00027312L, 0x0002A550L,
 0x0002D414L, 0x00030000L, 0x0003298BL, 0x0003510EL,
 0x000376CFL, 0x00039B05L, 0x0003BDDDL, 0x0003DF7CL,
 0x00040000L, 0x00041F84L, 0x00043E1EL, 0x00045BE1L,
 0x000478DEL, 0x00049524L, 0x0004B0BFL, 0x0004CBBCL,
 0x0004E624L, 0x00050000L, 0x00051959L, 0x00053237L,
 0x00054AA0L, 0x0005629AL, 0x00057A2BL, 0x00059159L,
 0x0005A828L, 0x0005BE9CL, 0x0005D4B9L, 0x0005EA84L,
 0x00060000L, 0x00061530L, 0x00062A17L, 0x00063EB8L,
 0x00065316L, 0x00066733L, 0x00067B12L, 0x00068EB4L,
 0x0006A21DL, 0x0006B54DL, 0x0006C847L, 0x0006DB0CL,
 0x0006ED9FL, 0x00070000L, 0x00071232L, 0x00072435L,
 0x0007360BL, 0x000747B5L, 0x00075935L, 0x00076A8CL,
 0x00077BBBL, 0x00078CC2L, 0x00079DA3L, 0x0007AE60L,
 0x0007BEF8L, 0x0007CF6DL, 0x0007DFBFL, 0x0007EFF0L,
 0x00080000L, 0x00080FF0L, 0x00081FC1L, 0x00082F73L,
 0x00083F08L, 0x00084E7FL, 0x00085DDAL, 0x00086D18L,
 0x00087C3BL, 0x00088B44L, 0x00089A32L, 0x0008A906L,
 0x0008B7C2L, 0x0008C664L, 0x0008D4EEL, 0x0008E361L,
 0x0008F1BCL, 0x00090000L, 0x00090E2EL, 0x00091C45L,
 0x00092A47L, 0x00093834L, 0x0009460CL, 0x000953CFL,
 0x0009617EL, 0x00096F19L, 0x00097CA1L, 0x00098A16L,
 0x00099777L, 0x0009A4C6L, 0x0009B203L, 0x0009BF2EL,
 0x0009CC47L, 0x0009D94FL, 0x0009E645L, 0x0009F32BL,
 0x000A0000L, 0x000A0CC5L, 0x000A1979L, 0x000A261EL,
 0x000A32B3L, 0x000A3F38L, 0x000A4BAEL, 0x000A5816L,
 0x000A646EL, 0x000A70B8L, 0x000A7CF3L, 0x000A8921L,
 0x000A9540L, 0x000AA151L, 0x000AAD55L, 0x000AB94BL,
 0x000AC534L, 0x000AD110L, 0x000ADCDFL, 0x000AE8A1L,
 0x000AF457L, 0x000B0000L, 0x000B0B9DL, 0x000B172DL,
 0x000B22B2L, 0x000B2E2BL, 0x000B3998L, 0x000B44F9L,
 0x000B504FL, 0x000B5B9AL, 0x000B66D9L, 0x000B720EL,
 0x000B7D37L, 0x000B8856L, 0x000B936AL, 0x000B9E74L,
 0x000BA973L, 0x000BB467L, 0x000BBF52L, 0x000BCA32L,
 0x000BD508L, 0x000BDFD5L, 0x000BEA98L, 0x000BF551L,
 0x000C0000L, 0x000C0AA6L, 0x000C1543L, 0x000C1FD6L,
 0x000C2A60L, 0x000C34E1L, 0x000C3F59L, 0x000C49C8L,
 0x000C542EL, 0x000C5E8CL, 0x000C68E0L, 0x000C732DL,
 0x000C7D70L, 0x000C87ACL, 0x000C91DFL, 0x000C9C0AL,
 0x000CA62CL, 0x000CB047L, 0x000CBA59L, 0x000CC464L,
 0x000CCE66L, 0x000CD861L, 0x000CE254L, 0x000CEC40L,
 0x000CF624L, 0x000D0000L, 0x000D09D5L, 0x000D13A2L,
 0x000D1D69L, 0x000D2727L, 0x000D30DFL, 0x000D3A90L,
 0x000D4439L, 0x000D4DDCL, 0x000D5777L, 0x000D610CL,
 0x000D6A9AL, 0x000D7421L, 0x000D7DA1L, 0x000D871BL,
 0x000D908EL, 0x000D99FAL, 0x000DA360L, 0x000DACBFL,
 0x000DB618L, 0x000DBF6BL, 0x000DC8B7L, 0x000DD1FEL,
 0x000DDB3DL, 0x000DE477L, 0x000DEDABL, 0x000DF6D8L,
 0x000E0000L, 0x000E0922L, 0x000E123DL, 0x000E1B53L,
 0x000E2463L, 0x000E2D6DL, 0x000E3672L, 0x000E3F70L,
 0x000E4869L, 0x000E515DL, 0x000E5A4BL, 0x000E6333L,
 0x000E6C16L, 0x000E74F3L, 0x000E7DCBL, 0x000E869DL,
 0x000E8F6BL, 0x000E9832L, 0x000EA0F5L, 0x000EA9B2L,
 0x000EB26BL, 0x000EBB1EL, 0x000EC3CBL, 0x000ECC74L,
 0x000ED518L, 0x000EDDB7L, 0x000EE650L, 0x000EEEE5L,
 0x000EF775L, 0x000F0000L, 0x000F0886L, 0x000F1107L,
 0x000F1984L, 0x000F21FCL, 0x000F2A6FL, 0x000F32DDL,
 0x000F3B47L, 0x000F43ACL, 0x000F4C0CL, 0x000F5468L,
 0x000F5CBFL, 0x000F6512L, 0x000F6D60L, 0x000F75AAL,
 0x000F7DEFL, 0x000F8630L, 0x000F8E6DL, 0x000F96A5L,
 0x000F9ED9L, 0x000FA709L, 0x000FAF34L, 0x000FB75BL,
 0x000FBF7EL, 0x000FC79DL, 0x000FCFB7L, 0x000FD7CEL,
 0x000FDFE0L, 0x000FE7EEL, 0x000FEFF8L, 0x000FF7FEL,
 0x00100000L,
};

/**
 * clutter_sqrtx:
 * @x: a #ClutterFixed
 *
 * A fixed point implementation of squre root
 *
 * Return value: #ClutterFixed square root.
 *
 * Since: 0.2
 */
ClutterFixed
clutter_sqrtx (ClutterFixed x)
{
    /* The idea for this comes from the Alegro library, exploiting the
     * fact that,
     *            sqrt (x) = sqrt (x/d) * sqrt (d);
     *
     *            For d == 2^(n):
     *
     *            sqrt (x) = sqrt (x/2^(2n)) * 2^n
     *
     * By locating suitable n for given x such that x >> 2n is in <0,255>
     * we can use a LUT of precomputed values.
     *
     * This algorithm provides both good performance and precission;
     * on ARM this function is about 5 times faster than c-lib sqrt, whilst
     * producing errors < 1%.
     *
     */
    int t = 0;
    int sh = 0;
    unsigned int mask = 0x40000000;
    unsigned fract = x & 0x0000ffff;
    unsigned int d1, d2;
    ClutterFixed v1, v2;

    if (x <= 0)
	return 0;

    if (x > CFX_255 || x < CFX_ONE)
    {
	/*
	 * Find the highest bit set
	 */
#if __arm__
	/* This actually requires at least arm v5, but gcc does not seem
	 * to set the architecture defines correctly, and it is I think
	 * very unlikely that anyone will want to use clutter on anything
	 * less than v5.
	 */
	int bit;
	__asm__ ("clz  %0, %1\n"
		 "rsb  %0, %0, #31\n"
		 :"=r"(bit)
		 :"r" (x));

	/* make even (2n) */
	bit &= 0xfffffffe;
#else
	/* TODO -- add i386 branch using bshr
	 *
	 * NB: it's been said that the bshr instruction is poorly implemented
	 *     and that it is possible to write a faster code in C using binary
	 *     search -- at some point we should explore this
	 */
	int bit = 30;
	while (bit >= 0)
	{
	    if (x & mask)
		break;

	    mask = (mask >> 1 | mask >> 2);
	    bit -= 2;
	}
#endif

	/* now bit indicates the highest bit set; there are two scenarios
	 *
	 * 1) bit < 23:  Our number is smaller so we shift it left to maximase
	 *               precision (< 16 really, since <16,23> never goes
	 *               through here.
	 *
	 * 2) bit > 23:  our number is above the table, so we shift right
	 */

	sh = ((bit - 22) >> 1);
	if (bit >= 8)
	    t = (x >> (16 - 22 + bit));
	else
	    t = (x << (22 - 16 - bit));
    }
    else
    {
	t = CLUTTER_FIXED_TO_INT (x);
    }

    /* Do a weighted average of the two nearest values */
    v1 = sqrt_tbl[t];
    v2 = sqrt_tbl[t+1];

    /*
     * 12 is fairly arbitrary -- we want integer that is not too big to cost
     * us precission
     */
    d1 = (unsigned)(fract) >> 12;
    d2 = ((unsigned)CFX_ONE >> 12) - d1;

    x = ((v1*d2) + (v2*d1))/(CFX_ONE >> 12);

    if (sh > 0)
	x = x << sh;
    else if (sh < 0)
	x = (x >> (1 + ~sh));

    return x;
}

/**
 * clutter_sqrti:
 * @x: integer value
 *
 * Very fast fixed point implementation of square root for integers.
 *
 * This function is at least 6x faster than clib sqrt() on x86, and (this is
 * not a typo!) about 500x faster on ARM without FPU. It's error is < 5%
 * for arguments < #CLUTTER_SQRTI_ARG_5_PERCENT and < 10% for arguments <
 * #CLUTTER_SQRTI_ARG_10_PERCENT. The maximum argument that can be passed to
 * this function is CLUTTER_SQRTI_ARG_MAX.
 *
 * Return value: integer square root.
 *
 *
 * Since: 0.2
 */
gint
clutter_sqrti (gint number)
{
#if defined __SSE2__
    /* The GCC built-in with SSE2 (sqrtsd) is up to twice as fast as
     * the pure integer code below. It is also more accurate.
     */
    return __builtin_sqrt (number);
#else
    /* This is a fixed point implementation of the Quake III sqrt algorithm,
     * described, for example, at
     *   http://www.codemaestro.com/reviews/review00000105.html
     *
     * While the original QIII is extremely fast, the use of floating division
     * and multiplication makes it perform very on arm processors without FPU.
     *
     * The key to successfully replacing the floating point operations with
     * fixed point is in the choice of the fixed point format. The QIII
     * algorithm does not calculate the square root, but its reciprocal ('y'
     * below), which is only at the end turned to the inverse value. In order
     * for the algorithm to produce satisfactory results, the reciprocal value
     * must be represented with sufficient precission; the 16.16 we use
     * elsewhere in clutter is not good enough, and 10.22 is used instead.
     */
    ClutterFixed x;
    guint32 y_1;        /* 10.22 fixed point */
    guint32 f = 0x600000; /* '1.5' as 10.22 fixed */

    union
    {
	float f;
	guint32 i;
    } flt, flt2;

    flt.f = number;

    x = CLUTTER_INT_TO_FIXED (number) / 2;

    /* The QIII initial estimate */
    flt.i = 0x5f3759df - ( flt.i >> 1 );

    /* Now, we convert the float to 10.22 fixed. We exploit the mechanism
     * described at http://www.d6.com/users/checker/pdfs/gdmfp.pdf.
     *
     * We want 22 bit fraction; a single precission float uses 23 bit
     * mantisa, so we only need to add 2^(23-22) (no need for the 1.5
     * multiplier as we are only dealing with positive numbers).
     *
     * Note: we have to use two separate variables here -- for some reason,
     * if we try to use just the flt variable, gcc on ARM optimises the whole
     * addition out, and it all goes pear shape, since without it, the bits
     * in the float will not be correctly aligned.
     */
    flt2.f = flt.f + 2.0;
    flt2.i &= 0x7FFFFF;

    /* Now we correct the estimate */
    y_1 = (flt2.i >> 11) * (flt2.i >> 11);
    y_1 = (y_1 >> 8) * (x >> 8);

    y_1 = f - y_1;
    flt2.i = (flt2.i >> 11) * (y_1 >> 11);

    /* If the original argument is less than 342, we do another
     * iteration to improve precission (for arguments >= 342, the single
     * iteration produces generally better results).
     */
    if (x < 171)
      {
	y_1 = (flt2.i >> 11) * (flt2.i >> 11);
	y_1 = (y_1 >> 8) * (x >> 8);

	y_1 = f - y_1;
	flt2.i = (flt2.i >> 11) * (y_1 >> 11);
      }

    /* Invert, round and convert from 10.22 to an integer
     * 0x1e3c68 is a magical rounding constant that produces slightly
     * better results than 0x200000.
     */
    return (number * flt2.i + 0x1e3c68) >> 22;
#endif
}

/**
 * clutter_qmulx:
 * @op1: #ClutterFixed
 * @op2: #ClutterFixed
 *
 * Multiplies two fixed values using 64bit arithmetic; this provides
 * significantly better precission than the #CLUTTER_FIXED_MUL macro,
 * but at performance cost (about 2.7 times slowdown on ARMv5e, and 2 times
 * on x86).
 *
 * Return value: the result of the operation
 *
 * Since: 0.4
 */
ClutterFixed
clutter_qmulx (ClutterFixed op1, ClutterFixed op2)
{
#ifdef __arm__
    /* This provides about 12% speedeup on the gcc -O2 optimised
     * C version
     *
     * Based on code found in the following thread:
     * http://lists.mplayerhq.hu/pipermail/ffmpeg-devel/2006-August/014405.html
     */
    int res_low, res_hi;

    __asm__ ("smull %0, %1, %2, %3     \n"
	     "mov   %0, %0,     lsr %4 \n"
	     "add   %1, %0, %1, lsl %5 \n"
	     : "=r"(res_hi), "=r"(res_low)\
	     : "r"(op1), "r"(op2), "i"(CFX_Q), "i"(32-CFX_Q));

    return (ClutterFixed) res_low;
#else
    long long r = (long long) op1 * (long long) op2;

    return (unsigned int)(r >> CFX_Q);
#endif
}

/**
 * clutter_qdivx:
 * @op1: #ClutterFixed
 * @op2: #ClutterFixed
 *
 * Divides two fixed values using 64bit arithmetic; this provides
 * significantly better precission than the #CLUTTER_FIXED_DIV macro,
 * but at performance cost.
 *
 * Return value: #ClutterFixed
 *
 * Since: 0.4
 */
ClutterFixed
clutter_qdivx (ClutterFixed op1,
               ClutterFixed op2)
{
  return (ClutterFixed) ((((gint64) op1) << CFX_Q) / op2);
}

/*
 * The log2x() and pow2x() functions
 *
 * The implementation of the log2x() and pow2x() exploits the well-documented
 * fact that the exponent part of IEEE floating number provides a good estimate
 * of log2 of that number, while the mantisa serves as a good error-correction.
 *
 * The implemenation here uses a quadratic error correction as described by
 * Ian Stephenson at http://www.dctsystems.co.uk/Software/power.html.
 */

/**
 * clutter_log2x :
 * @x: value to calculate base 2 logarithm from
 *
 * Calculates base 2 logarithm.
 *
 * This function is some 2.5 times faster on x86, and over 12 times faster on
 * fpu-less arm, than using libc log().
 *
 * Return value: base 2 logarithm.
 *
 * Since: 0.4
 */
ClutterFixed
clutter_log2x (guint x)
{
  /* Note: we could easily have a version for ClutterFixed x, but the int
   *       precission is enough for the current purposes.
   */
  union
  {
    float        f;
    ClutterFixed i;
  } flt;

  ClutterFixed magic = 0x58bb;
  ClutterFixed y;

  /*
   * Convert x to float, then extract exponent.
   *
   * We want the result to be 16.16 fixed, so we shift (23-16) bits only
   */
  flt.f = x;
  flt.i >>= 7;
  flt.i -= CLUTTER_INT_TO_FIXED (127);

  y = CLUTTER_FIXED_FRACTION (flt.i);

  y = CFX_MUL ((y - CFX_MUL (y, y)), magic);

  return flt.i + y;
}

/**
 * clutter_pow2x :
 * @x: exponent
 *
 * Calculates 2 to x power.
 *
 * This function is around 11 times faster on x86, and around 22 times faster
 * on fpu-less arm than libc pow(2, x).
 *
 * Return value: 2 in x power.
 *
 * Since: 0.4
 */
guint
clutter_pow2x (ClutterFixed x)
{
  /* Note: we could easily have a version that produces ClutterFixed result,
   *       but the the range would be limited to x < 15, and the int precission
   *       is enough for the current purposes.
   */

  union
  {
    float        f;
    guint32      i;
  } flt;

  ClutterFixed magic = 0x56f7;
  ClutterFixed y;

  flt.i = x;

  /*
   * Reverse of the log2x function -- convert the fixed value to a suitable
   * floating point exponent, and mantisa adjusted with quadratic error
   * correction y.
   */
  y = CLUTTER_FIXED_FRACTION (x);
  y = CFX_MUL ((y - CFX_MUL (y, y)), magic);

  /* Shift the exponent into it's position in the floating point
   * representation; as our number is not int but 16.16 fixed, shift only
   * by (23 - 16)
   */
  flt.i += (CLUTTER_INT_TO_FIXED (127) - y);
  flt.i <<= 7;

  return CLUTTER_FLOAT_TO_UINT (flt.f);
}


/**
 * clutter_powx :
 * @x: base
 * @y: #ClutterFixed exponent
 *
 * Calculates x to y power. (Note, if x is a constant it will be faster to
 * calculate the power as clutter_pow2x (CLUTTER_FIXED_MUL(y, log2 (x)))
 *
 * Return value: x in y power.
 *
 * Since: 0.4
 */
guint
clutter_powx (guint x, ClutterFixed y)
{
  return clutter_pow2x (CFX_MUL (y, clutter_log2x (x)));
}

static GTypeInfo _info = {
  0,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  0,
  0,
  NULL,
  NULL,
};

static GTypeFundamentalInfo _finfo = { 0, };

static void
clutter_value_init_fixed (GValue *value)
{
  value->data[0].v_int = 0;
}

static void
clutter_value_copy_fixed (const GValue *src,
                          GValue       *dest)
{
  dest->data[0].v_int = src->data[0].v_int;
}

static gchar *
clutter_value_collect_fixed (GValue      *value,
                             guint        n_collect_values,
                             GTypeCValue *collect_values,
                             guint        collect_flags)
{
  value->data[0].v_int = collect_values[0].v_int;

  return NULL;
}

static gchar *
clutter_value_lcopy_fixed (const GValue *value,
                           guint         n_collect_values,
                           GTypeCValue  *collect_values,
                           guint         collect_flags)
{
  gint32 *fixed_p = collect_values[0].v_pointer;

  if (!fixed_p)
    return g_strdup_printf ("value location for `%s' passed as NULL",
                            G_VALUE_TYPE_NAME (value));

  *fixed_p = value->data[0].v_int;

  return NULL;
}

static void
clutter_value_transform_fixed_int (const GValue *src,
                                   GValue       *dest)
{
  dest->data[0].v_int = CLUTTER_FIXED_TO_INT (src->data[0].v_int);
}

static void
clutter_value_transform_fixed_double (const GValue *src,
                                      GValue       *dest)
{
  dest->data[0].v_double = CLUTTER_FIXED_TO_DOUBLE (src->data[0].v_int);
}

static void
clutter_value_transform_fixed_float (const GValue *src,
                                     GValue       *dest)
{
  dest->data[0].v_float = CLUTTER_FIXED_TO_FLOAT (src->data[0].v_int);
}

static void
clutter_value_transform_int_fixed (const GValue *src,
                                   GValue       *dest)
{
  dest->data[0].v_int = CLUTTER_INT_TO_FIXED (src->data[0].v_int);
}

static void
clutter_value_transform_double_fixed (const GValue *src,
                                      GValue       *dest)
{
  dest->data[0].v_int = CLUTTER_FLOAT_TO_FIXED (src->data[0].v_double);
}

static void
clutter_value_transform_float_fixed (const GValue *src,
                                     GValue       *dest)
{
  dest->data[0].v_int = CLUTTER_FLOAT_TO_FIXED (src->data[0].v_float);
}


static const GTypeValueTable _clutter_fixed_value_table = {
  clutter_value_init_fixed,
  NULL,
  clutter_value_copy_fixed,
  NULL,
  "i",
  clutter_value_collect_fixed,
  "p",
  clutter_value_lcopy_fixed
};

GType
clutter_fixed_get_type (void)
{
  static GType _clutter_fixed_type = 0;

  if (G_UNLIKELY (_clutter_fixed_type == 0))
    {
      _info.value_table = & _clutter_fixed_value_table;
      _clutter_fixed_type =
        g_type_register_fundamental (g_type_fundamental_next (),
                                     I_("ClutterFixed"),
                                     &_info, &_finfo, 0);

      g_value_register_transform_func (_clutter_fixed_type, G_TYPE_INT,
                                       clutter_value_transform_fixed_int);
      g_value_register_transform_func (G_TYPE_INT, _clutter_fixed_type,
                                       clutter_value_transform_int_fixed);
      g_value_register_transform_func (_clutter_fixed_type, G_TYPE_FLOAT,
                                       clutter_value_transform_fixed_float);
      g_value_register_transform_func (G_TYPE_FLOAT, _clutter_fixed_type,
                                       clutter_value_transform_float_fixed);
      g_value_register_transform_func (_clutter_fixed_type, G_TYPE_DOUBLE,
                                       clutter_value_transform_fixed_double);
      g_value_register_transform_func (G_TYPE_DOUBLE, _clutter_fixed_type,
                                       clutter_value_transform_double_fixed);
    }

  return _clutter_fixed_type;
}

/**
 * clutter_value_set_fixed:
 * @value: a #GValue initialized to #CLUTTER_TYPE_FIXED
 * @fixed_: the fixed point value to set
 *
 * Sets @value to @fixed_.
 *
 * Since: 0.8
 */
void
clutter_value_set_fixed (GValue       *value,
                         ClutterFixed  fixed_)
{
  g_return_if_fail (CLUTTER_VALUE_HOLDS_FIXED (value));

  value->data[0].v_int = fixed_;
}

/**
 * clutter_value_get_fixed:
 * @value: a #GValue initialized to #CLUTTER_TYPE_FIXED
 *
 * Gets the fixed point value stored inside @value.
 *
 * Return value: the value inside the passed #GValue
 *
 * Since: 0.8
 */
ClutterFixed
clutter_value_get_fixed (const GValue *value)
{
  g_return_val_if_fail (CLUTTER_VALUE_HOLDS_FIXED (value), 0);

  return value->data[0].v_int;
}

static void
param_fixed_init (GParamSpec *pspec)
{
  ClutterParamSpecFixed *fspec = CLUTTER_PARAM_SPEC_FIXED (pspec);

  fspec->minimum = CLUTTER_MINFIXED;
  fspec->maximum = CLUTTER_MAXFIXED;
  fspec->default_value = 0;
}

static void
param_fixed_set_default (GParamSpec *pspec,
                         GValue     *value)
{
  value->data[0].v_int = CLUTTER_PARAM_SPEC_FIXED (pspec)->default_value;
}

static gboolean
param_fixed_validate (GParamSpec *pspec,
                      GValue     *value)
{
  ClutterParamSpecFixed *fspec = CLUTTER_PARAM_SPEC_FIXED (pspec);
  gint oval = CLUTTER_FIXED_TO_INT (value->data[0].v_int);
  gint min, max, val;

  g_assert (CLUTTER_IS_PARAM_SPEC_FIXED (pspec));

  /* we compare the integer part of the value because the minimum
   * and maximum values cover just that part of the representation
   */

  min = fspec->minimum;
  max = fspec->maximum;
  val = CLUTTER_FIXED_TO_INT (value->data[0].v_int);

  val = CLAMP (val, min, max);
  if (val != oval)
    {
      value->data[0].v_int = val;
      return TRUE;
    }

  return FALSE;
}

static gint
param_fixed_values_cmp (GParamSpec   *pspec,
                        const GValue *value1,
                        const GValue *value2)
{
  if (value1->data[0].v_int < value2->data[0].v_int)
    return -1;
  else
    return value1->data[0].v_int > value2->data[0].v_int;
}

GType
clutter_param_fixed_get_type (void)
{
  static GType pspec_type = 0;

  if (G_UNLIKELY (pspec_type == 0))
    {
      const GParamSpecTypeInfo pspec_info = {
        sizeof (ClutterParamSpecFixed),
        16,
        param_fixed_init,
        CLUTTER_TYPE_FIXED,
        NULL,
        param_fixed_set_default,
        param_fixed_validate,
        param_fixed_values_cmp,
      };

      pspec_type = g_param_type_register_static (I_("ClutterParamSpecFixed"),
                                                 &pspec_info);
    }

  return pspec_type;
}

/**
 * clutter_param_spec_fixed:
 * @name: name of the property
 * @nick: short name
 * @blurb: description (can be translatable)
 * @minimum: lower boundary
 * @maximum: higher boundary
 * @default_value: default value
 * @flags: flags for the param spec
 *
 * Creates a #GParamSpec for properties using #ClutterFixed values
 *
 * Return value: the newly created #GParamSpec
 *
 * Since: 0.8
 */
GParamSpec *
clutter_param_spec_fixed (const gchar *name,
                          const gchar *nick,
                          const gchar *blurb,
                          ClutterUnit  minimum,
                          ClutterUnit  maximum,
                          ClutterUnit  default_value,
                          GParamFlags  flags)
{
  ClutterParamSpecFixed *fspec;

  g_return_val_if_fail (default_value >= minimum && default_value <= maximum,
                        NULL);

  fspec = g_param_spec_internal (CLUTTER_TYPE_PARAM_FIXED,
                                 name, nick, blurb,
                                 flags);
  fspec->minimum = minimum;
  fspec->maximum = maximum;
  fspec->default_value = default_value;

  return G_PARAM_SPEC (fspec);
}
