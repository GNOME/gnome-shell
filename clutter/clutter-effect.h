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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(__CLUTTER_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <clutter/clutter.h> can be included directly."
#endif

#ifndef __CLUTTER_EFFECT_H__
#define __CLUTTER_EFFECT_H__

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

GType                  clutter_effect_template_get_type           (void) G_GNUC_CONST;
ClutterEffectTemplate *clutter_effect_template_new                (ClutterTimeline       *timeline,
                                                                   ClutterAlphaFunc       alpha_func);
ClutterEffectTemplate *clutter_effect_template_new_full           (ClutterTimeline       *timeline,
                                                                   ClutterAlphaFunc       alpha_func,
                                                                   gpointer               user_data,
                                                                   GDestroyNotify         notify);
ClutterEffectTemplate *clutter_effect_template_new_for_duration   (guint                  msecs,
                                                                   ClutterAlphaFunc       alpha_func);
void                   clutter_effect_template_construct          (ClutterEffectTemplate *template_,
                                                                   ClutterTimeline       *timeline,
                                                                   ClutterAlphaFunc       alpha_func,
                                                                   gpointer               user_data,
                                                                   GDestroyNotify         notify);
void                   clutter_effect_template_set_timeline_clone (ClutterEffectTemplate *template_,
								   gboolean               setting);
gboolean               clutter_effect_template_get_timeline_clone (ClutterEffectTemplate *template_);


/*
 * Clutter effects
 */

ClutterTimeline *clutter_effect_fade   (ClutterEffectTemplate     *template_,
                                        ClutterActor              *actor,
                                        guint8                     opacity_end,
                                        ClutterEffectCompleteFunc  func,
                                        gpointer                   data);
ClutterTimeline *clutter_effect_depth  (ClutterEffectTemplate     *template_,
                                        ClutterActor              *actor,
                                        gint                       depth_end,
                                        ClutterEffectCompleteFunc  func,
                                        gpointer                   data);
ClutterTimeline *clutter_effect_move   (ClutterEffectTemplate     *template_,
                                        ClutterActor              *actor,
                                        gint                       x,
                                        gint                       y,
                                        ClutterEffectCompleteFunc  func,
                                        gpointer                   data);
ClutterTimeline *clutter_effect_path   (ClutterEffectTemplate     *template_,
                                        ClutterActor              *actor,
                                        const ClutterKnot         *knots,
                                        guint                      n_knots,
                                        ClutterEffectCompleteFunc  func,
                                        gpointer                   data);
ClutterTimeline *clutter_effect_scale  (ClutterEffectTemplate     *template_,
                                        ClutterActor              *actor,
                                        gdouble                    x_scale_end,
                                        gdouble                    y_scale_end,
                                        ClutterEffectCompleteFunc  func,
                                        gpointer                   data);
ClutterTimeline *clutter_effect_rotate (ClutterEffectTemplate     *template_,
                                        ClutterActor              *actor,
                                        ClutterRotateAxis          axis,
                                        gdouble                    angle,
                                        gint                       center_x,
                                        gint                       center_y,
                                        gint                       center_z,
                                        ClutterRotateDirection     direction,
                                        ClutterEffectCompleteFunc  func,
                                        gpointer                   data);

G_END_DECLS

#endif /* __CLUTTER_EFFECT_H__ */
