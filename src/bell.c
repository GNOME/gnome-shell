/* Metacity visual bell */

/* 
 * Copyright (C) 2002 Sun Microsystems Inc.
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
#include "bell.h"
#include "screen.h"
#include "prefs.h"

static void
meta_bell_flash_screen (MetaDisplay *display, 
			MetaScreen  *screen)
{
  Window root = screen->xroot;
  int width = screen->width;
  int height = screen->height;
  
  if (screen->flash_window == None)
    {
      Visual *visual = (Visual *)CopyFromParent;
      XSetWindowAttributes xswa;
      int depth = CopyFromParent;
      xswa.save_under = True;
      xswa.override_redirect = True;
      /* 
       * TODO: use XGetVisualInfo and determine which is an
       * overlay, if one is present, and use the Overlay visual
       * for this window (for performance reasons).  
       * Not sure how to tell this yet... 
       */
      screen->flash_window = XCreateWindow (display->xdisplay, root,
					    0, 0, width, height,
					    0, depth,
					    InputOutput,
					    visual,
				    /* note: XSun doesn't like SaveUnder here */
					    CWSaveUnder | CWOverrideRedirect,
					    &xswa);
      XSelectInput (display->xdisplay, screen->flash_window, ExposureMask);
      XMapWindow (display->xdisplay, screen->flash_window);
      XSync (display->xdisplay, False);
      XFlush (display->xdisplay);
      XUnmapWindow (display->xdisplay, screen->flash_window);
    }
  else
    {
      /* just draw something in the window */
      GC gc = XCreateGC (display->xdisplay, screen->flash_window, 0, NULL);
      XMapWindow (display->xdisplay, screen->flash_window);
      XSetForeground (display->xdisplay, gc,
		      WhitePixel (display->xdisplay, 
				  XScreenNumberOfScreen (screen->xscreen)));
      XFillRectangle (display->xdisplay, screen->flash_window, gc,
		      0, 0, width, height);
      XSetForeground (display->xdisplay, gc,
		      BlackPixel (display->xdisplay, 
				  XScreenNumberOfScreen (screen->xscreen)));
      XFillRectangle (display->xdisplay, screen->flash_window, gc,
		      0, 0, width, height);
      XFlush (display->xdisplay);
      XSync (display->xdisplay, False);
      XUnmapWindow (display->xdisplay, screen->flash_window);
    }

  if (meta_prefs_get_focus_mode () != META_FOCUS_MODE_CLICK &&
      !display->mouse_mode)
    meta_display_increment_focus_sentinel (display);
  XFlush (display->xdisplay);
}

#ifdef HAVE_XKB
static void
meta_bell_flash_fullscreen (MetaDisplay *display, 
			    XkbAnyEvent *xkb_ev)
{
  XkbBellNotifyEvent *xkb_bell_ev = (XkbBellNotifyEvent *) xkb_ev;
  MetaScreen *screen;

  g_assert (xkb_ev->xkb_type == XkbBellNotify);
  if (xkb_bell_ev->window != None)
    {
      screen = meta_display_screen_for_xwindow (display, xkb_bell_ev->window);
      if (screen)
	meta_bell_flash_screen (display, screen);
    }
  else 
    {
      GSList *screen_list = display->screens;
      while (screen_list) 
	{
	  screen = (MetaScreen *) screen_list->data;
	  meta_bell_flash_screen (display, screen);
	  screen_list = screen_list->next;
	}
    }
}

static gboolean 
meta_bell_unflash_frame (gpointer data)
{
  MetaFrame *frame = (MetaFrame *) data;
  frame->is_flashing = 0;
  meta_frame_queue_draw (frame);
  return FALSE;
}

static void
meta_bell_flash_window_frame (MetaWindow *window)
{
  g_assert (window->frame != NULL);
  window->frame->is_flashing = 1;
  meta_frame_queue_draw (window->frame);
  g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE, 100, 
      meta_bell_unflash_frame, window->frame, NULL);
}

static void
meta_bell_flash_frame (MetaDisplay *display, 
		       XkbAnyEvent *xkb_ev)
{
  XkbBellNotifyEvent *xkb_bell_event = (XkbBellNotifyEvent *) xkb_ev;
  MetaWindow *window;
  
  g_assert (xkb_ev->xkb_type == XkbBellNotify);
  window = meta_display_lookup_x_window (display, xkb_bell_event->window);
  if (!window && (display->focus_window) && (display->focus_window->frame))
    {
      window = display->focus_window;
    }
  if (window)
    {
      meta_bell_flash_window_frame (window);
    }
  else /* revert to fullscreen flash if there's no focussed window */
    {
      meta_bell_flash_fullscreen (display, xkb_ev);
    }
}

static void
meta_bell_visual_notify (MetaDisplay *display, 
			 XkbAnyEvent *xkb_ev)
{
  switch (meta_prefs_get_visual_bell_type ()) 
    {
    case META_VISUAL_BELL_FULLSCREEN_FLASH:
      meta_bell_flash_fullscreen (display, xkb_ev);
      break;
    case META_VISUAL_BELL_FRAME_FLASH:
      meta_bell_flash_frame (display, xkb_ev); /* does nothing yet */
      break;
    case META_VISUAL_BELL_INVALID:
      /* do nothing */
      break;
    }
}

void
meta_bell_notify (MetaDisplay *display, 
		  XkbAnyEvent *xkb_ev)
{
  /* flash something */
  if (meta_prefs_get_visual_bell ()) 
    meta_bell_visual_notify (display, xkb_ev);
}
#endif

void
meta_bell_set_audible (MetaDisplay *display, gboolean audible)
{
#ifdef HAVE_XKB
  XkbChangeEnabledControls (display->xdisplay,
			    XkbUseCoreKbd,
			    XkbAudibleBellMask,
			    audible ? XkbAudibleBellMask : 0);
#endif  
}

gboolean
meta_bell_init (MetaDisplay *display)
{
#ifdef HAVE_XKB
  int xkb_base_error_type, xkb_opcode;

  if (!XkbQueryExtension (display->xdisplay, &xkb_opcode, 
			  &display->xkb_base_event_type, 
			  &xkb_base_error_type, 
			  NULL, NULL))
    {
      display->xkb_base_event_type = -1;
      g_message ("could not find XKB extension.");
      return FALSE;
    }
  else 
    {
      unsigned int mask = XkbBellNotifyMask;
      gboolean visual_bell_auto_reset = FALSE; 
      /* TRUE if and when non-broken version is available */
      XkbSelectEvents (display->xdisplay,
		       XkbUseCoreKbd,
		       XkbBellNotifyMask,
		       XkbBellNotifyMask);
      XkbChangeEnabledControls (display->xdisplay,
				XkbUseCoreKbd,
				XkbAudibleBellMask,
				meta_prefs_bell_is_audible () 
				? XkbAudibleBellMask : 0);
      if (visual_bell_auto_reset) {
	XkbSetAutoResetControls (display->xdisplay,
				 XkbAudibleBellMask,
				 &mask,
				 &mask);
      }
      return TRUE;
    }
#endif
  return FALSE;
}

void
meta_bell_shutdown (MetaDisplay *display)
{
#ifdef HAVE_XKB
  /* TODO: persist initial bell state in display, reset here */
  XkbChangeEnabledControls (display->xdisplay,
			    XkbUseCoreKbd,
			    XkbAudibleBellMask,
			    XkbAudibleBellMask);
#endif
}

void
meta_bell_notify_frame_destroy (MetaFrame *frame)
{
  if (frame->is_flashing) 
    g_idle_remove_by_data (frame);
}
