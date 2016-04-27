/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

void
_cogl_uprof_init (void);

void
_cogl_profile_trace_message (const char *format, ...);

#else

#define COGL_STATIC_TIMER(A,B,C,D,E) extern void _cogl_dummy_decl (void)
#define COGL_STATIC_COUNTER(A,B,C,D) extern void _cogl_dummy_decl (void)
#define COGL_COUNTER_INC(A,B) G_STMT_START{ (void)0; }G_STMT_END
#define COGL_COUNTER_DEC(A,B) G_STMT_START{ (void)0; }G_STMT_END
#define COGL_TIMER_START(A,B) G_STMT_START{ (void)0; }G_STMT_END
#define COGL_TIMER_STOP(A,B) G_STMT_START{ (void)0; }G_STMT_END

#define _cogl_profile_trace_message g_message

#endif

#endif /* __COGL_PROFILE_H__ */

