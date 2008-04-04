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

#include "cogl.h"

#include <GL/glx.h>
#include <GL/gl.h>

#include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterStageGLX,
                         clutter_stage_glx,
                         CLUTTER_TYPE_STAGE_X11,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init));

static void
clutter_stage_glx_unrealize (ClutterActor *actor)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (actor);
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (actor);
  gboolean was_offscreen;

  /* Note unrealize should free up any backend stage related resources */
  CLUTTER_MARK();

  g_object_get (stage_x11->wrapper, "offscreen", &was_offscreen, NULL);

  /* Chain up so all children get unrealized, needed to move texture data
   * across contexts
  */
  CLUTTER_ACTOR_CLASS (clutter_stage_glx_parent_class)->unrealize (actor);

  clutter_x11_trap_x_errors ();

  /* Unrealize all shaders, since the GL context is going away */
  clutter_shader_release_all ();

  if (G_UNLIKELY (was_offscreen))
    {
      if (stage_glx->glxpixmap)
        {
          glXDestroyGLXPixmap (stage_x11->xdpy,stage_glx->glxpixmap);
          stage_glx->glxpixmap = None;
        }

      if (stage_x11->xpixmap)
        {
          XFreePixmap (stage_x11->xdpy, stage_x11->xpixmap);
          stage_x11->xpixmap = None;
        }
    }
  else
    {
      if (!stage_x11->is_foreign_xwin && stage_x11->xwin != None)
        {
          XDestroyWindow (stage_x11->xdpy, stage_x11->xwin);
          stage_x11->xwin = None;
        }
      else
        stage_x11->xwin = None;
    }

  /* As unrealised the context will now get cleared */
  clutter_stage_ensure_current (stage_x11->wrapper);

  XSync (stage_x11->xdpy, False);

  clutter_x11_untrap_x_errors ();

  CLUTTER_MARK ();
}

