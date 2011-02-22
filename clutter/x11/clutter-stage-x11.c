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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#include "config.h"

#include <math.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "clutter-backend-x11.h"
#include "clutter-stage-x11.h"
#include "clutter-x11.h"

#include "clutter-actor-private.h"
#include "clutter-debug.h"
#include "clutter-device-manager-private.h"
#include "clutter-enum-types.h"
#include "clutter-event-translator.h"
#include "clutter-event-private.h"
#include "clutter-feature.h"
#include "clutter-main.h"
#include "clutter-paint-volume-private.h"
#include "clutter-private.h"
#include "clutter-stage-private.h"

#include "cogl/cogl.h"

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#define STAGE_X11_IS_MAPPED(s)  ((((ClutterStageX11 *) (s))->wm_state & STAGE_X11_WITHDRAWN) == 0)

static void clutter_stage_window_iface_init     (ClutterStageWindowIface     *iface);
static void clutter_event_translator_iface_init (ClutterEventTranslatorIface *iface);

static GHashTable *clutter_stages_by_xid = NULL;

#define clutter_stage_x11_get_type      _clutter_stage_x11_get_type

G_DEFINE_TYPE_WITH_CODE (ClutterStageX11,
                         clutter_stage_x11,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init)
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_EVENT_TRANSLATOR,
                                                clutter_event_translator_iface_init));

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

  CLUTTER_NOTE (BACKEND, "%s NET_WM state", add ? "adding" : "removing");

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
              DefaultRootWindow (backend_x11->xdpy),
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              (XEvent *)&xclient);
}

static void
update_state (ClutterStageX11   *stage_x11,
              ClutterBackendX11 *backend_x11,
              Atom              *state,
              gboolean           add)
{
  if (add)
    {
      /* FIXME: This wont work if we support more states */
      XChangeProperty (backend_x11->xdpy,
                       stage_x11->xwin,
                       backend_x11->atom_NET_WM_STATE, XA_ATOM, 32,
                       PropModeReplace,
                       (unsigned char *) state, 1);
    }
  else
    {
       /* FIXME: This wont work if we support more states */
       XDeleteProperty (backend_x11->xdpy,
                        stage_x11->xwin,
                        backend_x11->atom_NET_WM_STATE);
    }
}

