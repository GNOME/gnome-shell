#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

G_DEFINE_TYPE (ClutterStageEGL, clutter_stage_egl, CLUTTER_TYPE_STAGE);

static void
clutter_stage_egl_show (ClutterActor *actor)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (actor);

  /* we are always shown... */

  return;
}

static void
clutter_stage_egl_hide (ClutterActor *actor)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (actor);

  /* we are always shown... */

  return;
}

static void
clutter_stage_egl_unrealize (ClutterActor *actor)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (actor);
  gboolean was_offscreen;

  CLUTTER_MARK();

  g_object_get (actor, "offscreen", &was_offscreen, NULL);

  if (stage_egl->egl_surface)
    eglDestroySurface (clutter_egl_display(), stage_egl->egl_surface);
  stage_egl->egl_surface = NULL;

  if (stage_egl->egl_context)
    eglDestroyContext (clutter_egl_display(), stage_egl->egl_context);
  stage_egl->egl_context = NULL;

  eglMakeCurrent (clutter_egl_display(), 
		  EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

  stage_egl->egl_context = EGL_NO_CONTEXT;
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
			       EGL_DEPTH_SIZE,     16,
			       EGL_ALPHA_SIZE,     EGL_DONT_CARE,
			       EGL_STENCIL_SIZE,   EGL_DONT_CARE,
			       EGL_SURFACE_TYPE,   EGL_WINDOW_BIT,
			       EGL_NONE };
      
      status = eglGetConfigs (clutter_egl_display(), 
			      configs, 
			      2, 
			      &config_count);
      
      if (status != EGL_TRUE)
	g_warning ("eglGetConfigs");		
      
      status = eglChooseConfig (clutter_egl_display(), 
				cfg_attribs, 
				configs, 
				sizeof configs / sizeof configs[0], 
				&config_count);

      if (status != EGL_TRUE)
	g_warning ("eglChooseConfig");		

      if (stage_egl->egl_context)
	eglDestroyContext (clutter_egl_display(), stage_egl->egl_context);

      if (stage_egl->egl_surface)
	eglDestroySurface (clutter_egl_display(), stage_egl->egl_surface);

      stage_egl->egl_surface 
	= eglCreateWindowSurface (clutter_egl_display(), 
				  configs[0], 
				  NULL, 
				  NULL);

      eglQuerySurface (clutter_egl_display(),
		       stage_egl->egl_surface,
		       EGL_WIDTH,
		       &stage_egl->surface_width);

      eglQuerySurface (clutter_egl_display(),
		       stage_egl->egl_surface,
		       EGL_HEIGHT,
		       &stage_egl->surface_height);

      if (stage_egl->egl_surface == EGL_NO_SURFACE)
	g_warning ("eglCreateWindowSurface");
      
      CLUTTER_NOTE (BACKEND, "surface is %ix%i", 
		    stage_egl->surface_width, stage_egl->surface_height);

      stage_egl->egl_context = eglCreateContext (clutter_egl_display(), 
						 configs[0], 
						 EGL_NO_CONTEXT, 
						 NULL);

      if (stage_egl->egl_context == EGL_NO_CONTEXT)
	g_warning ("eglCreateContext");
      
      status = eglMakeCurrent (clutter_egl_display(), 
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
  box->x2 = box->x1 + CLUTTER_UNITS_FROM_INT (stage_egl->surface_width);
  box->y2 = box->y1 + CLUTTER_UNITS_FROM_INT (stage_egl->surface_height);
}

static void
clutter_stage_egl_request_coords (ClutterActor        *self,
				  ClutterActorBox     *box)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (self);

  /* framebuffer no resize */
  box->x1 = 0;
  box->y1 = 0;
  box->x2 = CLUTTER_UNITS_FROM_INT (stage_egl->surface_width);
  box->y2 = CLUTTER_UNITS_FROM_INT (stage_egl->surface_height);
}

static void
clutter_stage_egl_set_fullscreen (ClutterStage *stage,
                                  gboolean      fullscreen)
{
  g_warning ("Stage of type `%s' do not support ClutterStage::set_fullscreen",
             G_OBJECT_TYPE_NAME (stage));
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
  stage_class->set_offscreen = clutter_stage_egl_set_offscreen;
  stage_class->draw_to_pixbuf = clutter_stage_egl_draw_to_pixbuf;
}

static void
clutter_stage_egl_init (ClutterStageEGL *stage)
{
  ;
}


