/*
 * Copyright 2024 GNOME Foundation, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Philip Withnall <pwithnall@gnome.org>
 */

#include "config.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <stdint.h>
#include <sys/timerfd.h>

#include "shell-time-change-source.h"

typedef struct
{
  GSource source;

  int fd;  /* (owned) (nullable)  */
  void *tag;  /* (owned) (nullable) */
} ShellTimeChangeSource;

static int
arm_timerfd (int fd)
{
  struct itimerspec its = {
    /* Get the biggest value we can in the `time_t`, as the timerfd will fire
     * spuriously when that time is reached. Unfortunately there is no
     * `TIME_T_MAX`. */
    .it_value.tv_sec = (sizeof (time_t) >= 8) ? UINT64_MAX : UINT32_MAX,
  };
  int flags = TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET;

  if (timerfd_settime (fd, flags, &its, NULL) == 0)
    return 0;

  if (errno != EINVAL)
    return -1;

  /* Try again with a smaller timeout. It’s possible that libc supports
   * 64-bit time while the kernel doesn’t. */
  its.it_value.tv_sec = UINT32_MAX;
  return timerfd_settime (fd, flags, &its, NULL);
}

static void
shell_time_change_source_cleanup_fd (ShellTimeChangeSource *self)
{
  /* Make sure the FD is closed. */
  if (self->tag != NULL)
    {
      g_source_remove_unix_fd ((GSource *) self, self->tag);
      self->tag = NULL;
    }

  g_clear_fd (&self->fd, NULL);
}

static gboolean
shell_time_change_source_dispatch (GSource     *source,
                                   GSourceFunc  callback,
                                   gpointer     user_data)
{
  ShellTimeChangeSource *self = (ShellTimeChangeSource *) source;

  if (callback == NULL)
    {
      g_warning ("ShellTimeChangeSource dispatched without callback. "
                 "You must call g_source_set_callback().");
      return G_SOURCE_REMOVE;
    }

  if (callback (user_data))
    {
      /* The timerfd_settime() call can’t really fail in this situation.
       * The man page says it can return ECANCELED, but will still be re-armed. */
      int retval = arm_timerfd (self->fd);
      int errsv = errno;
      g_assert (retval == 0 ||
                (retval < 0 && errsv == ECANCELED));

      return G_SOURCE_CONTINUE;
    }

  /* Clean up the source’s resources early, as FDs are precious, and the user
   * might leave the source hanging around for a long time before finalising it. */
  shell_time_change_source_cleanup_fd (self);

  return G_SOURCE_REMOVE;
}

static void
shell_time_change_source_finalize (GSource *source)
{
  ShellTimeChangeSource *self = (ShellTimeChangeSource *) source;

  shell_time_change_source_cleanup_fd (self);
}

static const GSourceFuncs shell_time_change_source_funcs = {
  NULL,  /* prepare */
  NULL,  /* check */
  shell_time_change_source_dispatch,
  shell_time_change_source_finalize,
  NULL, NULL
};

/**
 * shell_time_change_source_new:
 * @error: return location for a #GError, or %NULL
 *
 * Creates a #GSource which is dispatched every time the system realtime clock
 * changes relative to the monotonic clock.
 *
 * This typically happens after NTP synchronisation.
 *
 * On error, a #GFileError will be returned. This happens if a timerfd cannot be
 * created.
 *
 * Any callback attached to the returned #GSource must have type
 * #GSourceFunc.
 *
 * Returns: (transfer full): the newly created #GSource, or %NULL on error
 */
GSource *
shell_time_change_source_new (GError **error)
{
  ShellTimeChangeSource *self;
  g_autoptr(GSource) source = NULL;
  g_autofd int fd = -1;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* Create a timerfd with the maximum possible timeout, but set
   * `TFD_TIMER_CANCEL_ON_SET` so that it fires if the realtime clock changes
   * relative to the monotonic clock.
   *
   * This is a one-shot source: it’ll need to be recreated after that. */
  fd = timerfd_create (CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
  if (fd < 0 || arm_timerfd (fd) < 0)
    {
      int errsv = errno;
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errsv),
                   "Error creating timerfd: %s", g_strerror (errsv));
      return NULL;
    }

  source = g_source_new ((GSourceFuncs *) &shell_time_change_source_funcs,
                         sizeof (ShellTimeChangeSource));
  self = (ShellTimeChangeSource *) source;

  self->tag = g_source_add_unix_fd (source, fd, G_IO_IN);
  self->fd = g_steal_fd (&fd);

  return g_steal_pointer (&source);
}
