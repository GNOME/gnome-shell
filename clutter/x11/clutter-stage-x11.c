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

#include "../clutter-stage-window.h"
#include "../clutter-main.h"
#include "../clutter-feature.h"
#include "../clutter-color.h"
#include "../clutter-util.h"
#include "../clutter-event.h"
#include "../clutter-enum-types.h"
#include "../clutter-private.h"
#include "../clutter-debug.h"
#include "../clutter-units.h"

#include "cogl/cogl.h"

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterStageX11,
                         clutter_stage_x11,
                         CLUTTER_TYPE_GROUP,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init));

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

  resize = clutter_stage_get_user_resizable (stage_x11->wrapper);

  if (stage_x11->xwin != None && stage_x11->is_foreign_xwin == FALSE)
    {
      XSizeHints *size_hints;
      ClutterUnit min_width, min_height;

      size_hints = XAllocSizeHints();

      clutter_actor_get_preferred_width (CLUTTER_ACTOR (stage_x11),
                                         -1,
                                         &min_width, NULL);
      clutter_actor_get_preferred_height (CLUTTER_ACTOR (stage_x11),
                                          min_width,
                                          &min_height, NULL);

      size_hints->min_width = CLUTTER_UNITS_TO_DEVICE (min_width);
      size_hints->min_height = CLUTTER_UNITS_TO_DEVICE (min_height);
      size_hints->flags = PMinSize;

      if (!resize)
        {
          size_hints->max_width = size_hints->min_width;
          size_hints->max_height = size_hints->min_height;
          size_hints->flags |= PMaxSize;
        }

      XSetWMNormalHints (stage_x11->xdpy, stage_x11->xwin, size_hints);

      XFree(size_hints);
    }
}

static void
clutter_stage_x11_show (ClutterActor *actor)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (actor);

  CLUTTER_ACTOR_CLASS (clutter_stage_x11_parent_class)->show (actor);

  if (stage_x11->xwin)
    {
      /* Fire off a redraw to avoid flicker on first map.
       * Appears not to work perfectly on intel drivers at least.
       */
      clutter_redraw (stage_x11->wrapper);

      XSync (stage_x11->xdpy, FALSE);
      XMapWindow (stage_x11->xdpy, stage_x11->xwin);
    }
}

static void
clutter_stage_x11_hide (ClutterActor *actor)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (actor);

  if (stage_x11->xwin)
    XUnmapWindow (stage_x11->xdpy, stage_x11->xwin);
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
clutter_stage_x11_get_preferred_width (ClutterActor *self,
                                       ClutterUnit   for_height,
                                       ClutterUnit  *min_width_p,
                                       ClutterUnit  *natural_width_p)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (self);
  gboolean resize;

  if (stage_x11->fullscreen_on_map)
    {
      int width;

      width = DisplayWidth (stage_x11->xdpy, stage_x11->xscreen);

      if (min_width_p)
        *min_width_p = CLUTTER_UNITS_FROM_DEVICE (width);

      if (natural_width_p)
        *natural_width_p = CLUTTER_UNITS_FROM_DEVICE (width);

      return;
    }

  resize = clutter_stage_get_user_resizable (stage_x11->wrapper);

  if (min_width_p)
    {
      /* FIXME need API to set this */
      if (resize)
        *min_width_p = CLUTTER_UNITS_FROM_DEVICE (1);
      else
        *min_width_p = CLUTTER_UNITS_FROM_DEVICE (stage_x11->xwin_width);
    }

  if (natural_width_p)
    *natural_width_p = CLUTTER_UNITS_FROM_DEVICE (stage_x11->xwin_width);
}

