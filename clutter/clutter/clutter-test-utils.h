/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2013  Emmanuele Bassi <ebassi@gnome.org>
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
 */

#ifndef __CLUTTER_TEST_UTILS_H__
#define __CLUTTER_TEST_UTILS_H__

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#include <clutter/clutter-types.h>
#include <clutter/clutter-actor.h>
#include <clutter/clutter-color.h>

G_BEGIN_DECLS

/**
 * CLUTTER_TEST_UNIT:
 * @path: the GTest path for the test function
 * @func: the GTestFunc function
 *
 * Adds @func at the given @path in the test suite.
 *
 * Since: 1.18
 */
#define CLUTTER_TEST_UNIT(path,func) \
  clutter_test_add (path, func);

/**
 * CLUTTER_TEST_SUITE:
 * @units: a list of %CLUTTER_TEST_UNIT definitions
 *
 * Defines the entry point and initializes a Clutter test unit, e.g.:
 *
 * |[
 * CLUTTER_TEST_SUITE (
 *   CLUTTER_TEST_UNIT ("/foobarize", foobarize)
 *   CLUTTER_TEST_UNIT ("/bar-enabled", bar_enabled)
 * )
 * ]|
 *
 * Expands to:
 *
 * |[
 * int
 * main (int   argc,
 *       char *argv[])
 * {
 *   clutter_test_init (&argc, &argv);
 *
 *   clutter_test_add ("/foobarize", foobarize);
 *   clutter_test_add ("/bar-enabled", bar_enabled);
 *
 *   return clutter_test_run ();
 * }
 * ]|
 *
 * Since: 1.18
 */
#define CLUTTER_TEST_SUITE(units) \
int \
main (int argc, char *argv[]) \
{ \
  clutter_test_init (&argc, &argv); \
\
  { \
    units \
  } \
\
  return clutter_test_run (); \
}

CLUTTER_AVAILABLE_IN_1_18
void            clutter_test_init               (int            *argc,
                                                 char         ***argv);
CLUTTER_AVAILABLE_IN_1_18
int             clutter_test_run                (void);

CLUTTER_AVAILABLE_IN_1_18
void            clutter_test_add                (const char     *test_path,
                                                 GTestFunc       test_func);
CLUTTER_AVAILABLE_IN_1_18
void            clutter_test_add_data           (const char     *test_path,
                                                 GTestDataFunc   test_func,
                                                 gpointer        test_data);
CLUTTER_AVAILABLE_IN_1_18
void            clutter_test_add_data_full      (const char     *test_path,
                                                 GTestDataFunc   test_func,
                                                 gpointer        test_data,
                                                 GDestroyNotify  test_notify);

CLUTTER_AVAILABLE_IN_1_18
ClutterActor *  clutter_test_get_stage          (void);

#define clutter_test_assert_actor_at_point(stage,point,actor) \
G_STMT_START { \
  const ClutterPoint *__p = (point); \
  ClutterActor *__actor = (actor); \
  ClutterActor *__stage = (stage); \
  ClutterActor *__res; \
  if (clutter_test_check_actor_at_point (__stage, __p, actor, &__res)) ; else { \
    const char *__str1 = clutter_actor_get_name (__actor) != NULL \
                       ? clutter_actor_get_name (__actor) \
                       : G_OBJECT_TYPE_NAME (__actor); \
    const char *__str2 = clutter_actor_get_name (__res) != NULL \
                       ? clutter_actor_get_name (__res) \
                       : G_OBJECT_TYPE_NAME (__res); \
    char *__msg = g_strdup_printf ("assertion failed (actor %s at %.2f,%.2f): found actor %s", \
                                   __str1, __p->x, __p->y, __str2); \
    g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, __msg); \
    g_free (__msg); \
  } \
} G_STMT_END

#define clutter_test_assert_color_at_point(stage,point,color) \
G_STMT_START { \
  const ClutterPoint *__p = (point); \
  const ClutterColor *__c = (color); \
  ClutterActor *__stage = (stage); \
  ClutterColor __res; \
  if (clutter_test_check_color_at_point (__stage, __p, __c, &__res)) ; else { \
    char *__str1 = clutter_color_to_string (__c); \
    char *__str2 = clutter_color_to_string (&__res); \
    char *__msg = g_strdup_printf ("assertion failed (color %s at %.2f,%.2f): found color %s", \
                                   __str1, __p->x, __p->y, __str2); \
    g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, __msg); \
    g_free (__msg); \
    g_free (__str1); \
    g_free (__str2); \
  } \
} G_STMT_END

CLUTTER_AVAILABLE_IN_1_18
gboolean        clutter_test_check_actor_at_point       (ClutterActor       *stage,
                                                         const ClutterPoint *point,
                                                         ClutterActor       *actor,
                                                         ClutterActor      **result);
CLUTTER_AVAILABLE_IN_1_18
gboolean        clutter_test_check_color_at_point       (ClutterActor       *stage,
                                                         const ClutterPoint *point,
                                                         const ClutterColor *color,
                                                         ClutterColor       *result);

G_END_DECLS

#endif /* __CLUTTER_TEST_UTILS_H__ */
