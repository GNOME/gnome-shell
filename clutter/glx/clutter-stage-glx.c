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

#include <X11/extensions/Xfixes.h>

#include <GL/glx.h>
#include <GL/gl.h>

#include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>

G_DEFINE_TYPE (ClutterStageGlx, clutter_stage_glx, CLUTTER_TYPE_STAGE);

static inline void
frustum (GLfloat left,
	 GLfloat right,
	 GLfloat bottom,
	 GLfloat top,
	 GLfloat nearval,
	 GLfloat farval)
{
  GLfloat x, y, a, b, c, d;
  GLfloat m[16];

  x = (2.0 * nearval) / (right - left);
  y = (2.0 * nearval) / (top - bottom);
  a = (right + left) / (right - left);
  b = (top + bottom) / (top - bottom);
  c = -(farval + nearval) / ( farval - nearval);
  d = -(2.0 * farval * nearval) / (farval - nearval);

#define M(row,col)  m[col*4+row]
  M(0,0) = x;     M(0,1) = 0.0F;  M(0,2) = a;      M(0,3) = 0.0F;
  M(1,0) = 0.0F;  M(1,1) = y;     M(1,2) = b;      M(1,3) = 0.0F;
  M(2,0) = 0.0F;  M(2,1) = 0.0F;  M(2,2) = c;      M(2,3) = d;
  M(3,0) = 0.0F;  M(3,1) = 0.0F;  M(3,2) = -1.0F;  M(3,3) = 0.0F;
#undef M

  glMultMatrixf (m);
}

static inline void
perspective (GLfloat fovy,
	     GLfloat aspect,
	     GLfloat zNear,
	     GLfloat zFar)
{
  GLfloat xmin, xmax, ymin, ymax;

  ymax = zNear * tan (fovy * M_PI / 360.0);
  ymin = -ymax;
  xmin = ymin * aspect;
  xmax = ymax * aspect;

  frustum (xmin, xmax, ymin, ymax, zNear, zFar);
}

static void
sync_viewport (ClutterStageGlx *stage_glx)
{
  glViewport (0, 0, stage_glx->xwin_width, stage_glx->xwin_height);
  
  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  
  perspective (60.0f, 1.0f, 0.1f, 100.0f);
  
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();

  /* Then for 2D like transform */

  /* camera distance from screen, 0.5 * tan (FOV) */
#define DEFAULT_Z_CAMERA 0.866025404f

  glTranslatef (-0.5f, -0.5f, -DEFAULT_Z_CAMERA);
  glScalef ( 1.0f / stage_glx->xwin_width, 
	    -1.0f / stage_glx->xwin_height, 
	     1.0f / stage_glx->xwin_width);
  glTranslatef (0.0f, -1.0 * stage_glx->xwin_height, 0.0f);
}

static void
clutter_stage_glx_show (ClutterActor *actor)
{
  ClutterStageGlx *stage_glx = CLUTTER_STAGE_GLX (actor);

  if (stage_glx->xwin)
    XMapWindow (stage_glx->xdpy, stage_glx->xwin);
}

static void
clutter_stage_glx_hide (ClutterActor *actor)
{
  ClutterStageGlx *stage_glx = CLUTTER_STAGE_GLX (actor);

  if (stage_glx->xwin)
    XUnmapWindow (stage_glx->xdpy, stage_glx->xwin);
}

static void
clutter_stage_glx_unrealize (ClutterActor *actor)
{
  ClutterStageGlx *stage_glx = CLUTTER_STAGE_GLX (actor);
  gboolean was_offscreen;

  CLUTTER_MARK();

  g_object_get (actor, "offscreen", &was_offscreen, NULL);

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
}

static void
clutter_stage_glx_realize (ClutterActor *actor)
{
  ClutterStageGlx *stage_glx = CLUTTER_STAGE_GLX (actor);
  gboolean is_offscreen;

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
        CLUTTER_NOTE (MISC, "XCreateSimpleWindow");
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

      if (stage_glx->gl_context)
	glXDestroyContext (stage_glx->xdpy, stage_glx->gl_context);

      CLUTTER_NOTE (GL, "glXCreateContext");
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
	GLX_RGBA, 
	GLX_RED_SIZE, 1,
	GLX_GREEN_SIZE, 1,
	GLX_BLUE_SIZE, 1,
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
	  
          CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);

	  return;
	}

      if (stage_glx->gl_context)
	glXDestroyContext (stage_glx->xdpy, stage_glx->gl_context);

      stage_glx->xpixmap = XCreatePixmap (stage_glx->xdpy,
				          stage_glx->xwin_root,
				          stage_glx->xwin_width, 
				          stage_glx->xwin_height,
				          stage_glx->xvisinfo->depth);

      stage_glx->glxpixmap = glXCreateGLXPixmap (stage_glx->xdpy,
					         stage_glx->xvisinfo,
					         stage_glx->xpixmap);
      
      /* indirect */
      stage_glx->gl_context = glXCreateContext (stage_glx->xdpy, 
					        stage_glx->xvisinfo, 
					        0, 
					        False);
      
      glXMakeCurrent (stage_glx->xdpy,
                      stage_glx->glxpixmap,
                      stage_glx->gl_context);

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

  CLUTTER_NOTE (GL,
                "\n"
		"===========================================\n"
		"GL_VENDOR: %s\n"
		"GL_RENDERER: %s\n"
		"GL_VERSION: %s\n"
		"GL_EXTENSIONS: %s\n"
		"Direct Rendering: %s\n"
		"===========================================\n",
		glGetString (GL_VENDOR),
		glGetString (GL_RENDERER),
		glGetString (GL_VERSION),
		glGetString (GL_EXTENSIONS),
		glXIsDirect (stage_glx->xdpy, stage_glx->gl_context) ? "yes"
                                                                     : "no");

  sync_viewport (stage_glx);
}

