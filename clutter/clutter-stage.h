/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006 OpenedHand
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

#ifndef __CLUTTER_STAGE_H__
#define __CLUTTER_STAGE_H__

#include <clutter/clutter-types.h>
#include <clutter/clutter-group.h>
#include <clutter/clutter-color.h>
#include <clutter/clutter-event.h>

G_BEGIN_DECLS

#define CLUTTER_TYPE_PERSPECTIVE        (clutter_perspective_get_type ())
#define CLUTTER_TYPE_FOG                (clutter_fog_get_type ())
#define CLUTTER_TYPE_STAGE              (clutter_stage_get_type())

#define CLUTTER_STAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_STAGE, ClutterStage))

#define CLUTTER_STAGE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_STAGE, ClutterStageClass))

#define CLUTTER_IS_STAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_STAGE))

#define CLUTTER_IS_STAGE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_STAGE))

#define CLUTTER_STAGE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_STAGE, ClutterStageClass))

/**
 * CLUTTER_STAGE_WIDTH:
 *
 * Macro that evaluates to the width of the default stage
 *
 * Since: 0.2
 */
#define CLUTTER_STAGE_WIDTH() \
 (clutter_actor_get_width (clutter_stage_get_default ()))

/**
 * CLUTTER_STAGE_HEIGHT:
 *
 * Macro that evaluates to the height of the default stage
 *
 * Since: 0.2
 */
#define CLUTTER_STAGE_HEIGHT() \
 (clutter_actor_get_height (clutter_stage_get_default ()))

/**
 * ClutterPickMode:
 * @CLUTTER_PICK_NONE: Do not paint any actor
 * @CLUTTER_PICK_REACTIVE: Paint only the reactive actors
 * @CLUTTER_PICK_ALL: Paint all actors
 *
 * Controls the paint cycle of the scene graph when in pick mode
 *
 * Since: 1.0
 */
typedef enum {
  CLUTTER_PICK_NONE = 0,
  CLUTTER_PICK_REACTIVE,
  CLUTTER_PICK_ALL
} ClutterPickMode;

typedef struct _ClutterPerspective  ClutterPerspective;
typedef struct _ClutterFog          ClutterFog;

typedef struct _ClutterStageClass   ClutterStageClass;
typedef struct _ClutterStagePrivate ClutterStagePrivate;

/**
 * ClutterStage:
 *
 * The #ClutterStage structure contains only private data
 * and should be accessed using the provided API
 *
 * Since: 0.1
 */
struct _ClutterStage
{
  /*< private >*/
  ClutterGroup parent_instance;

  ClutterStagePrivate *priv;
};
/**
 * ClutterStageClass:
 * @fullscreen: handler for the #ClutterStage::fullscreen signal
 * @unfullscreen: handler for the #ClutterStage::unfullscreen signal
 * @activate: handler for the #ClutterStage::activate signal
 * @deactivate: handler for the #ClutterStage::deactive signal
 *
 * The #ClutterStageClass structure contains only private data
 *
 * Since: 0.1
 */

struct _ClutterStageClass
{
  /*< private >*/
  ClutterGroupClass parent_class;

  /*< public >*/
  /* signals */
  void (* fullscreen)   (ClutterStage *stage);
  void (* unfullscreen) (ClutterStage *stage);
  void (* activate)     (ClutterStage *stage);
  void (* deactivate)   (ClutterStage *stage);

  /*< private >*/
  /* padding for future expansion */
  gpointer _padding_dummy[32];
};



/**
 * ClutterPerspective:
 * @fovy: the field of view angle, in degrees, in the y direction
 * @aspect: the aspect ratio that determines the field of view in the x
 *   direction. The aspect ratio is the ratio of x (width) to y (height)
 * @z_near: the distance from the viewer to the near clipping
 *   plane (always positive)
 * @z_far: the distance from the viewer to the far clipping
 *   plane (always positive)
 *
 * Stage perspective definition. #ClutterPerspective is only used by
 * the fixed point version of clutter_stage_set_perspective().
 *
 * Since: 0.4
 */
struct _ClutterPerspective
{
  gfloat fovy;
  gfloat aspect;
  gfloat z_near;
  gfloat z_far;
};

