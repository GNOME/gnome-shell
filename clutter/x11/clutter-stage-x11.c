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

#define STAGE_X11_IS_MAPPED(s)  ((((ClutterStageX11 *) (s))->wm_state & STAGE_X11_WITHDRAWN) == 0)

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterStageX11,
                         clutter_stage_x11,
                         G_TYPE_OBJECT,
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
clutter_stage_x11_fix_window_size (ClutterStageX11 *stage_x11,
                                   gint             new_width,
                                   gint             new_height)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendX11 *backend_x11;
  gboolean resize;

  g_return_if_fail (CLUTTER_IS_BACKEND_X11 (backend));
  backend_x11 = CLUTTER_BACKEND_X11 (backend);

  resize = clutter_stage_get_user_resizable (stage_x11->wrapper);

  if (stage_x11->xwin != None && !stage_x11->is_foreign_xwin)
    {
      guint min_width, min_height;
      XSizeHints *size_hints;

      size_hints = XAllocSizeHints();

      clutter_stage_get_minimum_size (stage_x11->wrapper,
                                      &min_width,
                                      &min_height);

      if (new_width <= 0)
        new_width = min_width;

      if (new_height <= 0)
        new_height = min_height;

      size_hints->flags = 0;

      /* If we are going fullscreen then we don't want any
         restrictions on the window size */
      if (!stage_x11->fullscreen_on_map)
        {
          if (resize)
            {
              size_hints->min_width = min_width;
              size_hints->min_height = min_height;
              size_hints->flags = PMinSize;
            }
          else
            {
              size_hints->min_width = new_width;
              size_hints->min_height = new_height;
              size_hints->max_width = new_width;
              size_hints->max_height = new_height;
              size_hints->flags = PMinSize | PMaxSize;
            }
        }

      XSetWMNormalHints (backend_x11->xdpy, stage_x11->xwin, size_hints);

      XFree(size_hints);
    }
}

void
clutter_stage_x11_set_wm_protocols (ClutterStageX11 *stage_x11)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendX11 *backend_x11;
  Atom protocols[2];
  int n = 0;
  
  g_return_if_fail (CLUTTER_IS_BACKEND_X11 (backend));
  backend_x11 = CLUTTER_BACKEND_X11 (backend);

  protocols[n++] = backend_x11->atom_WM_DELETE_WINDOW;
  protocols[n++] = backend_x11->atom_NET_WM_PING;

  XSetWMProtocols (backend_x11->xdpy, stage_x11->xwin, protocols, n);
}

static void
clutter_stage_x11_get_geometry (ClutterStageWindow *stage_window,
                                ClutterGeometry    *geometry)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendX11 *backend_x11;
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  gboolean is_fullscreen;

  g_return_if_fail (CLUTTER_IS_BACKEND_X11 (backend));
  backend_x11 = CLUTTER_BACKEND_X11 (backend);

  is_fullscreen = FALSE;
  g_object_get (G_OBJECT (stage_x11->wrapper),
                "fullscreen-set", &is_fullscreen,
                NULL);

  if (is_fullscreen || stage_x11->fullscreen_on_map)
    {
      geometry->width = DisplayWidth (backend_x11->xdpy, backend_x11->xscreen_num);
      geometry->height = DisplayHeight (backend_x11->xdpy, backend_x11->xscreen_num);

      return;
    }

  geometry->width = stage_x11->xwin_width;
  geometry->height = stage_x11->xwin_height;
}

