#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-stage-egl.h"
#include "clutter-eglx.h"

#include "../clutter-main.h"
#include "../clutter-feature.h"
#include "../clutter-color.h"
#include "../clutter-util.h"
#include "../clutter-event.h"
#include "../clutter-enum-types.h"
#include "../clutter-private.h"
#include "../clutter-debug.h"
#include "../clutter-units.h"

G_DEFINE_TYPE (ClutterStageEGL, clutter_stage_egl, CLUTTER_TYPE_STAGE_X11);

static void
clutter_stage_egl_unrealize (ClutterActor *actor)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (actor);
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (actor);
  gboolean was_offscreen;

  CLUTTER_MARK();

  g_object_get (actor, "offscreen", &was_offscreen, NULL);

  if (G_UNLIKELY (was_offscreen))
    {
      /* No support as yet for this */
    }
  else
    {
      if (stage_x11->xwin != None)
	{
	  XDestroyWindow (stage_x11->xdpy, stage_x11->xwin);
	  stage_x11->xwin = None;
	}
      else
	stage_x11->xwin = None;
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
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (actor);

  EGLConfig            configs[2];
  EGLint               config_count;
  EGLBoolean           status;

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

      if (stage_x11->xwin == None)
	stage_x11->xwin
	  = XCreateSimpleWindow(stage_x11->xdpy,
				stage_x11->xwin_root,
				0, 0,
				stage_x11->xwin_width,
				stage_x11->xwin_height,
				0, 0,
				WhitePixel (stage_x11->xdpy,
					    stage_x11->xscreen));

      XSelectInput(stage_x11->xdpy,
		   stage_x11->xwin,
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
				  (NativeWindowType)stage_x11->xwin,
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
			       stage_egl->egl_surface,
			       stage_egl->egl_context);

      if (status != EGL_TRUE)
	g_warning ("eglMakeCurrent");
    }
  else
    {
      /* FIXME */
    }

  CLUTTER_SET_PRIVATE_FLAGS(actor, CLUTTER_ACTOR_SYNC_MATRICES);
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
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (gobject);

  if (stage_x11->xwin)
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

  actor_class->realize = clutter_stage_egl_realize;
  actor_class->unrealize = clutter_stage_egl_unrealize;
  stage_class->draw_to_pixbuf = clutter_stage_egl_draw_to_pixbuf;
}

static void
clutter_stage_egl_init (ClutterStageEGL *stage)
{
  ;
}


