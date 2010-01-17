/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity visual bell */

/* 
 * Copyright (C) 2002 Sun Microsystems Inc.
 * Copyright (C) 2005, 2006 Elijah Newren
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

/**
 * \file bell.c Ring the bell or flash the screen
 *
 * Sometimes, X programs "ring the bell", whatever that means. Metacity lets
 * the user configure the bell to be audible or visible (aka visual), and
 * if it's visual it can be configured to be frame-flash or fullscreen-flash.
 * We never get told about audible bells; X handles them just fine by itself.
 *
 * Visual bells come in at meta_bell_notify(), which checks we are actually
 * in visual mode and calls through to bell_visual_notify(). That
 * function then checks what kind of visual flash you like, and calls either
 * bell_flash_fullscreen()-- which calls bell_flash_screen() to do
 * its work-- or bell_flash_frame(), which flashes the focussed window
 * using bell_flash_window_frame(), unless there is no such window, in
 * which case it flashes the screen instead. bell_flash_window_frame()
 * flashes the frame and calls bell_unflash_frame() as a timeout to
 * remove the flash.
 *
 * The visual bell was the result of a discussion in Bugzilla here:
 * <http://bugzilla.gnome.org/show_bug.cgi?id=99886>.
 *
 * Several of the functions in this file are ifdeffed out entirely if we are
 * found not to have the XKB extension, which is required to do these clever
 * things with bells; some others are entirely no-ops in that case.
 */

#include <config.h>
#include "bell.h"
#include "screen-private.h"
#include "prefs.h"
#include <canberra-gtk.h>

/**
 * Flashes one entire screen.  This is done by making a window the size of the
 * whole screen (or reusing the old one, if it's still around), mapping it,
 * painting it white and then black, and then unmapping it. We set saveunder so
 * that all the windows behind it come back immediately.
 *
 * Unlike frame flashes, we don't do fullscreen flashes with a timeout; rather,
 * we do them in one go, because we don't have to rely on the theme code
 * redrawing the frame for us in order to do the flash.
 *
 * \param display  The display which owns the screen (rather redundant)
 * \param screen   The screen to flash
 *
 * \bug The way I read it, this appears not to do the flash
 * the first time we flash a particular display. Am I wrong?
 *
 * \bug This appears to destroy our current XSync status.
 */
static void
bell_flash_screen (MetaDisplay *display, 
			MetaScreen  *screen)
{
  Window root = screen->xroot;
  int width = screen->rect.width;
  int height = screen->rect.height;
  
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
      XFreeGC (display->xdisplay, gc);
    }

  if (meta_prefs_get_focus_mode () != META_FOCUS_MODE_CLICK &&
      !display->mouse_mode)
    meta_display_increment_focus_sentinel (display);
  XFlush (display->xdisplay);
}

/**
 * Flashes one screen, or all screens, in response to a bell event.
 * If the event is on a particular window, flash the screen that
 * window is on. Otherwise, flash every screen on this display.
 *
 * If the configure script found we had no XKB, this does not exist.
 *
 * \param display  The display the event came in on
 * \param xkb_ev   The bell event
 */
#ifdef HAVE_XKB
static void
bell_flash_fullscreen (MetaDisplay *display, 
			    XkbAnyEvent *xkb_ev)
{
  XkbBellNotifyEvent *xkb_bell_ev = (XkbBellNotifyEvent *) xkb_ev;
  MetaScreen *screen;

  g_assert (xkb_ev->xkb_type == XkbBellNotify);
  if (xkb_bell_ev->window != None)
    {
      screen = meta_display_screen_for_xwindow (display, xkb_bell_ev->window);
      if (screen)
	bell_flash_screen (display, screen);
    }
  else 
    {
      GSList *screen_list = display->screens;
      while (screen_list) 
	{
	  screen = (MetaScreen *) screen_list->data;
	  bell_flash_screen (display, screen);
	  screen_list = screen_list->next;
	}
    }
}

/**
 * Makes a frame be not flashed; this is the timeout half of
 * bell_flash_window_frame(). This is done simply by clearing the
 * flash flag and queuing a redraw of the frame.
 *
 * If the configure script found we had no XKB, this does not exist.
 *
 * \param data  The frame to unflash, cast to a gpointer so it can go into
 *              a callback function.
 * \return Always FALSE, so we don't get called again.
 *
 * \bug This is the parallel to bell_flash_window_frame(), so it should
 * really be called meta_bell_unflash_window_frame().
 */
static gboolean 
bell_unflash_frame (gpointer data)
{
  MetaFrame *frame = (MetaFrame *) data;
  frame->is_flashing = 0;
  meta_frame_queue_draw (frame);
  return FALSE;
}