static void
clutter_stage_x11_get_preferred_height (ClutterActor *self,
                                        ClutterUnit   for_width,
                                        ClutterUnit  *min_height_p,
                                        ClutterUnit  *natural_height_p)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (self);
  gboolean resize;

  if (stage_x11->fullscreen_on_map)
    {
      int height;

      height = DisplayHeight (stage_x11->xdpy, stage_x11->xscreen);

      if (min_height_p)
        *min_height_p = CLUTTER_UNITS_FROM_DEVICE (height);

      if (natural_height_p)
        *natural_height_p = CLUTTER_UNITS_FROM_DEVICE (height);

      return;
    }

  resize = clutter_stage_get_user_resizable (stage_x11->wrapper);

  if (min_height_p)
    {
      if (resize)
        *min_height_p = CLUTTER_UNITS_FROM_DEVICE (1); /* FIXME need API
                                                        * to set this
                                                        */
      else
        *min_height_p = CLUTTER_UNITS_FROM_DEVICE (stage_x11->xwin_height);
    }

  if (natural_height_p)
    *natural_height_p = CLUTTER_UNITS_FROM_DEVICE (stage_x11->xwin_height);
}

static void
clutter_stage_x11_allocate (ClutterActor          *self,
                            const ClutterActorBox *box,
                            gboolean               origin_changed)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (self);
  ClutterActorClass *parent_class;
  gint new_width, new_height;

  new_width  = ABS (CLUTTER_UNITS_TO_INT (box->x2 - box->x1));
  new_height = ABS (CLUTTER_UNITS_TO_INT (box->y2 - box->y1));

  if (new_width == 0 || new_height == 0)
    {
      /* Should not happen, if this turns up we need to debug it and
       * determine the cleanest way to fix.
       */
      g_warning ("X11 stage not allowed to have 0 width or height");
      new_width = 1;
      new_height = 1;
    }

  if (new_width != stage_x11->xwin_width ||
      new_height != stage_x11->xwin_height)
    {
      stage_x11->xwin_width  = new_width;
      stage_x11->xwin_height = new_height;

      if (stage_x11->xwin != None &&
	  !stage_x11->is_foreign_xwin)
        {
          CLUTTER_NOTE (BACKEND, "%s: XResizeWindow[%x] (%d, %d)",
                        G_STRLOC,
                        (unsigned int) stage_x11->xwin,
                        stage_x11->xwin_width,
                        stage_x11->xwin_height);

          XResizeWindow (stage_x11->xdpy,
                         stage_x11->xwin,
                         stage_x11->xwin_width,
                         stage_x11->xwin_height);
        }

      clutter_stage_x11_fix_window_size (stage_x11);

      if (stage_x11->xpixmap != None)
        {
          /* Need to recreate to resize */
          clutter_actor_unrealize (self);
          clutter_actor_realize (self);
        }
    }

  /* chain up to fill in actor->priv->allocation */
  parent_class = CLUTTER_ACTOR_CLASS (clutter_stage_x11_parent_class);
  parent_class->allocate (self, box, origin_changed);
}

static inline void
set_wm_title (ClutterStageX11 *stage_x11)
{
  ClutterBackendX11 *backend_x11 = stage_x11->backend;

  if (stage_x11->xwin == None)
    return;

  if (stage_x11->title == NULL)
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
                       (unsigned char *) stage_x11->title, 
                       (int) strlen (stage_x11->title));
    }
}

