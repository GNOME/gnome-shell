/* Metacity restart app */

/* 
 * Copyright (C) 2001 Havoc Pennington
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

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

int
main (int argc, char **argv)
{
  XEvent xev;
  
  gtk_init (&argc, &argv);
  
  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = gdk_display;
  xev.xclient.window = gdk_x11_get_default_root_xwindow ();
  xev.xclient.message_type = XInternAtom (gdk_display,
                                          "_METACITY_RESTART_MESSAGE",
                                          False);
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = 0;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;

  XSendEvent (gdk_display,
              gdk_x11_get_default_root_xwindow (),
              False,
	      SubstructureRedirectMask | SubstructureNotifyMask,
	      &xev);

  XFlush (gdk_display);
  XSync (gdk_display, False);

  return 0;
}

