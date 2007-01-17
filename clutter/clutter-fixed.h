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

#ifndef _HAVE_CLUTTER_FIXED_H
#define _HAVE_CLUTTER_FIXED_H

#include <glib.h>

G_BEGIN_DECLS

typedef gint32 ClutterFixed;
typedef gint32 ClutterAngle;    /* angle such that 1024 == 2*PI */

#define CFX_Q      16		/* Decimal part size in bits */
#define CFX_ONE    (1 << CFX_Q)	/* 1 */
#define CFX_MAX    0x7fffffff
#define CFX_MIN    0x80000000

/*
 * some commonly used constants
 */
#define CFX_PI     0x0003243f
#define CFX_2PI    0x0006487f
#define CFX_PI_2   0x00019220   /* pi/2 */
#define CFX_PI_4   0x0000c910   /* pi/4 */
#define CFX_PI8192 0x6487ed51   /* pi * 0x2000, to improve precision */    

#define CFX_360 CLUTTER_INT_TO_FIXED (360)
#define CFX_240 CLUTTER_INT_TO_FIXED (240)
#define CFX_180 CLUTTER_INT_TO_FIXED (180)
#define CFX_120 CLUTTER_INT_TO_FIXED (120)
#define CFX_60  CLUTTER_INT_TO_FIXED (60)
#define CFX_255 CLUTTER_INT_TO_FIXED (255)

#define CLUTTER_FIXED_TO_FLOAT(x) ((float)((int)(x)/65536.0))

#define CLUTTER_FIXED_TO_DOUBLE(x) ((double)((int)(x)/65536.0))

#define CLUTTER_FLOAT_TO_FIXED(x)   \
  ( (ABS(x) > 32767.0) ?            \
            (((x)/(x))*0x7fffffff)  \
          : ((long)((x) * 65536.0  + ((x) < 0 ? -0.5 : 0.5))) )


#define CLUTTER_INT_TO_FIXED(x) ((x) << CFX_Q)

#define CLUTTER_FIXED_INT(x) ((x) >> CFX_Q)

#define CLUTTER_FIXED_FRACTION(x) ((x) & ((1 << CFX_Q) - 1))

#define CLUTTER_FIXED_FLOOR(x) \
   (((x) >= 0) ? ((x) >> CFX_Q) : ~((~(x)) >> CFX_Q))

#define CLUTTER_FIXED_CEIL(x) CLUTTER_FIXED_FLOOR(x + 0xffff) 

#define CLUTTER_FIXED_MUL(x,y) ((x) >> 8) * ((y) >> 8)

#define CLUTTER_FIXED_DIV(x,y) ((((x) << 8)/(y)) << 8)

/* some handy short aliases to avoid exessively long lines */
#define CFX_INT CLUTTER_FIXED_INT
#define CFX_MUL CLUTTER_FIXED_MUL
#define CFX_DIV CLUTTER_FIXED_DIV

/* Fixed point math routines */
ClutterFixed clutter_sinx (ClutterFixed anx);
ClutterFixed clutter_sini (ClutterAngle angle);

/* convenience macros for the cos functions */
#define clutter_cosx(x) clutter_fixed_sin((x) - CFX_PI_2)
#define clutter_cosi(x) clutter_sini((x) - 256)

ClutterFixed clutter_sqrtx (ClutterFixed x);
int clutter_sqrti (int x);

G_END_DECLS

#endif
