/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2009, 2011 Intel Corporation.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

/* This is included multiple times with different definitions for
 * these macros. The macros are given the following arguments:
 *
 * COGL_EXT_BEGIN:
 *
 * @name: a unique symbol name for this feature
 *
 * @min_gl_major: the major part of the minimum GL version where these
 * functions are available in core, or 255 if it isn't available in
 * any version.
 * @min_gl_minor: the minor part of the minimum GL version where these
 * functions are available in core, or 255 if it isn't available in
 * any version.
 *
 * @gles_availability: flags to specify which versions of GLES the
 * functions are available in. Should be a combination of
 * COGL_EXT_IN_GLES and COGL_EXT_IN_GLES2.
 *
 * @extension_suffixes: A zero-separated list of suffixes in a
 * string. These are appended to the extension name to get a complete
 * extension name to try. The suffix is also appended to all of the
 * function names. The suffix can optionally include a ':' to specify
 * an alternate suffix for the function names.
 *
 * @extension_names: A list of extension names to try. If any of these
 * extensions match then it will be used.
 */

/* These functions are only available in GLES and are used as
   replacements for some GL equivalents that only accept double
   arguments */
COGL_EXT_BEGIN (only_in_gles1,
                255, 255,
                COGL_EXT_IN_GLES,
                "\0",
                "\0")
COGL_EXT_FUNCTION (void, glClipPlanef,
                   (GLenum plane, const GLfloat *equation))
COGL_EXT_END ()

COGL_EXT_BEGIN (multitexture_part1, 1, 3,
                COGL_EXT_IN_GLES,
                "ARB\0",
                "multitexture\0")
COGL_EXT_FUNCTION (void, glClientActiveTexture,
                   (GLenum                texture))
COGL_EXT_END ()