static void
clutter_stage_glx_paint (ClutterActor *self)
{
  ClutterStageGlx *stage_glx = CLUTTER_STAGE_GLX (self);
  ClutterStage *stage = CLUTTER_STAGE (self);
  ClutterColor stage_color;
  static GTimer *timer = NULL; 
  static guint timer_n_frames = 0;
  static ClutterActorClass *parent_class = NULL;

  CLUTTER_NOTE (PAINT, " Redraw enter");

  if (!parent_class)
    parent_class = g_type_class_peek_parent (CLUTTER_STAGE_GET_CLASS (stage));

  if (clutter_get_show_fps ())
    {
      if (!timer)
	timer = g_timer_new ();
    }

  clutter_stage_get_color (stage, &stage_color);

  glClearColor (((float) stage_color.red / 0xff * 1.0),
	        ((float) stage_color.green / 0xff * 1.0),
	        ((float) stage_color.blue / 0xff * 1.0),
	        0.0);
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  glDisable (GL_LIGHTING); 
  glDisable (GL_DEPTH_TEST);

  parent_class->paint (self);

  if (stage_glx->xwin)
    {
      clutter_feature_wait_for_vblank ();
      glXSwapBuffers (stage_glx->xdpy, stage_glx->xwin);
    }
  else
    {
      glXWaitGL ();
      CLUTTER_GLERR ();
    }

  if (clutter_get_show_fps ())
    {
      timer_n_frames++;

      if (g_timer_elapsed (timer, NULL) >= 1.0)
	{
	  g_print ("*** FPS: %i ***\n", timer_n_frames);
	  timer_n_frames = 0;
	  g_timer_start (timer);
	}
    }

  CLUTTER_NOTE (PAINT, " Redraw leave");
}

static void
clutter_stage_glx_allocate_coords (ClutterActor    *self,
                                   ClutterActorBox *box)
{
  /* Do nothing, just stop group_allocate getting called */

  /* TODO: sync up with any configure events from WM ??  */
  return;
}

static void
clutter_stage_glx_request_coords (ClutterActor    *self,
				  ClutterActorBox *box)
{
  ClutterStageGlx *stage_glx = CLUTTER_STAGE_GLX (self);
  gint new_width, new_height;

  /* FIXME: some how have X configure_notfiys call this ? */
  new_width  = ABS (box->x2 - box->x1);
  new_height = ABS (box->y2 - box->y1); 

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

      sync_viewport (stage_glx);
    }

  if (stage_glx->xwin != None) /* Do we want to bother ? */
    XMoveWindow (stage_glx->xdpy,
		 stage_glx->xwin,
		 box->x1,
		 box->y1);
}

static void
clutter_stage_glx_set_fullscreen (ClutterStage *stage,
                                  gboolean      fullscreen)
{
  ClutterStageGlx *stage_glx = CLUTTER_STAGE_GLX (stage);
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

  sync_viewport (stage_glx);
}

static void
clutter_stage_glx_set_cursor_visible (ClutterStage *stage,
                                      gboolean      show_cursor)
{
  ClutterStageGlx *stage_glx = CLUTTER_STAGE_GLX (stage);

  if (stage_glx->xwin == None)
    return;

  CLUTTER_NOTE (MISC, "setting cursor state (%s) over stage window (%u)",
                show_cursor ? "visible" : "invisible",
                (unsigned int) stage_glx->xwin);

  if (show_cursor)
    {
#ifdef HAVE_XFIXES
      XFixesShowCursor (stage_glx->xdpy, stage_glx->xwin);
#else
      XUndefineCursor (stage_glx->xdpy, stage_glx->xwin);
#endif
    }
  else
    {
#ifdef HAVE_XFIXES
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
#endif
    }

  sync_viewport (stage_glx);
}

static void
clutter_stage_glx_set_offscreen (ClutterStage *stage,
                                 gboolean      offscreen)
{

}