/**
 * ClutterFog:
 * @z_near: starting distance from the viewer to the near clipping
 *   plane (always positive)
 * @z_far: final distance from the viewer to the far clipping
 *   plane (always positive)
 *
 * Fog settings used to create the depth cueing effect.
 *
 * Since: 0.6
 */
struct _ClutterFog
{
  gfloat z_near;
  gfloat z_far;
};

GType         clutter_perspective_get_type    (void) G_GNUC_CONST;
GType         clutter_fog_get_type            (void) G_GNUC_CONST;
GType         clutter_stage_get_type          (void) G_GNUC_CONST;

ClutterActor *clutter_stage_get_default       (void);
ClutterActor *clutter_stage_new               (void);

void          clutter_stage_set_color         (ClutterStage       *stage,
                                               const ClutterColor *color);
void          clutter_stage_get_color         (ClutterStage       *stage,
                                               ClutterColor       *color);
void          clutter_stage_set_perspective   (ClutterStage       *stage,
			                       ClutterPerspective *perspective);
void          clutter_stage_get_perspective   (ClutterStage       *stage,
			                       ClutterPerspective *perspective);
void          clutter_stage_set_fullscreen    (ClutterStage       *stage,
                                               gboolean            fullscreen);
gboolean      clutter_stage_get_fullscreen    (ClutterStage       *stage);
void          clutter_stage_show_cursor       (ClutterStage       *stage);
void          clutter_stage_hide_cursor       (ClutterStage       *stage);

ClutterActor *clutter_stage_get_actor_at_pos  (ClutterStage       *stage,
                                               ClutterPickMode     pick_mode,
                                               gint                x,
                                               gint                y);
guchar *      clutter_stage_read_pixels       (ClutterStage       *stage,
                                               gint                x,
                                               gint                y,
                                               gint                width,
                                               gint                height);
gboolean      clutter_stage_event             (ClutterStage       *stage,
                                               ClutterEvent       *event);

void                  clutter_stage_set_title          (ClutterStage *stage,
                                                        const gchar  *title);
G_CONST_RETURN gchar *clutter_stage_get_title          (ClutterStage *stage);
void                  clutter_stage_set_user_resizable (ClutterStage *stage,
						        gboolean      resizable);
gboolean              clutter_stage_get_user_resizable (ClutterStage *stage);
void                  clutter_stage_set_use_fog        (ClutterStage *stage,
                                                        gboolean      fog);
gboolean              clutter_stage_get_use_fog        (ClutterStage *stage);
void                  clutter_stage_set_fog            (ClutterStage *stage,
                                                        ClutterFog   *fog);
void                  clutter_stage_get_fog            (ClutterStage *stage,
                                                        ClutterFog   *fog);

void                  clutter_stage_set_key_focus      (ClutterStage *stage,
                                                        ClutterActor *actor);
ClutterActor *        clutter_stage_get_key_focus      (ClutterStage *stage);
void                  clutter_stage_ensure_current     (ClutterStage *stage);
void                  clutter_stage_queue_redraw       (ClutterStage *stage);
gboolean              clutter_stage_is_default         (ClutterStage *stage);
void                  clutter_stage_ensure_viewport    (ClutterStage *stage);
void                  clutter_stage_ensure_redraw      (ClutterStage *stage);

void     clutter_stage_set_throttle_motion_events (ClutterStage *stage,
                                                   gboolean      throttle);
gboolean clutter_stage_get_throttle_motion_events (ClutterStage *stage);

void                  clutter_stage_set_use_alpha      (ClutterStage *stage,
                                                        gboolean      use_alpha);
gboolean              clutter_stage_get_use_alpha      (ClutterStage *stage);

/* Commodity macro, for mallum only */
#define clutter_stage_add(stage,actor)                  G_STMT_START {  \
  if (CLUTTER_IS_STAGE ((stage)) && CLUTTER_IS_ACTOR ((actor)))         \
    {                                                                   \
      ClutterContainer *_container = (ClutterContainer *) (stage);      \
      ClutterActor *_actor = (ClutterActor *) (actor);                  \
      clutter_container_add_actor (_container, _actor);                 \
    }                                                   } G_STMT_END

G_END_DECLS

#endif /* __CLUTTER_STAGE_H__ */