static void
clutter_stage_x11_fix_window_size (ClutterStageX11 *stage_x11,
                                   gint             new_width,
                                   gint             new_height)
{
  ClutterBackendX11 *backend_x11 = stage_x11->backend;

  if (stage_x11->xwin != None && !stage_x11->is_foreign_xwin)
    {
      guint min_width, min_height;
      XSizeHints *size_hints;
      gboolean resize;

      resize = clutter_stage_get_user_resizable (stage_x11->wrapper);

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
      if (!stage_x11->fullscreening)
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

static void
clutter_stage_x11_set_wm_protocols (ClutterStageX11 *stage_x11)
{
  ClutterBackendX11 *backend_x11 = stage_x11->backend;
  Atom protocols[2];
  int n = 0;
  
  protocols[n++] = backend_x11->atom_WM_DELETE_WINDOW;
  protocols[n++] = backend_x11->atom_NET_WM_PING;

  XSetWMProtocols (backend_x11->xdpy, stage_x11->xwin, protocols, n);
}

static void
clutter_stage_x11_get_geometry (ClutterStageWindow *stage_window,
                                ClutterGeometry    *geometry)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  ClutterBackendX11 *backend_x11 = stage_x11->backend;

  /* If we're fullscreen, return the size of the display. */
  if ((stage_x11->state & CLUTTER_STAGE_STATE_FULLSCREEN) &&
      stage_x11->fullscreening)
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
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  ClutterBackendX11 *backend_x11 = stage_x11->backend;

  if (stage_x11->is_foreign_xwin)
    {
      /* If this is a foreign window we won't get a ConfigureNotify,
       * so we need to manually set the size and queue a relayout on the
       * stage here (as is normally done in response to ConfigureNotify).
       */
      stage_x11->xwin_width = width;
      stage_x11->xwin_height = height;
      clutter_actor_queue_relayout (CLUTTER_ACTOR (stage_x11->wrapper));
      return;
    }

  /* If we're going fullscreen, don't mess with the size */
  if (stage_x11->fullscreening)
    return;

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

  if (stage_x11->xwin != None)
    {
      clutter_stage_x11_fix_window_size (stage_x11, width, height);

      if (width != stage_x11->xwin_width ||
          height != stage_x11->xwin_height)
        {
          CLUTTER_NOTE (BACKEND, "%s: XResizeWindow[%x] (%d, %d)",
                        G_STRLOC,
                        (unsigned int) stage_x11->xwin,
                        width,
                        height);

          CLUTTER_SET_PRIVATE_FLAGS (stage_x11->wrapper,
                                     CLUTTER_IN_RESIZE);

          /* XXX: in this case we can rely on a subsequent
           * ConfigureNotify that will result in the stage
           * being reallocated so we don't actively do anything
           * to affect the stage allocation here. */
          XResizeWindow (backend_x11->xdpy,
                         stage_x11->xwin,
                         width,
                         height);
        }
    }
}

static inline void
set_wm_pid (ClutterStageX11 *stage_x11)
{
  ClutterBackendX11 *backend_x11 = stage_x11->backend;
  long pid;

  if (stage_x11->xwin == None || stage_x11->is_foreign_xwin)
    return;

  /* this will take care of WM_CLIENT_MACHINE and WM_LOCALE_NAME */
  XSetWMProperties (backend_x11->xdpy, stage_x11->xwin,
                    NULL,
                    NULL,
                    NULL, 0,
                    NULL, NULL, NULL);

  pid = getpid ();
  XChangeProperty (backend_x11->xdpy,
                   stage_x11->xwin,
                   backend_x11->atom_NET_WM_PID, XA_CARDINAL, 32,
                   PropModeReplace,
                   (guchar *) &pid, 1);
}

static inline void
set_wm_title (ClutterStageX11 *stage_x11)
{
  ClutterBackendX11 *backend_x11 = stage_x11->backend;

  if (stage_x11->xwin == None || stage_x11->is_foreign_xwin)
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
  ClutterBackendX11 *backend_x11 = stage_x11->backend;

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
  ClutterBackendX11 *backend_x11 = stage_x11->backend;
  ClutterDeviceManager *device_manager;
  int event_flags;

  set_wm_pid (stage_x11);
  set_wm_title (stage_x11);
  set_cursor_visible (stage_x11);


  /* the masks for the events we want to select on a stage window;
   * KeyPressMask and KeyReleaseMask are necessary even with XI1
   * because key events are broken with that extension, and will
   * be fixed by XI2
   */
  event_flags = StructureNotifyMask
              | FocusChangeMask
              | ExposureMask
              | PropertyChangeMask
              | EnterWindowMask
              | LeaveWindowMask
              | KeyPressMask
              | KeyReleaseMask
              | ButtonPressMask
              | ButtonReleaseMask
              | PointerMotionMask;

  /* we unconditionally select input events even with event retrieval
   * disabled because we need to guarantee that the Clutter internal
   * state is maintained when calling clutter_x11_handle_event() without
   * requiring applications or embedding toolkits to select events
   * themselves. if we did that, we'd have to document the events to be
   * selected, and also update applications and embedding toolkits each
   * time we added a new mask, or a new class of events.
   *
   * see: http://bugzilla.clutter-project.org/show_bug.cgi?id=998
   * for the rationale of why we did conditional selection. it is now
   * clear that a compositor should clear out the input region, since
   * it cannot assume a perfectly clean slate coming from us.
   *
   * see: http://bugzilla.clutter-project.org/show_bug.cgi?id=2228
   * for an example of things that break if we do conditional event
   * selection.
   */
  XSelectInput (backend_x11->xdpy, stage_x11->xwin, event_flags);

  /* input events also depent on the actual device, so we need to
   * use the device manager to let every device select them, using
   * the event mask we passed to XSelectInput as the template
   */
  device_manager = clutter_device_manager_get_default ();
  _clutter_device_manager_select_stage_events (device_manager,
                                               stage_x11->wrapper,
                                               event_flags);

  /* no user resize.. */
  clutter_stage_x11_fix_window_size (stage_x11,
                                     stage_x11->xwin_width,
                                     stage_x11->xwin_height);
  clutter_stage_x11_set_wm_protocols (stage_x11);

  CLUTTER_NOTE (BACKEND, "Successfully realized stage");

  return TRUE;
}

static void
clutter_stage_x11_set_fullscreen (ClutterStageWindow *stage_window,
                                  gboolean            is_fullscreen)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  ClutterBackendX11 *backend_x11 = stage_x11->backend;
  ClutterStage *stage = stage_x11->wrapper;
  gboolean was_fullscreen;

  if (stage == NULL || CLUTTER_ACTOR_IN_DESTRUCTION (stage))
    return;

  was_fullscreen = ((stage_x11->state & CLUTTER_STAGE_STATE_FULLSCREEN) != 0);
  is_fullscreen = !!is_fullscreen;

  if (was_fullscreen == is_fullscreen)
    return;

  CLUTTER_NOTE (BACKEND, "%ssetting fullscreen", is_fullscreen ? "" : "un");

  if (is_fullscreen)
    {
#if 0
      int width, height;

      /* FIXME: this will do the wrong thing for dual-headed
         displays. This will return the size of the combined display
         but Metacity (at least) will fullscreen to only one of the
         displays. This will cause the actor to report the wrong size
         until the ConfigureNotify for the correct size is received */
      width  = DisplayWidth (backend_x11->xdpy, backend_x11->xscreen_num);
      height = DisplayHeight (backend_x11->xdpy, backend_x11->xscreen_num);
#endif

      /* Set the fullscreen hint so we can retain the old size of the window. */
      stage_x11->fullscreening = TRUE;

      if (stage_x11->xwin != None)
        {
          /* if the actor is not mapped we resize the stage window to match
           * the size of the screen; this is useful for e.g. EGLX to avoid
           * a resize when calling clutter_stage_fullscreen() before showing
           * the stage
           */
          if (!STAGE_X11_IS_MAPPED (stage_x11))
            {
              CLUTTER_NOTE (BACKEND, "Fullscreening unmapped stage");

              update_state (stage_x11, backend_x11,
                            &backend_x11->atom_NET_WM_STATE_FULLSCREEN,
                            TRUE);
            }
          else
            {
              CLUTTER_NOTE (BACKEND, "Fullscreening mapped stage");

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
      stage_x11->fullscreening = FALSE;

      if (stage_x11->xwin != None)
        {
          if (!STAGE_X11_IS_MAPPED (stage_x11))
            {
              CLUTTER_NOTE (BACKEND, "Un-fullscreening unmapped stage");

              update_state (stage_x11, backend_x11,
                            &backend_x11->atom_NET_WM_STATE_FULLSCREEN,
                            FALSE);
            }
          else
            {
              CLUTTER_NOTE (BACKEND, "Un-fullscreening mapped stage");

              send_wmspec_change_state (backend_x11,
                                        stage_x11->xwin,
                                        backend_x11->atom_NET_WM_STATE_FULLSCREEN,
                                        FALSE);

              /* Fix the window size to restore the minimum/maximum
                 restriction */
              clutter_stage_x11_fix_window_size (stage_x11,
                                                 stage_x11->xwin_width,
                                                 stage_x11->xwin_height);
            }
        }
    }

  /* XXX: Note we rely on the ConfigureNotify mechanism as the common
   * mechanism to handle notifications of new X window sizes from the
   * X server so we don't actively change the stage viewport here or
   * queue a relayout etc. */
}

static void
clutter_stage_x11_set_cursor_visible (ClutterStageWindow *stage_window,
                                      gboolean            cursor_visible)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);

  stage_x11->is_cursor_visible = !!cursor_visible;
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

  clutter_stage_x11_fix_window_size (stage_x11,
                                     stage_x11->xwin_width,
                                     stage_x11->xwin_height);
}