static ClutterActor *
clutter_stage_glx_get_actor_at_pos (ClutterStage *stage,
                                    gint          x,
                                    gint          y)
{
  ClutterActor *found = NULL;
  GLuint buff[64] = { 0 };
  GLint hits;
  GLint view[4];
 
  glSelectBuffer (sizeof (buff), buff);
  glGetIntegerv (GL_VIEWPORT, view);
  glRenderMode (GL_SELECT);

  glInitNames ();

  glPushName (0);
 
  glMatrixMode (GL_PROJECTION);
  glPushMatrix ();
  glLoadIdentity ();

  /* This is gluPickMatrix(x, y, 1.0, 1.0, view); */
  glTranslatef ((view[2] - 2 * (x - view[0])),
	        (view[3] - 2 * (y - view[1])), 0);
  glScalef (view[2], -view[3], 1.0);

  perspective (60.0f, 1.0f, 0.1f, 100.0f); 

  glMatrixMode (GL_MODELVIEW);

  clutter_actor_paint (CLUTTER_ACTOR (stage));

  glMatrixMode (GL_PROJECTION);
  glPopMatrix ();

  hits = glRenderMode(GL_RENDER);

  if (hits != 0)
    {
#if 0
      gint i
      for (i = 0; i < hits; i++)
	g_print ("Hit at %i\n", buff[i * 4 + 3]);
#endif
  
      found = clutter_group_find_child_by_id (CLUTTER_GROUP (stage), 
					      buff[(hits-1) * 4 + 3]);
    }

  clutter_stage_flush (stage);

  return found;
}

static void
snapshot_pixbuf_free (guchar   *pixels,
		      gpointer  data)
{
  g_free (pixels);
}

static void
clutter_stage_glx_draw_to_pixbuf (ClutterStage *stage,
                                  GdkPixbuf    *dest,
                                  gint          x,
                                  gint          y,
                                  gint          width,
                                  gint          height)
{
  guchar *data;
  GdkPixbuf *pixb;
  ClutterActor *actor;
  ClutterStageGlx *stage_glx;
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

      dest = gdk_pixbuf_xlib_get_from_drawable (NULL,
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
      data = g_malloc0 (sizeof (guchar) * width * height * 4);

      glReadPixels (x, 
		    clutter_actor_get_height (actor) - y - height,
		    width, 
		    height, GL_RGBA, GL_UNSIGNED_BYTE, data);
      
      pixb = gdk_pixbuf_new_from_data (data,
				       GDK_COLORSPACE_RGB, 
				       TRUE, 
				       8, 
				       width, height,
				       width * 4,
				       snapshot_pixbuf_free,
				       NULL);
      
      dest = gdk_pixbuf_flip (pixb, TRUE); 

      g_object_unref (pixb);
   }
}

static void
clutter_stage_glx_flush (ClutterStage *stage)
{
  sync_viewport (CLUTTER_STAGE_GLX (stage));
}

static void
clutter_stage_glx_dispose (GObject *gobject)
{
  ClutterStageGlx *stage_glx = CLUTTER_STAGE_GLX (gobject);

  if (stage_glx->xwin)
    clutter_actor_unrealize (CLUTTER_ACTOR (stage_glx));

  G_OBJECT_CLASS (clutter_stage_glx_parent_class)->dispose (gobject);
}

static void
clutter_stage_glx_class_init (ClutterStageGlxClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  ClutterStageClass *stage_class = CLUTTER_STAGE_CLASS (klass);

  gobject_class->dispose = clutter_stage_glx_dispose;
  
  actor_class->show = clutter_stage_glx_show;
  actor_class->hide = clutter_stage_glx_hide;
  actor_class->realize = clutter_stage_glx_realize;
  actor_class->unrealize = clutter_stage_glx_unrealize;
  actor_class->paint = clutter_stage_glx_paint;
  actor_class->request_coords = clutter_stage_glx_request_coords;
  actor_class->allocate_coords = clutter_stage_glx_allocate_coords;
  
  stage_class->set_fullscreen = clutter_stage_glx_set_fullscreen;
  stage_class->set_cursor_visible = clutter_stage_glx_set_cursor_visible;
  stage_class->set_offscreen = clutter_stage_glx_set_offscreen;
  stage_class->get_actor_at_pos = clutter_stage_glx_get_actor_at_pos;
  stage_class->draw_to_pixbuf = clutter_stage_glx_draw_to_pixbuf;
  stage_class->flush = clutter_stage_glx_flush;
}

static void
clutter_stage_glx_init (ClutterStageGlx *stage)
{
  stage->xdpy = NULL;
  stage->xwin_root = None;
  stage->xscreen = 0;

  stage->xwin = None;
  stage->xwin_width = 640;
  stage->xwin_height = 480;
  stage->xvisinfo = None;

  stage->is_foreign_xwin = FALSE;
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

void
clutter_glx_set_stage_foreign (ClutterStage *stage,
                               Window        window)
{
  g_return_if_fail (CLUTTER_IS_STAGE_GLX (stage));

  /* FIXME */
}