static void
clutter_stage_glx_realize (ClutterActor *actor)
{
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (actor);
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (actor);
  ClutterBackendGLX *backend_glx;
  gboolean         is_offscreen;

  CLUTTER_NOTE (MISC, "Realizing main stage");

  g_object_get (stage_x11->wrapper, "offscreen", &is_offscreen, NULL);

  backend_glx = CLUTTER_BACKEND_GLX (clutter_get_default_backend ());

  if (G_LIKELY (!is_offscreen))
    {
      int gl_attributes[] = 
        {
          GLX_RGBA, 
          GLX_DOUBLEBUFFER,
          GLX_RED_SIZE, 1,
          GLX_GREEN_SIZE, 1,
          GLX_BLUE_SIZE, 1,
          GLX_STENCIL_SIZE, 1,
          0
        };

      if (stage_x11->xvisinfo)
        {
          XFree (stage_x11->xvisinfo);
          stage_x11->xvisinfo = None;
        }

      /* The following check seems strange */
      if (stage_x11->xvisinfo == None)
        stage_x11->xvisinfo = glXChooseVisual (stage_x11->xdpy,
                                               stage_x11->xscreen,
                                               gl_attributes);
      if (!stage_x11->xvisinfo)
        {
          g_critical ("Unable to find suitable GL visual.");

          CLUTTER_ACTOR_UNSET_FLAGS (stage_x11->wrapper, CLUTTER_ACTOR_REALIZED);
          CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
          return;
        }

      if (stage_x11->xwin == None)
        {
          XSetWindowAttributes xattr;
          unsigned long mask;

          CLUTTER_NOTE (MISC, "Creating stage X window");

          /* window attributes */  
          xattr.background_pixel = WhitePixel (stage_x11->xdpy,
                                               stage_x11->xscreen);
          xattr.border_pixel = 0;
          xattr.colormap = XCreateColormap (stage_x11->xdpy, 
                                            stage_x11->xwin_root,
                                            stage_x11->xvisinfo->visual,
                                            AllocNone);
          mask = CWBackPixel | CWBorderPixel | CWColormap;
          stage_x11->xwin = XCreateWindow (stage_x11->xdpy,
                                           stage_x11->xwin_root,
                                           0, 0,
                                           stage_x11->xwin_width,
                                           stage_x11->xwin_height,
                                           0,
                                           stage_x11->xvisinfo->depth,
                                           InputOutput,
                                           stage_x11->xvisinfo->visual,
                                           mask, &xattr);
        }

      CLUTTER_NOTE (MISC, "XSelectInput");
      XSelectInput (stage_x11->xdpy, stage_x11->xwin,
                    StructureNotifyMask |
                    FocusChangeMask |
                    ExposureMask |
                    /* FIXME: we may want to eplicity enable MotionMask */
                    PointerMotionMask |
                    KeyPressMask | KeyReleaseMask |
                    ButtonPressMask | ButtonReleaseMask |
                    PropertyChangeMask);

      /* no user resize.. */
      clutter_stage_x11_fix_window_size (stage_x11);
      clutter_stage_x11_set_wm_protocols (stage_x11);

      if (backend_glx->gl_context == None)
        {
          CLUTTER_NOTE (GL, "Creating GL Context");
          backend_glx->gl_context =  glXCreateContext (stage_x11->xdpy, 
                                                       stage_x11->xvisinfo, 
                                                       0,
                                                       True);

          if (backend_glx->gl_context == None)
            {
              g_critical ("Unable to create suitable GL context.");

              CLUTTER_ACTOR_UNSET_FLAGS (stage_x11->wrapper, CLUTTER_ACTOR_REALIZED);
              CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);

              return;
            }
        }

      CLUTTER_NOTE (GL, "glXMakeCurrent");
      CLUTTER_ACTOR_SET_FLAGS (stage_x11->wrapper, CLUTTER_ACTOR_REALIZED);
      clutter_stage_ensure_current (stage_x11->wrapper);
    }
  else
    {
      int gl_attributes[] = {
        GLX_DEPTH_SIZE,    0,
        GLX_ALPHA_SIZE,    0,
        GLX_RED_SIZE, 1,
        GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1,
        GLX_USE_GL,
        GLX_RGBA,
        0
      };

      if (stage_x11->xvisinfo)
         XFree (stage_x11->xvisinfo);

      stage_x11->xvisinfo = NULL;

      CLUTTER_NOTE (GL, "glXChooseVisual");
      stage_x11->xvisinfo = glXChooseVisual (stage_x11->xdpy,
                                             stage_x11->xscreen,
                                             gl_attributes);
      if (!stage_x11->xvisinfo)
        {
          g_critical ("Unable to find suitable GL visual.");
          goto fail;
        }

     
      stage_x11->xpixmap = XCreatePixmap (stage_x11->xdpy,
                                          stage_x11->xwin_root,
                                          stage_x11->xwin_width, 
                                          stage_x11->xwin_height,
                                          DefaultDepth (stage_x11->xdpy,
                                                        stage_x11->xscreen));

      stage_glx->glxpixmap = glXCreateGLXPixmap (stage_x11->xdpy,
                                                 stage_x11->xvisinfo,
                                                 stage_x11->xpixmap);

      if (backend_glx->gl_context == None)
        {
          CLUTTER_NOTE (GL, "Creating GL Context");

          /* FIXME: we probably need a seperate offscreen context here
           * - though it likely makes most sense to drop offscreen stages
           * and rely on FBO's instead and GLXPixmaps seems mostly broken
           * anyway..
          */
          backend_glx->gl_context =  glXCreateContext (stage_x11->xdpy, 
                                                       stage_x11->xvisinfo, 
                                                       0,
                                                       False);

          if (backend_glx->gl_context == None)
            {
              g_critical ("Unable to create suitable GL context.");

              CLUTTER_ACTOR_UNSET_FLAGS (stage_x11->wrapper, CLUTTER_ACTOR_REALIZED);
              CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);

              return;
            }
        }

      clutter_x11_trap_x_errors ();

      /* below will call glxMakeCurrent */
      clutter_stage_ensure_current (stage_x11->wrapper);

      if (clutter_x11_untrap_x_errors ())
        {
          g_critical ("Unable to set up offscreen context.");
          goto fail;
        }
    }

  /* Make sure the viewport gets set up correctly */
  CLUTTER_SET_PRIVATE_FLAGS (stage_x11->wrapper, CLUTTER_ACTOR_SYNC_MATRICES);
  return;
  
