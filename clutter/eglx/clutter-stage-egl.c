#include "config.h"

#include "clutter-stage-egl.h"
#include "clutter-egl.h"

#include "../clutter-main.h"
#include "../clutter-feature.h"
#include "../clutter-color.h"
#include "../clutter-util.h"
#include "../clutter-event.h"
#include "../clutter-enum-types.h"
#include "../clutter-private.h"
#include "../clutter-debug.h"
#include "../clutter-units.h"

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

G_DEFINE_TYPE (ClutterStageEGL, clutter_stage_egl, CLUTTER_TYPE_STAGE);

/* This is currently an EGL on X implementation (eg for use with vincent) 
 *
 *
 */

static void
clutter_stage_egl_show (ClutterActor *actor)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (actor);

  if (stage_egl->xwin)
    XMapWindow (stage_egl->xdpy, stage_egl->xwin);
}

static void
clutter_stage_egl_hide (ClutterActor *actor)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (actor);

  if (stage_egl->xwin)
    XUnmapWindow (stage_egl->xdpy, stage_egl->xwin);
}

static void
clutter_stage_egl_unrealize (ClutterActor *actor)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (actor);
  gboolean was_offscreen;

  CLUTTER_MARK();

  g_object_get (actor, "offscreen", &was_offscreen, NULL);

  if (G_UNLIKELY (was_offscreen))
    {
      /* No support as yet for this */
    }
  else
    {
      if (stage_egl->xwin != None)
	{
	  XDestroyWindow (stage_egl->xdpy, stage_egl->xwin);
	  stage_egl->xwin = None;
	}
      else
	stage_egl->xwin = None;
    }

  if (stage_egl->egl_surface)
    eglDestroySurface (clutter_eglx_display(), stage_egl->egl_surface);
  stage_egl->egl_surface = NULL;

  if (stage_egl->egl_context)
    eglDestroyContext (clutter_eglx_display(), stage_egl->egl_context);
  stage_egl->egl_context = NULL;

  eglMakeCurrent (clutter_eglx_display(), 
		  EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

  stage_egl->egl_context = None;
}

static void
clutter_stage_egl_realize (ClutterActor *actor)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (actor);

  EGLConfig            configs[2];
  EGLint               config_count;
  EGLBoolean           status;
  ClutterPerspective   perspective;

  gboolean is_offscreen;

  CLUTTER_NOTE (BACKEND, "Realizing main stage");

  g_object_get (actor, "offscreen", &is_offscreen, NULL);

  if (G_LIKELY (!is_offscreen))
    {
      EGLint cfg_attribs[] = { EGL_BUFFER_SIZE,    EGL_DONT_CARE,
			       EGL_RED_SIZE,       5,
			       EGL_GREEN_SIZE,     6,
			       EGL_BLUE_SIZE,      5,
			       EGL_NONE };
      
      status = eglGetConfigs (clutter_eglx_display(), 
			      configs, 
			      2, 
			      &config_count);
      
      if (status != EGL_TRUE)
	g_warning ("eglGetConfigs");		
      
      status = eglChooseConfig (clutter_eglx_display(), 
				cfg_attribs, 
				configs, 
				sizeof configs / sizeof configs[0], 
				&config_count);

      if (status != EGL_TRUE)
	g_warning ("eglChooseConfig");		
      
      if (stage_egl->xwin == None)
	stage_egl->xwin 
	  = XCreateSimpleWindow(clutter_eglx_get_default_xdisplay(),
				clutter_eglx_get_default_root_window(),
				0, 0,
				stage_egl->xwin_width, 
				stage_egl->xwin_height,
				0, 0, 
				WhitePixel(clutter_eglx_get_default_xdisplay(), 
					   clutter_eglx_get_default_screen()));

      XSelectInput(clutter_eglx_get_default_xdisplay(), 
		   stage_egl->xwin, 
		   StructureNotifyMask
		   |ExposureMask
		   /* FIXME: we may want to eplicity enable MotionMask */
		   |PointerMotionMask
		   |KeyPressMask
		   |KeyReleaseMask
		   |ButtonPressMask
		   |ButtonReleaseMask
		   |PropertyChangeMask);

      if (stage_egl->egl_context)
	eglDestroyContext (clutter_eglx_display(), stage_egl->egl_context);

      if (stage_egl->egl_surface)
	eglDestroySurface (clutter_eglx_display(), stage_egl->egl_surface);

      stage_egl->egl_surface 
	= eglCreateWindowSurface (clutter_eglx_display(), 
				  configs[0], 
				  (NativeWindowType)stage_egl->xwin, 
				  NULL);

      if (stage_egl->egl_surface == EGL_NO_SURFACE)
	g_warning ("eglCreateWindowSurface");
      
      stage_egl->egl_context = eglCreateContext (clutter_eglx_display(), 
						 configs[0], 
						 EGL_NO_CONTEXT, 
						 NULL);

      if (stage_egl->egl_context == EGL_NO_CONTEXT)
	g_warning ("eglCreateContext");
      
      status = eglMakeCurrent (clutter_eglx_display(), 
			       stage_egl->egl_surface, 
			       EGL_NO_SURFACE, 
			       stage_egl->egl_context);

      if (status != EGL_TRUE)
	g_warning ("eglMakeCurrent");		

      
    }
  else
    {
      /* FIXME */
    }

  clutter_stage_get_perspectivex (CLUTTER_STAGE (actor), &perspective);
  cogl_setup_viewport (clutter_actor_get_width (actor),
		       clutter_actor_get_height (actor),
		       perspective.fovy,
		       perspective.aspect,
		       perspective.z_near,
		       perspective.z_far);
}

