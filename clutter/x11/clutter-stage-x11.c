/* Clutter.
 * An OpenGL based 'interactive canvas' library.
 * Authored By Matthew Allum  <mallum@openedhand.com>
 * Copyright (C) 2006-2007 OpenedHand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-backend-x11.h"
#include "clutter-stage-x11.h"
#include "clutter-x11.h"

#include "../clutter-main.h"
#include "../clutter-feature.h"
#include "../clutter-color.h"
#include "../clutter-util.h"
#include "../clutter-event.h"
#include "../clutter-enum-types.h"
#include "../clutter-private.h"
#include "../clutter-debug.h"
#include "../clutter-units.h"

#include "cogl.h"

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>

G_DEFINE_TYPE (ClutterStageX11, clutter_stage_x11, CLUTTER_TYPE_STAGE);

#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

static void
send_wmspec_change_state (ClutterBackendX11 *backend_x11,
			  Window             window,
			  Atom               state,
			  gboolean           add)
{
  XClientMessageEvent xclient;

  memset (&xclient, 0, sizeof (xclient));

  xclient.type         = ClientMessage;
  xclient.window       = window;
  xclient.message_type = backend_x11->atom_NET_WM_STATE;
  xclient.format       = 32;

  xclient.data.l[0] = add ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
  xclient.data.l[1] = state;
  xclient.data.l[2] = 0;
  xclient.data.l[3] = 0;
  xclient.data.l[4] = 0;

  XSendEvent (backend_x11->xdpy, 
	      DefaultRootWindow(backend_x11->xdpy), 
	      False,
              SubstructureRedirectMask|SubstructureNotifyMask,
              (XEvent *)&xclient);
}

void
clutter_stage_x11_fix_window_size (ClutterStageX11 *stage_x11)
{
  gboolean resize;

  resize = clutter_stage_get_user_resizable (CLUTTER_STAGE (stage_x11));

  if (stage_x11->xwin != None && stage_x11->is_foreign_xwin == FALSE)
    {
      XSizeHints *size_hints;

      size_hints = XAllocSizeHints();

      if (!resize)
	{
	  size_hints->max_width = size_hints->min_width =
            stage_x11->xwin_width;
	  size_hints->max_height = size_hints->min_height =
            stage_x11->xwin_height;
	  size_hints->flags = PMinSize|PMaxSize;
	}

      XSetWMNormalHints (stage_x11->xdpy, stage_x11->xwin, size_hints);

      XFree(size_hints);
    }
}

static void
clutter_stage_x11_show (ClutterActor *actor)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (actor);

  if (stage_x11->xwin)
    {
      /* Fire off a redraw to avoid flicker on first map.
       * Appears not to work perfectly on intel drivers at least.
      */
      clutter_redraw();

      XSync (stage_x11->xdpy, FALSE);
      XMapWindow (stage_x11->xdpy, stage_x11->xwin);
    }

  /* chain up */
  CLUTTER_ACTOR_CLASS (clutter_stage_x11_parent_class)->show (actor);
}

static void
clutter_stage_x11_hide (ClutterActor *actor)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (actor);

  if (stage_x11->xwin)
    XUnmapWindow (stage_x11->xdpy, stage_x11->xwin);

  /* chain up */
  CLUTTER_ACTOR_CLASS (clutter_stage_x11_parent_class)->hide (actor);
}

void
clutter_stage_x11_set_wm_protocols (ClutterStageX11 *stage_x11)
{
  ClutterBackendX11 *backend_x11 = stage_x11->backend;
  Atom protocols[2];
  int n = 0;
  
  protocols[n++] = backend_x11->atom_WM_DELETE_WINDOW;
  protocols[n++] = backend_x11->atom_NET_WM_PING;

  XSetWMProtocols (stage_x11->xdpy, stage_x11->xwin, protocols, n);
}

