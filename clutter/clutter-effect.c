/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Jorn Baayen  <jorn@openedhand.com>
 *             Emmanuele Bassi  <ebassi@openedhand.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:clutter-effect
 * @short_description: Utility Class for basic visual effects
 *
 * The #ClutterEffectTemplate class provides a simple API for applying
 * pre-defined effects to a single actor. It works as a wrapper around
 * the #ClutterBehaviour objects 
 *
 * Since: 0.4
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "clutter-alpha.h"
#include "clutter-main.h"
#include "clutter-marshal.h"
#include "clutter-private.h"
#include "clutter-debug.h"
#include "clutter-behaviour-bspline.h"
#include "clutter-behaviour-ellipse.h"
#include "clutter-behaviour-opacity.h"
#include "clutter-behaviour-path.h"
#include "clutter-behaviour-rotate.h"
#include "clutter-behaviour-scale.h"

#include "clutter-effect.h"

typedef struct ClutterEffectClosure
{
  ClutterActor             *actor;
  ClutterTimeline          *timeline;
  ClutterAlpha             *alpha;
  ClutterBehaviour         *behave;
  
  gulong                    signal_id;
  
  ClutterEffectCompleteFunc completed_func;
  gpointer                  completed_data;
  ClutterEffectTemplate     *template;
}
ClutterEffectClosure;

G_DEFINE_TYPE (ClutterEffectTemplate, clutter_effect_template, G_TYPE_OBJECT);

#define EFFECT_TEMPLATE_PRIVATE(o)   \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
   CLUTTER_TYPE_EFFECT_TEMPLATE,     \
   ClutterEffectTemplatePrivate))

struct _ClutterEffectTemplatePrivate
{
  ClutterTimeline *timeline;

  ClutterAlphaFunc alpha_func;
  gpointer alpha_data;
  GDestroyNotify alpha_notify;
};

enum
{
  PROP_0,

  PROP_ALPHA_FUNC,
  PROP_TIMELINE,
};

static void
clutter_effect_template_finalize (GObject *gobject)
{
  ClutterEffectTemplate *template = CLUTTER_EFFECT_TEMPLATE (gobject);
  ClutterEffectTemplatePrivate *priv = template->priv;

  if (priv->alpha_notify)
    {
      priv->alpha_notify (priv->alpha_data);
      priv->alpha_notify = NULL;
    }

  priv->alpha_data = NULL;
  priv->alpha_func = NULL;

  G_OBJECT_CLASS (clutter_effect_template_parent_class)->finalize (gobject);
}

static void
clutter_effect_template_dispose (GObject *object)
{
  ClutterEffectTemplate        *template;
  ClutterEffectTemplatePrivate *priv;

  template  = CLUTTER_EFFECT_TEMPLATE (object);
  priv      = template->priv;

  if (priv->timeline)
    {
      g_object_unref (priv->timeline);
      priv->timeline   = NULL;
    }

  G_OBJECT_CLASS (clutter_effect_template_parent_class)->dispose (object);
}

