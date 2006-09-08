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
 * SECTION:clutter-alpha
 * @short_description: A class for calculating an alpha value as a function
 * of time.
 *
 * #ClutterAlpha is a class for calculating an alpha value as a function
 * of time.
 *
 * When you create a #ClutterAlpha object you should attach a #ClutterTimeline
 * to it, to be used as a time source.  You need to supply the alpha
 * function to be invoked at each timeline "tick"; some common alpha functions
 * are available.
 */

#include "config.h"

#include "clutter-alpha.h"
#include "clutter-main.h"
#include "clutter-marshal.h"

G_DEFINE_TYPE (ClutterAlpha,
               clutter_alpha,
               G_TYPE_INITIALLY_UNOWNED);

struct _ClutterAlphaPrivate
{
  ClutterTimeline *timeline;

  guint32 alpha;
  
  ClutterAlphaFunc func;
  gpointer data;
  GDestroyNotify destroy;

  gint delay;
  guint is_paused : 1;
};

enum
{
  PROP_0,

  PROP_TIMELINE,
  PROP_DELAY,
  PROP_IS_PAUSED,
  PROP_ALPHA
};

/* Alpha funcs */

/**
 * clutter_alpha_ramp_inc_func:
 * @alpha: a #ClutterAlpha
 * @data: user data (ignored)
 *
 * Return value:
 *
 * Since: 0.2
 */
guint32 
clutter_alpha_ramp_inc_func (ClutterAlpha *alpha,
                             gpointer      data)
{
  ClutterTimeline *timeline;
  gint current_frame_num, nframes;

  g_return_val_if_fail (CLUTTER_IS_ALPHA (alpha), 0);

  timeline = clutter_alpha_get_timeline (alpha);

  current_frame_num = clutter_timeline_get_current_frame (timeline);
  nframes = clutter_timeline_get_n_frames (timeline);

  return (current_frame_num * CLUTTER_ALPHA_MAX) / nframes;
}

/**
 * clutter_alpha_ramp_dec_func:
 * @alpha: a #ClutterAlpha
 * @data: user data (ignored)
 *
 * Return value:
 *
 * Since: 0.2
 */
guint32 
clutter_alpha_ramp_dec_func (ClutterAlpha *alpha,
                             gpointer      data)
{
  ClutterTimeline *timeline;
  gint current_frame_num, nframes;

  g_return_val_if_fail (CLUTTER_IS_ALPHA (alpha), 0);

  timeline = clutter_alpha_get_timeline (alpha);

  current_frame_num = clutter_timeline_get_current_frame (timeline);
  nframes = clutter_timeline_get_n_frames (timeline);

  return ((nframes - current_frame_num) * CLUTTER_ALPHA_MAX) / nframes;
}

/**
 * clutter_alpha_ramp_func:
 * @alpha: a #ClutterAlpha
 * @data: user data (ignored)
 *
 * Return value:
 *
 * Since: 0.2
 */
guint32 
clutter_alpha_ramp_func (ClutterAlpha *alpha,
                         gpointer      data)
{
  ClutterTimeline *timeline;
  gint current_frame_num, nframes;
  guint32 retval;

  g_return_val_if_fail (CLUTTER_IS_ALPHA (alpha), 0);

  timeline = clutter_alpha_get_timeline (alpha);

  current_frame_num = clutter_timeline_get_current_frame (timeline);
  nframes = clutter_timeline_get_n_frames (timeline);

  if (current_frame_num > (nframes / 2))
    {
      retval = (nframes - current_frame_num) * CLUTTER_ALPHA_MAX;
      retval = retval / (nframes / 2);
    }
  else
    {
      retval = current_frame_num * CLUTTER_ALPHA_MAX;
      retval = retval / (nframes / 2);
    }

  return retval;
}

/* Object */