static void
clutter_stage_x11_resize (ClutterStageWindow *stage_window,
                          gint                width,
                          gint                height)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendX11 *backend_x11;
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  ClutterStage *stage = stage_x11->wrapper;
  gboolean resize;

  resize = clutter_stage_get_user_resizable (stage_x11->wrapper);

  g_return_if_fail (CLUTTER_IS_BACKEND_X11 (backend));
  backend_x11 = CLUTTER_BACKEND_X11 (backend);

  if (width == 0 || height == 0)
    {
      /* Should not happen, if this turns up we need to debug it and
       * determine the cleanest way to fix.
       */
      g_warning ("X11 stage not allowed to have 0 width or height");
      width = 1;
      height = 1;
    }

  CLUTTER_NOTE (BACKEND, "New size received: (%d, %d)", width, height);

  if (width != stage_x11->xwin_width ||
      height != stage_x11->xwin_height)
    {
      stage_x11->xwin_width  = width;
      stage_x11->xwin_height = height;

      if (stage_x11->xwin != None && !stage_x11->is_foreign_xwin)
        {
          CLUTTER_NOTE (BACKEND, "%s: XResizeWindow[%x] (%d, %d)",
                        G_STRLOC,
                        (unsigned int) stage_x11->xwin,
                        stage_x11->xwin_width,
                        stage_x11->xwin_height);

          CLUTTER_SET_PRIVATE_FLAGS (stage_x11->wrapper,
                                     CLUTTER_STAGE_IN_RESIZE);

          XResizeWindow (backend_x11->xdpy,
                         stage_x11->xwin,
                         stage_x11->xwin_width,
                         stage_x11->xwin_height);

          /* If the viewport hasn't previously been initialized then even
           * though we can't guarantee that the server will honour our request
           * we need to ensure a valid viewport is set before our first paint.
           */
          if (G_UNLIKELY (!stage_x11->viewport_initialized))
            {
              ClutterPerspective perspective;
              clutter_stage_get_perspective (stage, &perspective);
              _cogl_setup_viewport (stage_x11->xwin_width,
                                    stage_x11->xwin_height,
                                    perspective.fovy,
                                    perspective.aspect,
                                    perspective.z_near,
                                    perspective.z_far);
            }
        }

      if (!resize)
        clutter_stage_x11_fix_window_size (stage_x11, width, height);
    }
}

static inline void
set_wm_pid (ClutterStageX11 *stage_x11)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);
  long pid;

  if (stage_x11->xwin == None)
    return;

  /* this will take care of WM_CLIENT_MACHINE and WM_LOCALE_NAME */
  XSetWMProperties (backend_x11->xdpy, stage_x11->xwin,
                    NULL,
                    NULL,
                    NULL, 0,
                    NULL, NULL, NULL);

  pid = getpid();
  XChangeProperty (backend_x11->xdpy,
                   stage_x11->xwin,
                   backend_x11->atom_NET_WM_PID, XA_CARDINAL, 32,
                   PropModeReplace,
                   (guchar *) &pid, 1);
}

static inline void
set_wm_title (ClutterStageX11 *stage_x11)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendX11 *backend_x11;

  g_return_if_fail (CLUTTER_IS_BACKEND_X11 (backend));
  backend_x11 = CLUTTER_BACKEND_X11 (backend);

  if (stage_x11->xwin == None)
    return;

  if (stage_x11->title == NULL)
    {
      XDeleteProperty (backend_x11->xdpy,
                       stage_x11->xwin, 
                       backend_x11->atom_NET_WM_NAME);
    }
  else
    {
      XChangeProperty (backend_x11->xdpy,
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
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendX11 *backend_x11;

  g_return_if_fail (CLUTTER_IS_BACKEND_X11 (backend));
  backend_x11 = CLUTTER_BACKEND_X11 (backend);

  if (stage_x11->xwin == None)
    return;

  CLUTTER_NOTE (BACKEND, "setting cursor state ('%s') over stage window (%u)",
                stage_x11->is_cursor_visible ? "visible" : "invisible",
                (unsigned int) stage_x11->xwin);

  if (stage_x11->is_cursor_visible)
    {
#if 0 /* HAVE_XFIXES - seems buggy/unreliable */
      XFixesShowCursor (backend_x11->xdpy, stage_x11->xwin);
#else
      XUndefineCursor (backend_x11->xdpy, stage_x11->xwin);
#endif /* HAVE_XFIXES */
    }
  else
    {
#if 0 /* HAVE_XFIXES - seems buggy/unreliable, check cursor in firefox 
       *               loading page after hiding.  
      */
      XFixesHideCursor (backend_x11->xdpy, stage_x11->xwin);
#else
      XColor col;
      Pixmap pix;
      Cursor curs;

      pix = XCreatePixmap (backend_x11->xdpy, stage_x11->xwin, 1, 1, 1);
      memset (&col, 0, sizeof (col));
      curs = XCreatePixmapCursor (backend_x11->xdpy,
                                  pix, pix,
                                  &col, &col,
                                  1, 1);
      XFreePixmap (backend_x11->xdpy, pix);
      XDefineCursor (backend_x11->xdpy, stage_x11->xwin, curs);
#endif /* HAVE_XFIXES */
    }
}

static gboolean
clutter_stage_x11_realize (ClutterStageWindow *stage_window)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);

  set_wm_pid (stage_x11);
  set_wm_title (stage_x11);
  set_cursor_visible (stage_x11);

  return TRUE;
}