static void
clutter_stage_egl_query_coords (ClutterActor        *self,
                                   ClutterActorBox     *box)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (self);

  box->x1 = box->y1 = 0;
  box->x2 = box->x1 + CLUTTER_UNITS_FROM_INT (stage_egl->xwin_width);
  box->y2 = box->y1 + CLUTTER_UNITS_FROM_INT (stage_egl->xwin_height);
}

static void
clutter_stage_egl_request_coords (ClutterActor        *self,
				  ClutterActorBox     *box)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (self);
  gint new_width, new_height;

  /* FIXME: some how have X configure_notfiys call this ? */
  new_width  = ABS (CLUTTER_UNITS_TO_INT (box->x2 - box->x1));
  new_height = ABS (CLUTTER_UNITS_TO_INT (box->y2 - box->y1)); 

  if (new_width != stage_egl->xwin_width ||
      new_height != stage_egl->xwin_height)
    {
      stage_egl->xwin_width  = new_width;
      stage_egl->xwin_height = new_height;

      if (stage_egl->xwin != None)
	XResizeWindow (stage_egl->xdpy, 
		       stage_egl->xwin,
		       stage_egl->xwin_width,
		       stage_egl->xwin_height);

      CLUTTER_SET_PRIVATE_FLAGS(self, CLUTTER_ACTOR_SYNC_MATRICES);
    }

  if (stage_egl->xwin != None) /* Do we want to bother ? */
    XMoveWindow (stage_egl->xdpy,
		 stage_egl->xwin,
		 CLUTTER_UNITS_TO_INT (box->x1),
		 CLUTTER_UNITS_TO_INT (box->y1));
}

static void
clutter_stage_egl_set_fullscreen (ClutterStage *stage,
                                  gboolean      fullscreen)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (stage);
  Atom atom_WM_STATE, atom_WM_STATE_FULLSCREEN;

  atom_WM_STATE = XInternAtom (stage_egl->xdpy, "_NET_WM_STATE", False);
  atom_WM_STATE_FULLSCREEN = XInternAtom (stage_egl->xdpy,
                                          "_NET_WM_STATE_FULLSCREEN",
                                          False);

  if (fullscreen)
    {
      gint width, height;

      width = DisplayWidth (stage_egl->xdpy, stage_egl->xscreen);
      height = DisplayHeight (stage_egl->xdpy, stage_egl->xscreen);

      clutter_actor_set_size (CLUTTER_ACTOR (stage_egl), width, height);

      if (stage_egl->xwin != None)
	XChangeProperty (stage_egl->xdpy,
                         stage_egl->xwin,
                         atom_WM_STATE, XA_ATOM, 32,
                         PropModeReplace,
                         (unsigned char *) &atom_WM_STATE_FULLSCREEN, 1);
    }
  else
    {
      if (stage_egl->xwin != None)
        XDeleteProperty (stage_egl->xdpy, stage_egl->xwin, atom_WM_STATE);
    }

  CLUTTER_SET_PRIVATE_FLAGS(stage, CLUTTER_ACTOR_SYNC_MATRICES);
}

