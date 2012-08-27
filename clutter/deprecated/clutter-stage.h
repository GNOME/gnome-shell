/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Copyright (C) 2011 Intel Corp
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

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
 *
 *
 * Deprecated: 1.2: Use clutter_actor_get_width() instead
 */
#define CLUTTER_STAGE_WIDTH()           (clutter_actor_get_width (clutter_stage_get_default ()))

/**
 * CLUTTER_STAGE_HEIGHT:
 *
 * Macro that evaluates to the height of the default stage
 *
 *
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

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_stage_new)
ClutterActor *  clutter_stage_get_default       (void);

CLUTTER_DEPRECATED_IN_1_10
gboolean        clutter_stage_is_default        (ClutterStage       *stage);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_queue_redraw)
void            clutter_stage_queue_redraw      (ClutterStage       *stage);

CLUTTER_DEPRECATED_IN_1_10
void            clutter_stage_set_use_fog       (ClutterStage       *stage,
                                                 gboolean            fog);

CLUTTER_DEPRECATED_IN_1_10
gboolean        clutter_stage_get_use_fog       (ClutterStage       *stage);

CLUTTER_DEPRECATED_IN_1_10
void            clutter_stage_set_fog           (ClutterStage       *stage,
                                                 ClutterFog         *fog);

CLUTTER_DEPRECATED_IN_1_10
void            clutter_stage_get_fog           (ClutterStage       *stage,
                                                 ClutterFog         *fog);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_set_background_color)
void            clutter_stage_set_color         (ClutterStage       *stage,
                                                 const ClutterColor *color);

CLUTTER_DEPRECATED_IN_1_10_FOR(clutter_actor_get_background_color)
void            clutter_stage_get_color         (ClutterStage       *stage,
                                                 ClutterColor       *color);

G_END_DECLS

#endif /* __CLUTTER_STAGE_DEPRECATED_H__ */