static inline void
update_wm_hints (ClutterStageX11 *stage_x11)
{
  ClutterBackendX11 *backend_x11 = stage_x11->backend;
  XWMHints wm_hints;

  if (stage_x11->wm_state & STAGE_X11_WITHDRAWN)
    return;

  if (stage_x11->is_foreign_xwin)
    return;

  wm_hints.flags = StateHint | InputHint;
  wm_hints.initial_state = NormalState;
  wm_hints.input = stage_x11->accept_focus ? True : False;

  XSetWMHints (backend_x11->xdpy, stage_x11->xwin, &wm_hints);
}

static void
clutter_stage_x11_set_accept_focus (ClutterStageWindow *stage_window,
                                    gboolean            accept_focus)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);

  stage_x11->accept_focus = !!accept_focus;
  update_wm_hints (stage_x11);
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
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  ClutterBackendX11 *backend_x11 = stage_x11->backend;

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

          set_stage_state (stage_x11, STAGE_X11_WITHDRAWN, 0);

          update_wm_hints (stage_x11);

          if (stage_x11->fullscreening)
            clutter_stage_x11_set_fullscreen (stage_window, TRUE);
          else
            clutter_stage_x11_set_fullscreen (stage_window, FALSE);
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
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  ClutterBackendX11 *backend_x11 = stage_x11->backend;

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
  ClutterEventTranslator *translator = CLUTTER_EVENT_TRANSLATOR (gobject);
  ClutterBackendX11 *backend = CLUTTER_STAGE_X11 (gobject)->backend;

  _clutter_backend_remove_event_translator (CLUTTER_BACKEND (backend),
                                            translator);

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
  stage->fullscreening = FALSE;
  stage->is_cursor_visible = TRUE;
  stage->accept_focus = TRUE;

  stage->title = NULL;

  stage->wrapper = NULL;
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  iface->get_wrapper = clutter_stage_x11_get_wrapper;
  iface->set_title = clutter_stage_x11_set_title;
  iface->set_fullscreen = clutter_stage_x11_set_fullscreen;
  iface->set_cursor_visible = clutter_stage_x11_set_cursor_visible;
  iface->set_user_resizable = clutter_stage_x11_set_user_resizable;
  iface->set_accept_focus = clutter_stage_x11_set_accept_focus;
  iface->show = clutter_stage_x11_show;
  iface->hide = clutter_stage_x11_hide;
  iface->resize = clutter_stage_x11_resize;
  iface->get_geometry = clutter_stage_x11_get_geometry;
  iface->realize = clutter_stage_x11_realize;
}

