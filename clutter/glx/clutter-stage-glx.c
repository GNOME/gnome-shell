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

#include "cogl.h"

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#include <GL/glx.h>
#include <GL/gl.h>

#include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>

G_DEFINE_TYPE (ClutterStageGLX, clutter_stage_glx, CLUTTER_TYPE_STAGE);

static void
clutter_stage_glx_show (ClutterActor *actor)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (actor);

  if (stage_glx->xwin)
    {
      /* Fire off a redraw to avoid flicker on first map.
       * Appears not to work perfectly on intel drivers at least.
      */
      clutter_redraw();
      XSync (stage_glx->xdpy, FALSE);
      XMapWindow (stage_glx->xdpy, stage_glx->xwin);
    }
}

static void
clutter_stage_glx_hide (ClutterActor *actor)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (actor);

  if (stage_glx->xwin)
    XUnmapWindow (stage_glx->xdpy, stage_glx->xwin);
}

static void
clutter_stage_glx_unrealize (ClutterActor *actor)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (actor);
  gboolean was_offscreen;

  CLUTTER_MARK();

  g_object_get (actor, "offscreen", &was_offscreen, NULL);

  clutter_glx_trap_x_errors ();

  if (G_UNLIKELY (was_offscreen))
    {
      if (stage_glx->glxpixmap)
	{
	  glXDestroyGLXPixmap (stage_glx->xdpy, stage_glx->glxpixmap);
	  stage_glx->glxpixmap = None;
	}

      if (stage_glx->xpixmap)
	{
	  XFreePixmap (stage_glx->xdpy, stage_glx->xpixmap);
	  stage_glx->xpixmap = None;
	}
    }
  else
    {
      if (!stage_glx->is_foreign_xwin && stage_glx->xwin != None)
	{
	  XDestroyWindow (stage_glx->xdpy, stage_glx->xwin);
	  stage_glx->xwin = None;
	}
      else
	stage_glx->xwin = None;
    }

  glXMakeCurrent (stage_glx->xdpy, None, NULL);
  if (stage_glx->gl_context != None)
    {
      glXDestroyContext (stage_glx->xdpy, stage_glx->gl_context);
      stage_glx->gl_context = None;
    }

  XSync (stage_glx->xdpy, False);

  clutter_glx_untrap_x_errors ();

  CLUTTER_MARK ();
}

static void
set_wm_protocols (Display *xdisplay,
                  Window   xwindow)
{
  Atom protocols[2];
  int n = 0;
  
  protocols[n++] = XInternAtom (xdisplay, "WM_DELETE_WINDOW", False);
  protocols[n++] = XInternAtom (xdisplay, "_NET_WM_PING", False);

  XSetWMProtocols (xdisplay, xwindow, protocols, n);
}

static void
clutter_stage_glx_realize (ClutterActor *actor)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (actor);
  gboolean is_offscreen;
  ClutterPerspective perspective;

  CLUTTER_NOTE (MISC, "Realizing main stage");

  g_object_get (actor, "offscreen", &is_offscreen, NULL);

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

      if (stage_glx->xvisinfo)
	XFree (stage_glx->xvisinfo);

      if (stage_glx->xvisinfo == None)
	stage_glx->xvisinfo = glXChooseVisual (stage_glx->xdpy,
                                               stage_glx->xscreen,
					       gl_attributes);
      if (!stage_glx->xvisinfo)
	{
	  g_critical ("Unable to find suitable GL visual.");
	  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
	  return;
	}

      if (stage_glx->xwin == None)
        {
        CLUTTER_NOTE (MISC, "Creating stage X window");
	stage_glx->xwin = XCreateSimpleWindow (stage_glx->xdpy,
                                               stage_glx->xwin_root,
                                               0, 0,
                                               stage_glx->xwin_width,
                                               stage_glx->xwin_height,
                                               0, 0, 
                                               WhitePixel (stage_glx->xdpy,
                                                           stage_glx->xscreen));
        }
      
      CLUTTER_NOTE (MISC, "XSelectInput");
      XSelectInput (stage_glx->xdpy, stage_glx->xwin,
                    StructureNotifyMask |
                    ExposureMask |
		   /* FIXME: we may want to eplicity enable MotionMask */
		    PointerMotionMask |
		    KeyPressMask | KeyReleaseMask |
		    ButtonPressMask | ButtonReleaseMask |
		    PropertyChangeMask);

      set_wm_protocols (stage_glx->xdpy, stage_glx->xwin);

      if (stage_glx->gl_context)
	glXDestroyContext (stage_glx->xdpy, stage_glx->gl_context);

      CLUTTER_NOTE (GL, "Creating GL Context");
      stage_glx->gl_context = glXCreateContext (stage_glx->xdpy, 
					        stage_glx->xvisinfo, 
					        0, 
					        True);
      
      if (stage_glx->gl_context == None)
	{
	  g_critical ("Unable to create suitable GL context.");

	  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
	  
          return;
	}

      CLUTTER_NOTE (GL, "glXMakeCurrent");
      glXMakeCurrent (stage_glx->xdpy, stage_glx->xwin, stage_glx->gl_context);
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

      if (stage_glx->xvisinfo)
	XFree (stage_glx->xvisinfo);

      CLUTTER_NOTE (GL, "glXChooseVisual");
      stage_glx->xvisinfo = glXChooseVisual (stage_glx->xdpy,
					     stage_glx->xscreen,
					     gl_attributes);
      if (!stage_glx->xvisinfo)
	{
	  g_critical ("Unable to find suitable GL visual.");
	  goto fail;
	}

      if (stage_glx->gl_context)
	glXDestroyContext (stage_glx->xdpy, stage_glx->gl_context);

      stage_glx->xpixmap = XCreatePixmap (stage_glx->xdpy,
				          stage_glx->xwin_root,
				          stage_glx->xwin_width, 
				          stage_glx->xwin_height,
					  DefaultDepth (stage_glx->xdpy,
							stage_glx->xscreen));

      stage_glx->glxpixmap = glXCreateGLXPixmap (stage_glx->xdpy,
					         stage_glx->xvisinfo,
					         stage_glx->xpixmap);
      
      /* indirect */
      stage_glx->gl_context = glXCreateContext (stage_glx->xdpy, 
					        stage_glx->xvisinfo, 
					        0, 
					        False);

      clutter_glx_trap_x_errors ();

      glXMakeCurrent (stage_glx->xdpy,
                      stage_glx->glxpixmap,
                      stage_glx->gl_context);

      if (clutter_glx_untrap_x_errors ())
	{
	  g_critical ("Unable to set up offscreen context.");
	  goto fail;
	}

