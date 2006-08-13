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

#include "clutter-behaviours.h"
#include "clutter-enum-types.h"
#include "clutter-private.h" 	/* for DBG */
#include "clutter-timeline.h"

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

ClutterBehaviour*
clutter_behaviour_path_new (ClutterTimeline *timeline,
			    gint             x1,
			    gint             y1,
			    gint             x2,
			    gint             y2)
{
  ClutterBehaviourPath *behave;

  behave = g_object_new (CLUTTER_TYPE_BEHAVIOUR_PATH, 
			 "timeline", timeline,
			 NULL);

  return CLUTTER_BEHAVIOUR(behave);
}

/* opacity */

G_DEFINE_TYPE (ClutterBehaviourOpacity,   \
               clutter_behaviour_opacity, \
	       CLUTTER_TYPE_BEHAVIOUR);

struct ClutterBehaviourOpacityPrivate
{
  guint8  opacity_start;
  guint8  opacity_end;
  gulong  handler_id; 	 /* FIXME: handle in parent class ?  */
};

#define CLUTTER_BEHAVIOUR_OPACITY_GET_PRIVATE(obj)    \
              (G_TYPE_INSTANCE_GET_PRIVATE ((obj),    \
               CLUTTER_TYPE_BEHAVIOUR_OPACITY,        \
               ClutterBehaviourOpacityPrivate))

static void 
clutter_behaviour_opacity_dispose (GObject *object)
{
  ClutterBehaviourOpacity *self = CLUTTER_BEHAVIOUR_OPACITY(object); 

  if (self->priv)
    {
      if (self->priv->handler_id)
	g_signal_handler_disconnect 
	  (clutter_behaviour_get_timelime (CLUTTER_BEHAVIOUR(self)), 
					   self->priv->handler_id);
    }

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
  GObjectClass        *object_class;

  object_class = (GObjectClass*) klass;

  object_class->finalize     = clutter_behaviour_opacity_finalize;
  object_class->dispose      = clutter_behaviour_opacity_dispose;

  g_type_class_add_private (object_class, sizeof (ClutterBehaviourOpacityPrivate));
}

static void
clutter_behaviour_opacity_init (ClutterBehaviourOpacity *self)
{
  ClutterBehaviourOpacityPrivate *priv;

  self->priv = priv = CLUTTER_BEHAVIOUR_OPACITY_GET_PRIVATE (self);
}

static void
clutter_behaviour_opacity_frame_foreach (ClutterActor            *actor,
					 ClutterBehaviourOpacity *behave)
{
  gint32                          alpha;
  guint8                          opacity;
  ClutterBehaviourOpacityPrivate *priv;

  priv = CLUTTER_BEHAVIOUR_OPACITY_GET_PRIVATE (behave);

  alpha = clutter_timeline_get_alpha 
               (clutter_behaviour_get_timelime (CLUTTER_BEHAVIOUR(behave)));

  opacity = (alpha * (priv->opacity_end - priv->opacity_start)) 
                    / CLUTTER_TIMELINE_MAX_ALPHA;

  opacity += priv->opacity_start;

  clutter_actor_set_opacity (actor, opacity);
}

static void
clutter_behaviour_opacity_frame (ClutterTimeline *timelime, 
				 gint             frame_num, 
				 gpointer         data)
{
  ClutterBehaviourOpacity        *behave;

  behave = CLUTTER_BEHAVIOUR_OPACITY(data);

  clutter_behaviour_actors_foreach 
                     (CLUTTER_BEHAVIOUR(behave),
		      (GFunc)clutter_behaviour_opacity_frame_foreach,
		      GINT_TO_POINTER(frame_num));
}

ClutterBehaviour*
clutter_behaviour_opacity_new (ClutterTimeline *timeline,
			       guint8           opacity_start,
			       guint8           opacity_end)
{
  ClutterBehaviourOpacity *behave;

  behave = g_object_new (CLUTTER_TYPE_BEHAVIOUR_OPACITY, 
			 "timeline", timeline,
			 NULL);

  behave->priv->opacity_start = opacity_start;
  behave->priv->opacity_end   = opacity_end;

  /* FIXME: Should be part of regular behave functionality */
  behave->priv->handler_id 
           = g_signal_connect (timeline, 
			       "new-frame",  
			       G_CALLBACK (clutter_behaviour_opacity_frame), 
			       behave);  
  
  return CLUTTER_BEHAVIOUR(behave);
}