static void
clutter_effect_template_set_property (GObject      *object, 
				      guint         prop_id,
				      const GValue *value, 
				      GParamSpec   *pspec)
{
  ClutterEffectTemplate        *template;
  ClutterEffectTemplatePrivate *priv;

  template  = CLUTTER_EFFECT_TEMPLATE (object);
  priv      = template->priv; 

  switch (prop_id) 
    {
    case PROP_ALPHA_FUNC:
      priv->alpha_func = g_value_get_pointer (value);
      break;
    case PROP_TIMELINE:
      priv->timeline = g_value_get_object (value);
      g_object_ref(priv->timeline);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clutter_effect_template_get_property (GObject    *object, 
				      guint       prop_id,
				      GValue     *value, 
				      GParamSpec *pspec)
{
  ClutterEffectTemplate        *template;
  ClutterEffectTemplatePrivate *priv;

  template  = CLUTTER_EFFECT_TEMPLATE (object);
  priv      = template->priv;

  switch (prop_id) 
    {
    case PROP_ALPHA_FUNC:
      g_value_set_pointer (value, priv->alpha_func);
      break;
    case PROP_TIMELINE:
      g_value_set_object (value, priv->timeline);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    } 
}

static void
clutter_effect_template_class_init (ClutterEffectTemplateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (ClutterEffectTemplatePrivate));

  object_class->finalize = clutter_effect_template_finalize;
  object_class->dispose = clutter_effect_template_dispose;
  object_class->set_property = clutter_effect_template_set_property;
  object_class->get_property = clutter_effect_template_get_property;

  /**
   * ClutterEffectTemplate:alpha-func:
   *
   * #ClutterAlphaFunc to be used by the template
   *
   * Since: 0.4
   */
  g_object_class_install_property 
           (object_class,
	    PROP_ALPHA_FUNC,
	    g_param_spec_pointer ("alpha-func",
				  "Alpha-Function",
				  "Alpha reference Function", 
				  G_PARAM_CONSTRUCT_ONLY |
				  CLUTTER_PARAM_READWRITE));
  /**
   * ClutterEffectTemplate:timeline:
   *
   * #ClutterTimeline to be used by the template
   *
   * Since: 0.4
   */
  g_object_class_install_property 
           (object_class,
	    PROP_TIMELINE,
	    g_param_spec_object ("timeline",
				 "Timeline",
				 "Timeline to use as a reference for the Template",
				 CLUTTER_TYPE_TIMELINE,
				 G_PARAM_CONSTRUCT_ONLY |
				 CLUTTER_PARAM_READWRITE));
}

static void
clutter_effect_template_init (ClutterEffectTemplate *self)
{
  self->priv = EFFECT_TEMPLATE_PRIVATE (self);
}

static void
clutter_effect_template_set_alpha_func (ClutterEffectTemplate *self,
                                        ClutterAlphaFunc       alpha_func,
                                        gpointer               alpha_data,
                                        GDestroyNotify         alpha_notify)
{
  ClutterEffectTemplatePrivate *priv;

  priv = self->priv;

  if (priv->alpha_notify)
    {
      priv->alpha_notify (priv->alpha_data);
      priv->alpha_notify = NULL;
    }

  priv->alpha_data = alpha_data;
  priv->alpha_notify = alpha_notify;
  priv->alpha_func = alpha_func;
}

/**
 * clutter_effect_template_new:
 * @timeline:  A #ClutterTimeline for the template (will be cloned)
 * @alpha_func: An alpha func to use for the template.
 *
 * Creates a new #ClutterEffectTemplate, to be used with the effects API.
 *
 * A #ClutterEffectTemplate binds a timeline and an alpha function and can
 * be used as a template for multiple calls of clutter_effect_fade(),
 * clutter_effect_move() and clutter_effect_scale().
 *
 * This API is intended for simple animations involving a single actor;
 * for more complex animations, you should see #ClutterBehaviour and the
 * derived classes.
 *
 * Return value: a #ClutterEffectTemplate
 *
 * Since: 0.4
 */
ClutterEffectTemplate *
clutter_effect_template_new (ClutterTimeline *timeline, 
			     ClutterAlphaFunc alpha_func)
{
  ClutterEffectTemplate *retval;

  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), NULL);
  g_return_val_if_fail (alpha_func != NULL, NULL);

  retval = g_object_new (CLUTTER_TYPE_EFFECT_TEMPLATE, 
                         "timeline", timeline,
                         NULL);

  clutter_effect_template_set_alpha_func (retval, alpha_func, NULL, NULL);

  return retval;
}

/**
 * clutter_effect_template_new_full:
 * @timeline: a #ClutterTimeline
 * @alpha_func: an alpha function to use for the template
 * @user_data: data to be passed to the alpha function, or %NULL
 * @notify: function to be called when disposing the alpha function's use
 *   data, or %NULL
 *
 * Creates a new #ClutterEffectTemplate, to be used with the effects API.
 *
 * A #ClutterEffectTemplate binds a timeline and an alpha function and can
 * be used as a template for multiple calls of clutter_effect_fade(),
 * clutter_effect_move() and clutter_effect_scale().
 *
 * This API is intended for simple animations involving a single actor;
 * for more complex animations, you should see #ClutterBehaviour and the
 * derived classes.
 *
 * This function is intended for language bindings only: if @notify is
 * not %NULL it will be called to dispose of @user_data.
 *
 * Return value: the newly created #ClutterEffectTemplate object
 *
 * Since: 0.4
 */