static inline void
set_user_time (ClutterBackendX11 *backend_x11,
               ClutterStageX11   *stage_x11,
               long               timestamp)
{
  if (timestamp != CLUTTER_CURRENT_TIME)
    {
      XChangeProperty (backend_x11->xdpy,
                       stage_x11->xwin,
                       backend_x11->atom_NET_WM_USER_TIME,
                       XA_CARDINAL, 32,
                       PropModeReplace,
                       (unsigned char *) &timestamp, 1);
    }
}

static gboolean
handle_wm_protocols_event (ClutterBackendX11 *backend_x11,
                           ClutterStageX11   *stage_x11,
                           XEvent            *xevent)
{
  Atom atom = (Atom) xevent->xclient.data.l[0];

  if (atom == backend_x11->atom_WM_DELETE_WINDOW &&
      xevent->xany.window == stage_x11->xwin)
    {
      /* the WM_DELETE_WINDOW is a request: we do not destroy
       * the window right away, as it might contain vital data;
       * we relay the event to the application and we let it
       * handle the request
       */
      CLUTTER_NOTE (EVENT, "Delete stage %s[%p], win:0x%x",
                    _clutter_actor_get_debug_name (CLUTTER_ACTOR (stage_x11->wrapper)),
                    stage_x11->wrapper,
                    (unsigned int) stage_x11->xwin);

      set_user_time (backend_x11, stage_x11, xevent->xclient.data.l[1]);

      return TRUE;
    }
  else if (atom == backend_x11->atom_NET_WM_PING &&
           xevent->xany.window == stage_x11->xwin)
    {
      XClientMessageEvent xclient = xevent->xclient;

      xclient.window = backend_x11->xwin_root;
      XSendEvent (backend_x11->xdpy, xclient.window,
                  False,
                  SubstructureRedirectMask | SubstructureNotifyMask,
                  (XEvent *) &xclient);
      return FALSE;
    }

  /* do not send any of the WM_PROTOCOLS events to the queue */
  return FALSE;
}

static gboolean
clipped_redraws_cool_off_cb (void *data)
{
  ClutterStageX11 *stage_x11 = data;

  stage_x11->clipped_redraws_cool_off = 0;

  return FALSE;
}

