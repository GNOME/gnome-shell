/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' library.
 *
 * Authored By Matthew Allum  <mallum@openedhand.com>
 *             Jorn Baayen  <jorn@openedhand.com>
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
 * SECTION:clutter-alpha
 * @short_description: A class for calculating an alpha value as a function
 * of time.
 *
 * #ClutterAlpha is a class for calculating an alpha value as a function
 * of time.
 */


#include "clutter-alpha.h"
#include "clutter-main.h"
#include "clutter-marshal.h"

G_DEFINE_TYPE (ClutterAlpha, clutter_alpha, G_TYPE_OBJECT);

struct ClutterAlphaPrivate
{
  ClutterTimeline *timeline;
  guint            timeline_new_frame_id;
  guint32          alpha;
  ClutterAlphaFunc func;
};

enum
{
  PROP_0,
  PROP_TIMELINE,
  PROP_FUNC,
  PROP_ALPHA
};

/* Alpha funcs */

guint32 
clutter_alpha_ramp_inc_func (ClutterAlpha *alpha)
{
  int current_frame_num, nframes;

  current_frame_num =
          clutter_timeline_get_current_frame (alpha->priv->timeline);
  nframes =
          clutter_timeline_get_n_frames (alpha->priv->timeline);

  return (current_frame_num * CLUTTER_ALPHA_MAX_ALPHA) / nframes;
}

guint32 
clutter_alpha_ramp_dec_func (ClutterAlpha *alpha)
{
  int current_frame_num, nframes;

  current_frame_num =
          clutter_timeline_get_current_frame (alpha->priv->timeline);
  nframes =
          clutter_timeline_get_n_frames (alpha->priv->timeline);

  return ((nframes - current_frame_num) * CLUTTER_ALPHA_MAX_ALPHA) / nframes;
}

guint32 
clutter_alpha_ramp_func (ClutterAlpha *alpha)
{
  int current_frame_num, nframes;

  current_frame_num =
          clutter_timeline_get_current_frame (alpha->priv->timeline);
  nframes =
          clutter_timeline_get_n_frames (alpha->priv->timeline);

  if (current_frame_num > (nframes/2))
    {
      return ((nframes - current_frame_num) 
	      * CLUTTER_ALPHA_MAX_ALPHA) / (nframes/2);
    }
  else
    {
      return (current_frame_num * CLUTTER_ALPHA_MAX_ALPHA) / (nframes/2);
    }
}

/* Object */

static void
timeline_new_frame_cb (ClutterTimeline *timeline,
                       guint            current_frame_num,
                       ClutterAlpha    *alpha)
{
  ClutterAlphaPrivate *priv;

  priv = alpha->priv;

  /* Update alpha value */
  if (priv->func) {
    priv->alpha = priv->func(alpha);

    g_object_notify (G_OBJECT (alpha), "alpha");
  }
}

