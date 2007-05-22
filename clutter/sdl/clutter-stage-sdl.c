#include "config.h"

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

#include "cogl.h"

G_DEFINE_TYPE (ClutterStageSDL, clutter_stage_sdl, CLUTTER_TYPE_STAGE);

static void
clutter_stage_sdl_show (ClutterActor *actor)
{
  ;
}

static void
clutter_stage_sdl_hide (ClutterActor *actor)
{
  /* No way to easily unmap SDL window ? */
  ;
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

  g_object_get (actor, "offscreen", &is_offscreen, NULL);
  g_object_get (actor, "fullscreen", &is_fullscreen, NULL);

  if (G_LIKELY (!is_offscreen))
    {
      gint flags = SDL_OPENGL;

      if (is_fullscreen) flags |= SDL_FULLSCREEN;

      SDL_GL_SetAttribute(SDL_GL_ACCUM_RED_SIZE, 0);
      SDL_GL_SetAttribute(SDL_GL_ACCUM_GREEN_SIZE, 0);
      SDL_GL_SetAttribute(SDL_GL_ACCUM_BLUE_SIZE, 0);
      SDL_GL_SetAttribute(SDL_GL_ACCUM_ALPHA_SIZE, 0);

      if (SDL_SetVideoMode(stage_sdl->win_width, 
			   stage_sdl->win_height, 
			   0, flags) == NULL)
	{
	  CLUTTER_NOTE (BACKEND, "SDL appears not to handle this mode - %s",
			SDL_GetError());
	  CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
	  return;
	}
    }
  else
    {
      /* FIXME */
      g_warning("SDL Backend does not yet support offscreen rendering\n");
      CLUTTER_ACTOR_UNSET_FLAGS (actor, CLUTTER_ACTOR_REALIZED);
      return;
    }

  _clutter_stage_sync_viewport (CLUTTER_STAGE (stage_sdl));
}

static void
clutter_stage_sdl_paint (ClutterActor *self)
{
  ClutterStage    *stage = CLUTTER_STAGE (self);
  ClutterColor     stage_color;
  static GTimer   *timer = NULL; 
  static guint     timer_n_frames = 0;

  CLUTTER_NOTE (PAINT, " Redraw enter");

  if (clutter_get_show_fps ())
    {
      if (!timer)
	timer = g_timer_new ();
    }

  clutter_stage_get_color (stage, &stage_color);

  cogl_paint_init (&stage_color);

  /* Basically call up to ClutterGroup paint here */
  CLUTTER_ACTOR_CLASS (clutter_stage_sdl_parent_class)->paint (self);

  /* Why this paint is done in backend as likely GL windowing system
   * specific calls, like swapping buffers.
  */
  
  SDL_GL_SwapBuffers();

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
clutter_stage_sdl_allocate_coords (ClutterActor    *self,
                                   ClutterActorBox *box)
{
  ClutterStageSDL *stage_sdl = CLUTTER_STAGE_SDL (self);

  box->x1 = box->y1 = 0;
  box->x2 = box->x1 + CLUTTER_UNITS_FROM_INT (stage_sdl->win_width);
  box->y2 = box->y1 + CLUTTER_UNITS_FROM_INT (stage_sdl->win_height);
}

static void
clutter_stage_sdl_request_coords (ClutterActor    *self,
				  ClutterActorBox *box)
{
  ClutterStageSDL *stage_sdl = CLUTTER_STAGE_SDL (self);
  gint new_width, new_height;

  /* FIXME: some how have X configure_notfiys call this ? */
  new_width  = ABS (CLUTTER_UNITS_TO_INT (box->x2 - box->x1));
  new_height = ABS (CLUTTER_UNITS_TO_INT (box->y2 - box->y1));

  if (new_width != stage_sdl->win_width ||
      new_height != stage_sdl->win_height)
    {
      if (SDL_SetVideoMode(new_width, 
			   new_height, 
			   0, SDL_OPENGL) == NULL)
	{
	  box->x2 = box->x1 + stage_sdl->win_width;
	  box->y2 = box->y1 + stage_sdl->win_height;

	  /* Failed */
	  return;
	}

      stage_sdl->win_width  = new_width;
      stage_sdl->win_height = new_height;

      _clutter_stage_sync_viewport (CLUTTER_STAGE (stage_sdl));
    }
}

static void
clutter_stage_sdl_set_fullscreen (ClutterStage *stage,
                                  gboolean      fullscreen)
{
  ClutterStageSDL *stage_sdl = CLUTTER_STAGE_SDL (stage);
  int              flags = SDL_OPENGL;

  if (fullscreen) flags |= SDL_FULLSCREEN;

  SDL_SetVideoMode(stage_sdl->win_width,
		   stage_sdl->win_height,
		   0, flags);
}

static void
clutter_stage_sdl_set_cursor_visible (ClutterStage *stage,
                                      gboolean      show_cursor)
{
  SDL_ShowCursor(show_cursor);
}

static void
clutter_stage_sdl_set_offscreen (ClutterStage *stage,
                                 gboolean      offscreen)
{
  g_warning ("Stage of type `%s' do not support ClutterStage::set_offscreen",
             G_OBJECT_TYPE_NAME (stage));
}

static void
clutter_stage_sdl_draw_to_pixbuf (ClutterStage *stage,
                                  GdkPixbuf    *dest,
                                  gint          x,
                                  gint          y,
                                  gint          width,
                                  gint          height)
{
  g_warning ("Stage of type `%s' do not support ClutterStage::draw_to_pixbuf",
             G_OBJECT_TYPE_NAME (stage));
}

static void
clutter_stage_sdl_dispose (GObject *gobject)
{
  ClutterStageSDL *stage_sdl = CLUTTER_STAGE_SDL (gobject);

  clutter_actor_unrealize (CLUTTER_ACTOR (stage_sdl));

  G_OBJECT_CLASS (clutter_stage_sdl_parent_class)->dispose (gobject);
}

static void
clutter_stage_sdl_class_init (ClutterStageSDLClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  ClutterStageClass *stage_class = CLUTTER_STAGE_CLASS (klass);

  gobject_class->dispose = clutter_stage_sdl_dispose;
  
  actor_class->show = clutter_stage_sdl_show;
  actor_class->hide = clutter_stage_sdl_hide;
  actor_class->realize = clutter_stage_sdl_realize;
  actor_class->unrealize = clutter_stage_sdl_unrealize;
  actor_class->paint = clutter_stage_sdl_paint;
  actor_class->request_coords = clutter_stage_sdl_request_coords;
  actor_class->allocate_coords = clutter_stage_sdl_allocate_coords;
  
  stage_class->set_fullscreen = clutter_stage_sdl_set_fullscreen;
  stage_class->set_cursor_visible = clutter_stage_sdl_set_cursor_visible;
  stage_class->set_offscreen = clutter_stage_sdl_set_offscreen;
  stage_class->draw_to_pixbuf = clutter_stage_sdl_draw_to_pixbuf;
}

static void
clutter_stage_sdl_init (ClutterStageSDL *stage)
{
  stage->win_width = 640;
  stage->win_height = 480;
}