ClutterEffectTemplate *
clutter_effect_template_new_full (ClutterTimeline  *timeline,
                                  ClutterAlphaFunc  alpha_func,
                                  gpointer          user_data,
                                  GDestroyNotify    notify)
{
  ClutterEffectTemplate *retval;

  g_return_val_if_fail (CLUTTER_IS_TIMELINE (timeline), NULL);
  g_return_val_if_fail (alpha_func != NULL, NULL);

  retval = g_object_new (CLUTTER_TYPE_EFFECT_TEMPLATE,
                         "timeline", timeline,
                         NULL);

  clutter_effect_template_set_alpha_func (retval, alpha_func, user_data, notify);

  return retval;
}

static void
clutter_effect_closure_destroy (ClutterEffectClosure *c)
{
  g_signal_handler_disconnect (c->timeline, c->signal_id);
  clutter_behaviour_remove (c->behave, c->actor);

  g_object_unref (c->actor);
  g_object_unref (c->template);
  g_object_unref (c->behave);
  g_object_unref (c->alpha);
  g_object_unref (c->timeline);

  g_slice_free (ClutterEffectClosure, c);
}

static ClutterEffectClosure *
clutter_effect_closure_new (ClutterEffectTemplate *template,
			    ClutterActor          *actor, 
			    GCallback              complete)
{
  ClutterEffectClosure *c;
  ClutterEffectTemplatePrivate *priv = EFFECT_TEMPLATE_PRIVATE(template);

  c = g_slice_new0(ClutterEffectClosure);

  g_object_ref (actor);
  g_object_ref (template);

  c->template = template;
  c->actor    = actor;
  c->timeline = clutter_timeline_clone (priv->timeline);
  c->alpha    = clutter_alpha_new_full (c->timeline,
					priv->alpha_func,
					priv->alpha_data,
                                        NULL);

  c->signal_id =
    g_signal_connect (c->timeline, "completed",G_CALLBACK (complete), c);

  return c;
}

static void
on_effect_complete (ClutterTimeline *timeline,
		    gpointer         user_data)
{
  ClutterEffectClosure *c =  (ClutterEffectClosure*)user_data;

  if (c->completed_func)
    c->completed_func(c->actor, c->completed_data);

  clutter_effect_closure_destroy (c);
}

/**
 * clutter_effect_fade:
 * @template_: A #ClutterEffectTemplate
 * @actor: A #ClutterActor to apply the effect to.
 * @start_opacity: Initial opacity value to apply to actor
 * @end_opacity: Final opacity value to apply to actor
 * @completed_func: A #ClutterEffectCompleteFunc to call on effect
 *   completion or %NULL
 * @completed_data: Data to pass to supplied  #ClutterEffectCompleteFunc
 *   or %NULL
 *
 * Simple effect for fading a single #ClutterActor.
 *
 * Return value: a #ClutterTimeline for the effect. Will be unrefed by
 * the effect when completed.
 *
 * Since: 0.4
 */
ClutterTimeline *
clutter_effect_fade (ClutterEffectTemplate     *template_,
		     ClutterActor              *actor,
		     guint8                     start_opacity,
		     guint8                     end_opacity,
		     ClutterEffectCompleteFunc  completed_func,
		     gpointer                   completed_data)
{
  ClutterEffectClosure *c;

  c = clutter_effect_closure_new (template_,
				  actor, 
				  G_CALLBACK (on_effect_complete));

  c->completed_func = completed_func;
  c->completed_data = completed_data;

  clutter_actor_set_opacity (actor, start_opacity);

  c->behave = clutter_behaviour_opacity_new (c->alpha, 
					     start_opacity, 
					     end_opacity);
  
  clutter_behaviour_apply (c->behave, actor);
  clutter_timeline_start (c->timeline);
  
  return c->timeline;
}

/**
 * clutter_effect_move:
 * @template_: A #ClutterEffectTemplate
 * @actor: A #ClutterActor to apply the effect to.
 * @knots: An array of #ClutterKnots representing path for the actor
 * @n_knots: Number of #ClutterKnots in passed array.
 * @completed_func: A #ClutterEffectCompleteFunc to call on effect
 *   completion or %NULL
 * @completed_data: Data to pass to supplied  #ClutterEffectCompleteFunc
 *   or %NULL
 *
 * Simple effect for moving a single #ClutterActor along a path.
 *
 * Return value: a #ClutterTimeline for the effect. Will be unreferenced by
 *   the effect when completed.
 *
 * Since: 0.4
 */
