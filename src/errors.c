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

typedef struct _ErrorTrap  ErrorTrap;

struct _ErrorTrap
{
  int error_code;
};

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
  ErrorTrap *et;

  et = g_new (ErrorTrap, 1);

  et->error_code = Success;
  display->error_traps = g_slist_prepend (display->error_traps, et);  
}

int
meta_error_trap_pop (MetaDisplay *display)
{
  int result;
  ErrorTrap *et;
  GSList *next;
  
  if (display->error_traps == NULL)
    meta_bug ("No error trap to pop\n");

  XSync (display->xdisplay, False);
  
  et = display->error_traps->data;

  result = et->error_code;

  next = display->error_traps->next;
  g_slist_free_1 (display->error_traps);
  display->error_traps = next;

  g_free (et);
  
  if (result != Success)
    {
      gchar buf[64];
                
      XGetErrorText (display->xdisplay, result, buf, 63);

      meta_verbose ("Popping error code %d (%s)\n",
                    result, buf);
    }
  
  return result;
}


static int
x_error_handler (Display     *xdisplay,
                 XErrorEvent *error)
{
  if (error->error_code)
    {
      MetaDisplay *display;

      display = meta_display_for_x_display (xdisplay);

      if (display == NULL)
        meta_bug ("Error received for unknown display?\n");
      
      if (display->error_traps == NULL)
        {
          gchar buf[64];

	  XGetErrorText (xdisplay, error->error_code, buf, 63);
          
          meta_bug ("Received an X Window System error without handling it.\n"
                    "The error was '%s'.\n"
                    "  (Details: serial %ld error_code %d request_code %d minor_code %d)\n",
                    buf,
                    error->serial, 
                    error->error_code, 
                    error->request_code,
                    error->minor_code);
	}
      else
        {
          ErrorTrap *et;

          et = display->error_traps->data;

          et->error_code = error->error_code;
        }
    }
  
  return 0;
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

  meta_display_close (display);
  
  /* I believe Xlib will force an exit after we return, which
   * seems sort of broken to me, but if true we should probably just
   * exit for ourselves. But for now I'm not doing it.
   */
  return 0;
}