static void
timeline_new_frame_cb (ClutterTimeline *timeline,
                       guint            current_frame_num,
                       ClutterAlpha    *alpha)
{
  ClutterAlphaPrivate *priv = alpha->priv;

  if (priv->is_paused)
    return;

  if ((priv->delay != -1) && (current_frame_num < priv->delay))
    return;

  /* Update alpha value */
  if (priv->func)
    {
      g_object_ref (alpha);
      
      priv->alpha = priv->func (alpha, priv->data);
      g_object_notify (G_OBJECT (alpha), "alpha");
      
      g_object_unref (alpha);
    }
}

static void 
clutter_alpha_set_property (GObject      *object, 
			    guint         prop_id,
			    const GValue *value, 
			    GParamSpec   *pspec)
{
  ClutterAlpha *alpha = CLUTTER_ALPHA (object);

  switch (prop_id) 
    {
    case PROP_TIMELINE:
      clutter_alpha_set_timeline (alpha, g_value_get_object (value));
      break;
    case PROP_DELAY:
      alpha->priv->delay = g_value_get_int (value);
      break;
    case PROP_IS_PAUSED:
      alpha->priv->is_paused = g_value_get_boolean (value);
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
  ClutterAlphaPrivate *priv;

  priv = CLUTTER_ALPHA (object)->priv;

  switch (prop_id) 
    {
    case PROP_TIMELINE:
      g_value_set_object (value, priv->timeline);
      break;
    case PROP_ALPHA:
      g_value_set_uint (value, priv->alpha);
      break;
    case PROP_DELAY:
      g_value_set_int (value, priv->delay);
      break;
    case PROP_IS_PAUSED:
      g_value_set_boolean (value, priv->is_paused);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
clutter_alpha_finalize (GObject *object)
{
  ClutterAlphaPrivate *priv = CLUTTER_ALPHA (object)->priv;

  if (priv->destroy)
    {
      priv->destroy (priv->data);
      
      priv->data = NULL;
      priv->destroy = NULL;
      priv->func = NULL;
    }

  G_OBJECT_CLASS (clutter_alpha_parent_class)->finalize (object);
}

static void 
clutter_alpha_dispose (GObject *object)
{
  ClutterAlpha *self = CLUTTER_ALPHA (object);

  clutter_alpha_set_timeline (self, NULL);

  G_OBJECT_CLASS (clutter_alpha_parent_class)->dispose (object);
}


static void
clutter_alpha_class_init (ClutterAlphaClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = clutter_alpha_set_property;
  object_class->get_property = clutter_alpha_get_property;
  object_class->finalize     = clutter_alpha_finalize;
  object_class->dispose      = clutter_alpha_dispose;

  g_type_class_add_private (klass, sizeof (ClutterAlphaPrivate));

  /**
   * ClutterAlpha:timeline
   *
   * The #ClutterTimeline object to be used when calculating
   * the alpha value.
   *
   * Since: 0.2
   */
  g_object_class_install_property (object_class,
    PROP_TIMELINE,
    g_param_spec_object ("timeline",
		         "Timeline",
		         "Timeline",
		         CLUTTER_TYPE_TIMELINE,
		         G_PARAM_READWRITE));
  /**
   * ClutterAlpha:alpha
   *
   * The last computed value of the alpha function.
   *
   * Since: 0.2
   */
  g_object_class_install_property (object_class,
    PROP_ALPHA,
    g_param_spec_uint ("alpha",
		       "Alpha",
		       "Alpha value",
		       CLUTTER_ALPHA_MIN,
		       CLUTTER_ALPHA_MAX,
		       CLUTTER_ALPHA_MIN,
		       G_PARAM_READABLE));
  /**
   * ClutterAlpha:delay
   *
   * The number of frames that should be skipped before
   * starting the calculation of the alpha function.
   *
   * Since: 0.2
   */
  g_object_class_install_property (object_class,
    PROP_DELAY,
    g_param_spec_int ("delay",
                      "Delay",
                      "The number of frames that should be skipped "
                      "before computing the alpha function",
                      0,
                      G_MAXINT,
                      0,
                      G_PARAM_READWRITE));
  /**
   * ClutterAlpha:is-paused
   *
   * Whether the #ClutterAlpha should be paused or not;
   * the timeline bound to the #ClutterAlpha object will
   * continue to run.
   *
   * Since: 0.2
   */
  g_object_class_install_property (object_class,
    PROP_IS_PAUSED,
    g_param_spec_boolean ("is-paused",
                          "Is Paused",
                          "Whether the alpha should be paused or not",
                          FALSE,
                          G_PARAM_READWRITE));
}

static void
clutter_alpha_init (ClutterAlpha *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
					    CLUTTER_TYPE_ALPHA,
					    ClutterAlphaPrivate);

  self->priv->func = CLUTTER_ALPHA_RAMP_INC;

  self->priv->delay = -1;
  self->priv->is_paused = FALSE;
}

/**
 * clutter_alpha_get_value:
 * @alpha: A #ClutterAlpha
 *
 * Query the current alpha value.
 *
 * Return value: The current alpha value for the alpha
 *
 * Since: 0.2
 */
guint32
clutter_alpha_get_value (ClutterAlpha *alpha)
{
  g_return_val_if_fail (CLUTTER_IS_ALPHA (alpha), FALSE);
  
  return alpha->priv->alpha;
}

/**
 * clutter_alpha_set_func:
 * @alpha: A #ClutterAlpha
 * @func: A #ClutterAlphaAlphaFunc
 * @data: the data to be passed to func or %NULL
 * @destroy: the function to be called when removing the previous
 *   alpha function or %NULL
 *
 * Since: 0.2
 */
void
clutter_alpha_set_func (ClutterAlpha     *alpha,
		        ClutterAlphaFunc  func,
                        gpointer          data,
                        GDestroyNotify    destroy)
{
  ClutterAlphaPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ALPHA (alpha));
  g_return_if_fail (func != NULL);
  
  priv = alpha->priv;

  if (priv->destroy)
    {
      priv->destroy (priv->data);
      priv->destroy = NULL;
      priv->data = NULL;
    }
  
  priv->func = func;
  priv->data = data;
  priv->destroy = destroy;
}

/**
 * clutter_alpha_set_timeline:
 * @alpha: A #ClutterAlpha
 * @timeline: A #ClutterTimeline or %NULL to unset the timeline
 *
 * Binds @timeline to @alpha.  Since an alpha is a function of time,
 * @timeline will be used to compute the alpha each time the "new-frame"
 * signal of @timeline is emitted.
 *
 * Since: 0.2
 */
void
clutter_alpha_set_timeline (ClutterAlpha    *alpha,
                            ClutterTimeline *timeline)
{
  ClutterAlphaPrivate *priv;

  g_return_if_fail (CLUTTER_IS_ALPHA (alpha));
  g_return_if_fail (timeline == NULL || CLUTTER_IS_TIMELINE (timeline));

  priv = alpha->priv;

  g_object_ref (alpha);

  if (priv->timeline)
    {
      g_signal_handlers_disconnect_by_func (priv->timeline,
                                            timeline_new_frame_cb,
                                            alpha);

      g_object_unref (priv->timeline);
      priv->timeline = NULL;
    }

  if (timeline)
    {
      priv->timeline = g_object_ref (timeline);

      g_signal_connect (priv->timeline, "new-frame",
                        G_CALLBACK (timeline_new_frame_cb),
                        alpha);
    }

  g_object_notify (G_OBJECT (alpha), "timeline");

  g_object_unref (alpha);
}

/**
 * clutter_alpha_get_timeline:
 * @alpha: A #ClutterAlpha
 *
 * Gets the #ClutterTimeline object bound to @alpha.
 *
 * Return value: The #ClutterTimeline bount to @alpa or %NULL.
 *
 * Since: 0.2
 */
ClutterTimeline *
clutter_alpha_get_timeline (ClutterAlpha *alpha)
{
  g_return_val_if_fail (CLUTTER_IS_ALPHA (alpha), NULL);

  return alpha->priv->timeline;
}

/**
 * clutter_alpha_get_delay:
 * @alpha: a #ClutterAlpha
 *
 * Gets the delay used by @alpha.  See clutter_alpha_set_delay().
 *
 * Return value: the number of frames to wait, or -1
 *
 * Since: 0.2
 */
gint
clutter_alpha_get_delay (ClutterAlpha *alpha)
{
  g_return_val_if_fail (CLUTTER_IS_ALPHA (alpha), -1);

  return alpha->priv->delay;
}

/**
 * clutter_alpha_set_delay:
 * @alpha: a #ClutterAlpha
 * @delay: the number of frames to wait or -1 for no delay
 *
 * Sets the number of timeline frames to wait before starting to
 * compute the alpha value.
 *
 * Since: 0.2
 */
void
clutter_alpha_set_delay (ClutterAlpha *alpha,
                         gint          delay)
{
  g_return_if_fail (CLUTTER_IS_ALPHA (alpha));

  if (alpha->priv->delay != delay)
    {
      g_object_ref (alpha);

      alpha->priv->delay = delay;
      g_object_notify (G_OBJECT (alpha), "delay");

      g_object_unref (alpha);
    }
}

/**
 * clutter_alpha_get_is_paused:
 * @alpha: a #ClutterAlpha
 *
 * Gets whether @alpha is in paused state or not.
 * See clutter_alpha_set_is_paused().
 *
 * Return value: %TRUE if @alpha is paused
 *
 * Since: 0.2
 */
gboolean
clutter_alpha_get_is_paused (ClutterAlpha *alpha)
{
  g_return_val_if_fail (CLUTTER_IS_ALPHA (alpha), FALSE);

  return alpha->priv->is_paused;
}

/**
 * clutter_alpha_set_is_paused:
 * @alpha: a #ClutterAlpha
 * @is_paused: %TRUE to pause the alpha
 *
 * Pauses the calculation of the alpha value of @alpha.  This is
 * independent of the running state of the #ClutterTimeline bount
 * to @alpha.
 *
 * Since: 0.2
 */
void
clutter_alpha_set_is_paused (ClutterAlpha *alpha,
                             gboolean      is_paused)
{
  g_return_if_fail (CLUTTER_IS_ALPHA (alpha));

  if (alpha->priv->is_paused != is_paused)
    {
      g_object_ref (alpha);

      alpha->priv->is_paused = is_paused;
      g_object_notify (G_OBJECT (alpha), "is-paused");

      g_object_unref (alpha);
    }
}
/**
 * clutter_alpha_new:
 * @timeline: a #ClutterTimeline or %NULL
 * @func: a #ClutterAlphaFunc alpha function
 * @data: data to be passed to func, or %NULL
 *
 * Creates a new #ClutterAlpha instance, using @timeline as the time
 * source and @func as the function to compute the alpha(t) value.
 *
 * Return value: a new #ClutterAlpha
 *
 * Since: 0.2
 */
ClutterAlpha *
clutter_alpha_new (ClutterTimeline  *timeline,
		   ClutterAlphaFunc  func,
                   gpointer          data)
{
  return clutter_alpha_new_full (timeline, func, data, NULL);
}

/**
 * clutter_alpha_new_full:
 * @timeline: #ClutterTimeline or %NULL
 * @func: a #ClutterAlphaFunc
 * @data: data to be passed to func
 * @destroy: function to be called when removing the alpha function
 *
 * Creates a new #ClutterAlpha instance, using @timeline as the
 * time source and @func as the function to compute the alpha(t)
 * value.
 *
 * You should use this constructor in bindings or if you want to
 * let the #ClutterAlpha object control the lifetime of @data.
 *
 * Return value: the newly created #ClutterAlpha instance
 *
 * Since: 0.2
 */
ClutterAlpha *
clutter_alpha_new_full (ClutterTimeline  *timeline,
                        ClutterAlphaFunc  func,
                        gpointer          data,
                        GDestroyNotify    destroy)
{
  ClutterAlpha *retval;

  g_return_val_if_fail (timeline == NULL || CLUTTER_IS_TIMELINE (timeline), NULL);
  g_return_val_if_fail (func != NULL, NULL);

  retval = g_object_new (CLUTTER_TYPE_ALPHA,
                         "timeline", timeline,
                         NULL);
  clutter_alpha_set_func (retval, func, data, destroy);

  return retval;
}