fail:
  /* For one reason or another we cant realize the stage.. */
  CLUTTER_ACTOR_UNSET_FLAGS (stage_x11->wrapper, CLUTTER_ACTOR_REALIZED);
  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
}

static void
snapshot_pixbuf_free (guchar   *pixels,
                      gpointer  data)
{
  g_free (pixels);
}

static GdkPixbuf *
clutter_stage_glx_draw_to_pixbuf (ClutterStageWindow *stage_window,
                                  gint                x,
                                  gint                y,
                                  gint                width,
                                  gint                height)
{
  guchar *data;
  GdkPixbuf *pixb;
  ClutterActor *actor;
  ClutterStageGLX *stage_glx;
  ClutterStageX11 *stage_x11;
  gboolean is_offscreen = FALSE;

  stage_glx = CLUTTER_STAGE_GLX (stage_window);
  stage_x11 = CLUTTER_STAGE_X11 (stage_window);
  actor = CLUTTER_ACTOR (stage_window);

  if (width < 0)
    width = clutter_actor_get_width (actor);

  if (height < 0)
    height = clutter_actor_get_height (actor);

  g_object_get (stage_x11->wrapper, "offscreen", &is_offscreen, NULL);

  if (G_UNLIKELY (is_offscreen))
    {
      gdk_pixbuf_xlib_init (stage_x11->xdpy, stage_x11->xscreen);

      pixb = gdk_pixbuf_xlib_get_from_drawable (NULL,
                                                (Drawable) stage_x11->xpixmap,
                                                DefaultColormap (stage_x11->xdpy,
                                                                 stage_x11->xscreen),
                                                stage_x11->xvisinfo->visual,
                                                x, y,
                                                0, 0,
                                                width, height);
    }
  else
    {
      GdkPixbuf *tmp = NULL, *tmp2 = NULL;
      gint stride;

      stride = ((width * 4 + 3) &~ 3);

      data = g_malloc0 (sizeof (guchar) * stride * height);

      glReadPixels (x, 
                    clutter_actor_get_height (actor) - y - height,
                    width, 
                    height, GL_RGBA, GL_UNSIGNED_BYTE, data);

      tmp = gdk_pixbuf_new_from_data (data,
                                      GDK_COLORSPACE_RGB, 
                                      TRUE, 
                                      8, 
                                      width, height,
                                      stride,
                                      snapshot_pixbuf_free,
                                      NULL);

      tmp2 = gdk_pixbuf_flip (tmp, TRUE); 
      g_object_unref (tmp);

      pixb = gdk_pixbuf_rotate_simple (tmp2, GDK_PIXBUF_ROTATE_UPSIDEDOWN);
      g_object_unref (tmp2);
   }

  return pixb;
}

static void
clutter_stage_glx_dispose (GObject *gobject)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (gobject);
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (gobject);

  if (stage_x11->xwin)
    clutter_actor_unrealize (CLUTTER_ACTOR (stage_glx));

  G_OBJECT_CLASS (clutter_stage_glx_parent_class)->dispose (gobject);
}

static void
clutter_stage_glx_class_init (ClutterStageGLXClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  gobject_class->dispose = clutter_stage_glx_dispose;
  
  actor_class->realize = clutter_stage_glx_realize;
  actor_class->unrealize = clutter_stage_glx_unrealize;
}

static void
clutter_stage_glx_init (ClutterStageGLX *stage)
{
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  iface->draw_to_pixbuf = clutter_stage_glx_draw_to_pixbuf;

  /* the rest is inherited from ClutterStageX11 */
}
