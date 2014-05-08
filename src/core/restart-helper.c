/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * SECTION:restart-helper
 * @short_description: helper program during a restart
 *
 * To smoothly restart Mutter, we want to keep the composite
 * overlay window enabled during the restart. This is done by
 * spawning this program, which keeps a reference to the the composite
 * overlay window until Mutter picks it back up.
 */

/*
 * Copyright (C) 2014 Red Hat, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>

int
main (int    argc,
      char **argv)
{
  Display *display = XOpenDisplay (NULL);
  Window selection_window;
  XSetWindowAttributes xwa;
  unsigned long mask = 0;

  xwa.override_redirect = True;
  mask |= CWOverrideRedirect;


  XCompositeGetOverlayWindow (display, DefaultRootWindow (display));

  selection_window = XCreateWindow (display,
				    DefaultRootWindow (display),
				    -100, -100, 1, 1, 0,
				    0,
				    InputOnly,
				    DefaultVisual (display, DefaultScreen (display)),
				    mask, &xwa);

  XSetSelectionOwner (display,
		      XInternAtom (display, "_MUTTER_RESTART_HELPER", False),
		      selection_window,
		      CurrentTime);

  /* Mutter looks for an (arbitrary) line printed to stdout to know that
   * we have started and have a reference to the COW. XSync() so that
   * everything is set on the X server before Mutter starts restarting.
   */
  XSync (display, False);

  printf ("STARTED\n");
  fflush (stdout);

  while (True)
    {
      XEvent xev;

      XNextEvent (display, &xev);
      /* Mutter restarted and unset the selection to indicate that
       * it has a reference on the COW again */
      if (xev.xany.type == SelectionClear)
	return 0;
    }
}