#if 0
      /* Debug code for monitoring a off screen pixmap via window */
      {
	Colormap cmap;
	XSetWindowAttributes swa;

	cmap = XCreateColormap(clutter_glx_display(),
			       clutter_glx_root_window(), 
			       backend->xvisinfo->visual, AllocNone);

	/* create a window */
	swa.colormap = cmap; 

	foo_win = XCreateWindow(clutter_glx_display(),
				clutter_glx_root_window(), 
				0, 0, 
				backend->xwin_width, backend->xwin_height,
				0, 
				backend->xvisinfo->depth, 
				InputOutput, 
				backend->xvisinfo->visual,
				CWColormap, &swa);

	XMapWindow(clutter_glx_display(), foo_win);
      }
#endif

    }

  clutter_stage_get_perspectivex (CLUTTER_STAGE (actor), &perspective);
  cogl_setup_viewport (clutter_actor_get_width (actor),
		       clutter_actor_get_height (actor),
		       perspective.fovy,
		       perspective.aspect,
		       perspective.z_near,
		       perspective.z_far);
  
  return;
  
 fail:

  /* For one reason or another we cant realize the stage.. */
  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
  return;
}

static void
clutter_stage_glx_query_coords (ClutterActor        *self,
				ClutterActorBox     *box)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (self);

  box->x1 = box->y1 = 0;
  box->x2 = box->x1 + CLUTTER_UNITS_FROM_INT (stage_glx->xwin_width);
  box->y2 = box->y1 + CLUTTER_UNITS_FROM_INT (stage_glx->xwin_height);
}

static void
clutter_stage_glx_request_coords (ClutterActor        *self,
				  ClutterActorBox     *box)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (self);
  gint new_width, new_height;

  new_width  = ABS (CLUTTER_UNITS_TO_INT (box->x2 - box->x1));
  new_height = ABS (CLUTTER_UNITS_TO_INT (box->y2 - box->y1)); 

  if (new_width != stage_glx->xwin_width ||
      new_height != stage_glx->xwin_height)
    {
      stage_glx->xwin_width  = new_width;
      stage_glx->xwin_height = new_height;

      if (stage_glx->xwin != None)
	XResizeWindow (stage_glx->xdpy, 
		       stage_glx->xwin,
		       stage_glx->xwin_width,
		       stage_glx->xwin_height);

      if (stage_glx->xpixmap != None)
	{
	  /* Need to recreate to resize */
	  clutter_actor_unrealize (self);
	  clutter_actor_realize (self);
	}

      CLUTTER_SET_PRIVATE_FLAGS(self, CLUTTER_ACTOR_SYNC_MATRICES);
    }

  if (stage_glx->xwin != None) /* Do we want to bother ? */
    XMoveWindow (stage_glx->xdpy,
		 stage_glx->xwin,
		 CLUTTER_UNITS_TO_INT (box->x1),
		 CLUTTER_UNITS_TO_INT (box->y1));
}