static ClutterTranslateReturn
clutter_stage_x11_translate_event (ClutterEventTranslator *translator,
                                   gpointer                native,
                                   ClutterEvent           *event)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (translator);
  ClutterTranslateReturn res = CLUTTER_TRANSLATE_CONTINUE;
  ClutterBackendX11 *backend_x11 = stage_x11->backend;
  Window stage_xwindow = stage_x11->xwin;
  XEvent *xevent = native;
  ClutterStage *stage;

  stage = clutter_x11_get_stage_from_window (xevent->xany.window);
  if (stage == NULL)
    return CLUTTER_TRANSLATE_CONTINUE;

  switch (xevent->type)
    {
    case ConfigureNotify:
      if (!stage_x11->is_foreign_xwin)
        {
          gboolean size_changed = FALSE;

          CLUTTER_NOTE (BACKEND, "ConfigureNotify[%x] (%d, %d)",
                        (unsigned int) stage_x11->xwin,
                        xevent->xconfigure.width,
                        xevent->xconfigure.height);

          /* When fullscreen, we'll keep the xwin_width/height
             variables to track the old size of the window and we'll
             assume all ConfigureNotifies constitute a size change */
          if ((stage_x11->state & CLUTTER_STAGE_STATE_FULLSCREEN))
            size_changed = TRUE;
          else if ((stage_x11->xwin_width != xevent->xconfigure.width) ||
                   (stage_x11->xwin_height != xevent->xconfigure.height))
            {
              size_changed = TRUE;
              stage_x11->xwin_width = xevent->xconfigure.width;
              stage_x11->xwin_height = xevent->xconfigure.height;
            }

          clutter_actor_set_size (CLUTTER_ACTOR (stage),
                                  xevent->xconfigure.width,
                                  xevent->xconfigure.height);

          CLUTTER_UNSET_PRIVATE_FLAGS (stage_x11->wrapper, CLUTTER_IN_RESIZE);

          if (size_changed)
            {
              /* XXX: This is a workaround for a race condition when
               * resizing windows while there are in-flight
               * glXCopySubBuffer blits happening.
               *
               * The problem stems from the fact that rectangles for the
               * blits are described relative to the bottom left of the
               * window and because we can't guarantee control over the X
               * window gravity used when resizing so the gravity is
               * typically NorthWest not SouthWest.
               *
               * This means if you grow a window vertically the server
               * will make sure to place the old contents of the window
               * at the top-left/north-west of your new larger window, but
               * that may happen asynchronous to GLX preparing to do a
               * blit specified relative to the bottom-left/south-west of
               * the window (based on the old smaller window geometry).
               *
               * When the GLX issued blit finally happens relative to the
               * new bottom of your window, the destination will have
               * shifted relative to the top-left where all the pixels you
               * care about are so it will result in a nasty artefact
               * making resizing look very ugly!
               *
               * We can't currently fix this completely, in-part because
               * the window manager tends to trample any gravity we might
               * set.  This workaround instead simply disables blits for a
               * while if we are notified of any resizes happening so if
               * the user is resizing a window via the window manager then
               * they may see an artefact for one frame but then we will
               * fallback to redrawing the full stage until the cooling
               * off period is over.
               */
              if (stage_x11->clipped_redraws_cool_off)
                g_source_remove (stage_x11->clipped_redraws_cool_off);

              stage_x11->clipped_redraws_cool_off =
                g_timeout_add_seconds (1, clipped_redraws_cool_off_cb,
                                       stage_x11);

              /* Queue a relayout - we want glViewport to be called
               * with the correct values, and this is done in ClutterStage
               * via cogl_onscreen_clutter_backend_set_size ().
               *
               * We queue a relayout, because if this ConfigureNotify is
               * in response to a size we set in the application, the
               * set_size() call above is essentially a null-op.
               *
               * Make sure we do this only when the size has changed,
               * otherwise we end up relayouting on window moves.
               */
              clutter_actor_queue_relayout (CLUTTER_ACTOR (stage));

              /* the resize process is complete, so we can ask the stage
               * to set up the GL viewport with the new size
               */
              clutter_stage_ensure_viewport (stage);
            }
        }
      break;

    case PropertyNotify:
      if (xevent->xproperty.atom == backend_x11->atom_NET_WM_STATE &&
          xevent->xproperty.window == stage_xwindow &&
          !stage_x11->is_foreign_xwin)
        {
          Atom     type;
          gint     format;
          gulong   n_items, bytes_after;
          guchar  *data = NULL;
          gboolean fullscreen_set = FALSE;

          clutter_x11_trap_x_errors ();
          XGetWindowProperty (backend_x11->xdpy, stage_xwindow,
                              backend_x11->atom_NET_WM_STATE,
                              0, G_MAXLONG,
                              False, XA_ATOM,
                              &type, &format, &n_items,
                              &bytes_after, &data);
          clutter_x11_untrap_x_errors ();

          if (type != None && data != NULL)
            {
              Atom *atoms = (Atom *) data;
              gulong i;
              gboolean is_fullscreen = FALSE;

              for (i = 0; i < n_items; i++)
                {
                  if (atoms[i] == backend_x11->atom_NET_WM_STATE_FULLSCREEN)
                    fullscreen_set = TRUE;
                }

              is_fullscreen =
                (stage_x11->state & CLUTTER_STAGE_STATE_FULLSCREEN);

              if (fullscreen_set != is_fullscreen)
                {
                  if (fullscreen_set)
                    stage_x11->state |= CLUTTER_STAGE_STATE_FULLSCREEN;
                  else
                    stage_x11->state &= ~CLUTTER_STAGE_STATE_FULLSCREEN;

                  stage_x11->fullscreening = fullscreen_set;

                  event->any.type = CLUTTER_STAGE_STATE;
                  event->any.source = CLUTTER_ACTOR (stage);
                  event->any.stage = stage;
                  event->stage_state.changed_mask =
                    CLUTTER_STAGE_STATE_FULLSCREEN;
                  event->stage_state.new_state = stage_x11->state;

                  res = CLUTTER_TRANSLATE_QUEUE;
                }

              XFree (data);
            }
        }
      break;

    case FocusIn:
      if (!(stage_x11->state & CLUTTER_STAGE_STATE_ACTIVATED))
        {
          /* TODO: check the detail? */
          stage_x11->state |= CLUTTER_STAGE_STATE_ACTIVATED;

          event->type = CLUTTER_STAGE_STATE;
          event->any.source = CLUTTER_ACTOR (stage);
          event->any.stage = stage;
          event->stage_state.changed_mask = CLUTTER_STAGE_STATE_ACTIVATED;
          event->stage_state.new_state = stage_x11->state;

          res = CLUTTER_TRANSLATE_QUEUE;
        }
      break;

    case FocusOut:
      if (stage_x11->state & CLUTTER_STAGE_STATE_ACTIVATED)
        {
          /* TODO: check the detail? */
          stage_x11->state &= ~CLUTTER_STAGE_STATE_ACTIVATED;

          event->any.type = CLUTTER_STAGE_STATE;
          event->any.source = CLUTTER_ACTOR (stage);
          event->any.stage = stage;
          event->stage_state.changed_mask = CLUTTER_STAGE_STATE_ACTIVATED;
          event->stage_state.new_state = stage_x11->state;

          res = CLUTTER_TRANSLATE_QUEUE;
        }
      break;

    case Expose:
      {
        XExposeEvent *expose = (XExposeEvent *) xevent;
        ClutterPaintVolume clip;
        ClutterVertex origin;

        CLUTTER_NOTE (EVENT,
                      "expose for stage: %s[%p], win:0x%x - "
                      "redrawing area (x: %d, y: %d, width: %d, height: %d)",
                      _clutter_actor_get_debug_name (CLUTTER_ACTOR (stage)),
                      stage,
                      (unsigned int) stage_xwindow,
                      expose->x,
                      expose->y,
                      expose->width,
                      expose->height);

        origin.x = expose->x;
        origin.y = expose->y;
        origin.z = 0;

        _clutter_paint_volume_init_static (&clip, CLUTTER_ACTOR (stage));

        clutter_paint_volume_set_origin (&clip, &origin);
        clutter_paint_volume_set_width (&clip, expose->width);
        clutter_paint_volume_set_height (&clip, expose->height);

        _clutter_actor_queue_redraw_with_clip (CLUTTER_ACTOR (stage), 0, &clip);

        clutter_paint_volume_free (&clip);
      }
      break;

    case DestroyNotify:
      CLUTTER_NOTE (EVENT,
                    "Destroy notification received for stage %s[%p], win:0x%x",
                    _clutter_actor_get_debug_name (CLUTTER_ACTOR (stage)),
                    stage,
                    (unsigned int) stage_xwindow);
      event->any.type = CLUTTER_DESTROY_NOTIFY;
      event->any.stage = stage;
      res = CLUTTER_TRANSLATE_QUEUE;
      break;

    case ClientMessage:
      CLUTTER_NOTE (EVENT, "Client message for stage %s[%p], win:0x%x",
                    _clutter_actor_get_debug_name (CLUTTER_ACTOR (stage)),
                    stage,
                    (unsigned int) stage_xwindow);
      if (handle_wm_protocols_event (backend_x11, stage_x11, xevent))
        {
          event->any.type = CLUTTER_DELETE;
          event->any.stage = stage;
          res = CLUTTER_TRANSLATE_QUEUE;
        }
      break;

    case MappingNotify:
      CLUTTER_NOTE (EVENT, "Refresh keyboard mapping");
      XRefreshKeyboardMapping (&xevent->xmapping);
      backend_x11->keymap_serial += 1;
      res = CLUTTER_TRANSLATE_REMOVE;
      break;

    default:
      res = CLUTTER_TRANSLATE_CONTINUE;
      break;
    }

  return res;
}