static void
clutter_stage_x11_set_fullscreen (ClutterStageWindow *stage_window,
                                  gboolean            is_fullscreen)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendX11 *backend_x11;
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  ClutterStage *stage = stage_x11->wrapper;

  g_return_if_fail (CLUTTER_IS_BACKEND_X11 (backend));
  backend_x11 = CLUTTER_BACKEND_X11 (backend);

  if (stage == NULL)
    return;

  CLUTTER_SET_PRIVATE_FLAGS (stage, CLUTTER_ACTOR_SYNC_MATRICES);

  if (is_fullscreen)
    {
      int width, height;

      /* FIXME: this will do the wrong thing for dual-headed
         displays. This will return the size of the combined display
         but Metacity (at least) will fullscreen to only one of the
         displays. This will cause the actor to report the wrong size
         until the ConfigureNotify for the correct size is received */
      width  = DisplayWidth (backend_x11->xdpy, backend_x11->xscreen_num);
      height = DisplayHeight (backend_x11->xdpy, backend_x11->xscreen_num);

      /* we force the stage to the screen size here, in order to
       * get the fullscreen stage size right after the call to
       * clutter_stage_fullscreen(). XXX this might break in case
       * the stage is not fullscreened, but if that does not happen
       * we are massively screwed anyway
       */
      stage_x11->xwin_width = width;
      stage_x11->xwin_height = height;

      if (!STAGE_X11_IS_MAPPED (stage_x11))
        stage_x11->fullscreen_on_map = TRUE;

      if (stage_x11->xwin != None)
        {
          /* if the actor is not mapped we resize the stage window to match
           * the size of the screen; this is useful for e.g. EGLX to avoid
           * a resize when calling clutter_stage_fullscreen() before showing
           * the stage
           */
          if (!CLUTTER_ACTOR_IS_MAPPED (stage_x11))
            {
              /* FIXME: This wont work if we support more states */
              XChangeProperty (backend_x11->xdpy,
                               stage_x11->xwin,
                               backend_x11->atom_NET_WM_STATE, XA_ATOM, 32,
                               PropModeReplace,
                               (unsigned char *) &backend_x11->atom_NET_WM_STATE_FULLSCREEN, 1);
            }
          else
            {
              /* We need to fix the window size so that it will remove
                 the maximum and minimum window hints. Otherwise
                 metacity will honour the restrictions and not
                 fullscreen correctly. */
              clutter_stage_x11_fix_window_size (stage_x11, -1, -1);

              send_wmspec_change_state (backend_x11, stage_x11->xwin,
                                        backend_x11->atom_NET_WM_STATE_FULLSCREEN,
                                        TRUE);
            }
        }
    }
  else
    {
      if (!STAGE_X11_IS_MAPPED (stage_x11))
        stage_x11->fullscreen_on_map = FALSE;

      if (stage_x11->xwin != None)
        {
          if (!CLUTTER_ACTOR_IS_MAPPED (stage_x11))
            {
              /* FIXME: This wont work if we support more states */
              XDeleteProperty (backend_x11->xdpy,
                               stage_x11->xwin, 
                               backend_x11->atom_NET_WM_STATE);
            }
          else
            {
              send_wmspec_change_state (backend_x11,
                                        stage_x11->xwin,
                                        backend_x11->atom_NET_WM_STATE_FULLSCREEN,
                                        FALSE);

              /* Fix the window size to restore the minimum/maximum
                 restriction */
              clutter_stage_x11_fix_window_size (stage_x11, -1, -1);
            }
        }
    }

  clutter_stage_ensure_viewport (CLUTTER_STAGE (stage_x11->wrapper));
  clutter_actor_queue_relayout (CLUTTER_ACTOR (stage_x11->wrapper));
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

  clutter_stage_x11_fix_window_size (stage_x11, -1, -1);
}

static void
update_wm_hints (ClutterStageX11 *stage_x11)
{
  ClutterBackend *backend;
  ClutterBackendX11 *backend_x11;
  XWMHints wm_hints;

  if (stage_x11->wm_state & STAGE_X11_WITHDRAWN)
    return;

  if (stage_x11->is_foreign_xwin)
    return;

  backend = clutter_get_default_backend ();

  g_return_if_fail (CLUTTER_IS_BACKEND_X11 (backend));
  backend_x11 = CLUTTER_BACKEND_X11 (backend);

  wm_hints.flags = StateHint;
  wm_hints.initial_state = NormalState;

  XSetWMHints (backend_x11->xdpy, stage_x11->xwin, &wm_hints);
}