static void
clutter_stage_x11_query_coords (ClutterActor        *self,
				ClutterActorBox     *box)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (self);

  box->x1 = box->y1 = 0;
  box->x2 = box->x1 + CLUTTER_UNITS_FROM_INT (stage_x11->xwin_width);
  box->y2 = box->y1 + CLUTTER_UNITS_FROM_INT (stage_x11->xwin_height);
}

static void
clutter_stage_x11_request_coords (ClutterActor        *self,
				  ClutterActorBox     *box)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (self);
  gint new_width, new_height;

  new_width  = ABS (CLUTTER_UNITS_TO_INT (box->x2 - box->x1));
  new_height = ABS (CLUTTER_UNITS_TO_INT (box->y2 - box->y1)); 

  if (new_width != stage_x11->xwin_width ||
      new_height != stage_x11->xwin_height)
    {
      stage_x11->xwin_width  = new_width;
      stage_x11->xwin_height = new_height;

      if (stage_x11->xwin != None)
	{
	  XResizeWindow (stage_x11->xdpy, 
			 stage_x11->xwin,
			 stage_x11->xwin_width,
			 stage_x11->xwin_height);

	  clutter_stage_x11_fix_window_size (stage_x11);
	}
      
      if (stage_x11->xpixmap != None)
	{
	  /* Need to recreate to resize */
	  clutter_actor_unrealize (self);
	  clutter_actor_realize (self);
	}

      CLUTTER_SET_PRIVATE_FLAGS(self, CLUTTER_ACTOR_SYNC_MATRICES);
    }

  if (stage_x11->xwin != None) /* Do we want to bother ? */
    XMoveWindow (stage_x11->xdpy,
		 stage_x11->xwin,
		 CLUTTER_UNITS_TO_INT (box->x1),
		 CLUTTER_UNITS_TO_INT (box->y1));
}

static void
clutter_stage_x11_set_fullscreen (ClutterStage *stage,
                                  gboolean      fullscreen)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage);
  ClutterBackendX11 *backend_x11 = stage_x11->backend;

  static gboolean was_resizeable = FALSE;

  if (fullscreen)
    {
      if (stage_x11->xwin != None)
	{
          /* if the actor is not mapped we resize the stage window to match
           * the size of the screen; this is useful for e.g. EGLX to avoid
           * a resize when calling clutter_stage_fullscreen() before showing
           * the stage
           */
	  if (!CLUTTER_ACTOR_IS_MAPPED (stage_x11))
	    {
	      gint width, height;

	      width  = DisplayWidth (stage_x11->xdpy, stage_x11->xscreen);
	      height = DisplayHeight (stage_x11->xdpy, stage_x11->xscreen);

	      clutter_actor_set_size (CLUTTER_ACTOR (stage_x11), 
				      width, height);

	      /* FIXME: This wont work if we support more states */
	      XChangeProperty (stage_x11->xdpy,
                               stage_x11->xwin,
                               backend_x11->atom_NET_WM_STATE, XA_ATOM, 32,
                               PropModeReplace,
                               (unsigned char *) &backend_x11->atom_NET_WM_STATE_FULLSCREEN, 1);
	    }
	  else
	    {
	      /* We need to set window user resize-able for metacity at 
	       * at least to allow the window to fullscreen *sigh* 	 
	      */
	      if (clutter_stage_get_user_resizable (stage) == TRUE)
		was_resizeable = TRUE;
	      else
		clutter_stage_set_user_resizable (stage, TRUE);

	      send_wmspec_change_state(backend_x11, stage_x11->xwin,
				       backend_x11->atom_NET_WM_STATE_FULLSCREEN,
				       TRUE);
	    }

          stage_x11->fullscreen_on_map = TRUE;
	}
    }
  else
    {
      if (stage_x11->xwin != None)
	{
	  if (!CLUTTER_ACTOR_IS_MAPPED (stage_x11))
	    {
	      /* FIXME: This wont work if we support more states */
	      XDeleteProperty (stage_x11->xdpy, 
			       stage_x11->xwin, 
			       backend_x11->atom_NET_WM_STATE);
	    }
	  else
	    {
	      clutter_stage_set_user_resizable (stage, TRUE);

	      send_wmspec_change_state(backend_x11,
				       stage_x11->xwin,
				       backend_x11->atom_NET_WM_STATE_FULLSCREEN,
				       FALSE);

	      /* reset the windows state - this isn't fun - see above */
	      if (!was_resizeable)
		clutter_stage_set_user_resizable (stage, FALSE);

	      was_resizeable = FALSE;
	    }

          stage_x11->fullscreen_on_map = FALSE;
	}
    }

  CLUTTER_SET_PRIVATE_FLAGS (stage, CLUTTER_ACTOR_SYNC_MATRICES);
}

