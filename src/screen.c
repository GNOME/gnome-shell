/* Metacity X screen handler */

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

#include "screen.h"
#include "util.h"
#include "errors.h"
#include "window.h"
#include <cursorfont.h>

MetaScreen*
meta_screen_new (MetaDisplay *display,
                 int          number)
{
  MetaScreen *screen;
  Window xroot;
  Display *xdisplay;
  Cursor cursor;
  
  /* Only display->name, display->xdisplay, and display->error_traps
   * can really be used in this function, since normally screens are
   * created from the MetaDisplay constructor
   */
  
  xdisplay = display->xdisplay;
  
  meta_verbose ("Trying screen %d on display '%s'\n",
                number, display->name);

  xroot = RootWindow (xdisplay, number);

  /* FVWM checks for None here, I don't know if this
   * ever actually happens
   */
  if (xroot == None)
    {
      meta_warning (_("Screen %d on display '%s' is invalid\n"),
                    number, display->name);
      return NULL;
    }

  /* Select our root window events */
  meta_error_trap_push (display);
  XSelectInput (xdisplay,
                xroot,
                SubstructureRedirectMask | SubstructureNotifyMask |
                ColormapChangeMask | PropertyChangeMask |
                LeaveWindowMask | EnterWindowMask |
                ButtonPressMask | ButtonReleaseMask);
  if (meta_error_trap_pop (display) != Success)
    {
      meta_warning (_("Screen %d on display '%s' already has a window manager\n"),
                    number, display->name);
      return NULL;
    }

  cursor = XCreateFontCursor (display->xdisplay, XC_left_ptr);
  XDefineCursor (display->xdisplay, xroot, cursor);
  XFreeCursor (display->xdisplay, cursor);
  
  screen = g_new (MetaScreen, 1);

  screen->display = display;
  screen->number = number;
  screen->xscreen = ScreenOfDisplay (xdisplay, number);
  screen->xroot = xroot;  

  meta_verbose ("Added screen %d on display '%s' root 0x%lx\n",
                screen->number, screen->display->name, screen->xroot);  
  
  return screen;
}

void
meta_screen_free (MetaScreen *screen)
{
  g_free (screen);
}

void
meta_screen_manage_all_windows (MetaScreen *screen)
{
  Window ignored1, ignored2;
  Window *children;
  unsigned int n_children;
  int i;

  /* Must grab server to avoid obvious race condition */
  meta_display_grab (screen->display);

  meta_error_trap_push (screen->display);
  
  XQueryTree (screen->display->xdisplay,
              screen->xroot,
              &ignored1, &ignored2, &children, &n_children);

  if (meta_error_trap_pop (screen->display))
    {
      meta_display_ungrab (screen->display);
      return;
    }
  
  i = 0;
  while (i < n_children)
    {
      meta_window_new (screen->display, children[i]);

      ++i;
    }

  meta_display_ungrab (screen->display);
  
  if (children)
    XFree (children);
}