static void
clutter_stage_glx_set_fullscreen (ClutterStage *stage,
                                  gboolean      fullscreen)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage);
  Atom atom_WM_STATE, atom_WM_STATE_FULLSCREEN;

  atom_WM_STATE = XInternAtom (stage_glx->xdpy, "_NET_WM_STATE", False);
  atom_WM_STATE_FULLSCREEN = XInternAtom (stage_glx->xdpy,
                                          "_NET_WM_STATE_FULLSCREEN",
                                          False);

  if (fullscreen)
    {
      gint width, height;

      width = DisplayWidth (stage_glx->xdpy, stage_glx->xscreen);
      height = DisplayHeight (stage_glx->xdpy, stage_glx->xscreen);

      clutter_actor_set_size (CLUTTER_ACTOR (stage_glx), width, height);

      if (stage_glx->xwin != None)
	XChangeProperty (stage_glx->xdpy,
                         stage_glx->xwin,
                         atom_WM_STATE, XA_ATOM, 32,
                         PropModeReplace,
                         (unsigned char *) &atom_WM_STATE_FULLSCREEN, 1);
    }
  else
    {
      if (stage_glx->xwin != None)
        XDeleteProperty (stage_glx->xdpy, stage_glx->xwin, atom_WM_STATE);
    }

  CLUTTER_SET_PRIVATE_FLAGS(stage, CLUTTER_ACTOR_SYNC_MATRICES);
}

static void
clutter_stage_glx_set_cursor_visible (ClutterStage *stage,
                                      gboolean      show_cursor)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage);

  if (stage_glx->xwin == None)
    return;

  CLUTTER_NOTE (BACKEND, "setting cursor state ('%s') over stage window (%u)",
                show_cursor ? "visible" : "invisible",
                (unsigned int) stage_glx->xwin);

  if (show_cursor)
    {
#if 0 /* HAVE_XFIXES - borked on fiesty at least so disabled until further 
       *               investigation.	 
       */
      XFixesShowCursor (stage_glx->xdpy, stage_glx->xwin);
#else
      XUndefineCursor (stage_glx->xdpy, stage_glx->xwin);
#endif /* HAVE_XFIXES */
    }
  else
    {
#if 0 /* HAVE_XFIXES - borked */
      XFixesHideCursor (stage_glx->xdpy, stage_glx->xwin);
#else
      XColor col;
      Pixmap pix;
      Cursor curs;

      pix = XCreatePixmap (stage_glx->xdpy, stage_glx->xwin, 1, 1, 1);
      memset (&col, 0, sizeof (col));
      curs = XCreatePixmapCursor (stage_glx->xdpy, 
                                  pix, pix,
                                  &col, &col,
                                  1, 1);
      XFreePixmap (stage_glx->xdpy, pix);
      XDefineCursor (stage_glx->xdpy, stage_glx->xwin, curs);
#endif /* HAVE_XFIXES */
    }
}

static void
clutter_stage_glx_set_title (ClutterStage *stage,
			     const gchar  *title)
{
  ClutterStageGLX *stage_glx = CLUTTER_STAGE_GLX (stage);
  Atom             atom_NET_WM_NAME, atom_UTF8_STRING;

  if (stage_glx->xwin == None)
    return;

  /* FIXME: pre create these to avoid too many round trips */
  atom_NET_WM_NAME  = XInternAtom (stage_glx->xdpy, "_NET_WM_NAME", False);
  atom_UTF8_STRING  = XInternAtom (stage_glx->xdpy, "UTF8_STRING", False);

  if (title == NULL)
    {
      XDeleteProperty (stage_glx->xdpy, 
		       stage_glx->xwin, 
		       atom_NET_WM_NAME);
    }
  else
    {
      XChangeProperty (stage_glx->xdpy, 
		       stage_glx->xwin, 
		       atom_NET_WM_NAME, 
		       atom_UTF8_STRING, 
		       8, 
		       PropModeReplace, 
		       (unsigned char*)title, 
		       (int)strlen(title));
    }
}

static void
clutter_stage_glx_set_offscreen (ClutterStage *stage,
                                 gboolean      offscreen)
{
  /* Do nothing ? */
}

static void
snapshot_pixbuf_free (guchar   *pixels,
		      gpointer  data)
{
  g_free (pixels);
}