static void
clutter_stage_x11_set_cursor_visible (ClutterStage *stage,
                                      gboolean      show_cursor)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage);

  if (stage_x11->xwin == None)
    return;

  CLUTTER_NOTE (BACKEND, "setting cursor state ('%s') over stage window (%u)",
                show_cursor ? "visible" : "invisible",
                (unsigned int) stage_x11->xwin);

  if (show_cursor)
    {
#if HAVE_XFIXES
      XFixesShowCursor (stage_x11->xdpy, stage_x11->xwin);
#else
      XUndefineCursor (stage_x11->xdpy, stage_x11->xwin);
#endif /* HAVE_XFIXES */
    }
  else
    {
#if HAVE_XFIXES
      XFixesHideCursor (stage_x11->xdpy, stage_x11->xwin);
#else
      XColor col;
      Pixmap pix;
      Cursor curs;

      pix = XCreatePixmap (stage_x11->xdpy, stage_x11->xwin, 1, 1, 1);
      memset (&col, 0, sizeof (col));
      curs = XCreatePixmapCursor (stage_x11->xdpy, 
                                  pix, pix,
                                  &col, &col,
                                  1, 1);
      XFreePixmap (stage_x11->xdpy, pix);
      XDefineCursor (stage_x11->xdpy, stage_x11->xwin, curs);
#endif /* HAVE_XFIXES */
    }
}

static void
clutter_stage_x11_set_title (ClutterStage *stage,
			     const gchar  *title)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage);
  ClutterBackendX11 *backend_x11 = stage_x11->backend;

  if (stage_x11->xwin == None)
    return;

  if (title == NULL)
    {
      XDeleteProperty (stage_x11->xdpy, 
		       stage_x11->xwin, 
		       backend_x11->atom_NET_WM_NAME);
    }
  else
    {
      XChangeProperty (stage_x11->xdpy, 
		       stage_x11->xwin, 
		       backend_x11->atom_NET_WM_NAME, 
		       backend_x11->atom_UTF8_STRING, 
		       8, 
		       PropModeReplace, 
		       (unsigned char*)title, 
		       (int)strlen(title));
    }
}

static void
clutter_stage_x11_set_user_resize (ClutterStage *stage,
				   gboolean      value)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage);

  clutter_stage_x11_fix_window_size (stage_x11);
}

static void
clutter_stage_x11_set_offscreen (ClutterStage *stage,
                                 gboolean      offscreen)
{
  /* Do nothing ? */
}

static void
clutter_stage_x11_dispose (GObject *gobject)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (gobject);

  if (stage_x11->xwin)
    clutter_actor_unrealize (CLUTTER_ACTOR (stage_x11));

  G_OBJECT_CLASS (clutter_stage_x11_parent_class)->dispose (gobject);
}

static void
clutter_stage_x11_class_init (ClutterStageX11Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  ClutterStageClass *stage_class = CLUTTER_STAGE_CLASS (klass);

  gobject_class->dispose = clutter_stage_x11_dispose;
  
  actor_class->show = clutter_stage_x11_show;
  actor_class->hide = clutter_stage_x11_hide;
  actor_class->request_coords = clutter_stage_x11_request_coords;
  actor_class->query_coords = clutter_stage_x11_query_coords;
  
  stage_class->set_fullscreen = clutter_stage_x11_set_fullscreen;
  stage_class->set_cursor_visible = clutter_stage_x11_set_cursor_visible;
  stage_class->set_offscreen = clutter_stage_x11_set_offscreen;
  stage_class->set_title = clutter_stage_x11_set_title;
  stage_class->set_user_resize = clutter_stage_x11_set_user_resize;
}

