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

#ifndef __CLUTTER_PROFILE_H__
#define __CLUTTER_PROFILE_H__

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  CLUTTER_PROFILE_PICKING_ONLY    = 1 << 0,
  CLUTTER_PROFILE_DISABLE_REPORT  = 1 << 1
} ClutterProfileFlag;

#ifdef CLUTTER_ENABLE_PROFILE

#include <uprof.h>

extern UProfContext *   _clutter_uprof_context;
extern guint            clutter_profile_flags;

#define CLUTTER_STATIC_TIMER    UPROF_STATIC_TIMER
#define CLUTTER_STATIC_COUNTER  UPROF_STATIC_COUNTER
#define CLUTTER_COUNTER_INC     UPROF_COUNTER_INC
#define CLUTTER_COUNTER_DEC     UPROF_COUNTER_DEC
#define CLUTTER_TIMER_START     UPROF_TIMER_START
#define CLUTTER_TIMER_STOP      UPROF_TIMER_STOP

void    _clutter_uprof_init             (void);
void    _clutter_profile_suspend        (void);
void    _clutter_profile_resume         (void);

#else /* CLUTTER_ENABLE_PROFILE */

#ifdef __COUNTER__
#define CLUTTER_STATIC_TIMER(A,B,C,D,E) extern void G_PASTE (_clutter_dummy_decl, __COUNTER__) (void)
#define CLUTTER_STATIC_COUNTER(A,B,C,D) extern void G_PASTE (_clutter_dummy_decl, __COUNTER__) (void)
#else
#define CLUTTER_STATIC_TIMER(A,B,C,D,E) extern void G_PASTE (_clutter_dummy_decl, __LINE__) (void)
#define CLUTTER_STATIC_COUNTER(A,B,C,D) extern void G_PASTE (_clutter_dummy_decl, __LINE__) (void)
#endif
#define CLUTTER_COUNTER_INC(A,B)        G_STMT_START { } G_STMT_END
#define CLUTTER_COUNTER_DEC(A,B)        G_STMT_START { } G_STMT_END
#define CLUTTER_TIMER_START(A,B)        G_STMT_START { } G_STMT_END
#define CLUTTER_TIMER_STOP(A,B)         G_STMT_START { } G_STMT_END

#define _clutter_uprof_init             G_STMT_START { } G_STMT_END
#define _clutter_profile_suspend        G_STMT_START { } G_STMT_END
#define _clutter_profile_resume         G_STMT_START { } G_STMT_END

#endif /* CLUTTER_ENABLE_PROFILE */

G_END_DECLS

#endif /* _CLUTTER_PROFILE_H_ */