static GdkPixbuf*
clutter_stage_glx_draw_to_pixbuf (ClutterStage *stage,
                                  gint          x,
                                  gint          y,
                                  gint          width,
                                  gint          height)
{
  guchar *data;
  GdkPixbuf *pixb;
  ClutterActor *actor;
  ClutterStageGLX *stage_glx;
  gboolean is_offscreen = FALSE;

  stage_glx = CLUTTER_STAGE_GLX (stage);
  actor = CLUTTER_ACTOR (stage);

  if (width < 0)
    width = clutter_actor_get_width (actor);

  if (height < 0)
    height = clutter_actor_get_height (actor);

  g_object_get (stage, "offscreen", &is_offscreen, NULL);

  if (G_UNLIKELY (is_offscreen))
    {
      gdk_pixbuf_xlib_init (stage_glx->xdpy, stage_glx->xscreen);

      pixb = gdk_pixbuf_xlib_get_from_drawable (NULL,
                                                (Drawable) stage_glx->xpixmap,
                                                DefaultColormap (stage_glx->xdpy,
                                                                 stage_glx->xscreen),
                                                stage_glx->xvisinfo->visual,
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

  if (stage_glx->xwin)
    clutter_actor_unrealize (CLUTTER_ACTOR (stage_glx));

  G_OBJECT_CLASS (clutter_stage_glx_parent_class)->dispose (gobject);
}

static void
clutter_stage_glx_class_init (ClutterStageGLXClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  ClutterStageClass *stage_class = CLUTTER_STAGE_CLASS (klass);

  gobject_class->dispose = clutter_stage_glx_dispose;
  
  actor_class->show = clutter_stage_glx_show;
  actor_class->hide = clutter_stage_glx_hide;
  actor_class->realize = clutter_stage_glx_realize;
  actor_class->unrealize = clutter_stage_glx_unrealize;
  actor_class->request_coords = clutter_stage_glx_request_coords;
  actor_class->query_coords = clutter_stage_glx_query_coords;
  
  stage_class->set_fullscreen = clutter_stage_glx_set_fullscreen;
  stage_class->set_cursor_visible = clutter_stage_glx_set_cursor_visible;
  stage_class->set_offscreen = clutter_stage_glx_set_offscreen;
  stage_class->draw_to_pixbuf = clutter_stage_glx_draw_to_pixbuf;
  stage_class->set_title = clutter_stage_glx_set_title;
}

static void
clutter_stage_glx_init (ClutterStageGLX *stage)
{
  stage->xdpy = NULL;
  stage->xwin_root = None;
  stage->xscreen = 0;

  stage->xwin = None;
  stage->xwin_width = 640;
  stage->xwin_height = 480;
  stage->xvisinfo = None;

  stage->is_foreign_xwin = FALSE;

  CLUTTER_SET_PRIVATE_FLAGS(stage, CLUTTER_ACTOR_SYNC_MATRICES);
}

/**
 * clutter_glx_get_stage_window:
 * @stage: a #ClutterStage
 *
 * FIXME
 *
 * Return value: FIXME
 *
 * Since: 0.4
 */
Window
clutter_glx_get_stage_window (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE_GLX (stage), None);

  return CLUTTER_STAGE_GLX (stage)->xwin;
}

/**
 * clutter_glx_get_stage_visual:
 * @stage: a #ClutterStage
 *
 * FIXME
 *
 * Return value: FIXME
 *
 * Since: 0.4
 */
XVisualInfo *
clutter_glx_get_stage_visual (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE_GLX (stage), NULL);

  return CLUTTER_STAGE_GLX (stage)->xvisinfo;
}

/**
 * clutter_glx_set_stage_foreign:
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
clutter_glx_set_stage_foreign (ClutterStage *stage,
                               Window        xwindow)
{
  ClutterStageGLX *stage_glx;
  ClutterActor *actor;
  gint x, y;
  guint width, height, border, depth;
  Window root_return;
  Status status;
  ClutterGeometry geom;

  g_return_val_if_fail (CLUTTER_IS_STAGE_GLX (stage), FALSE);
  g_return_val_if_fail (xwindow != None, FALSE);

  stage_glx = CLUTTER_STAGE_GLX (stage);
  actor = CLUTTER_ACTOR (stage);

  clutter_glx_trap_x_errors ();

  status = XGetGeometry (stage_glx->xdpy,
                         xwindow,
                         &root_return,
                         &x, &y,
                         &width, &height,
                         &border,
                         &depth);
  
  if (clutter_glx_untrap_x_errors () ||
      !status ||
      width == 0 || height == 0 ||
      depth != stage_glx->xvisinfo->depth)
    {
      return FALSE;
    }

  clutter_actor_unrealize (actor);

  stage_glx->xwin = xwindow;
  stage_glx->is_foreign_xwin = TRUE;

  geom.x = x;
  geom.y = y;
  geom.width = stage_glx->xwin_width = width;
  geom.height = stage_glx->xwin_height = height;

  clutter_actor_set_geometry (actor, &geom);
  clutter_actor_realize (actor);

  return TRUE;
}
