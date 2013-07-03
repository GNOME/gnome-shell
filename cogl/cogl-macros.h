/*
 * Cogl.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2012 Intel Corporation
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
 */

#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_MACROS_H__
#define __COGL_MACROS_H__

#include <cogl/cogl-version.h>

/* These macros are used to mark deprecated functions, and thus have
 * to be exposed in a public header.
 *
 * They are only intended for internal use and should not be used by
 * other projects.
 */
#if defined(COGL_DISABLE_DEPRECATION_WARNINGS) || defined(COGL_COMPILATION)

#define COGL_DEPRECATED
#define COGL_DEPRECATED_FOR(f)
#define COGL_UNAVAILABLE(maj,min)

#else /* COGL_DISABLE_DEPRECATION_WARNINGS */

#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1)
#define COGL_DEPRECATED __attribute__((__deprecated__))
#elif defined(_MSC_VER) && (_MSC_VER >= 1300)
#define COGL_DEPRECATED __declspec(deprecated)
#else
#define COGL_DEPRECATED
#endif

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
#define COGL_DEPRECATED_FOR(f) __attribute__((__deprecated__("Use '" #f "' instead")))
#elif defined(_MSC_FULL_VER) && (_MSC_FULL_VER > 140050320)
#define COGL_DEPRECATED_FOR(f) __declspec(deprecated("is deprecated. Use '" #f "' instead"))
#else
#define COGL_DEPRECATED_FOR(f) G_DEPRECATED
#endif

#if    __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
#define COGL_UNAVAILABLE(maj,min) __attribute__((deprecated("Not available before " #maj "." #min)))
#elif defined(_MSC_FULL_VER) && (_MSC_FULL_VER > 140050320)
#define COGL_UNAVAILABLE(maj,min) __declspec(deprecated("is not available before " #maj "." #min))
#else
#define COGL_UNAVAILABLE(maj,min)
#endif

#endif /* COGL_DISABLE_DEPRECATION_WARNINGS */

/**
 * COGL_VERSION_MIN_REQUIRED:
 *
 * A macro that should be defined by the user prior to including the
 * cogl.h header.
 *
 * The definition should be one of the predefined Cogl version macros,
 * such as: %COGL_VERSION_1_8, %COGL_VERSION_1_10, ...
 *
 * This macro defines the lower bound for the Cogl API to be used.
 *
 * If a function has been deprecated in a newer version of Cogl, it
 * is possible to use this symbol to avoid the compiler warnings without
 * disabling warnings for every deprecated function.
 *
 * Since: 1.16
 */
#ifndef COGL_VERSION_MIN_REQUIRED
# define COGL_VERSION_MIN_REQUIRED (COGL_VERSION_CURRENT_STABLE)
#endif

/**
 * COGL_VERSION_MAX_ALLOWED:
 *
 * A macro that should be define by the user prior to including the
 * cogl.h header.
 *
 * The definition should be one of the predefined Cogl version macros,
 * such as: %COGL_VERSION_1_0, %COGL_VERSION_1_2, ...
 *
 * This macro defines the upper bound for the Cogl API to be used.
 *
 * If a function has been introduced in a newer version of Cogl, it
 * is possible to use this symbol to get compiler warnings when trying
 * to use that function.
 *
 * Since: 1.16
 */
#ifndef COGL_VERSION_MAX_ALLOWED
# if COGL_VERSION_MIN_REQUIRED > COGL_VERSION_PREVIOUS_STABLE
#  define COGL_VERSION_MAX_ALLOWED COGL_VERSION_MIN_REQUIRED
# else
#  define COGL_VERSION_MAX_ALLOWED COGL_VERSION_CURRENT_STABLE
# endif
#endif

/* sanity checks */
#if COGL_VERSION_MAX_ALLOWED < COGL_VERSION_MIN_REQUIRED
# error "COGL_VERSION_MAX_ALLOWED must be >= COGL_VERSION_MIN_REQUIRED"
#endif
#if COGL_VERSION_MIN_REQUIRED < COGL_VERSION_1_0
# error "COGL_VERSION_MIN_REQUIRED must be >= COGL_VERSION_1_0"
#endif

/* XXX: Every new stable minor release should add a set of macros here */
#if COGL_VERSION_MIN_REQUIRED >= COGL_VERSION_1_0
# define COGL_DEPRECATED_IN_1_0              COGL_DEPRECATED
# define COGL_DEPRECATED_IN_1_0_FOR(f)       COGL_DEPRECATED_FOR(f)
#else
# define COGL_DEPRECATED_IN_1_0
# define COGL_DEPRECATED_IN_1_0_FOR(f)
#endif

#if COGL_VERSION_MAX_ALLOWED < COGL_VERSION_1_0
# define COGL_AVAILABLE_IN_1_0               COGL_UNAVAILABLE(1, 0)
#else
# define COGL_AVAILABLE_IN_1_0
#endif

#if COGL_VERSION_MIN_REQUIRED >= COGL_VERSION_1_2
# define COGL_DEPRECATED_IN_1_2              COGL_DEPRECATED
# define COGL_DEPRECATED_IN_1_2_FOR(f)       COGL_DEPRECATED_FOR(f)
#else
# define COGL_DEPRECATED_IN_1_2
# define COGL_DEPRECATED_IN_1_2_FOR(f)
#endif

