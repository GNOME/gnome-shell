/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2008,2009,2013 Intel Corporation.
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
 *
 */

#ifndef __COGL_PATH_H__
#define __COGL_PATH_H__

/**
 * SECTION:cogl-paths
 * @short_description: Functions for constructing and drawing 2D paths.
 *
 * There are two levels on which drawing with cogl-paths can be used.
 * The highest level functions construct various simple primitive
 * shapes to be either filled or stroked. Using a lower-level set of
 * functions more complex and arbitrary paths can be constructed by
 * concatenating straight line, bezier curve and arc segments.
 *
 * When constructing arbitrary paths, the current pen location is
 * initialized using the move_to command. The subsequent path segments
 * implicitly use the last pen location as their first vertex and move
 * the pen location to the last vertex they produce at the end. Also
 * there are special versions of functions that allow specifying the
 * vertices of the path segments relative to the last pen location
 * rather then in the absolute coordinates.
 */

#include <cogl/cogl-defines.h>

#ifdef COGL_HAS_GTYPE_SUPPORT
#include <cogl-path/cogl-path-enum-types.h>
#endif

#include <cogl-path/cogl-path-types.h>

#ifdef COGL_ENABLE_EXPERIMENTAL_2_0_API
#include <cogl-path/cogl2-path-functions.h>
#else
#include <cogl-path/cogl1-path-functions.h>
#endif

#endif /* __COGL_PATH_H__ */

