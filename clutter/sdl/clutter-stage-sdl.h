#ifndef __CLUTTER_STAGE_SDL_H__
#define __CLUTTER_STAGE_SDL_H__

#include <glib-object.h>
#include <clutter/clutter-stage.h>

#define CLUTTER_TYPE_STAGE_SDL                  (clutter_stage_sdl_get_type ())
#define CLUTTER_STAGE_SDL(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_TYPE_STAGE_SDL, ClutterStageSDL))
#define CLUTTER_IS_STAGE_SDL(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_TYPE_STAGE_SDL))
#define CLUTTER_STAGE_SDL_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_TYPE_STAGE_SDL, ClutterStageSDLClass))
#define CLUTTER_IS_STAGE_SDL_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_TYPE_STAGE_SDL))
#define CLUTTER_STAGE_SDL_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_TYPE_STAGE_SDL, ClutterStageSDLClass))

typedef struct _ClutterStageSDL         ClutterStageSDL;
typedef struct _ClutterStageSDLClass    ClutterStageSDLClass;

struct _ClutterStageSDL
{
  ClutterActor parent_instance;

  gint         win_width;
  gint         win_height;

  ClutterStage *wrapper;
};

struct _ClutterStageSDLClass
{
  ClutterActorClass parent_class;
};

GType clutter_stage_sdl_get_type (void) G_GNUC_CONST;

#endif /* __CLUTTER_STAGE_SDL_H__ */