#if COGL_VERSION_MAX_ALLOWED < COGL_VERSION_1_2
# define COGL_AVAILABLE_IN_1_2               COGL_UNAVAILABLE(1, 2)
#else
# define COGL_AVAILABLE_IN_1_2
#endif

#if COGL_VERSION_MIN_REQUIRED >= COGL_VERSION_1_4
# define COGL_DEPRECATED_IN_1_4              COGL_DEPRECATED
# define COGL_DEPRECATED_IN_1_4_FOR(f)       COGL_DEPRECATED_FOR(f)
#else
# define COGL_DEPRECATED_IN_1_4
# define COGL_DEPRECATED_IN_1_4_FOR(f)
#endif

#if COGL_VERSION_MAX_ALLOWED < COGL_VERSION_1_4
# define COGL_AVAILABLE_IN_1_4               COGL_UNAVAILABLE(1, 4)
#else
# define COGL_AVAILABLE_IN_1_4
#endif

#if COGL_VERSION_MIN_REQUIRED >= COGL_VERSION_1_6
# define COGL_DEPRECATED_IN_1_6              COGL_DEPRECATED
# define COGL_DEPRECATED_IN_1_6_FOR(f)       COGL_DEPRECATED_FOR(f)
#else
# define COGL_DEPRECATED_IN_1_6
# define COGL_DEPRECATED_IN_1_6_FOR(f)
#endif

#if COGL_VERSION_MAX_ALLOWED < COGL_VERSION_1_6
# define COGL_AVAILABLE_IN_1_6               COGL_UNAVAILABLE(1, 6)
#else
# define COGL_AVAILABLE_IN_1_6
#endif

#if COGL_VERSION_MIN_REQUIRED >= COGL_VERSION_1_8
# define COGL_DEPRECATED_IN_1_8              COGL_DEPRECATED
# define COGL_DEPRECATED_IN_1_8_FOR(f)       COGL_DEPRECATED_FOR(f)
#else
# define COGL_DEPRECATED_IN_1_8
# define COGL_DEPRECATED_IN_1_8_FOR(f)
#endif

#if COGL_VERSION_MAX_ALLOWED < COGL_VERSION_1_8
# define COGL_AVAILABLE_IN_1_8               COGL_UNAVAILABLE(1, 8)
#else
# define COGL_AVAILABLE_IN_1_8
#endif

#if COGL_VERSION_MIN_REQUIRED >= COGL_VERSION_1_10
# define COGL_DEPRECATED_IN_1_10             COGL_DEPRECATED
# define COGL_DEPRECATED_IN_1_10_FOR(f)      COGL_DEPRECATED_FOR(f)
#else
# define COGL_DEPRECATED_IN_1_10
# define COGL_DEPRECATED_IN_1_10_FOR(f)
#endif

#if COGL_VERSION_MAX_ALLOWED < COGL_VERSION_1_10
# define COGL_AVAILABLE_IN_1_10              COGL_UNAVAILABLE(1, 10)
#else
# define COGL_AVAILABLE_IN_1_10
#endif

#if COGL_VERSION_MIN_REQUIRED >= COGL_VERSION_1_12
# define COGL_DEPRECATED_IN_1_12             COGL_DEPRECATED
# define COGL_DEPRECATED_IN_1_12_FOR(f)      COGL_DEPRECATED_FOR(f)
#else
# define COGL_DEPRECATED_IN_1_12
# define COGL_DEPRECATED_IN_1_12_FOR(f)
#endif

#if COGL_VERSION_MAX_ALLOWED < COGL_VERSION_1_12
# define COGL_AVAILABLE_IN_1_12              COGL_UNAVAILABLE(1, 12)
#else
# define COGL_AVAILABLE_IN_1_12
#endif

#if COGL_VERSION_MIN_REQUIRED >= COGL_VERSION_1_14
# define COGL_DEPRECATED_IN_1_14             COGL_DEPRECATED
# define COGL_DEPRECATED_IN_1_14_FOR(f)      COGL_DEPRECATED_FOR(f)
#else
# define COGL_DEPRECATED_IN_1_14
# define COGL_DEPRECATED_IN_1_14_FOR(f)
#endif

#if COGL_VERSION_MAX_ALLOWED < COGL_VERSION_1_14
# define COGL_AVAILABLE_IN_1_14              COGL_UNAVAILABLE(1, 14)
#else
# define COGL_AVAILABLE_IN_1_14
#endif

#if COGL_VERSION_MIN_REQUIRED >= COGL_VERSION_1_16
# define COGL_DEPRECATED_IN_1_16             COGL_DEPRECATED
# define COGL_DEPRECATED_IN_1_16_FOR(f)      COGL_DEPRECATED_FOR(f)
#else
# define COGL_DEPRECATED_IN_1_16
# define COGL_DEPRECATED_IN_1_16_FOR(f)
#endif

#if COGL_VERSION_MAX_ALLOWED < COGL_VERSION_1_16
# define COGL_AVAILABLE_IN_1_16              COGL_UNAVAILABLE(1, 16)
#else
# define COGL_AVAILABLE_IN_1_16
#endif

#endif /* __COGL_MACROS_H__ */
