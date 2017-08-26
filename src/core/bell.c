/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Mutter visual bell */

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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * SECTION:bell
 * @short_description: Ring the bell or flash the screen
 *
 * Sometimes, X programs "ring the bell", whatever that means. Mutter lets
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
#include "window-private.h"
#include "util-private.h"
#include "compositor/compositor-private.h"
#include "x11/meta-x11-display-private.h"
#include <meta/prefs.h>
#include <meta/compositor.h>
#ifdef HAVE_LIBCANBERRA
#include <canberra-gtk.h>
#endif

/**
 * bell_flash_fullscreen:
 * @display: The display the event came in on
 * @xkb_ev: The bell event
 *
 * Flashes one screen, or all screens, in response to a bell event.
 * If the event is on a particular window, flash the screen that
 * window is on. Otherwise, flash every screen on this display.
 *
 * If the configure script found we had no XKB, this does not exist.
 */
static void
bell_flash_fullscreen (MetaDisplay *display)
{
  meta_compositor_flash_screen (display->compositor, display->screen);
}

/**
 * bell_unflash_frame:
 * @data: The frame to unflash, cast to a gpointer so it can go into
 *        a callback function.
 *
 * Makes a frame be not flashed; this is the timeout half of
 * bell_flash_window_frame(). This is done simply by clearing the
 * flash flag and queuing a redraw of the frame.
 *
 * If the configure script found we had no XKB, this does not exist.
 *
 * Returns: Always FALSE, so we don't get called again.
 */

/*
 * Bug: This is the parallel to bell_flash_window_frame(), so it should
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
 * bell_flash_window_frame:
 * @window: The window to flash
 *
 * Makes a frame flash and then return to normal shortly afterwards.
 * This is done by setting a flag so that the theme
 * code will temporarily draw the frame as focussed if it's unfocussed and
 * vice versa, and then queueing a redraw. Lastly, we create a timeout so
 * that the flag can be unset and the frame re-redrawn.
 *
 * If the configure script found we had no XKB, this does not exist.
 */
static void
bell_flash_window_frame (MetaWindow *window)
{
  guint id;
  g_assert (window->frame != NULL);
  window->frame->is_flashing = 1;
  meta_frame_queue_draw (window->frame);
  /* Since this idle is added after the Clutter clock source, with
   * the same priority, it will be executed after it as well, so
   * we are guaranteed to get at least one frame drawn in the
   * flashed state, no matter how loaded we are.
   */
  id = g_timeout_add_full (META_PRIORITY_REDRAW, 100,
        bell_unflash_frame, window->frame, NULL);
  g_source_set_name_by_id (id, "[mutter] bell_unflash_frame");
}

static void
bell_flash_window (MetaWindow *window)
{
  meta_compositor_flash_window (window->display->compositor, window);
}

/**
 * bell_flash_frame:
 * @display:  The display the bell event came in on
 * @xkb_ev:   The bell event we just received
 *
 * Flashes the frame of the focused window. If there is no focused window,
 * flashes the screen.
 */
static void
bell_flash_frame (MetaDisplay *display,
                  MetaWindow  *window)
{
  if (window && window->frame)
    bell_flash_window_frame (window);
  else if (window)
    bell_flash_window (window);
  else
    bell_flash_fullscreen (display);
}

/**
 * bell_visual_notify:
 * @display: The display the bell event came in on
 * @xkb_ev: The bell event we just received
 *
 * Gives the user some kind of visual bell substitute, in response to a
 * bell event. What this is depends on the "visual bell type" pref.
 */
static void
bell_visual_notify (MetaDisplay *display,
                    MetaWindow  *window)
{
  switch (meta_prefs_get_visual_bell_type ())
    {
    case G_DESKTOP_VISUAL_BELL_FULLSCREEN_FLASH:
      bell_flash_fullscreen (display);
      break;
    case G_DESKTOP_VISUAL_BELL_FRAME_FLASH:
      bell_flash_frame (display, window);
      break;
    }
}