static void
clutter_event_translator_iface_init (ClutterEventTranslatorIface *iface)
{
  iface->translate_event = clutter_stage_x11_translate_event;
}

/**
 * clutter_x11_get_stage_window: (skip)
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
 * Return value: (transfer none): A #ClutterStage, or% NULL if a stage
 *   does not exist for the window
 *
 * Since: 0.8
 */
ClutterStage *
clutter_x11_get_stage_from_window (Window win)
{
  ClutterStageX11 *stage_x11;

  if (clutter_stages_by_xid == NULL)
    return NULL;

  stage_x11 = g_hash_table_lookup (clutter_stages_by_xid,
                                   GINT_TO_POINTER (win));

  if (stage_x11 != NULL)
    return stage_x11->wrapper;

  return NULL;
}

/**
 * clutter_x11_get_stage_visual: (skip)
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
 * Return value: (transfer full): An XVisualInfo suitable for creating a
 *   foreign stage. Use XFree() to free the returned value instead
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

  return _clutter_backend_x11_get_visual_info (backend_x11);
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
  ClutterBackendX11 *backend_x11 = fwd->stage_x11->backend;

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

  if (clutter_stages_by_xid == NULL)
    clutter_stages_by_xid = g_hash_table_new (NULL, NULL);

  g_hash_table_insert (clutter_stages_by_xid,
                       GINT_TO_POINTER (fwd->stage_x11->xwin),
                       fwd->stage_x11);

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

  g_return_val_if_fail (CLUTTER_IS_STAGE (stage), FALSE);
  g_return_val_if_fail (!CLUTTER_ACTOR_IN_DESTRUCTION (stage), FALSE);
  g_return_val_if_fail (xwindow != None, FALSE);

  impl = _clutter_stage_get_window (stage);
  stage_x11 = CLUTTER_STAGE_X11 (impl);
  backend_x11 = stage_x11->backend;

  xvisinfo = _clutter_backend_x11_get_visual_info (backend_x11);
  g_return_val_if_fail (xvisinfo != NULL, FALSE);

  clutter_x11_trap_x_errors ();

  status = XGetGeometry (backend_x11->xdpy, xwindow,
                         &root_return,
                         &x, &y,
                         &width, &height,
                         &border,
                         &depth);

  if (clutter_x11_untrap_x_errors () || !status)
    {
      g_critical ("Unable to retrieve the geometry of the foreign window: "
                  "XGetGeometry() failed (status code: %d)", status);
      return FALSE;
    }

  if (width == 0 || height == 0)
    {
      g_warning ("The size of the foreign window is 0x0");
      return FALSE;
    }

  if (depth != xvisinfo->depth)
    {
      g_warning ("The depth of the visual of the foreign window is %d, but "
                 "Clutter has been initialized to require a visual depth "
                 "of %d",
                 depth,
                 xvisinfo->depth);
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

  actor = CLUTTER_ACTOR (stage);

  _clutter_actor_rerealize (actor,
                            set_foreign_window_callback,
                            &fwd);

  /* Queue a relayout - so the stage will be allocated the new
   * window size.
   *
   * Note also that when the stage gets allocated the new
   * window size that will result in the stage's
   * priv->viewport being changed, which will in turn result
   * in the Cogl viewport changing when _clutter_do_redraw
   * calls _clutter_stage_maybe_setup_viewport().
   */
  clutter_actor_queue_relayout (actor);

  return TRUE;
}