ClutterTimeline *
clutter_effect_move (ClutterEffectTemplate     *template_,
		     ClutterActor              *actor,
		     const ClutterKnot         *knots,
		     guint                      n_knots,
		     ClutterEffectCompleteFunc  completed_func,
		     gpointer                   completed_data)
{
  ClutterEffectClosure *c;

  c = clutter_effect_closure_new (template_,
				  actor, 
				  G_CALLBACK (on_effect_complete));

  c->completed_func = completed_func;
  c->completed_data = completed_data;

  if (n_knots)
    clutter_actor_set_position (actor, knots[0].x, knots[0].y);

  c->behave = clutter_behaviour_path_new (c->alpha, knots, n_knots);
  
  clutter_behaviour_apply (c->behave, actor);
  clutter_timeline_start (c->timeline);
  
  return c->timeline;
}

/**
 * clutter_effect_scale:
 * @template_: A #ClutterEffectTemplate
 * @actor: A #ClutterActor to apply the effect to.
 * @scale_begin: Initial scale factor to apply to actor
 * @scale_end: Final scale factor to apply to actor
 * @gravity: A #ClutterGravity for the scale.
 * @completed_func: A #ClutterEffectCompleteFunc to call on effect
 *   completion or NULL
 * @completed_data: Data to pass to supplied  #ClutterEffectCompleteFunc
 *   or NULL
 *
 * Simple effect for scaling a single #ClutterActor.
 *
 * Return value: a #ClutterTimeline for the effect. Will be unreferenced by
 *   the effect when completed.
 *
 * Since: 0.4
 */
ClutterTimeline *
clutter_effect_scale (ClutterEffectTemplate     *template_,
		      ClutterActor              *actor,
		      gdouble                    scale_begin,
		      gdouble                    scale_end,
		      ClutterGravity             gravity,
		      ClutterEffectCompleteFunc  completed_func,
		      gpointer                   completed_data)
{
  ClutterEffectClosure *c;

  c = clutter_effect_closure_new (template_,
				  actor, 
				  G_CALLBACK (on_effect_complete));

  c->completed_func = completed_func;
  c->completed_data = completed_data;

  clutter_actor_set_scale_with_gravity (actor, 
					scale_begin, scale_begin, gravity);

  c->behave = clutter_behaviour_scale_new (c->alpha, 
					   scale_begin,
					   scale_end,
					   gravity);
  
  clutter_behaviour_apply (c->behave, actor);
  clutter_timeline_start (c->timeline);
  
  return c->timeline;
}

/**
 * clutter_effect_rotate_x:
 * @template_: A #ClutterEffectTemplate
 * @actor: A #ClutterActor to apply the effect to.
 * @angle_begin: Initial angle to apply to actor
 * @angle_end: Final angle to apply to actor
 * @center_y: Position on Y axis to rotate about.
 * @center_z: Position on Z axis to rotate about.
 * @direction: A #ClutterRotateDirection for the rotation.
 * @completed_func: A #ClutterEffectCompleteFunc to call on effect
 *   completion or NULL
 * @completed_data: Data to pass to supplied  #ClutterEffectCompleteFunc
 *   or NULL
 *
 * Simple effect for rotating a single #ClutterActor about y axis.
 *
 * Return value: a #ClutterTimeline for the effect. Will be unreferenced by
 *   the effect when completed.
 *
 * Since: 0.4
 */
ClutterTimeline *
clutter_effect_rotate_x (ClutterEffectTemplate     *template_,
			 ClutterActor              *actor,
			 gdouble                    angle_begin,
			 gdouble                    angle_end,
			 gint                       center_y,
			 gint                       center_z,
			 ClutterRotateDirection     direction,
			 ClutterEffectCompleteFunc  completed_func,
			 gpointer                   completed_data)
{
  ClutterEffectClosure *c;

  c = clutter_effect_closure_new (template_,
				  actor, 
				  G_CALLBACK (on_effect_complete));

  c->completed_func = completed_func;
  c->completed_data = completed_data;


  clutter_actor_rotate_x (actor, angle_begin, center_y, center_y);

  c->behave = clutter_behaviour_rotate_new (c->alpha,
					    CLUTTER_X_AXIS,
					    direction,
					    angle_begin,
					    angle_end);
  g_object_set (c->behave,
		"center-y", center_y,
		"center-z", center_z,
		NULL);
  
  clutter_behaviour_apply (c->behave, actor);
  clutter_timeline_start (c->timeline);
  
  return c->timeline;
}

