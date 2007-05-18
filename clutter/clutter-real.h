/* -*- mode:C; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Tomas Frydrych  <tf@openedhand.com>
 *
 * Copyright (C) 2007 OpenedHand
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

/**
 * SECTION:clutter-real
 * @short_description: An abstract numeric type encapsulating either float
 * or fixed point number, depending whether clutter was configured using
 * FPU or not.
 *
 * Since: 0.4
 */

#ifndef _HAVE_CLUTTER_REAL_H
#define _HAVE_CLUTTER_REAL_H

#include "clutter-fixed.h"

#if 1
#if CLUTTER_NO_FPU

#define CLUTTER_REAL_IS_FIXED() 1
#define CLUTTER_REAL_IS_FLOAT() 0

typedef ClutterFixed ClutterReal;

#define CLUTTER_REAL_MUL(x,y) CFX_MUL((x),(y))
#define CLUTTER_REAL_DIV(x,y) CFX_DIV((x),(y))
#define CLUTTER_REAL_ADD_INT(x,i) ((x) + CLUTTER_INT_TO_FIXED(i))
#define CLUTTER_REAL_SUB_INT(x,i) ((x) - CLUTTER_INT_TO_FIXED(i))

#define CLUTTER_REAL_TO_INT(x) CFX_INT((x) + (CFX_ONE >> 1))
#define CLUTTER_REAL_FROM_INT(i) CLUTTER_INT_TO_FIXED(i)

#define CLUTTER_REAL_TO_FLOAT(x) CLUTTER_FIXED_TO_FLOAT(x)
#define CLUTTER_REAL_FROM_FLOAT(f) CLUTTER_FLOAT_TO_FIXED(f)

#define CLUTTER_REAL_TO_FIXED(x) (x)
#define CLUTTER_REAL_FROM_FIXED(x) (x)

#define CLUTTER_REAL_ZERO 0

#else

#define CLUTTER_REAL_IS_FIXED() 0
#define CLUTTER_REAL_IS_FLOAT() 1

typedef float ClutterReal;

#define CLUTTER_REAL_MUL(x,y) ((x)*(y))
#define CLUTTER_REAL_DIV(x,y) ((x)/(y))
#define CLUTTER_REAL_ADD_INT(x,i) (x+i)
#define CLUTTER_REAL_SUB_INT(x,i) (x-i)

#define CLUTTER_REAL_TO_INT(x) CLUTTER_FLOAT_TO_INT(x+0.5)
#define CLUTTER_REAL_FROM_INT(i) ((float)i)

#define CLUTTER_REAL_TO_FLOAT(x) (x)
#define CLUTTER_REAL_FROM_FLOAT(f) (f)

#define CLUTTER_REAL_TO_FIXED(x) CLLUTER_FLOAT_TO_FIXED(x)
#define CLUTTER_REAL_FROM_FIXED(x) CLUTTER_FIXED_TO_FLOAT(x)

#define CLUTTER_REAL_ZERO 0.0

#endif

#else
/*
 * This is an int defintion for reference / debugging purposes only
 *
 * FIXME : remove this when no longer needed.
 */
typedef gint ClutterReal;

#define CLUTTER_REAL_MUL(x,y) ((x)*(y))
#define CLUTTER_REAL_DIV(x,y) ((x)/(y))
#define CLUTTER_REAL_ADD_INT(x,i) (x + i)
#define CLUTTER_REAL_SUB_INT(x,i) (x - i)

#define CLUTTER_REAL_TO_INT(x) (x)
#define CLUTTER_REAL_FROM_INT(i) (i)

#define CLUTTER_REAL_TO_FLOAT(x) ((float)(x))
#define CLUTTER_REAL_FROM_FLOAT(f) ((gint)(f))

#define CLUTTER_REAL_TO_FIXED(x) CLUTTER_INT_TO_FIXED(x)
#define CLUTTER_REAL_FROM_FIXED(x) CFX_INT(x)

#define CLUTTER_REAL_ZERO 0

#endif

#define CLUTTER_REAL_EZ(x) (CLUTTER_REAL_TO_INT(x) == 0)
#define CLUTTER_REAL_NZ(x) (CLUTTER_REAL_TO_INT(x) != 0)
#define CLUTTER_REAL_EQ(x,y) (CLUTTER_REAL_TO_INT(x) == CLUTTER_REAL_TO_INT(y))
#define CLUTTER_REAL_NE(x,y) (CLUTTER_REAL_TO_INT(x) != CLUTTER_REAL_TO_INT(y))
#define CLUTTER_REAL_LT(x,y) (CLUTTER_REAL_TO_INT(x) < CLUTTER_REAL_TO_INT(y))
#define CLUTTER_REAL_GT(x,y) (CLUTTER_REAL_TO_INT(x) > CLUTTER_REAL_TO_INT(y))
#define CLUTTER_REAL_LE(x,y) (CLUTTER_REAL_TO_INT(x) <= CLUTTER_REAL_TO_INT(y))
#define CLUTTER_REAL_GE(x,y) (CLUTTER_REAL_TO_INT(x) >= CLUTTER_REAL_TO_INT(y))

#endif /* ifndef _HAVE_CLUTTER_REAL_H */
