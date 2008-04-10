#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-backend-egl.h"
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
#include "../clutter-container.h"
#include "../clutter-stage.h"
#include "../clutter-stage-window.h"

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterStageEGL,
                         clutter_stage_egl,
                         CLUTTER_TYPE_STAGE_X11,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init));

static void
clutter_stage_egl_unrealize (ClutterActor *actor)
{
  ClutterStageEGL *stage_egl = CLUTTER_STAGE_EGL (actor);
  ClutterStageX11 *stage_x11 = CLUTTER_STAGE_X11 (actor);
  gboolean was_offscreen;

  CLUTTER_MARK();

  g_object_get (stage_x11->wrapper, "offscreen", &was_offscreen, NULL);

  CLUTTER_ACTOR_CLASS (clutter_stage_egl_parent_class)->unrealize (actor);

  clutter_x11_trap_x_errors ()

  if (G_UNLIKELY (was_offscreen))
    {
      /* No support as yet for this */
    }
  else
    {
      if (!stage_X11->is_foreign_xwin && stage_x11->xwin != None)
	{
	  XDestroyWindow (stage_x11->xdpy, stage_x11->xwin);
	  stage_x11->xwin = None;
	}
      else
	stage_x11->xwin = None;
    }

  if (stage_egl->egl_surface)
    {
      eglDestroySurface (clutter_eglx_display (), stage_egl->egl_surface);
      stage_egl->egl_surface = EGL_NO_SURFACE;
    }

  clutter_stage_ensure_current (stage_x11->wrapper);

  /* XSync (stage_x11->xdpy, False); */

  clutter_x11_untrap_x_errors ();

  CLUTTER_MARK ();
}

static void
clutter_stage_egl_realize (ClutterActor *actor)
{
  ClutterStageEGL   *stage_egl = CLUTTER_STAGE_EGL (actor);
  ClutterStageX11   *stage_x11 = CLUTTER_STAGE_X11 (actor);
  ClutterBackendEGL *backend_egl;
  EGLConfig          configs[2];
  EGLint             config_count;
  EGLBoolean         status;
  gboolean           is_offscreen = FALSE;

  CLUTTER_NOTE (BACKEND, "Realizing main stage");

  g_object_get (stage_x11->wrapper, "offscreen", &is_offscreen, NULL);

  backend_egl = CLUTTER_BACKEND_EGL (clutter_get_default_backend ());

  if (G_LIKELY (!is_offscreen))
    {
      EGLint cfg_attribs[] = {
        EGL_BUFFER_SIZE,    EGL_DONT_CARE,
	EGL_RED_SIZE,       5,
	EGL_GREEN_SIZE,     6,
	EGL_BLUE_SIZE,      5,
	EGL_NONE
      };

      status = eglGetConfigs (backend_egl->edpy,
			      configs,
			      2,
			      &config_count);

      if (status != EGL_TRUE)
	g_warning ("eglGetConfigs failed");

      status = eglChooseConfig (backend_egl->edpy,
				cfg_attribs,
				configs,
                                G_N_ELEMENTS (configs),
				&config_count);

      if (status != EGL_TRUE)
	g_warning ("eglChooseConfig failed");

      if (stage_x11->xwin == None)
	stage_x11->xwin =
	  XCreateSimpleWindow (stage_x11->xdpy,
                               stage_x11->xwin_root,
                               0, 0,
                               stage_x11->xwin_width,
                               stage_x11->xwin_height,
                               0, 0,
                               WhitePixel (stage_x11->xdpy,
                                           stage_x11->xscreen));

      XSelectInput (stage_x11->xdpy, stage_x11->xwin,
                    StructureNotifyMask
		    | ExposureMask
		    /* FIXME: we may want to eplicity enable MotionMask */
		    | PointerMotionMask
		    | KeyPressMask
		    | KeyReleaseMask
		    | ButtonPressMask
		    | ButtonReleaseMask
		    | PropertyChangeMask);

      if (stage_egl->egl_surface != EGL_NO_SURFACE)
        {
	  eglDestroySurface (backend_egl->edpy, stage_egl->egl_surface);
          stage_egl->egl_surface = EGL_NO_SURFACE;
        }

      stage_egl->egl_surface =
        eglCreateWindowSurface (backend_egl->edpy,
                                configs[0],
                                (NativeWindowType) stage_x11->xwin,
                                NULL);

      if (stage_egl->egl_surface == EGL_NO_SURFACE)
        {
          g_critical ("Unable to create an EGL surface");

          CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
          return;
        }

      if (G_UNLIKELY (backend_egl->egl_context == None))
        {
          CLUTTER_NOTE (GL, "Creating EGL Context");

          backend_egl->egl_context = eglCreateContext (backend_egl->edpy,
                                                       configs[0],
                                                       EGL_NO_CONTEXT,
                                                       NULL);

          if (backend_egl->egl_context == EGL_NO_CONTEXT)
            {
              g_critical ("Unable to create a suitable EGL context");

              CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
              return;
            }
        }

      /* this will make sure to set the current context */
      CLUTTER_NOTE (BACKEND, "Marking stage as realized and setting context");
      CLUTTER_ACTOR_SET_FLAGS (stage_x11->wrapper, CLUTTER_ACTOR_REALIZED);
      CLUTTER_ACTOR_SET_FLAGS (stage_x11, CLUTTER_ACTOR_REALIZED);
      clutter_stage_ensure_current (stage_x11->wrapper);
    }
  else
    {
      g_warning("EGLX Backend does not support offscreen rendering");
      CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
      return;
    }

  CLUTTER_SET_PRIVATE_FLAGS (stage_x11->wrapper, CLUTTER_ACTOR_SYNC_MATRICES);
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

static GdkPixbuf *
clutter_stage_egl_draw_to_pixbuf (ClutterStageWindow *stage_window,
                                  gint                x,
                                  gint                y,
                                  gint                width,
                                  gint                height)
{
  g_warning ("Stages of type `%s' do not support "
             "ClutterStageWindow::draw_to_pixbuf",
             G_OBJECT_TYPE_NAME (stage));
  return NULL;
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  iface->draw_to_pixbuf = clutter_stage_egl_draw_to_pixbuf;

  /* the rest is inherited from ClutterStageX11 */
}

static void
clutter_stage_egl_class_init (ClutterStageEGLClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  gobject_class->dispose = clutter_stage_egl_dispose;

  actor_class->realize = clutter_stage_egl_realize;
  actor_class->unrealize = clutter_stage_egl_unrealize;
}

static void
clutter_stage_egl_init (ClutterStageEGL *stage)
{
}
