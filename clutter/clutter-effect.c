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
 * SECTION:clutter-effect-template
 * @short_description: A utility class
 *
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

typedef struct _ClutterEffectTemplatePrivate ClutterEffectTemplatePrivate;

struct _ClutterEffectTemplatePrivate
{
  ClutterTimeline *timeline;
  ClutterAlphaFunc alpha_func;
};

enum
{
  PROP_0,
  PROP_ALPHA_FUNC,
  PROP_TIMELINE,
};

static void
clutter_effect_template_dispose (GObject *object)
{
  ClutterEffectTemplate        *template;
  ClutterEffectTemplatePrivate *priv;

  template  = CLUTTER_EFFECT_TEMPLATE(object);
  priv      = EFFECT_TEMPLATE_PRIVATE(template);

  g_object_unref (priv->timeline);

  priv->timeline   = NULL;
  priv->alpha_func = NULL;

  if (G_OBJECT_CLASS (clutter_effect_template_parent_class)->dispose)
    G_OBJECT_CLASS (clutter_effect_template_parent_class)->dispose (object);
}

static void
clutter_effect_template_finalize (GObject *object)
{

  G_OBJECT_CLASS (clutter_effect_template_parent_class)->finalize (object);
}

static void
clutter_effect_template_set_property (GObject      *object, 
				      guint         prop_id,
				      const GValue *value, 
				      GParamSpec   *pspec)
{
  ClutterEffectTemplate        *template;
  ClutterEffectTemplatePrivate *priv;

  template  = CLUTTER_EFFECT_TEMPLATE(object);
  priv      = EFFECT_TEMPLATE_PRIVATE(template);

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

  template  = CLUTTER_EFFECT_TEMPLATE(object);
  priv      = EFFECT_TEMPLATE_PRIVATE(template);

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

  object_class->dispose = clutter_effect_template_dispose;
  object_class->finalize = clutter_effect_template_finalize;

  object_class->set_property = clutter_effect_template_set_property;
  object_class->get_property = clutter_effect_template_get_property;

  g_object_class_install_property 
           (object_class,
	    PROP_ALPHA_FUNC,
	    g_param_spec_pointer ("alpha-func",
				  "Alpha-Function",
				  "Alpha reference Function", 
				  G_PARAM_CONSTRUCT_ONLY |
				  CLUTTER_PARAM_READWRITE));

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
}

/**
 * clutter_effect_template_new:
 *
 * FIXME
 *
 * Return value: a #ClutterEffectTemplate
 *
 * Since: 0.4
 */
ClutterEffectTemplate*
clutter_effect_template_new (ClutterTimeline *timeline, 
			     ClutterAlphaFunc alpha_func)
{
  return g_object_new (CLUTTER_TYPE_EFFECT_TEMPLATE, 
		       "timeline", timeline,
		       "alpha-func", alpha_func,
		       NULL);
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

static ClutterEffectClosure*
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
  c->timeline = clutter_timeline_copy (priv->timeline);
  c->alpha    = clutter_alpha_new_full (c->timeline,
					priv->alpha_func,
					NULL, NULL);

  c->signal_id  = g_signal_connect (c->timeline,
				    "completed",
				    G_CALLBACK(complete),
				    c);
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
 *
 * FIXME
 *
 * Return value: an alpha value.
 *
 * Since: 0.4
 */
ClutterTimeline*
clutter_effect_fade (ClutterEffectTemplate     *template,
		     ClutterActor             *actor,
		     guint8                    start_opacity,
		     guint8                    end_opacity,
		     ClutterEffectCompleteFunc completed_func,
		     gpointer                  completed_userdata)
{
  ClutterEffectClosure *c;

  c = clutter_effect_closure_new (template,
				  actor, 
				  G_CALLBACK (on_effect_complete));

  c->completed_func = completed_func;
  c->completed_data = completed_userdata;

  c->behave = clutter_behaviour_opacity_new (c->alpha, 
					     start_opacity, 
					     end_opacity);
  
  clutter_behaviour_apply (c->behave, actor);
  clutter_timeline_start (c->timeline);
  
  return c->timeline;
}

/**
 * clutter_effect_move:
 *
 * FIXME
 *
 * Return value: an alpha value.
 *
 * Since: 0.4
 */
ClutterTimeline*
clutter_effect_move (ClutterEffectTemplate    *template,
		     ClutterActor             *actor,
		     const ClutterKnot        *knots,
		     guint                     n_knots,
		     ClutterEffectCompleteFunc completed_func,
		     gpointer                  completed_userdata)
{
  ClutterEffectClosure *c;

  c = clutter_effect_closure_new (template,
				  actor, 
				  G_CALLBACK (on_effect_complete));

  c->completed_func = completed_func;
  c->completed_data = completed_userdata;

  c->behave = clutter_behaviour_path_new (c->alpha, knots, n_knots);
  
  clutter_behaviour_apply (c->behave, actor);
  clutter_timeline_start (c->timeline);
  
  return c->timeline;
}

/**
 * clutter_effect_scale:
 *
 * FIXME
 *
 * Return value: a #ClutterTimeline.
 *
 * Since: 0.4
 */
ClutterTimeline*
clutter_effect_scale (ClutterEffectTemplate    *template,
		      ClutterActor             *actor,
		      gdouble                   scale_begin,
		      gdouble                   scale_end,
		      ClutterGravity            gravity,
		      ClutterEffectCompleteFunc completed_func,
		      gpointer                  completed_userdata)
{
  ClutterEffectClosure *c;

  c = clutter_effect_closure_new (template,
				  actor, 
				  G_CALLBACK (on_effect_complete));

  c->completed_func = completed_func;
  c->completed_data = completed_userdata;

  c->behave = clutter_behaviour_scale_new (c->alpha, 
					   scale_begin,
					   scale_end,
					   gravity);
  
  clutter_behaviour_apply (c->behave, actor);
  clutter_timeline_start (c->timeline);
  
  return c->timeline;
}