static inline void
set_cursor_visible (ClutterStageX11 *stage_x11)
{
  if (stage_x11->xwin == None)
    return;

  CLUTTER_NOTE (BACKEND, "setting cursor state ('%s') over stage window (%u)",
                stage_x11->is_cursor_visible ? "visible" : "invisible",
                (unsigned int) stage_x11->xwin);

  if (stage_x11->is_cursor_visible)
    {
#if 0 /* HAVE_XFIXES - seems buggy/unreliable */
      XFixesShowCursor (stage_x11->xdpy, stage_x11->xwin);
#else
      XUndefineCursor (stage_x11->xdpy, stage_x11->xwin);
#endif /* HAVE_XFIXES */
    }
  else
    {
#if 0 /* HAVE_XFIXES - seems buggy/unreliable, check cursor in firefox 
       *               loading page after hiding.  
      */
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
clutter_stage_x11_realize (ClutterActor *actor)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (actor);

  set_wm_title (stage_x11);
  set_cursor_visible (stage_x11);
}

static void
clutter_stage_x11_set_fullscreen (ClutterStageWindow *stage_window,
                                  gboolean            is_fullscreen)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  ClutterBackendX11 *backend_x11 = stage_x11->backend;
  ClutterStage *stage = stage_x11->wrapper;
  static gboolean was_resizeable = FALSE;

  if (!stage)
    return;

  CLUTTER_SET_PRIVATE_FLAGS (stage, CLUTTER_ACTOR_SYNC_MATRICES);

  if (is_fullscreen)
    {
      int width, height;

      width  = DisplayWidth (stage_x11->xdpy, stage_x11->xscreen);
      height = DisplayHeight (stage_x11->xdpy, stage_x11->xscreen);

      /* we force the stage to the screen size here, in order to
       * get the fullscreen stage size right after the call to
       * clutter_stage_fullscreen(). XXX this might break in case
       * the stage is not fullscreened, but if that does not happen
       * we are massively screwed anyway
       */
      stage_x11->xwin_width = width;
      stage_x11->xwin_height = height;

      clutter_actor_set_size (CLUTTER_ACTOR (stage), width, height);

      if (stage_x11->xwin != None)
        {
          stage_x11->fullscreen_on_map = TRUE;

          /* if the actor is not mapped we resize the stage window to match
           * the size of the screen; this is useful for e.g. EGLX to avoid
           * a resize when calling clutter_stage_fullscreen() before showing
           * the stage
           */
          if (!CLUTTER_ACTOR_IS_MAPPED (stage_x11))
            {
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

              send_wmspec_change_state (backend_x11, stage_x11->xwin,
                                        backend_x11->atom_NET_WM_STATE_FULLSCREEN,
                                        TRUE);
            }
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

              send_wmspec_change_state (backend_x11,
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
}

static void
clutter_stage_x11_set_cursor_visible (ClutterStageWindow *stage_window,
                                      gboolean            cursor_visible)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);

  stage_x11->is_cursor_visible = (cursor_visible == TRUE);
  set_cursor_visible (stage_x11);
}

static void
clutter_stage_x11_set_title (ClutterStageWindow *stage_window,
                             const gchar        *title)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);

  g_free (stage_x11->title);
  stage_x11->title = g_strdup (title);
  set_wm_title (stage_x11);
}

static void
clutter_stage_x11_set_user_resizable (ClutterStageWindow *stage_window,
                                      gboolean            is_resizable)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);

  clutter_stage_x11_fix_window_size (stage_x11);
}

static ClutterActor *
clutter_stage_x11_get_wrapper (ClutterStageWindow *stage_window)
{
  return CLUTTER_ACTOR (CLUTTER_STAGE_X11 (stage_window)->wrapper);
}

static void
clutter_stage_x11_finalize (GObject *gobject)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (gobject);

  g_free (stage_x11->title);

  G_OBJECT_CLASS (clutter_stage_x11_parent_class)->finalize (gobject);
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

  gobject_class->finalize = clutter_stage_x11_finalize;
  gobject_class->dispose = clutter_stage_x11_dispose;

  actor_class->realize = clutter_stage_x11_realize;
  actor_class->show = clutter_stage_x11_show;
  actor_class->hide = clutter_stage_x11_hide;

  actor_class->get_preferred_width = clutter_stage_x11_get_preferred_width;
  actor_class->get_preferred_height = clutter_stage_x11_get_preferred_height;
  actor_class->allocate = clutter_stage_x11_allocate;
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
  stage->is_cursor_visible = TRUE;

  stage->title = NULL;

  stage->wrapper = NULL;

  CLUTTER_SET_PRIVATE_FLAGS (stage, CLUTTER_ACTOR_IS_TOPLEVEL);
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  iface->get_wrapper = clutter_stage_x11_get_wrapper;
  iface->set_title = clutter_stage_x11_set_title;
  iface->set_fullscreen = clutter_stage_x11_set_fullscreen;
  iface->set_cursor_visible = clutter_stage_x11_set_cursor_visible;
  iface->set_user_resizable = clutter_stage_x11_set_user_resizable;
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
  ClutterStageWindow *impl;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), None);

  impl = _clutter_stage_get_window (stage);
  g_assert (CLUTTER_IS_STAGE_X11 (impl));

  return CLUTTER_STAGE_X11 (impl)->xwin;
}

