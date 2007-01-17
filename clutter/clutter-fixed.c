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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <clutter-fixed.h>

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
    indx1 = CLUTTER_FIXED_INT (indx1);

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
    angle &= 0x7ff;
    
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
     * (There are faster algorithm's available; the Carmack 'magic'
     * algorithm, http://www.codemaestro.com/reviews/review00000105.html,
     * is about five times faster than this one when implemented
     * as fixed point, but it's error is much greater and grows with the
     * size of the argument (reaches about 10% around x == 800).
     *
     * Note: on systems with FPU, the clib sqrt can be noticeably faster
     *       than this function.
     */
    int t = 0;
    int sh = 0;
    unsigned int mask = 0x40000000;
    unsigned fract = x & 0x0000ffff;
    unsigned int d1, d2;
    
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
	/* TODO -- add i386 branch using bshr */
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
	t = CLUTTER_FIXED_INT (x);
    }

    /* Do a weighted average of the two nearest values */
    ClutterFixed v1 = sqrt_tbl[t];
    ClutterFixed v2 = sqrt_tbl[t+1];

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
 * A fixed point implementation of square root for integers
 *
 * Return value: integer square root (truncated).
 *
 *
 * Since: 0.2
 */
gint
clutter_sqrti (gint x)
{
    int t = 0;
    int sh = 0;
    unsigned int mask = 0x40000000;
    
    if (x <= 0)
	return 0;

    if (x > (sizeof (sqrt_tbl)/sizeof(ClutterFixed) - 1))
    {
	/*
	 * Find the highest bit set
	 */
#if __arm__
	/* This actually requires at least arm v5, but gcc does not seem
	 * to set the architecture defines correctly, and it is probably
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
	/* TODO -- add i386 branch using bshr */
	int bit = 30;
	while (bit >= 0)
	{
	    if (x & mask)
		break;

	    mask = (mask >> 1 | mask >> 2);
	    bit -= 2;
	}
#endif
	sh = ((bit - 6) >> 1);
	t = (x >> (bit - 6));
    }
    else
    {
	return (sqrt_tbl[x] >> CFX_Q);
    }

    x = sqrt_tbl[t];

    if (sh > 0)
	x = x << sh;
    else if (sh < 0)
	x = (x >> (1 + ~sh));
    
    return (x >> CFX_Q);
}