static void
clutter_stage_x11_init (ClutterStageX11 *stage)
{
  stage->xdpy = NULL;
  stage->xwin_root = None;
  stage->xscreen = 0;

  stage->xwin = None;
  stage->xwin_width = 640;
  stage->xwin_height = 480;
  stage->xvisinfo = None;

  stage->is_foreign_xwin = FALSE;
  stage->fullscreen_on_map = FALSE;

  CLUTTER_SET_PRIVATE_FLAGS (stage, CLUTTER_ACTOR_SYNC_MATRICES);
}

/**
 * clutter_x11_get_stage_window:
 * @stage: a #ClutterStage
 *
 * Gets the stages X Window. 
 *
 * Return value: An XID for the stage window.
 *
 * Since: 0.4
 */
Window
clutter_x11_get_stage_window (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE_X11 (stage), None);

  return CLUTTER_STAGE_X11 (stage)->xwin;
}

/**
 * clutter_x11_get_stage_visual:
 * @stage: a #ClutterStage
 *
 * Returns the stage XVisualInfo
 *
 * Return value: The XVisualInfo for the stage.
 *
 * Since: 0.4
 */
XVisualInfo *
clutter_x11_get_stage_visual (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE_X11 (stage), NULL);

  return CLUTTER_STAGE_X11 (stage)->xvisinfo;
}

/**
 * clutter_x11_set_stage_foreign:
 * @stage: a #ClutterStage
 * @xwindow: an existing X Window id
 *
 * Target the #ClutterStage to use an existing external X Window
 *
 * Return value: %TRUE if foreign window is valid
 *
 * Since: 0.4
 */
gboolean
clutter_x11_set_stage_foreign (ClutterStage *stage,
                               Window        xwindow)
{
  ClutterStageX11 *stage_x11;
  ClutterActor *actor;
  gint x, y;
  guint width, height, border, depth;
  Window root_return;
  Status status;
  ClutterGeometry geom;

  g_return_val_if_fail (CLUTTER_IS_STAGE_X11 (stage), FALSE);
  g_return_val_if_fail (xwindow != None, FALSE);

  stage_x11 = CLUTTER_STAGE_X11 (stage);
  actor = CLUTTER_ACTOR (stage);

  clutter_x11_trap_x_errors ();

  status = XGetGeometry (stage_x11->xdpy,
                         xwindow,
                         &root_return,
                         &x, &y,
                         &width, &height,
                         &border,
                         &depth);
  
  if (clutter_x11_untrap_x_errors () ||
      !status ||
      width == 0 || height == 0 ||
      depth != stage_x11->xvisinfo->depth)
    {
      return FALSE;
    }

  clutter_actor_unrealize (actor);

  stage_x11->xwin = xwindow;
  stage_x11->is_foreign_xwin = TRUE;

  geom.x = x;
  geom.y = y;
  geom.width = stage_x11->xwin_width = width;
  geom.height = stage_x11->xwin_height = height;

  clutter_actor_set_geometry (actor, &geom);
  clutter_actor_realize (actor);

  return TRUE;
}

void
clutter_stage_x11_map (ClutterStageX11 *stage_x11)
{
  CLUTTER_ACTOR_SET_FLAGS (stage_x11, CLUTTER_ACTOR_MAPPED);

  if (stage_x11->fullscreen_on_map)
    clutter_stage_fullscreen (CLUTTER_STAGE (stage_x11));
  else
    clutter_stage_unfullscreen (CLUTTER_STAGE (stage_x11));

  clutter_actor_queue_redraw (CLUTTER_ACTOR (stage_x11));
}

void
clutter_stage_x11_unmap (ClutterStageX11 *stage_x11)
{
  CLUTTER_ACTOR_UNSET_FLAGS (stage_x11, CLUTTER_ACTOR_MAPPED);
}

