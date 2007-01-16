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
 * clutter_fixed_sin:
 * @angle: a #ClutterFixed angle in radians
 *
 * Fixed point implementation of sine function
 * Return value: sine value (as fixed point).
 *
 * Since: 0.2
 */
ClutterFixed
clutter_fixed_sin (ClutterFixed angle)
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
    indx2;
    
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
 * clutter_angle_sin:
 * @angle: a #ClutterAngle
 *
 * Fast fixed point implementation of sine function.
 *
 * ClutterAngle is an integer such that that 1024 represents
 * full circle.
 * 
 * Return value: sine value (as fixed point).
 *
 * Since: 0.2
 */
ClutterFixed
clutter_angle_sin (ClutterAngle angle)
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
