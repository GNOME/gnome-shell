/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter X error handling */

/* 
 * Copyright (C) 2001 Havoc Pennington, error trapping inspired by GDK
 * code copyrighted by the GTK team.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>
#include "errors.h"
#include "display-private.h"
#include <errno.h>
#include <stdlib.h>
#include <gdk/gdk.h>

static int x_error_handler    (Display     *display,
                               XErrorEvent *error);
static int x_io_error_handler (Display     *display);

void
meta_errors_init (void)
{
  XSetErrorHandler (x_error_handler);
  XSetIOErrorHandler (x_io_error_handler);
}

typedef struct ForeignDisplay ForeignDisplay;

struct ForeignDisplay
{
    Display *dpy;
    ErrorHandler handler;
    gpointer data;
    ForeignDisplay *next;
};

static ForeignDisplay *foreign_displays;

void
meta_errors_register_foreign_display (Display      *foreign_dpy,
				      ErrorHandler  handler,
				      gpointer      data)
{
    ForeignDisplay *info = g_new0 (ForeignDisplay, 1);
    info->dpy = foreign_dpy;
    info->handler = handler;
    info->data = data;
    info->next = foreign_displays;
    foreign_displays = info;
}

static void
meta_error_trap_push_internal (MetaDisplay *display,
                               gboolean     need_sync)
{
  /* GDK resets the error handler on each push */
  int (* old_error_handler) (Display     *,
                             XErrorEvent *);

  if (need_sync)
    {
      XSync (display->xdisplay, False);
    }
  
  gdk_error_trap_push ();

  /* old_error_handler will just be equal to x_error_handler
   * for nested traps
   */
  old_error_handler = XSetErrorHandler (x_error_handler);
  
  /* Replace GDK handler, but save it so we can chain up */
  if (display->error_trap_handler == NULL)
    {
      g_assert (display->error_traps == 0);
      display->error_trap_handler = old_error_handler;
      g_assert (display->error_trap_handler != x_error_handler);
    }

  display->error_traps += 1;

  meta_topic (META_DEBUG_ERRORS, "%d traps remain\n", display->error_traps);
}

static int
meta_error_trap_pop_internal  (MetaDisplay *display,
                               gboolean     need_sync)
{
  int result;

  g_assert (display->error_traps > 0);

  if (need_sync)
    {
      XSync (display->xdisplay, False);
    }

  result = gdk_error_trap_pop ();

  display->error_traps -= 1;
  
  if (display->error_traps == 0)
    {
      /* check that GDK put our handler back; this
       * assumes that there are no pending GDK traps from GDK itself
       */
      
      int (* restored_error_handler) (Display     *,
                                      XErrorEvent *);

      restored_error_handler = XSetErrorHandler (x_error_handler);

      /* remove this */
      display->error_trap_handler = NULL;
    }

  meta_topic (META_DEBUG_ERRORS, "%d traps\n", display->error_traps);
  
  return result;
}

void
meta_error_trap_push (MetaDisplay *display)
{
  meta_error_trap_push_internal (display, FALSE);
}

void
meta_error_trap_pop (MetaDisplay *display,
                     gboolean     last_request_was_roundtrip)
{
  gboolean need_sync;

  /* we only have to sync when popping the outermost trap */
  need_sync = (display->error_traps == 1 && !last_request_was_roundtrip);

  if (need_sync)
    meta_topic (META_DEBUG_SYNC, "Syncing on error_trap_pop, traps = %d, roundtrip = %d\n",
                display->error_traps, last_request_was_roundtrip);

  display->error_trap_synced_at_last_pop = need_sync || last_request_was_roundtrip;
  
  meta_error_trap_pop_internal (display, need_sync);
}

void
meta_error_trap_push_with_return (MetaDisplay *display)
{
  gboolean need_sync;

  /* We don't sync on push_with_return if there are no traps
   * currently, because we assume that any errors were either covered
   * by a previous pop, or were fatal.
   *
   * More generally, we don't sync if we were synchronized last time
   * we popped. This is known to be the case if there are no traps,
   * but we also keep a flag so we know whether it's the case otherwise.
   */

  if (!display->error_trap_synced_at_last_pop)
    need_sync = TRUE;
  else
    need_sync = FALSE;

  if (need_sync)
    meta_topic (META_DEBUG_SYNC, "Syncing on error_trap_push_with_return, traps = %d\n",
                display->error_traps);
  
  meta_error_trap_push_internal (display, FALSE);
}

int
meta_error_trap_pop_with_return  (MetaDisplay *display,
                                  gboolean     last_request_was_roundtrip)
{
  if (!last_request_was_roundtrip)
    meta_topic (META_DEBUG_SYNC, "Syncing on error_trap_pop_with_return, traps = %d, roundtrip = %d\n",
                display->error_traps, last_request_was_roundtrip);

  display->error_trap_synced_at_last_pop = TRUE;
  
  return meta_error_trap_pop_internal (display,
                                       !last_request_was_roundtrip);
}

static int
x_error_handler (Display     *xdisplay,
                 XErrorEvent *error)
{
  int retval;
  gchar buf[64];
  MetaDisplay *display;
  ForeignDisplay *foreign;

  for (foreign = foreign_displays; foreign != NULL; foreign = foreign->next)
  {
      if (foreign->dpy == xdisplay)
      {
	  foreign->handler (xdisplay, error, foreign->data);

	  return 0;
      }
  }
  
  XGetErrorText (xdisplay, error->error_code, buf, 63);  

  display = meta_display_for_x_display (xdisplay);

  /* Display can be NULL here because the compositing manager
   * has its own Display, but Xlib only has one global error handler
   */
  if (display->error_traps > 0)
    {
      /* we're in an error trap, chain to the trap handler
       * saved from GDK
       */
      meta_verbose ("X error: %s serial %ld error_code %d request_code %d minor_code %d)\n",
                    buf,
                    error->serial, 
                    error->error_code, 
                    error->request_code,
                    error->minor_code);

      g_assert (display->error_trap_handler != NULL);
      g_assert (display->error_trap_handler != x_error_handler);
      
      retval = (* display->error_trap_handler) (xdisplay, error);
    }
  else
    {
      meta_bug ("Unexpected X error: %s serial %ld error_code %d request_code %d minor_code %d)\n",
                buf,
                error->serial, 
                error->error_code, 
                error->request_code,
                error->minor_code);

      retval = 1; /* compiler warning */
    }

  return retval;
}

static int
x_io_error_handler (Display *xdisplay)
{
  MetaDisplay *display;

  display = meta_display_for_x_display (xdisplay);

  if (display == NULL)
    meta_bug ("IO error received for unknown display?\n");
  
  if (errno == EPIPE)
    {
      meta_warning (_("Lost connection to the display '%s';\n"
                      "most likely the X server was shut down or you killed/destroyed\n"
                      "the window manager.\n"),
                    display->name);
    }
  else
    {
      meta_warning (_("Fatal IO error %d (%s) on display '%s'.\n"),
                    errno, g_strerror (errno),
                    display->name);
    }

  /* Xlib would force an exit anyhow */
  exit (1);
  
  return 0;
}
