/* Metacity X error handling */

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

void
meta_error_trap_push (MetaDisplay *display)
{
  /* GDK resets the error handler on each push */
  int (* old_error_handler) (Display     *,
                             XErrorEvent *);

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
}

int
meta_error_trap_pop (MetaDisplay *display)
{
  int result;

  g_assert (display->error_traps > 0);
  
  /* just use GDK trap, but we do the sync since GDK doesn't */
  XSync (display->xdisplay, False);

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
      g_assert (restored_error_handler == x_error_handler);

      /* remove this */
      display->error_trap_handler = NULL;
    }

  return result;
}

static int
x_error_handler (Display     *xdisplay,
                 XErrorEvent *error)
{
  int retval;
  gchar buf[64];
  MetaDisplay *display;
  
  XGetErrorText (xdisplay, error->error_code, buf, 63);  

  display = meta_display_for_x_display (xdisplay);
  
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


