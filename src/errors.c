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

#include "errors.h"
#include <errno.h>
#include <stdlib.h>
#include <gdk/gdk.h>

static int (* saved_gdk_error_handler)    (Display     *display,
                                           XErrorEvent *error);

static int (* saved_gdk_io_error_handler)    (Display  *display);     

static int x_error_handler    (Display     *display,
                               XErrorEvent *error);
static int x_io_error_handler (Display     *display);

void
meta_errors_init (void)
{
  saved_gdk_error_handler = XSetErrorHandler (x_error_handler);
  saved_gdk_io_error_handler = XSetIOErrorHandler (x_io_error_handler);
}

void
meta_error_trap_push (MetaDisplay *display)
{
  gdk_error_trap_push ();
}

int
meta_error_trap_pop (MetaDisplay *display)
{
  /* just use GDK trap */
  XSync (display->xdisplay, False);

  return gdk_error_trap_pop ();
}

static int
x_error_handler (Display     *xdisplay,
                 XErrorEvent *error)
{
  int retval;
  gchar buf[64];
  
  XGetErrorText (xdisplay, error->error_code, buf, 63);
  
  meta_verbose ("X error: %s serial %ld error_code %d request_code %d minor_code %d)\n",
                buf,
                error->serial, 
                error->error_code, 
                error->request_code,
                error->minor_code);
  
  retval = saved_gdk_error_handler (xdisplay, error);

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
