#ifndef __CLUTTER_STAGE_DEPRECATED_H__
#define __CLUTTER_STAGE_DEPRECATED_H__

#include <clutter/clutter-types.h>

G_BEGIN_DECLS

#ifndef CLUTTER_DISABLE_DEPRECATED

/**
 * CLUTTER_STAGE_WIDTH:
 *
 * Macro that evaluates to the width of the default stage
 *
 * Since: 0.2
 *
 * Deprecated: 1.2: Use clutter_actor_get_width() instead
 */
#define CLUTTER_STAGE_WIDTH()           (clutter_actor_get_width (clutter_stage_get_default ()))

/**
 * CLUTTER_STAGE_HEIGHT:
 *
 * Macro that evaluates to the height of the default stage
 *
 * Since: 0.2
 *
 * Deprecated: 1.2: use clutter_actor_get_height() instead
 */
#define CLUTTER_STAGE_HEIGHT()          (clutter_actor_get_height (clutter_stage_get_default ()))

/* Commodity macro, for mallum only */
#define clutter_stage_add(stage,actor)                  G_STMT_START {  \
  if (CLUTTER_IS_STAGE ((stage)) && CLUTTER_IS_ACTOR ((actor)))         \
    {                                                                   \
      ClutterContainer *_container = (ClutterContainer *) (stage);      \
      ClutterActor *_actor = (ClutterActor *) (actor);                  \
      clutter_container_add_actor (_container, _actor);                 \
    }                                                   } G_STMT_END

#endif /* CLUTTER_DISABLE_DEPRECATED */

CLUTTER_DEPRECATED_FOR(clutter_stage_new)
ClutterActor *  clutter_stage_get_default       (void);

CLUTTER_DEPRECATED
gboolean        clutter_stage_is_default        (ClutterStage *stage);

CLUTTER_DEPRECATED_FOR(clutter_actor_queue_redraw)
void            clutter_stage_queue_redraw      (ClutterStage *stage);

CLUTTER_DEPRECATED
void            clutter_stage_set_use_fog       (ClutterStage *stage,
                                                 gboolean      fog);

CLUTTER_DEPRECATED
gboolean        clutter_stage_get_use_fog       (ClutterStage *stage);

CLUTTER_DEPRECATED
void            clutter_stage_set_fog           (ClutterStage *stage,
                                                 ClutterFog   *fog);

CLUTTER_DEPRECATED
void            clutter_stage_get_fog           (ClutterStage *stage,
                                                 ClutterFog   *fog);
G_END_DECLS

#endif /* __CLUTTER_STAGE_DEPRECATED_H__ */
