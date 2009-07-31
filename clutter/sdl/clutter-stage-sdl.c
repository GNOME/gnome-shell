#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-stage-sdl.h"
#include "clutter-sdl.h"

#include "../clutter-main.h"
#include "../clutter-feature.h"
#include "../clutter-color.h"
#include "../clutter-util.h"
#include "../clutter-event.h"
#include "../clutter-enum-types.h"
#include "../clutter-private.h"
#include "../clutter-debug.h"
#include "../clutter-units.h"
#include "../clutter-stage-window.h"

#include "cogl/cogl.h"

static void clutter_stage_window_iface_init (ClutterStageWindowIface *iface);

G_DEFINE_TYPE_WITH_CODE (ClutterStageSDL,
                         clutter_stage_sdl,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init));

static ClutterActor *
clutter_stage_sdl_get_wrapper (ClutterStageWindow *stage_window)
{
  return CLUTTER_ACTOR (CLUTTER_STAGE_SDL (stage_window)->wrapper);
}

static void
clutter_stage_sdl_show (ClutterStageWindow *stage_window,
                        gboolean            do_raise G_GNUC_UNUSED)
{
  clutter_actor_map (clutter_stage_sdl_get_wrapper (stage_window));
}

static void
clutter_stage_sdl_hide (ClutterStageWindow *stage_window)
{
  clutter_actor_unmap (clutter_stage_sdl_get_wrapper (stage_window));
}

static void
clutter_stage_sdl_unrealize (ClutterStageWindow *stage_window)
{
}

static gboolean
clutter_stage_sdl_realize (ClutterStageWindow *stage_window)
{
  ClutterStageSDL *stage_sdl = CLUTTER_STAGE_SDL (stage_window);
  gboolean is_fullscreen = FALSE;
  gint flags = SDL_OPENGL;

  CLUTTER_NOTE (BACKEND, "Realizing main stage");

  g_object_get (stage_sdl->wrapper,
                "fullscreen-set", &is_fullscreen,
                NULL);

  if (is_fullscreen)
    flags |= SDL_FULLSCREEN;

  SDL_GL_SetAttribute (SDL_GL_ACCUM_RED_SIZE, 0);
  SDL_GL_SetAttribute (SDL_GL_ACCUM_GREEN_SIZE, 0);
  SDL_GL_SetAttribute (SDL_GL_ACCUM_BLUE_SIZE, 0);
  SDL_GL_SetAttribute (SDL_GL_ACCUM_ALPHA_SIZE, 0);

  if (SDL_SetVideoMode (stage_sdl->win_width,
                        stage_sdl->win_height,
                        0, flags) == NULL)
    {
      CLUTTER_NOTE (BACKEND, "SDL appears not to handle this mode - %s",
                    SDL_GetError ());

      return FALSE;
    }

  return TRUE;
}

static void
clutter_stage_sdl_get_geometry (ClutterStageWindow *stage_window,
                                ClutterGeometry    *geometry)
{
  ClutterStageSDL *stage_sdl = CLUTTER_STAGE_SDL (stage_window);
  gboolean is_fullscreen = FALSE;

  is_fullscreen = clutter_stage_get_fullscreen (stage_sdl->wrapper);

  if (is_fullscreen)
    {
      const SDL_VideoInfo *v_info;

      v_info = SDL_GetVideoInfo ();

      geometry->width = v_info->current_w;
      geometry->height = v_info->current_h;
    }
  else
    {
      geometry->width = stage_sdl->win_width;
      geometry->height = stage_sdl->win_height;
    }
}

static void
clutter_stage_sdl_resize (ClutterStageWindow *stage_window,
                          gint                width,
                          gint                height)
{
  ClutterStageSDL *stage_sdl = CLUTTER_STAGE_SDL (stage_window);

  if (width != stage_sdl->win_width ||
      height != stage_sdl->win_height)
    {
      if (SDL_SetVideoMode (width, height, 0, SDL_OPENGL) == NULL)
	{
	  /* Failed */
	  return;
	}

      stage_sdl->win_width  = width;
      stage_sdl->win_height = height;
    }
}

static void
clutter_stage_sdl_set_fullscreen (ClutterStageWindow *stage_window,
                                  gboolean            fullscreen)
{
  ClutterStageSDL *stage_sdl = CLUTTER_STAGE_SDL (stage_window);
  int              flags = SDL_OPENGL;

  if (fullscreen)
    flags |= SDL_FULLSCREEN;

  SDL_SetVideoMode (stage_sdl->win_width,
		    stage_sdl->win_height,
		    0, flags);
}

static void
clutter_stage_sdl_set_cursor_visible (ClutterStageWindow *stage_window,
                                      gboolean            show_cursor)
{
  SDL_ShowCursor (show_cursor);
}

static void
clutter_stage_sdl_set_title (ClutterStageWindow *stage_window,
			     const gchar        *title)
{
  SDL_WM_SetCaption  (title, NULL);
}

static void
clutter_stage_sdl_dispose (GObject *gobject)
{
  G_OBJECT_CLASS (clutter_stage_sdl_parent_class)->dispose (gobject);
}

static void
clutter_stage_sdl_class_init (ClutterStageSDLClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = clutter_stage_sdl_dispose;
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  iface->set_fullscreen = clutter_stage_sdl_set_fullscreen;
  iface->set_cursor_visible = clutter_stage_sdl_set_cursor_visible;
  iface->set_title = clutter_stage_sdl_set_title;
  iface->show = clutter_stage_sdl_show;
  iface->hide = clutter_stage_sdl_hide;
  iface->realize = clutter_stage_sdl_realize;
  iface->unrealize = clutter_stage_sdl_unrealize;
  iface->resize = clutter_stage_sdl_resize;
  iface->get_geometry = clutter_stage_sdl_get_geometry;
  iface->get_wrapper = clutter_stage_sdl_get_wrapper;
}

static void
clutter_stage_sdl_init (ClutterStageSDL *stage)
{
  stage->win_width = 640;
  stage->win_height = 480;
}