static void 
clutter_alpha_set_property (GObject      *object, 
			    guint         prop_id,
			    const GValue *value, 
			    GParamSpec   *pspec)
{
  ClutterAlpha        *alpha;
  ClutterAlphaPrivate *priv;

  alpha = CLUTTER_ALPHA(object);
  priv = alpha->priv;

  switch (prop_id) 
    {
    case PROP_TIMELINE:
      clutter_alpha_set_timeline (alpha, g_value_get_object (value));
      break;
    case PROP_FUNC:
      priv->func = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void 
clutter_alpha_get_property (GObject    *object, 
			    guint       prop_id,
			    GValue     *value, 
			    GParamSpec *pspec)
{
  ClutterAlpha        *alpha;
  ClutterAlphaPrivate *priv;

  alpha = CLUTTER_ALPHA(object);
  priv = alpha->priv;

  switch (prop_id) 
    {
    case PROP_TIMELINE:
      g_value_set_object (value, priv->timeline);
      break;
    case PROP_FUNC:
      g_value_set_pointer (value, priv->func);
      break;
    case PROP_ALPHA:
      g_value_set_uint (value, priv->alpha);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
clutter_alpha_finalize (GObject *object)
{
  G_OBJECT_CLASS (clutter_alpha_parent_class)->finalize (object);
}

static void 
clutter_alpha_dispose (GObject *object)
{
  ClutterAlpha *self = CLUTTER_ALPHA(object);

  clutter_alpha_set_timeline (self, NULL);

  G_OBJECT_CLASS (clutter_alpha_parent_class)->dispose (object);
}


static void
clutter_alpha_class_init (ClutterAlphaClass *klass)
{
  GObjectClass *object_class;

  object_class = (GObjectClass*) klass;

  object_class->set_property = clutter_alpha_set_property;
  object_class->get_property = clutter_alpha_get_property;
  object_class->finalize     = clutter_alpha_finalize;
  object_class->dispose      = clutter_alpha_dispose;

  g_type_class_add_private (klass, sizeof (ClutterAlphaPrivate));

  g_object_class_install_property
    (object_class, PROP_TIMELINE,
     g_param_spec_object ("timeline",
		          "Timeline",
		          "Timeline",
		          CLUTTER_TYPE_TIMELINE,
		          G_PARAM_READWRITE));

  g_object_class_install_property
    (object_class, PROP_FUNC,
     g_param_spec_pointer ("func",
		           "Alpha function",
		           "Alpha function",
		           G_PARAM_READWRITE));

  g_object_class_install_property
    (object_class, PROP_ALPHA,
     g_param_spec_uint ("alpha",
		        "Alpha value",
		        "Alpha value",
		        0,
		        CLUTTER_ALPHA_MAX_ALPHA,
		        0,
		        G_PARAM_READABLE));
}

static void
clutter_alpha_init (ClutterAlpha *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
					   CLUTTER_TYPE_ALPHA,
					   ClutterAlphaPrivate);

  self->priv->func = CLUTTER_ALPHA_RAMP_INC;
}

/**
 * clutter_alpha_get_alpha:
 * @alpha: A #ClutterAlpha
 *
 * Query the current alpha value.
 *
 * Return Value: The current alpha value for the alpha
 */
gint32
clutter_alpha_get_alpha (ClutterAlpha *alpha)
{
  g_return_val_if_fail (CLUTTER_IS_ALPHA (alpha), FALSE);
  
  return alpha->priv->alpha;
}

/**
 * clutter_alpha_set_func:
 * @alpha: A #ClutterAlpha
 * @func: A #ClutterAlphaAlphaFunc
 *
 */
void
clutter_alpha_set_func (ClutterAlpha    *alpha,
		        ClutterAlphaFunc func)
{
  alpha->priv->func = func;
}

/**
 * clutter_alpha_set_timeline:
 * @alpha: A #ClutterAlpha
 * @timeline: A #ClutterTimeline
 *
 * Binds @alpha to @timeline.
 */
void
clutter_alpha_set_timeline (ClutterAlpha *alpha,
                            ClutterTimeline *timeline)
{
  if (alpha->priv->timeline)
    {
      g_signal_handler_disconnect (alpha->priv->timeline,
                                   alpha->priv->timeline_new_frame_id);

      g_object_unref (alpha->priv->timeline);
      alpha->priv->timeline = NULL;
    }

  if (timeline)
    {
      alpha->priv->timeline = g_object_ref (timeline);

      alpha->priv->timeline_new_frame_id =
              g_signal_connect (alpha->priv->timeline,
                                "new-frame",
                                G_CALLBACK (timeline_new_frame_cb),
                                alpha);
    }
}

/**
 * clutter_alpha_get_timeline:
 * @alpha: A #ClutterAlpha
 *
 * Return value: The #ClutterTimeline
 */
ClutterTimeline *
clutter_alpha_get_timeline (ClutterAlpha *alpha)
{
  return alpha->priv->timeline;
}

/**
 * clutter_alpha_new:
 * @timeline: #ClutterTimeline timeline
 * @func: #ClutterAlphaFunc alpha function
 *
 * Create a new  #ClutterAlpha instance.
 *
 * Return Value: a new #ClutterAlpha
 */
ClutterAlpha*
clutter_alpha_new (ClutterTimeline *timeline,
		   ClutterAlphaFunc func)
{
  return g_object_new (CLUTTER_TYPE_ALPHA, 
		       "timeline", timeline, 
		       "func", func,
		       NULL);
}
