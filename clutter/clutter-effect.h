/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *
 * Copyright (C) 2006, 2007 OpenedHand
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _CLUTTER_EFFECT
#define _CLUTTER_EFFECT

#include <glib-object.h>
#include <clutter/clutter-actor.h>
#include <clutter/clutter-timeline.h>
#include <clutter/clutter-alpha.h>
#include <clutter/clutter-behaviour.h>

G_BEGIN_DECLS

/**
 * ClutterEffectCompleteFunc:
 * @actor: a #ClutterActor
 * @user_data: user data
 *
 * Callback function invoked when an effect is complete.
 *
 * Since: 0.4
 */
typedef void (*ClutterEffectCompleteFunc) (ClutterActor *actor,
					   gpointer      user_data);

#define CLUTTER_TYPE_EFFECT_TEMPLATE clutter_effect_template_get_type()

#define CLUTTER_EFFECT_TEMPLATE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CLUTTER_TYPE_EFFECT_TEMPLATE, ClutterEffectTemplate))

#define CLUTTER_EFFECT_TEMPLATE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CLUTTER_TYPE_EFFECT_TEMPLATE, ClutterEffectTemplateClass))

#define CLUTTER_IS_EFFECT_TEMPLATE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CLUTTER_TYPE_EFFECT_TEMPLATE))

#define CLUTTER_IS_EFFECT_TEMPLATE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CLUTTER_TYPE_EFFECT_TEMPLATE))

#define CLUTTER_EFFECT_TEMPLATE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CLUTTER_TYPE_EFFECT_TEMPLATE, ClutterEffectTemplateClass))
  
typedef struct _ClutterEffectTemplate           ClutterEffectTemplate;
typedef struct _ClutterEffectTemplatePrivate    ClutterEffectTemplatePrivate;
typedef struct _ClutterEffectTemplateClass      ClutterEffectTemplateClass;


struct _ClutterEffectTemplate
{
  /*< private >*/
  GObject parent_instance;

  ClutterEffectTemplatePrivate *priv;
};

struct _ClutterEffectTemplateClass
{
  /*< private >*/
  GObjectClass parent_class;

  /* padding, for future expansion */
  void (*_clutter_reserved1) (void);
  void (*_clutter_reserved2) (void);
  void (*_clutter_reserved3) (void);
  void (*_clutter_reserved4) (void);
};

GType                  clutter_effect_template_get_type (void) G_GNUC_CONST;
ClutterEffectTemplate *clutter_effect_template_new      (ClutterTimeline  *timeline,
                                                         ClutterAlphaFunc  alpha_func);
ClutterEffectTemplate *clutter_effect_template_new_full (ClutterTimeline  *timeline,
                                                         ClutterAlphaFunc  alpha_func,
                                                         gpointer          user_data,
                                                         GDestroyNotify    notify);

void                   clutter_effect_template_set_timeline_clone (ClutterEffectTemplate *template_,
								   gboolean               setting);
gboolean               clutter_effect_template_get_timeline_clone (ClutterEffectTemplate *template_);


/*
 * Clutter effects
 */

ClutterTimeline *clutter_effect_fade  (ClutterEffectTemplate     *template_,
                                       ClutterActor              *actor,
                                       guint8                     opacity_start,
                                       guint8                     opacity_end,
                                       ClutterEffectCompleteFunc  completed_func,
                                       gpointer                   completed_data);
ClutterTimeline *clutter_effect_depth (ClutterEffectTemplate     *template_,
                                       ClutterActor               *actor,
                                       gint                       depth_start,
                                       gint                       depth_end,
                                       ClutterEffectCompleteFunc  completed_func,
                                       gpointer                   completed_data);
ClutterTimeline *clutter_effect_move  (ClutterEffectTemplate     *template_,
                                       ClutterActor              *actor,
                                       const ClutterKnot         *knots,
                                       guint                      n_knots,
                                       ClutterEffectCompleteFunc  completed_func,
                                       gpointer                   completed_data);
ClutterTimeline *clutter_effect_scale (ClutterEffectTemplate     *template_,
                                       ClutterActor              *actor,
                                       gdouble                    scale_start,
                                       gdouble                    scale_end,
                                       ClutterGravity             gravity,
                                       ClutterEffectCompleteFunc  completed_func,
                                       gpointer                   completed_data);

ClutterTimeline *clutter_effect_rotate_x (ClutterEffectTemplate     *template_,
					  ClutterActor              *actor,
					  gdouble                    angle_start,
					  gdouble                    angle_end,
					  gint                       center_y,
					  gint                       center_z,
					  ClutterRotateDirection     direction,
					  ClutterEffectCompleteFunc  completed_func,
					  gpointer                   completed_data);
ClutterTimeline *clutter_effect_rotate_y (ClutterEffectTemplate     *template_,
					  ClutterActor              *actor,
					  gdouble                    angle_start,
					  gdouble                    angle_end,
					  gint                       center_x,
					  gint                       center_z,
					  ClutterRotateDirection     direction,
					  ClutterEffectCompleteFunc  completed_func,
					  gpointer                   completed_data);

ClutterTimeline *clutter_effect_rotate_z (ClutterEffectTemplate     *template_,
					  ClutterActor              *actor,
					  gdouble                    angle_start,
					  gdouble                    angle_end,
					  gint                       center_x,
					  gint                       center_y,
					  ClutterRotateDirection     direction,
					  ClutterEffectCompleteFunc  completed_func,
					  gpointer                   completed_data);

G_END_DECLS

#endif /* _CLUTTER_EFFECT */
