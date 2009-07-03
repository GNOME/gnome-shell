/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#ifndef __COGL_PROFILE_H__
#define __COGL_PROFILE_H__


#ifdef COGL_ENABLE_PROFILE

#include <uprof.h>

extern UProfContext *_cogl_uprof_context;

#define COGL_STATIC_TIMER    UPROF_STATIC_TIMER
#define COGL_STATIC_COUNTER  UPROF_STATIC_COUNTER
#define COGL_COUNTER_INC     UPROF_COUNTER_INC
#define COGL_COUNTER_DEC     UPROF_COUNTER_DEC
#define COGL_TIMER_START     UPROF_TIMER_START
#define COGL_TIMER_STOP      UPROF_TIMER_STOP

#else

#define COGL_STATIC_TIMER(A,B,C,D,E) extern void _cogl_dummy_decl (void)
#define COGL_STATIC_COUNTER(A,B,C,D) extern void _cogl_dummy_decl (void)
#define COGL_COUNTER_INC(A,B) G_STMT_START{ (void)0; }G_STMT_END
#define COGL_COUNTER_DEC(A,B) G_STMT_START{ (void)0; }G_STMT_END
#define COGL_TIMER_START(A,B) G_STMT_START{ (void)0; }G_STMT_END
#define COGL_TIMER_STOP(A,B) G_STMT_START{ (void)0; }G_STMT_END


#endif

#endif /* __COGL_PROFILE_H__ */

