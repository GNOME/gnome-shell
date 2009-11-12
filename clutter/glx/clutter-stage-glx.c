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

#include "clutter-backend-glx.h"
#include "clutter-stage-glx.h"
#include "clutter-glx.h"

#include "../clutter-main.h"
#include "../clutter-feature.h"
#include "../clutter-color.h"
#include "../clutter-util.h"
#include "../clutter-event.h"
#include "../clutter-enum-types.h"
#include "../clutter-private.h"
#include "../clutter-debug.h"
#include "../clutter-units.h"
#include "../clutter-shader.h"
#include "../clutter-group.h"
#include "../clutter-container.h"
#include "../clutter-stage.h"
#include "../clutter-stage-window.h"

#include "cogl/cogl.h"

#include <GL/glx.h>
#include <GL/gl.h>

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

static ClutterStageWindowIface *clutter_stage_glx_parent_iface = NULL;

G_DEFINE_TYPE_WITH_CODE (ClutterStageGLX,
                         clutter_stage_glx,
                         CLUTTER_TYPE_STAGE_X11,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init));

static void
clutter_stage_glx_unrealize (ClutterStageWindow *stage_window)
{
  ClutterBackend *backend = clutter_get_default_backend ();
  ClutterBackendX11 *backend_x11 = CLUTTER_BACKEND_X11 (backend);
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage_window);

  /* Note unrealize should free up any backend stage related resources */
  CLUTTER_NOTE (BACKEND, "Unrealizing stage");

  clutter_x11_trap_x_errors ();

  if (stage_glx->glxwin != None)
    {
      glXDestroyWindow (backend_x11->xdpy, stage_glx->glxwin);
      stage_glx->glxwin = None;
    }

  if (!stage_x11->is_foreign_xwin && stage_x11->xwin != None)
    {
      XDestroyWindow (backend_x11->xdpy, stage_x11->xwin);
      stage_x11->xwin = None;
    }
  else
    stage_x11->xwin = None;

  XSync (backend_x11->xdpy, False);

  clutter_x11_untrap_x_errors ();

  CLUTTER_MARK ();
}

static gboolean
clutter_stage_glx_realize (ClutterStageWindow *stage_window)
{
  ClutterStageX11   *stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  ClutterStageGLX   *stage_glx = CLUTTER_STAGE_GLX (stage_window);
  ClutterBackend    *backend;
  ClutterBackendGLX *backend_glx;
  ClutterBackendX11 *backend_x11;
  GError            *error;

  CLUTTER_NOTE (ACTOR, "Realizing stage '%s' [%p]",
                G_OBJECT_TYPE_NAME (stage_window),
                stage_window);

  backend     = clutter_get_default_backend ();
  backend_glx = CLUTTER_BACKEND_GLX (backend);
  backend_x11 = CLUTTER_BACKEND_X11 (backend);

  if (stage_x11->xwin == None)
    {
      XSetWindowAttributes xattr;
      unsigned long mask;
      XVisualInfo *xvisinfo;

      CLUTTER_NOTE (MISC, "Creating stage X window");

      xvisinfo = clutter_backend_x11_get_visual_info (backend_x11);
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

      CLUTTER_NOTE (BACKEND, "Stage [%p], window: 0x%x",
                    stage_window,
                    (unsigned int) stage_x11->xwin);

      XFree (xvisinfo);
    }

  if (stage_glx->glxwin == None)
    {
      int major;
      int minor;
      GLXFBConfig config;

      /* Try and create a GLXWindow to use with extensions dependent on
       * GLX versions >= 1.3 that don't accept regular X Windows as GLX
       * drawables. */
      if (glXQueryVersion (backend_x11->xdpy, &major, &minor) &&
          major == 1 && minor >= 3 &&
          _clutter_backend_glx_get_fbconfig (backend_glx, &config))
        {
          stage_glx->glxwin = glXCreateWindow (backend_x11->xdpy,
                                               config,
                                               stage_x11->xwin,
                                               NULL);
        }
    }

  if (clutter_x11_has_event_retrieval ())
    {
      if (clutter_x11_has_xinput ())
        {
          XSelectInput (backend_x11->xdpy, stage_x11->xwin,
                        StructureNotifyMask |
                        FocusChangeMask |
                        ExposureMask |
                        KeyPressMask | KeyReleaseMask |
                        EnterWindowMask | LeaveWindowMask |
                        PropertyChangeMask);
#ifdef HAVE_XINPUT
          _clutter_x11_select_events (stage_x11->xwin);
#endif
        }
      else
        XSelectInput (backend_x11->xdpy, stage_x11->xwin,
                      StructureNotifyMask |
                      FocusChangeMask |
                      ExposureMask |
                      PointerMotionMask |
                      KeyPressMask | KeyReleaseMask |
                      ButtonPressMask | ButtonReleaseMask |
                      EnterWindowMask | LeaveWindowMask |
                      PropertyChangeMask);

#ifdef GLX_INTEL_swap_event
      if (clutter_feature_available (CLUTTER_FEATURE_SWAP_EVENTS))
        {
          GLXDrawable drawable =
            stage_glx->glxwin ? stage_glx->glxwin : stage_x11->xwin;
          glXSelectEvent (backend_x11->xdpy,
                          drawable,
                          GLX_BUFFER_SWAP_COMPLETE_INTEL_MASK);
        }
#endif /* GLX_INTEL_swap_event */
    }

  /* no user resize.. */
  clutter_stage_x11_fix_window_size (stage_x11,
                                     stage_x11->xwin_width,
                                     stage_x11->xwin_height);
  clutter_stage_x11_set_wm_protocols (stage_x11);

  /* ask for a context; a no-op, if a context already exists */
  error = NULL;
  _clutter_backend_create_context (backend, &error);
  if (error)
    {
      g_critical ("Unable to realize stage: %s", error->message);
      g_error_free (error);
      return FALSE;
    }

  CLUTTER_NOTE (BACKEND, "Successfully realized stage");

  /* chain up to the StageX11 implementation */
  return clutter_stage_glx_parent_iface->realize (stage_window);
}

static int
clutter_stage_glx_get_pending_swaps (ClutterStageWindow *stage_window)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage_window);

  return stage_glx->pending_swaps;
}

static void
clutter_stage_glx_dispose (GObject *gobject)
{
  G_OBJECT_CLASS (clutter_stage_glx_parent_class)->dispose (gobject);
}

static void
clutter_stage_glx_class_init (ClutterStageGLXClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = clutter_stage_glx_dispose;
}

static void
clutter_stage_glx_init (ClutterStageGLX *stage)
{
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  clutter_stage_glx_parent_iface = g_type_interface_peek_parent (iface);

  iface->realize = clutter_stage_glx_realize;
  iface->unrealize = clutter_stage_glx_unrealize;
  iface->get_pending_swaps = clutter_stage_glx_get_pending_swaps;

  /* the rest is inherited from ClutterStageX11 */
}

