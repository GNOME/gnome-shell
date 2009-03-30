/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2009 Intel Corporation.
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

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef _CLUTTER_PROFILE_H_
#define _CLUTTER_PROFILE_H_

G_BEGIN_DECLS

#ifdef CLUTTER_ENABLE_PROFILE

#include <uprof.h>

extern UProfContext *_clutter_uprof_context;

#define CLUTTER_STATIC_TIMER    UPROF_STATIC_TIMER
#define CLUTTER_STATIC_COUNTER  UPROF_STATIC_COUNTER
#define CLUTTER_COUNTER_INC     UPROF_COUNTER_INC
#define CLUTTER_COUNTER_DEC     UPROF_COUNTER_DEC
#define CLUTTER_TIMER_START     UPROF_TIMER_START
#define CLUTTER_TIMER_STOP      UPROF_TIMER_STOP

#else /* CLUTTER_ENABLE_PROFILE */

#define CLUTTER_STATIC_TIMER(A,B,C,D,E) extern void _clutter_dummy_decl (void)
#define CLUTTER_STATIC_COUNTER(A,B,C,D) extern void _clutter_dummy_decl (void)
#define CLUTTER_COUNTER_INC(A,B) G_STMT_START{ (void)0; }G_STMT_END
#define CLUTTER_COUNTER_DEC(A,B) G_STMT_START{ (void)0; }G_STMT_END
#define CLUTTER_TIMER_START(A,B) G_STMT_START{ (void)0; }G_STMT_END
#define CLUTTER_TIMER_STOP(A,B) G_STMT_START{ (void)0; }G_STMT_END

#endif /* CLUTTER_ENABLE_PROFILE */

G_END_DECLS

#endif /* _CLUTTER_PROFILE_H_ */