/**
 * clutter_x11_get_stage_from_window:
 * @win: an X Window ID
 *
 * Gets the stage for a particular X window.  
 *
 * Return value: The stage or NULL if a stage does not exist for the window.
 *
 * Since: 0.8
 */
ClutterStage *
clutter_x11_get_stage_from_window (Window win)
{
  ClutterMainContext  *context;
  ClutterStageManager *stage_manager;
  GSList              *l;

  context = clutter_context_get_default ();

  stage_manager = context->stage_manager;

  /* FIXME: use a hash here for performance resaon */
  for (l = stage_manager->stages; l; l = l->next)
    {
      ClutterStage *stage = l->data;
      ClutterStageWindow *impl;

      impl = _clutter_stage_get_window (stage);
      g_assert (CLUTTER_IS_STAGE_X11 (impl));

      if (CLUTTER_STAGE_X11 (impl)->xwin == win)
        return stage;
    }

  return NULL;
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
  ClutterStageWindow *impl;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), NULL);

  impl = _clutter_stage_get_window (stage);
  g_assert (CLUTTER_IS_STAGE_X11 (impl));

  return CLUTTER_STAGE_X11 (impl)->xvisinfo;
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
  ClutterStageWindow *impl;
  ClutterActor *actor;
  gint x, y;
  guint width, height, border, depth;
  Window root_return;
  Status status;
  ClutterGeometry geom;

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);
  g_return_val_if_fail (xwindow != None, FALSE);

  actor = CLUTTER_ACTOR (stage);

  impl = _clutter_stage_get_window (stage);
  stage_x11 = CLUTTER_STAGE_X11 (impl);

  clutter_x11_trap_x_errors ();

  status = XGetGeometry (stage_x11->xdpy, xwindow,
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
      g_warning ("Unable to retrieve the new window geometry");
      return FALSE;
    }

  clutter_actor_unrealize (actor);

  CLUTTER_NOTE (BACKEND, "Setting foreign window (0x%x)", (int) xwindow);

  stage_x11->xwin = xwindow;
  stage_x11->is_foreign_xwin = TRUE;

  geom.x = x;
  geom.y = y;
  geom.width = stage_x11->xwin_width = width;
  geom.height = stage_x11->xwin_height = height;

  clutter_actor_set_geometry (actor, &geom);
  clutter_actor_realize (actor);

  CLUTTER_SET_PRIVATE_FLAGS (actor, CLUTTER_ACTOR_SYNC_MATRICES);

  return TRUE;
}

void
clutter_stage_x11_map (ClutterStageX11 *stage_x11)
{
  /* set the mapped flag on the implementation */
  CLUTTER_ACTOR_SET_FLAGS (stage_x11, CLUTTER_ACTOR_MAPPED);

  /* and on the wrapper itself */
  CLUTTER_ACTOR_SET_FLAGS (stage_x11->wrapper, CLUTTER_ACTOR_MAPPED);

  if (stage_x11->fullscreen_on_map)
    clutter_stage_fullscreen (CLUTTER_STAGE (stage_x11->wrapper));
  else
    clutter_stage_unfullscreen (CLUTTER_STAGE (stage_x11->wrapper));

  clutter_actor_queue_relayout (CLUTTER_ACTOR (stage_x11->wrapper));
}

void
clutter_stage_x11_unmap (ClutterStageX11 *stage_x11)
{
  /* like above, unset the MAPPED stage on both the implementation and
   * the wrapper
   */
  CLUTTER_ACTOR_UNSET_FLAGS (stage_x11, CLUTTER_ACTOR_MAPPED);
  CLUTTER_ACTOR_UNSET_FLAGS (stage_x11->wrapper, CLUTTER_ACTOR_MAPPED);
}
