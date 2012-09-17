/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gmain.c: Main loop abstraction, timeouts, and idle functions
 * Copyright 1998 Owen Taylor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * MT safe
 */

#include "config.h"
#include "glibconfig.h"

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif /* HAVE_SYS_TIME_H */

#ifdef G_OS_WIN32
#define STRICT
#include <windows.h>
#endif /* G_OS_WIN32 */

#include "gmessages.h"
#include "gmain.h"

/**
 * g_get_current_time:
 * @result: #GTimeVal structure in which to store current time.
 *
 * Equivalent to the UNIX gettimeofday() function, but portable.
 *
 * You may find g_get_real_time() to be more convenient.
 **/
void
g_get_current_time (GTimeVal *result)
{
#ifndef G_OS_WIN32
  struct timeval r;

  g_return_if_fail (result != NULL);

  /*this is required on alpha, there the timeval structs are int's
    not longs and a cast only would fail horribly*/
  gettimeofday (&r, NULL);
  result->tv_sec = r.tv_sec;
  result->tv_usec = r.tv_usec;
#else
  FILETIME ft;
  guint64 time64;

  g_return_if_fail (result != NULL);

  GetSystemTimeAsFileTime (&ft);
  memmove (&time64, &ft, sizeof (FILETIME));

  /* Convert from 100s of nanoseconds since 1601-01-01
   * to Unix epoch. Yes, this is Y2038 unsafe.
   */
  time64 -= G_GINT64_CONSTANT (116444736000000000);
  time64 /= 10;

  result->tv_sec = time64 / 1000000;
  result->tv_usec = time64 % 1000000;
#endif
}