static void
set_stage_state (ClutterStageX11      *stage_x11,
                 ClutterStageX11State  unset_flags,
                 ClutterStageX11State  set_flags)
{
  ClutterStageX11State new_stage_state, old_stage_state;

  old_stage_state = stage_x11->wm_state;

  new_stage_state = old_stage_state;
  new_stage_state |= set_flags;
  new_stage_state &= ~unset_flags;

  if (new_stage_state == old_stage_state)
    return;

  stage_x11->wm_state = new_stage_state;
}

static void
clutter_stage_x11_show (ClutterStageWindow *stage_window,
                        gboolean            do_raise)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendX11 *backend_x11;
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);

  g_return_if_fail (CLUTTER_IS_BACKEND_X11 (backend));
  backend_x11 = CLUTTER_BACKEND_X11 (backend);

  if (stage_x11->xwin != None)
    {
      if (do_raise && !stage_x11->is_foreign_xwin)
        {
          CLUTTER_NOTE (BACKEND, "Raising stage[%lu]",
                        (unsigned long) stage_x11->xwin);
          XRaiseWindow (backend_x11->xdpy, stage_x11->xwin);
        }

      if (!STAGE_X11_IS_MAPPED (stage_x11))
        {
          CLUTTER_NOTE (BACKEND, "Mapping stage[%lu]",
                        (unsigned long) stage_x11->xwin);

          if (stage_x11->fullscreen_on_map)
            clutter_stage_x11_set_fullscreen (stage_window, TRUE);
          else
            clutter_stage_x11_set_fullscreen (stage_window, FALSE);

          set_stage_state (stage_x11, STAGE_X11_WITHDRAWN, 0);

          update_wm_hints (stage_x11);
        }

      g_assert (STAGE_X11_IS_MAPPED (stage_x11));

      clutter_actor_map (CLUTTER_ACTOR (stage_x11->wrapper));

      if (!stage_x11->is_foreign_xwin)
        XMapWindow (backend_x11->xdpy, stage_x11->xwin);
    }
}

static void
clutter_stage_x11_hide (ClutterStageWindow *stage_window)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendX11 *backend_x11;
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);

  g_return_if_fail (CLUTTER_IS_BACKEND_X11 (backend));
  backend_x11 = CLUTTER_BACKEND_X11 (backend);

  if (stage_x11->xwin != None)
    {
      if (STAGE_X11_IS_MAPPED (stage_x11))
        set_stage_state (stage_x11, 0, STAGE_X11_WITHDRAWN);

      g_assert (!STAGE_X11_IS_MAPPED (stage_x11));

      clutter_actor_unmap (CLUTTER_ACTOR (stage_x11->wrapper));

      if (!stage_x11->is_foreign_xwin)
        XWithdrawWindow (backend_x11->xdpy, stage_x11->xwin, 0);
    }
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
  G_OBJECT_CLASS (clutter_stage_x11_parent_class)->dispose (gobject);
}

static void
clutter_stage_x11_class_init (ClutterStageX11Class *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = clutter_stage_x11_finalize;
  gobject_class->dispose = clutter_stage_x11_dispose;
}

