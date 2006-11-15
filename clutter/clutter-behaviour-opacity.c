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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-actor.h"
#include "clutter-behaviour.h"
#include "clutter-behaviour-opacity.h"
#include "clutter-enum-types.h"
#include "clutter-main.h"

#include <math.h>

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

static void
clutter_behaviour_opacity_frame_foreach (ClutterActor            *actor,
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
clutter_behaviour_alpha_notify (ClutterBehaviour *behave)
{
  clutter_behaviour_actors_foreach (behave,
                                    (GFunc)clutter_behaviour_opacity_frame_foreach,
                                    CLUTTER_BEHAVIOUR_OPACITY (behave));
}

static void 
clutter_behaviour_opacity_finalize (GObject *object)
{
  G_OBJECT_CLASS (clutter_behaviour_opacity_parent_class)->finalize (object);
}


static void
clutter_behaviour_opacity_class_init (ClutterBehaviourOpacityClass *klass)
{
  ClutterBehaviourClass *behave_class;

  behave_class = (ClutterBehaviourClass*) klass;

  behave_class->alpha_notify = clutter_behaviour_alpha_notify;

  g_type_class_add_private (klass, sizeof (ClutterBehaviourOpacityPrivate));
}

static void
clutter_behaviour_opacity_init (ClutterBehaviourOpacity *self)
{
  self->priv = CLUTTER_BEHAVIOUR_OPACITY_GET_PRIVATE (self);
}

ClutterBehaviour*
clutter_behaviour_opacity_new (ClutterAlpha *alpha,
			       guint8        opacity_start,
			       guint8        opacity_end)
{
  ClutterBehaviourOpacity *behave;

  behave = g_object_new (CLUTTER_TYPE_BEHAVIOUR_OPACITY, 
                         "alpha", alpha,
			 NULL);

  behave->priv->opacity_start = opacity_start;
  behave->priv->opacity_end   = opacity_end;

  return CLUTTER_BEHAVIOUR (behave);
}

