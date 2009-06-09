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
                         CLUTTER_TYPE_ACTOR,
                         G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_STAGE_WINDOW,
                                                clutter_stage_window_iface_init));

static void
clutter_stage_sdl_show (ClutterActor *actor)
{
  CLUTTER_ACTOR_SET_FLAGS (actor, CLUTTER_ACTOR_MAPPED);
  CLUTTER_ACTOR_SET_FLAGS (CLUTTER_STAGE_SDL (actor)->wrapper,
                           CLUTTER_ACTOR_MAPPED);

  CLUTTER_ACTOR_CLASS (clutter_stage_sdl_parent_class)->show (actor);
}

static void
clutter_stage_sdl_hide (ClutterActor *actor)
{
  /* No way to easily unmap SDL window ? */
  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_MAPPED);
  CLUTTER_ACTOR_UNSET_FLAGS (CLUTTER_STAGE_SDL (actor)->wrapper,
                             CLUTTER_ACTOR_MAPPED);

  CLUTTER_ACTOR_CLASS (clutter_stage_sdl_parent_class)->hide (actor);
}

static void
clutter_stage_sdl_unrealize (ClutterActor *actor)
{
  ;
}

static void
clutter_stage_sdl_realize (ClutterActor *actor)
{
  ClutterStageSDL *stage_sdl = CLUTTER_STAGE_SDL (actor);
  gboolean is_offscreen, is_fullscreen;

  CLUTTER_NOTE (BACKEND, "Realizing main stage");

  is_offscreen = is_fullscreen = FALSE;
  g_object_get (stage_sdl->wrapper,
                "offscreen", &is_offscreen,
                "fullscreen-set", &is_fullscreen,
                NULL);

  if (G_LIKELY (!is_offscreen))
    {
      gint flags = SDL_OPENGL;

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

          CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
          return;
	}
    }
  else
    {
      /* FIXME */
      g_critical ("SDL Backend does not yet support offscreen rendering");

      CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
    }
}

static void
clutter_stage_sdl_get_preferred_width (ClutterActor *self,
                                       gfloat        for_height,
                                       gfloat       *min_width_p,
                                       gfloat       *natural_width_p)
{
  ClutterStageSDL *stage_sdl = CLUTTER_STAGE_SDL (self);

  if (min_width_p)
    *min_width_p = CLUTTER_UNITS_FROM_DEVICE (stage_sdl->win_width);

  if (natural_width_p)
    *natural_width_p = CLUTTER_UNITS_FROM_DEVICE (stage_sdl->win_width);
}

static void
clutter_stage_sdl_get_preferred_height (ClutterActor *self,
                                        gfloat        for_width,
                                        gfloat       *min_height_p,
                                        gfloat       *natural_height_p)
{
  ClutterStageSDL *stage_sdl = CLUTTER_STAGE_SDL (self);

  if (min_height_p)
    *min_height_p = CLUTTER_UNITS_FROM_DEVICE (stage_sdl->win_height);

  if (natural_height_p)
    *natural_height_p = CLUTTER_UNITS_FROM_DEVICE (stage_sdl->win_height);
}

static void
clutter_stage_sdl_allocate (ClutterActor           *self,
                            const ClutterActorBox  *box,
                            ClutterAllocationFlags  flags)
{
  ClutterStageSDL *stage_sdl = CLUTTER_STAGE_SDL (self);
  gint new_width, new_height;
  ClutterActorClass *parent_class;

  /* FIXME: some how have X configure_notfiy call this ? */
  new_width  = ABS (CLUTTER_UNITS_TO_INT (box->x2 - box->x1));
  new_height = ABS (CLUTTER_UNITS_TO_INT (box->y2 - box->y1));

  if (new_width != stage_sdl->win_width ||
      new_height != stage_sdl->win_height)
    {
      if (SDL_SetVideoMode(new_width, 
			   new_height, 
			   0, SDL_OPENGL) == NULL)
	{
	  /* Failed */
	  return;
	}

      stage_sdl->win_width  = new_width;
      stage_sdl->win_height = new_height;

      CLUTTER_SET_PRIVATE_FLAGS (self, CLUTTER_ACTOR_SYNC_MATRICES);
    }

  parent_class = CLUTTER_ACTOR_CLASS (clutter_stage_sdl_parent_class);
  parent_class->allocate (self, box, flags);
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
  ClutterStageSDL *stage_sdl = CLUTTER_STAGE_SDL (gobject);

  G_OBJECT_CLASS (clutter_stage_sdl_parent_class)->dispose (gobject);
}

static void
clutter_stage_sdl_class_init (ClutterStageSDLClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  gobject_class->dispose = clutter_stage_sdl_dispose;
  
  actor_class->show = clutter_stage_sdl_show;
  actor_class->hide = clutter_stage_sdl_hide;
  actor_class->realize = clutter_stage_sdl_realize;
  actor_class->unrealize = clutter_stage_sdl_unrealize;
  actor_class->get_preferred_width = clutter_stage_sdl_get_preferred_width;
  actor_class->get_preferred_height = clutter_stage_sdl_get_preferred_height;
  actor_class->allocate = clutter_stage_sdl_allocate;
}

static void
clutter_stage_window_iface_init (ClutterStageWindowIface *iface)
{
  iface->set_fullscreen = clutter_stage_sdl_set_fullscreen;
  iface->set_cursor_visible = clutter_stage_sdl_set_cursor_visible;
  iface->set_title = clutter_stage_sdl_set_title;
}

static void
clutter_stage_sdl_init (ClutterStageSDL *stage)
{
  stage->win_width = 640;
  stage->win_height = 480;
}