void
_clutter_stage_x11_destroy_window_untrapped (ClutterStageX11 *stage_x11)
{
  Window xwin = stage_x11->xwin;

  if (clutter_stages_by_xid != NULL)
    {
      CLUTTER_NOTE (BACKEND, "Removing X11 stage 0x%x [%p]",
                    (unsigned int) xwin,
                    stage_x11);

      g_hash_table_remove (clutter_stages_by_xid, GINT_TO_POINTER (xwin));
    }

  if (!stage_x11->is_foreign_xwin && xwin != None)
    {
      ClutterBackendX11 *backend_x11 = stage_x11->backend;

      g_assert (clutter_stages_by_xid != NULL);

      XDestroyWindow (backend_x11->xdpy, xwin);
      stage_x11->xwin = None;
    }
  else
    stage_x11->xwin = None;
}

void
_clutter_stage_x11_destroy_window (ClutterStageX11 *stage_x11)
{
  if (stage_x11->xwin == None)
    return;

  clutter_x11_trap_x_errors ();

  _clutter_stage_x11_destroy_window_untrapped (stage_x11);

  clutter_x11_untrap_x_errors ();
}

gboolean
_clutter_stage_x11_create_window (ClutterStageX11 *stage_x11)
{
  ClutterBackendX11 *backend_x11 = stage_x11->backend;
  XSetWindowAttributes xattr;
  XVisualInfo *xvisinfo;
  unsigned long mask;
  gfloat width, height;

  if (stage_x11->xwin != None)
    return TRUE;

  CLUTTER_NOTE (MISC, "Creating stage X window");

  xvisinfo = _clutter_backend_x11_get_visual_info (backend_x11);
  if (xvisinfo == NULL)
    {
      g_critical ("Unable to find suitable GL visual.");
      return FALSE;
    }

  /* window attributes */
  xattr.background_pixel = WhitePixel (backend_x11->xdpy,
                                       backend_x11->xscreen_num);
  xattr.border_pixel = 0;
  xattr.colormap = XCreateColormap (backend_x11->xdpy,
                                    backend_x11->xwin_root,
                                    xvisinfo->visual,
                                    AllocNone);
  mask = CWBorderPixel | CWColormap;

  /* Call get_size - this will either get the geometry size (which
   * before we create the window is set to 640x480), or if a size
   * is set, it will get that. This lets you set a size on the
   * stage before it's realized.
   *
   * we also round to the nearest integer because stage sizes
   * should always be in pixels
   */
  clutter_actor_get_size (CLUTTER_ACTOR (stage_x11->wrapper), &width, &height);
  stage_x11->xwin_width = floorf (width + 0.5);
  stage_x11->xwin_height = floorf (height + 0.5);

  stage_x11->xwin = XCreateWindow (backend_x11->xdpy,
                                   backend_x11->xwin_root,
                                   0, 0,
                                   stage_x11->xwin_width,
                                   stage_x11->xwin_height,
                                   0,
                                   xvisinfo->depth,
                                   InputOutput,
                                   xvisinfo->visual,
                                   mask, &xattr);

  CLUTTER_NOTE (BACKEND, "Stage [%p], window: 0x%x, size: %dx%d",
                stage_x11,
                (unsigned int) stage_x11->xwin,
                stage_x11->xwin_width,
                stage_x11->xwin_height);

  XFree (xvisinfo);

  if (clutter_stages_by_xid == NULL)
    clutter_stages_by_xid = g_hash_table_new (NULL, NULL);

  g_hash_table_insert (clutter_stages_by_xid,
                       GINT_TO_POINTER (stage_x11->xwin),
                       stage_x11);

  return TRUE;
}

void
_clutter_stage_x11_set_user_time (ClutterStageX11 *stage_x11,
                                  guint32          user_time)
{
  set_user_time (stage_x11->backend, stage_x11, user_time);
}

gboolean
_clutter_stage_x11_get_root_coords (ClutterStageX11 *stage_x11,
                                    gint            *root_x,
                                    gint            *root_y)
{
  ClutterBackendX11 *backend_x11 = stage_x11->backend;
  gint return_val;
  Window child;
  gint tx, ty;

  return_val = XTranslateCoordinates (backend_x11->xdpy,
                                      stage_x11->xwin,
                                      backend_x11->xwin_root,
                                      0, 0, &tx, &ty,
                                      &child);

  if (root_x)
    *root_x = tx;

  if (root_y)
    *root_y = ty;

  return (return_val == 0);
}
