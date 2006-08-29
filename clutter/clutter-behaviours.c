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
 * SECTION:clutter-behaviour
 * @short_description: Class for providing behaviours to actors
 *
 */

#include "config.h"

#include "clutter-actor.h"
#include "clutter-behaviour.h"
#include "clutter-behaviours.h"
#include "clutter-enum-types.h"
#include "clutter-main.h"

G_DEFINE_TYPE (ClutterBehaviourPath,   \
               clutter_behaviour_path, \
	       CLUTTER_TYPE_BEHAVIOUR);

struct ClutterBehaviourPathPrivate
{
  gint x1, y1, x2, y2;
};

#define CLUTTER_BEHAVIOUR_PATH_GET_PRIVATE(obj)    \
              (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
               CLUTTER_TYPE_BEHAVIOUR_PATH,        \
               ClutterBehaviourPathPrivate))

static void 
clutter_behaviour_path_dispose (GObject *object)
{
  ClutterBehaviourPath *self = CLUTTER_BEHAVIOUR_PATH(object); 

  if (self->priv)
    {
      /* FIXME: remove all actors */
    }

  G_OBJECT_CLASS (clutter_behaviour_path_parent_class)->dispose (object);
}

static void 
clutter_behaviour_path_finalize (GObject *object)
{
  ClutterBehaviourPath *self = CLUTTER_BEHAVIOUR_PATH(object); 

  if (self->priv)
    {
      g_free(self->priv);
      self->priv = NULL;
    }

  G_OBJECT_CLASS (clutter_behaviour_path_parent_class)->finalize (object);
}


static void
clutter_behaviour_path_class_init (ClutterBehaviourPathClass *klass)
{
  GObjectClass        *object_class;

  object_class = (GObjectClass*) klass;

  object_class->finalize     = clutter_behaviour_path_finalize;
  object_class->dispose      = clutter_behaviour_path_dispose;

  g_type_class_add_private (object_class, sizeof (ClutterBehaviourPathPrivate));
}

static void
clutter_behaviour_path_init (ClutterBehaviourPath *self)
{
  ClutterBehaviourPathPrivate *priv;

  self->priv = priv = CLUTTER_BEHAVIOUR_PATH_GET_PRIVATE (self);
}

/* 

function line(x0, x1, y0, y1)
     boolean steep := abs(y1 - y0) > abs(x1 - x0)
     if steep then
         swap(x0, y0)
         swap(x1, y1)
     if x0 > x1 then
         swap(x0, x1)
         swap(y0, y1)
     int deltax := x1 - x0
     int deltay := abs(y1 - y0)
     int error := 0
     int ystep
     int y := y0
     if y0 < y1 then ystep := 1 else ystep := -1
     for x from x0 to x1
         if steep then plot(y,x) else plot(x,y)
         error := error + deltay
         if 2

 */

ClutterBehaviour*
clutter_behaviour_path_new (GObject    *object,
                            const char *property,
			    gint        x1,
			    gint        y1,
			    gint        x2,
			    gint        y2)
{
  ClutterBehaviourPath *behave;

  behave = g_object_new (CLUTTER_TYPE_BEHAVIOUR_PATH, 
                         "object", object,
                         "property", property,
			 NULL);

  return CLUTTER_BEHAVIOUR(behave);
}

/* opacity */

G_DEFINE_TYPE (ClutterBehaviourOpacity,   \
               clutter_behaviour_opacity, \
	       CLUTTER_TYPE_BEHAVIOUR);

struct ClutterBehaviourOpacityPrivate
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
  GParamSpec                     *pspec;

  priv = CLUTTER_BEHAVIOUR_OPACITY_GET_PRIVATE (behave);
  _behave = CLUTTER_BEHAVIOUR (behave);

  pspec = clutter_behaviour_get_param_spec (_behave);

  g_object_get (clutter_behaviour_get_object (_behave),
                pspec->name,
                &alpha,
                NULL);

  opacity = (alpha * (priv->opacity_end - priv->opacity_start)) 
                    / ((GParamSpecUInt *) pspec)->maximum;

  opacity += priv->opacity_start;

  CLUTTER_DBG("alpha %i opacity %i\n", alpha, opacity);

  clutter_actor_set_opacity (actor, opacity);
}

static void
clutter_behaviour_property_change (ClutterBehaviour *behave,
                                   GObject          *object,
                                   GParamSpec       *param_spec)
{
  g_return_if_fail (param_spec->value_type == G_TYPE_UINT);

  clutter_behaviour_actors_foreach 
                     (behave,
		      (GFunc)clutter_behaviour_opacity_frame_foreach,
		      CLUTTER_BEHAVIOUR_OPACITY(behave));
}

static void 
clutter_behaviour_opacity_dispose (GObject *object)
{
  G_OBJECT_CLASS (clutter_behaviour_opacity_parent_class)->dispose (object);
}

static void 
clutter_behaviour_opacity_finalize (GObject *object)
{
  ClutterBehaviourOpacity *self = CLUTTER_BEHAVIOUR_OPACITY(object); 

  if (self->priv)
    {
      g_free(self->priv);
      self->priv = NULL;
    }

  G_OBJECT_CLASS (clutter_behaviour_opacity_parent_class)->finalize (object);
}


static void
clutter_behaviour_opacity_class_init (ClutterBehaviourOpacityClass *klass)
{
  GObjectClass          *object_class;
  ClutterBehaviourClass *behave_class;

  object_class = (GObjectClass*) klass;

  object_class->finalize     = clutter_behaviour_opacity_finalize;
  object_class->dispose      = clutter_behaviour_opacity_dispose;

  behave_class = (ClutterBehaviourClass*) klass;

  behave_class->property_change = clutter_behaviour_property_change;

  g_type_class_add_private (object_class, sizeof (ClutterBehaviourOpacityPrivate));
}

static void
clutter_behaviour_opacity_init (ClutterBehaviourOpacity *self)
{
  ClutterBehaviourOpacityPrivate *priv;

  self->priv = priv = CLUTTER_BEHAVIOUR_OPACITY_GET_PRIVATE (self);
}

ClutterBehaviour*
clutter_behaviour_opacity_new (GObject    *object,
                               const char *property,
			       guint8      opacity_start,
			       guint8      opacity_end)
{
  ClutterBehaviourOpacity *behave;

  behave = g_object_new (CLUTTER_TYPE_BEHAVIOUR_OPACITY, 
                         "object", object,
                         "property", property,
			 NULL);

  behave->priv->opacity_start = opacity_start;
  behave->priv->opacity_end   = opacity_end;

  return CLUTTER_BEHAVIOUR(behave);
}

ClutterBehaviour*
clutter_behaviour_opacity_new_from_alpha (ClutterAlpha *alpha,
                                          guint8        opacity_start,
                                          guint8        opacity_end)
{
  return clutter_behaviour_opacity_new (G_OBJECT (alpha),
                                        "alpha",
                                        opacity_start,
                                        opacity_end);
}