static void
clutter_stage_egl_set_cursor_visible (ClutterStage *stage,
                                      gboolean      show_cursor)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (stage);

  if (stage_egl->xwin == None)
    return;

  CLUTTER_NOTE (MISC, "setting cursor state (%s) over stage window (%u)",
                show_cursor ? "visible" : "invisible",
                (unsigned int) stage_egl->xwin);

  if (show_cursor)
    {
#ifdef HAVE_XFIXES
      XFixesShowCursor (stage_egl->xdpy, stage_egl->xwin);
#else
      XUndefineCursor (stage_egl->xdpy, stage_egl->xwin);
#endif /* HAVE_XFIXES */
    }
  else
    {
#ifdef HAVE_XFIXES
      XFixesHideCursor (stage_egl->xdpy, stage_egl->xwin);
#else
      XColor col;
      Pixmap pix;
      Cursor curs;

      pix = XCreatePixmap (stage_egl->xdpy, stage_egl->xwin, 1, 1, 1);
      memset (&col, 0, sizeof (col));
      curs = XCreatePixmapCursor (stage_egl->xdpy, 
                                  pix, pix,
                                  &col, &col,
                                  1, 1);
      XFreePixmap (stage_egl->xdpy, pix);
      XDefineCursor (stage_egl->xdpy, stage_egl->xwin, curs);
#endif /* HAVE_XFIXES */
    }
}

static void
clutter_stage_egl_set_offscreen (ClutterStage *stage,
                                 gboolean      offscreen)
{
  g_warning ("Stage of type `%s' do not support ClutterStage::set_offscreen",
             G_OBJECT_TYPE_NAME (stage));
}

static GdkPixbuf*
clutter_stage_egl_draw_to_pixbuf (ClutterStage *stage,
                                  gint          x,
                                  gint          y,
                                  gint          width,
                                  gint          height)
{
  g_warning ("Stage of type `%s' do not support ClutterStage::draw_to_pixbuf",
             G_OBJECT_TYPE_NAME (stage));
  return NULL;
}

static void
clutter_stage_egl_dispose (GObject *gobject)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (gobject);

  if (stage_egl->xwin)
    clutter_actor_unrealize (CLUTTER_ACTOR (stage_egl));

  G_OBJECT_CLASS (clutter_stage_egl_parent_class)->dispose (gobject);
}

static void
clutter_stage_egl_class_init (ClutterStageEGLClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  ClutterStageClass *stage_class = CLUTTER_STAGE_CLASS (klass);

  gobject_class->dispose = clutter_stage_egl_dispose;
  
  actor_class->show = clutter_stage_egl_show;
  actor_class->hide = clutter_stage_egl_hide;
  actor_class->realize = clutter_stage_egl_realize;
  actor_class->unrealize = clutter_stage_egl_unrealize;
  actor_class->request_coords = clutter_stage_egl_request_coords;
  actor_class->query_coords = clutter_stage_egl_query_coords;
  
  stage_class->set_fullscreen = clutter_stage_egl_set_fullscreen;
  stage_class->set_cursor_visible = clutter_stage_egl_set_cursor_visible;
  stage_class->set_offscreen = clutter_stage_egl_set_offscreen;
  stage_class->draw_to_pixbuf = clutter_stage_egl_draw_to_pixbuf;
}

static void
clutter_stage_egl_init (ClutterStageEGL *stage)
{
  stage->xdpy = NULL;
  stage->xwin_root = None;
  stage->xscreen = 0;

  stage->xwin = None;
  stage->xwin_width = 640;
  stage->xwin_height = 480;
  stage->xvisinfo = None;
}

/**
 * clutter_eglx_get_stage_window:
 * @stage: a #ClutterStage
 *
 * FIXME
 *
 * Return value: FIXME
 *
 * Since: 0.4
 */
Window
clutter_eglx_get_stage_window (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE_EGL (stage), None);

  return CLUTTER_STAGE_EGL (stage)->xwin;
}

/**
 * clutter_eglx_get_stage_visual:
 * @stage: a #ClutterStage
 *
 * FIXME
 *
 * Return value: FIXME
 *
 * Since: 0.4
 */
XVisualInfo *
clutter_eglx_get_stage_visual (ClutterStage *stage)
{
  g_return_val_if_fail (CLUTTER_IS_STAGE_EGL (stage), NULL);

  return CLUTTER_STAGE_EGL (stage)->xvisinfo;
}

/**
 * clutter_eglx_set_stage_foreign:
 * @stage: a #ClutterStage
 * @window: FIXME
 *
 * FIXME
 *
 * Since: 0.4
 */
void
clutter_eglx_set_stage_foreign (ClutterStage *stage,
                               Window        window)
{
  g_return_if_fail (CLUTTER_IS_STAGE_EGL (stage));

  /* FIXME */
}

