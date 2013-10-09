/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2008,2009,2013 Intel Corporation.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_PATH_TYPES_H__
#define __COGL_PATH_TYPES_H__

#include <cogl/cogl-types.h>

COGL_BEGIN_DECLS

typedef struct _CoglPath CoglPath;

/**
 * CoglPathFillRule:
 * @COGL_PATH_FILL_RULE_NON_ZERO: Each time the line crosses an edge of
 * the path from left to right one is added to a counter and each time
 * it crosses from right to left the counter is decremented. If the
 * counter is non-zero then the point will be filled. See <xref
 * linkend="fill-rule-non-zero"/>.
 * @COGL_PATH_FILL_RULE_EVEN_ODD: If the line crosses an edge of the
 * path an odd number of times then the point will filled, otherwise
 * it won't. See <xref linkend="fill-rule-even-odd"/>.
 *
 * #CoglPathFillRule is used to determine how a path is filled. There
 * are two options - 'non-zero' and 'even-odd'. To work out whether any
 * point will be filled imagine drawing an infinetely long line in any
 * direction from that point. The number of times and the direction
 * that the edges of the path crosses this line determines whether the
 * line is filled as described below. Any open sub paths are treated
 * as if there was an extra line joining the first point and the last
 * point.
 *
 * The default fill rule when creating a path is %COGL_PATH_FILL_RULE_EVEN_ODD.
 *
 * <figure id="fill-rule-non-zero">
 *   <title>Example of filling various paths using the non-zero rule</title>
 *   <graphic fileref="fill-rule-non-zero.png" format="PNG"/>
 * </figure>
 *
 * <figure id="fill-rule-even-odd">
 *   <title>Example of filling various paths using the even-odd rule</title>
 *   <graphic fileref="fill-rule-even-odd.png" format="PNG"/>
 * </figure>
 *
 * Since: 1.4
 */
typedef enum {
  COGL_PATH_FILL_RULE_NON_ZERO,
  COGL_PATH_FILL_RULE_EVEN_ODD
} CoglPathFillRule;

COGL_END_DECLS

#endif /* __COGL_PATH_TYPES_H__ */