static void
clutter_stage_x11_init (ClutterStageX11 *stage)
{
  stage->xwin = None;
  stage->xwin_width = 640;
  stage->xwin_height = 480;

  stage->wm_state = STAGE_X11_WITHDRAWN;

  stage->is_foreign_xwin = FALSE;
  stage->fullscreen_on_map = FALSE;
  stage->is_cursor_visible = TRUE;
  stage->viewport_initialized = FALSE;

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
  iface->show = clutter_stage_x11_show;
  iface->hide = clutter_stage_x11_hide;
  iface->resize = clutter_stage_x11_resize;
  iface->get_geometry = clutter_stage_x11_get_geometry;
  iface->realize = clutter_stage_x11_realize;
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
  ClutterStageManager *stage_manager;
  const GSList        *stages, *s;

  stage_manager = clutter_stage_manager_get_default ();
  stages = clutter_stage_manager_peek_stages (stage_manager);

  /* XXX: might use a hash here for performance resaon */
  for (s = stages; s != NULL; s = s->next)
    {
      ClutterStage *stage = s->data;
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
 * Returns an XVisualInfo suitable for creating a foreign window for the given
 * stage. NOTE: It doesn't do as the name may suggest, which is return the
 * XVisualInfo that was used to create an existing window for the given stage.
 *
 * XXX: It might be best to deprecate this function and replace with something
 * along the lines of clutter_backend_x11_get_foreign_visual () or perhaps
 * clutter_stage_x11_get_foreign_visual ()
 *
 * Return value: An XVisualInfo suitable for creating a foreign stage. Use
 *   XFree() to free the returned value instead
 *
 * Deprecated: 1.2: Use clutter_x11_get_visual_info() instead
 *
 * Since: 0.4
 */
XVisualInfo *
clutter_x11_get_stage_visual (ClutterStage *stage)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendX11 *backend_x11;

  g_return_val_if_fail (CLUTTER_IS_BACKEND_X11 (backend), NULL);
  backend_x11 = CLUTTER_BACKEND_X11 (backend);

  return clutter_backend_x11_get_visual_info (backend_x11);
}

typedef struct {
  ClutterStageX11 *stage_x11;
  ClutterGeometry geom;
  Window xwindow;
  guint destroy_old_xwindow : 1;
} ForeignWindowData;

static void
set_foreign_window_callback (ClutterActor *actor,
                             void         *data)
{
  ForeignWindowData *fwd = data;
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);

  CLUTTER_NOTE (BACKEND, "Setting foreign window (0x%x)",
                (unsigned int) fwd->xwindow);

  if (fwd->destroy_old_xwindow && fwd->stage_x11->xwin != None)
    {
      CLUTTER_NOTE (BACKEND, "Destroying previous window (0x%x)",
                    (unsigned int) fwd->xwindow);
      XDestroyWindow (backend_x11->xdpy, fwd->stage_x11->xwin);
    }

  fwd->stage_x11->xwin = fwd->xwindow;
  fwd->stage_x11->is_foreign_xwin = TRUE;

  fwd->stage_x11->xwin_width = fwd->geom.width;
  fwd->stage_x11->xwin_height = fwd->geom.height;

  clutter_actor_set_geometry (actor, &fwd->geom);

  /* calling this with the stage unrealized will unset the stage
   * from the GL context; once the stage is realized the GL context
   * will be set again
   */
  clutter_stage_ensure_current (CLUTTER_STAGE (actor));
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
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendX11 *backend_x11;
  ClutterStageX11 *stage_x11;
  ClutterStageWindow *impl;
  ClutterActor *actor;
  gint x, y;
  guint width, height, border, depth;
  Window root_return;
  Status status;
  ForeignWindowData fwd;
  XVisualInfo *xvisinfo;

  g_return_val_if_fail (CLUTTER_IS_BACKEND_X11 (backend), FALSE);
  backend_x11 = CLUTTER_BACKEND_X11 (backend);

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);
  g_return_val_if_fail (xwindow != None, FALSE);

  actor = CLUTTER_ACTOR (stage);

  impl = _clutter_stage_get_window (stage);
  stage_x11 = CLUTTER_STAGE_X11 (impl);

  xvisinfo = clutter_backend_x11_get_visual_info (backend_x11);

  clutter_x11_trap_x_errors ();

  status = XGetGeometry (backend_x11->xdpy, xwindow,
                         &root_return,
                         &x, &y,
                         &width, &height,
                         &border,
                         &depth);

  if (clutter_x11_untrap_x_errors () ||
      !status ||
      width == 0 || height == 0 ||
      depth != xvisinfo->depth)
    {
      g_warning ("Unable to retrieve the new window geometry");
      return FALSE;
    }

  fwd.stage_x11 = stage_x11;
  fwd.xwindow = xwindow;

  /* destroy the old Window, if we have one and it's ours */
  if (stage_x11->xwin != None && !stage_x11->is_foreign_xwin)
    fwd.destroy_old_xwindow = TRUE;
  else
    fwd.destroy_old_xwindow = FALSE;

  fwd.geom.x = x;
  fwd.geom.y = y;
  fwd.geom.width = width;
  fwd.geom.height = height;

  _clutter_actor_rerealize (actor,
                            set_foreign_window_callback,
                            &fwd);

  clutter_stage_ensure_viewport (stage);

  return TRUE;
}