static gboolean
bell_audible_notify (MetaDisplay *display,
                     MetaWindow  *window)
{
#ifdef HAVE_LIBCANBERRA
  ca_proplist *p;
  int res;

  ca_proplist_create (&p);
  ca_proplist_sets (p, CA_PROP_EVENT_ID, "bell-window-system");
  ca_proplist_sets (p, CA_PROP_EVENT_DESCRIPTION, _("Bell event"));
  ca_proplist_sets (p, CA_PROP_CANBERRA_CACHE_CONTROL, "permanent");

  if (window)
    {
      ca_proplist_sets (p, CA_PROP_WINDOW_NAME, window->title);
      ca_proplist_setf (p, CA_PROP_WINDOW_X11_XID, "%lu", (unsigned long)window->xwindow);
      ca_proplist_sets (p, CA_PROP_APPLICATION_NAME, window->res_name);
      ca_proplist_setf (p, CA_PROP_APPLICATION_PROCESS_ID, "%d", window->net_wm_pid);
    }

  res = ca_context_play_full (ca_gtk_context_get (), 1, p, NULL, NULL);

  ca_proplist_destroy (p);

  return res == CA_SUCCESS || res == CA_ERROR_DISABLED;
#endif /* HAVE_LIBCANBERRA */

  return FALSE;
}

gboolean
meta_bell_notify (MetaDisplay *display,
                  MetaWindow  *window)
{
  /* flash something */
  if (meta_prefs_get_visual_bell ())
    bell_visual_notify (display, window);

  if (meta_prefs_bell_is_audible ())
    return bell_audible_notify (display, window);

  return TRUE;
}

void
meta_bell_set_audible (MetaDisplay *display, gboolean audible)
{
  MetaX11Display *x11_display = display->x11_display;
#ifdef HAVE_LIBCANBERRA
  /* When we are playing sounds using libcanberra support, we handle the
   * bell whether its an audible bell or a visible bell */
  gboolean enable_system_bell = FALSE;
#else
  gboolean enable_system_bell = audible;
#endif /* HAVE_LIBCANBERRA */

  XkbChangeEnabledControls (x11_display->xdisplay,
                            XkbUseCoreKbd,
                            XkbAudibleBellMask,
                            enable_system_bell ? XkbAudibleBellMask : 0);
}

gboolean
meta_bell_init (MetaDisplay *display)
{
  int xkb_base_error_type, xkb_opcode;
  MetaX11Display *x11_display = display->x11_display;

  if (!XkbQueryExtension (x11_display->xdisplay, &xkb_opcode,
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
      XkbSelectEvents (x11_display->xdisplay,
		       XkbUseCoreKbd,
		       XkbBellNotifyMask,
		       XkbBellNotifyMask);
      meta_bell_set_audible (display, meta_prefs_bell_is_audible ());
      if (visual_bell_auto_reset) {
        XkbSetAutoResetControls (x11_display->xdisplay,
				 XkbAudibleBellMask,
				 &mask,
				 &mask);
      }
      return TRUE;
    }
  return FALSE;
}

void
meta_bell_shutdown (MetaDisplay *display)
{
  MetaX11Display *x11_display = display->x11_display;

  /* TODO: persist initial bell state in display, reset here */
  XkbChangeEnabledControls (x11_display->xdisplay,
			    XkbUseCoreKbd,
			    XkbAudibleBellMask,
			    XkbAudibleBellMask);
}

/**
 * meta_bell_notify_frame_destroy:
 * @frame: The frame which is being destroyed
 *
 * Deals with a frame being destroyed. This is important because if we're
 * using a visual bell, we might be flashing the edges of the frame, and
 * so we'd have a timeout function waiting ready to un-flash them. If the
 * frame's going away, we can tell the timeout not to bother.
 */
void
meta_bell_notify_frame_destroy (MetaFrame *frame)
{
  if (frame->is_flashing)
    g_source_remove_by_funcs_user_data (&g_timeout_funcs, frame);
}
