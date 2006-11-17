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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:clutter-behaviour-opacity
 * @short_description: A behaviour class interpolating actors opacity between
 * two values.
 *
 * #ClutterBehaviourPath interpolates actors opacity between two values.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-actor.h"
#include "clutter-behaviour.h"
#include "clutter-behaviour-opacity.h"
#include "clutter-enum-types.h"
#include "clutter-main.h"

#include <math.h>

/**
 * SECTION:clutter-behaviour-opacity
 * @short_description: Behaviour controlling the opacity
 *
 * #ClutterBehaviourOpacity controls the opacity of a set of actors.
 *
 * Since: 0.2
 */

G_DEFINE_TYPE (ClutterBehaviourOpacity,
               clutter_behaviour_opacity,
	       CLUTTER_TYPE_BEHAVIOUR);

struct _ClutterBehaviourOpacityPrivate
{
  guint8 opacity_start;
  guint8 opacity_end;
};

#define CLUTTER_BEHAVIOUR_OPACITY_GET_PRIVATE(obj)    \
              (G_TYPE_INSTANCE_GET_PRIVATE ((obj),    \
               CLUTTER_TYPE_BEHAVIOUR_OPACITY,        \
               ClutterBehaviourOpacityPrivate))

enum
{
  PROP_0,

  PROP_OPACITY_START,
  PROP_OPACITY_END
};

static void
opacity_frame_foreach (ClutterActor            *actor,
                       ClutterBehaviourOpacity *behave)
{
  guint32                         alpha;
  guint8                          opacity;
  ClutterBehaviourOpacityPrivate *priv;
  ClutterBehaviour               *_behave;

  priv = CLUTTER_BEHAVIOUR_OPACITY_GET_PRIVATE (behave);
  _behave = CLUTTER_BEHAVIOUR (behave);

  alpha = clutter_alpha_get_alpha (clutter_behaviour_get_alpha (_behave));

  opacity = (alpha * (priv->opacity_end - priv->opacity_start)) 
             / CLUTTER_ALPHA_MAX_ALPHA;

  opacity += priv->opacity_start;

  CLUTTER_DBG ("alpha %i opacity %i\n", alpha, opacity);

  clutter_actor_set_opacity (actor, opacity);
}

static void
clutter_behaviour_alpha_notify (ClutterBehaviour *behave,
                                guint32           alpha_value)
{
  clutter_behaviour_actors_foreach (behave,
                                    (GFunc) opacity_frame_foreach,
                                    CLUTTER_BEHAVIOUR_OPACITY (behave));
}

static void 
clutter_behaviour_opacity_finalize (GObject *object)
{
  G_OBJECT_CLASS (clutter_behaviour_opacity_parent_class)->finalize (object);
}

static void
clutter_behaviour_opacity_set_property (GObject      *gobject,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  ClutterBehaviourOpacity *opacityb = CLUTTER_BEHAVIOUR_OPACITY (gobject);

  switch (prop_id)
    {
    case PROP_OPACITY_START:
      opacityb->priv->opacity_start = g_value_get_uint (value);
      break;
    case PROP_OPACITY_END:
      opacityb->priv->opacity_end = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_behaviour_opacity_get_property (GObject    *gobject,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  ClutterBehaviourOpacity *opacityb = CLUTTER_BEHAVIOUR_OPACITY (gobject);

  switch (prop_id)
    {
    case PROP_OPACITY_START:
      g_value_set_uint (value, opacityb->priv->opacity_start);
      break;
    case PROP_OPACITY_END:
      g_value_set_uint (value, opacityb->priv->opacity_end);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
clutter_behaviour_opacity_class_init (ClutterBehaviourOpacityClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterBehaviourClass *behave_class = CLUTTER_BEHAVIOUR_CLASS (klass);

  gobject_class->set_property = clutter_behaviour_opacity_set_property;
  gobject_class->get_property = clutter_behaviour_opacity_get_property;

  /**
   * ClutterBehaviourOpacity:opacity-start:
   *
   * Initial opacity level of the behaviour.
   *
   * Since: 0.2
   */
  g_object_class_install_property (gobject_class,
                                   PROP_OPACITY_START,
                                   g_param_spec_uint ("opacity-start",
                                                      "Opacity Start",
                                                      "Initial opacity level",
                                                      0, 255,
                                                      0,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT));
  
  /**
   * ClutterBehaviourOpacity:opacity-end:
   *
   * Final opacity level of the behaviour.
   *
   * Since: 0.2
   */
  g_object_class_install_property (gobject_class,
                                   PROP_OPACITY_END,
                                   g_param_spec_uint ("opacity-end",
                                                      "Opacity End",
                                                      "Final opacity level",
                                                      0, 255,
                                                      0,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT));

  behave_class->alpha_notify = clutter_behaviour_alpha_notify;

  g_type_class_add_private (klass, sizeof (ClutterBehaviourOpacityPrivate));
}

static void
clutter_behaviour_opacity_init (ClutterBehaviourOpacity *self)
{
  self->priv = CLUTTER_BEHAVIOUR_OPACITY_GET_PRIVATE (self);
}

/**
 * clutter_behaviour_opacity_new:
 * @alpha: a #ClutterAlpha instance, or %NULL
 * @opacity_start: minimum level of opacity
 * @opacity_end: maximum level of opacity
 *
 * Creates a new #ClutterBehaviourOpacity object, driven by @alpha
 * which controls the opacity property of every actor, making it
 * change in the interval between @opacity_start and @opacity_end.
 *
 * Return value: the newly created #ClutterBehaviourOpacity
 *
 * Since: 0.2
 */
ClutterBehaviour *
clutter_behaviour_opacity_new (ClutterAlpha *alpha,
			       guint8        opacity_start,
			       guint8        opacity_end)
{
  return g_object_new (CLUTTER_TYPE_BEHAVIOUR_OPACITY, 
                       "alpha", alpha,
                       "opacity-start", opacity_start,
                       "opacity-end", opacity_end,
                       NULL);
}