/**
 * clutter_effect_rotate_y:
 * @template_: A #ClutterEffectTemplate
 * @actor: A #ClutterActor to apply the effect to.
 * @angle_begin: Initial angle to apply to actor
 * @angle_end: Final angle to apply to actor
 * @center_x: Position on X axis to rotate about.
 * @center_z: Position on Z axis to rotate about.
 * @direction: A #ClutterRotateDirection for the rotation.
 * @completed_func: A #ClutterEffectCompleteFunc to call on effect
 *   completion or NULL
 * @completed_data: Data to pass to supplied  #ClutterEffectCompleteFunc
 *   or NULL
 *
 * Simple effect for rotating a single #ClutterActor about y axis.
 *
 * Return value: a #ClutterTimeline for the effect. Will be unreferenced by
 *   the effect when completed.
 *
 * Since: 0.4
 */
ClutterTimeline *
clutter_effect_rotate_y (ClutterEffectTemplate     *template_,
			 ClutterActor              *actor,
			 gdouble                    angle_begin,
			 gdouble                    angle_end,
			 gint                       center_x,
			 gint                       center_z,
			 ClutterRotateDirection     direction,
			 ClutterEffectCompleteFunc  completed_func,
			 gpointer                   completed_data)
{
  ClutterEffectClosure *c;

  c = clutter_effect_closure_new (template_,
				  actor, 
				  G_CALLBACK (on_effect_complete));

  c->completed_func = completed_func;
  c->completed_data = completed_data;


  clutter_actor_rotate_y (actor, angle_begin, center_x, center_z);

  c->behave = clutter_behaviour_rotate_new (c->alpha,
					    CLUTTER_Y_AXIS,
					    direction,
					    angle_begin,
					    angle_end);
  g_object_set (c->behave,
		"center-x", center_x,
		"center-z", center_z,
		NULL);
  
  clutter_behaviour_apply (c->behave, actor);
  clutter_timeline_start (c->timeline);
  
  return c->timeline;
}

/**
 * clutter_effect_rotate_z:
 * @template_: A #ClutterEffectTemplate
 * @actor: A #ClutterActor to apply the effect to.
 * @angle_begin: Initial angle to apply to actor
 * @angle_end: Final angle to apply to actor
 * @center_x: Position on X axis to rotate about.
 * @center_y: Position on Y axis to rotate about.
 * @direction: A #ClutterRotateDirection for the rotation.
 * @completed_func: A #ClutterEffectCompleteFunc to call on effect
 *   completion or NULL
 * @completed_data: Data to pass to supplied  #ClutterEffectCompleteFunc
 *   or NULL
 *
 * Simple effect for rotating a single #ClutterActor about z axis.
 *
 * Return value: a #ClutterTimeline for the effect. Will be unreferenced by
 *   the effect when completed.
 *
 * Since: 0.4
 */
ClutterTimeline *
clutter_effect_rotate_z (ClutterEffectTemplate     *template_,
			 ClutterActor              *actor,
			 gdouble                    angle_begin,
			 gdouble                    angle_end,
			 gint                       center_x,
			 gint                       center_y,
			 ClutterRotateDirection     direction,
			 ClutterEffectCompleteFunc  completed_func,
			 gpointer                   completed_data)
{
  ClutterEffectClosure *c;

  c = clutter_effect_closure_new (template_,
				  actor, 
				  G_CALLBACK (on_effect_complete));

  c->completed_func = completed_func;
  c->completed_data = completed_data;


  clutter_actor_rotate_z (actor, angle_begin, center_x, center_y);

  c->behave = clutter_behaviour_rotate_new (c->alpha,
					    CLUTTER_Z_AXIS,
					    direction,
					    angle_begin,
					    angle_end);
  g_object_set (c->behave,
		"center-x", center_x,
		"center-y", center_y,
		NULL);
  
  clutter_behaviour_apply (c->behave, actor);
  clutter_timeline_start (c->timeline);
  
  return c->timeline;
}