/**
 * Makes a frame flash and then return to normal shortly afterwards.
 * This is done by setting a flag so that the theme
 * code will temporarily draw the frame as focussed if it's unfocussed and
 * vice versa, and then queueing a redraw. Lastly, we create a timeout so
 * that the flag can be unset and the frame re-redrawn.
 *
 * If the configure script found we had no XKB, this does not exist.
 *
 * \param window  The window to flash
 */
static void
bell_flash_window_frame (MetaWindow *window)
{
  g_assert (window->frame != NULL);
  window->frame->is_flashing = 1;
  meta_frame_queue_draw (window->frame);
  g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE, 100, 
      bell_unflash_frame, window->frame, NULL);
}

/**
 * Flashes the frame of the focussed window. If there is no focussed window,
 * flashes the screen.
 *
 * \param display  The display the bell event came in on
 * \param xkb_ev   The bell event we just received
 */
static void
bell_flash_frame (MetaDisplay *display, 
		  XkbAnyEvent *xkb_ev)
{
  XkbBellNotifyEvent *xkb_bell_event = (XkbBellNotifyEvent *) xkb_ev;
  MetaWindow *window;
  
  g_assert (xkb_ev->xkb_type == XkbBellNotify);
  window = meta_display_lookup_x_window (display, xkb_bell_event->window);
  if (!window && (display->focus_window))
    {
      window = display->focus_window;
    }
  if (window && window->frame)
    {
      bell_flash_window_frame (window);
    }
  else /* revert to fullscreen flash if there's no focussed window */
    {
      bell_flash_fullscreen (display, xkb_ev);
    }
}

/**
 * Gives the user some kind of visual bell substitute, in response to a
 * bell event. What this is depends on the "visual bell type" pref.
 *
 * If the configure script found we had no XKB, this does not exist.
 *
 * \param display  The display the bell event came in on
 * \param xkb_ev   The bell event we just received
 *
 * \bug This should be merged with meta_bell_notify().
 */
static void
bell_visual_notify (MetaDisplay *display, 
			 XkbAnyEvent *xkb_ev)
{
  switch (meta_prefs_get_visual_bell_type ()) 
    {
    case META_VISUAL_BELL_FULLSCREEN_FLASH:
      bell_flash_fullscreen (display, xkb_ev);
      break;
    case META_VISUAL_BELL_FRAME_FLASH:
      bell_flash_frame (display, xkb_ev); /* does nothing yet */
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
    bell_visual_notify (display, xkb_ev);

  if (meta_prefs_bell_is_audible ()) 
    {
      ca_proplist *p;
      XkbBellNotifyEvent *xkb_bell_event = (XkbBellNotifyEvent*) xkb_ev;
      MetaWindow *window;
      int res;

      ca_proplist_create (&p);
      ca_proplist_sets (p, CA_PROP_EVENT_ID, "bell-window-system");
      ca_proplist_sets (p, CA_PROP_EVENT_DESCRIPTION, _("Bell event"));
      ca_proplist_sets (p, CA_PROP_CANBERRA_CACHE_CONTROL, "permanent");

      window = meta_display_lookup_x_window (display, xkb_bell_event->window);
      if (!window && (display->focus_window) && (display->focus_window->frame))
        window = display->focus_window;

      if (window)
        { 
          ca_proplist_sets (p, CA_PROP_WINDOW_NAME, window->title);
          ca_proplist_setf (p, CA_PROP_WINDOW_X11_XID, "%lu", (unsigned long)window->xwindow);
          ca_proplist_sets (p, CA_PROP_APPLICATION_NAME, window->res_name);
          ca_proplist_setf (p, CA_PROP_APPLICATION_PROCESS_ID, "%d", window->net_wm_pid);
        }

      /* First, we try to play a real sound ... */
      res = ca_context_play_full (ca_gtk_context_get (), 1, p, NULL, NULL);

      ca_proplist_destroy (p);

      if (res != CA_SUCCESS && res != CA_ERROR_DISABLED)
        {      
          /* ...and in case that failed we use the classic X11 bell. */
          XkbForceDeviceBell (display->xdisplay, 
                              xkb_bell_event->device, 
                              xkb_bell_event->bell_class, 
                              xkb_bell_event->bell_id, 
                              xkb_bell_event->percent);
        }
    }
}
#endif /* HAVE_XKB */

void
meta_bell_set_audible (MetaDisplay *display, gboolean audible)
{
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
                                0);
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

/**
 * Deals with a frame being destroyed. This is important because if we're
 * using a visual bell, we might be flashing the edges of the frame, and
 * so we'd have a timeout function waiting ready to un-flash them. If the
 * frame's going away, we can tell the timeout not to bother.
 *
 * \param frame  The frame which is being destroyed
 */
void
meta_bell_notify_frame_destroy (MetaFrame *frame)
{
  if (frame->is_flashing) 
    g_source_remove_by_funcs_user_data (&g_timeout_funcs, frame);
}
